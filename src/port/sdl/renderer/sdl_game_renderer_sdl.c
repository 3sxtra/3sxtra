/**
 * @file sdl_game_renderer_sdl.c
 * @brief SDL2D backend — game rendering via SDL_Renderer (SDL3's 2D API).
 *
 * Ported from the perf_fix_for_low_end_devices branch.
 * Uses SDL_RenderGeometry for batched quad rendering with z-sorting.
 * No shaders, no GL context, no ImGui — bare-bones renderer for
 * maximum compatibility on low-end devices (RPi4, old GPUs).
 */
#include "common.h"
#include "port/tracy_zones.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/renderer/sdl_game_renderer.h"
#include "port/sdl/renderer/sdl_game_renderer_internal.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"

#include <libgraph.h>

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>

#include "radix_sort.h"

#define RENDER_TASK_MAX 8192
#define TEXTURES_TO_DESTROY_MAX 1024

// ⚡ Optimization A: Structure-of-Arrays (SoA) for RenderTask
// Split 104-byte struct into parallel arrays to keep Z-sorting strictly within L1 cache.
static float task_z[RENDER_TASK_MAX];
static SDL_Texture* task_texture[RENDER_TASK_MAX];
static unsigned int task_th[RENDER_TASK_MAX]; // combined texture+palette handle for batch-breaking
static SDL_Vertex task_verts[RENDER_TASK_MAX][4];
static bool task_is_rect[RENDER_TASK_MAX];          // ⚡ true = axis-aligned rect eligible for SDL_RenderTexture

// ⚡ Software-frame SoA extensions — populated at enqueue time, consumed by sw_render_frame().
static SDL_FRect task_src_rect[RENDER_TASK_MAX];     // Source UV rect (normalized 0–1) for textured rects
static SDL_FRect task_dst_rect[RENDER_TASK_MAX];     // Destination pixel rect (screen space)
static SDL_FlipMode task_flip[RENDER_TASK_MAX];      // Horizontal/vertical flip flags
static uint32_t task_color32[RENDER_TASK_MAX];       // Packed RGBA8888 color for modulation

static SDL_Texture* cps3_canvas = NULL;

static const int cps3_width = 384;
static const int cps3_height = 224;
static SDL_Surface* surfaces[FL_TEXTURE_MAX] = { NULL };
static SDL_Palette* palettes[FL_PALETTE_MAX] = { NULL };

// Non-indexed texture cache (PSMCT16/PSMCT32 — eagerly created)
static SDL_Texture* texture_cache[FL_TEXTURE_MAX] = { NULL };

// ⚡ Per-texture multi-slot RGBA cache keyed by (tex, palette).
// Eliminates SDL_BlitSurface+SDL_UpdateTexture from the hot render loop.
// Each indexed texture can cache up to IDX_PAL_SLOTS pre-baked RGBA textures.
// Real-world max observed: 14 palettes/texture — 16 slots = zero evictions.
#define IDX_PAL_SLOTS 16
static SDL_Texture* idx_pal_tex[FL_TEXTURE_MAX][IDX_PAL_SLOTS];    // baked RGBA textures
static int          idx_pal_handle[FL_TEXTURE_MAX][IDX_PAL_SLOTS]; // palette handle (0=empty)
static uint32_t     idx_pal_hash[FL_TEXTURE_MAX][IDX_PAL_SLOTS];   // hash at bake time
static uint8_t      idx_pal_lru[FL_TEXTURE_MAX][IDX_PAL_SLOTS];    // LRU age stamp
static uint8_t      idx_pal_lru_clock[FL_TEXTURE_MAX];             // per-texture LRU tick

// ⚡ Parallel pixel cache — persistent RGBA8888 pixel data alongside SDL_Texture.
// Populated during bake_idx_tex, read by sw_raster_textured (zero per-frame bake cost).
static uint32_t*    idx_pal_pixels[FL_TEXTURE_MAX][IDX_PAL_SLOTS]; // cached RGBA8888 pixel buffers
static int          idx_pal_pixels_w[FL_TEXTURE_MAX][IDX_PAL_SLOTS]; // pixel buffer width
static int          idx_pal_pixels_h[FL_TEXTURE_MAX][IDX_PAL_SLOTS]; // pixel buffer height

// ⚡ Per-palette content hash — computed in CreatePalette, used to detect stale cache entries.
static uint32_t palette_hash[FL_PALETTE_MAX];

// ⚡ Shared scratch buffer for RGBA pixel conversion during bake_idx_tex.
// Large enough for the biggest texture observed (256×256 = 65536 px).
#define RGBA_SCRATCH_MAX (512 * 512)
static uint32_t rgba_scratch[RGBA_SCRATCH_MAX];

// ⚡ Non-indexed texture pixel cache — RGBA8888 conversion of PSMCT32/PSMCT16 surfaces.
// Populated lazily by sw_convert_nonidx_pixels, invalidated by UnlockTexture/DestroyTexture.
static uint32_t* nonidx_pixels[FL_TEXTURE_MAX];
static int       nonidx_pixels_w[FL_TEXTURE_MAX];
static int       nonidx_pixels_h[FL_TEXTURE_MAX];

// ⚡ Software-frame rendering state — CPU-side compositing into a single surface.
// Uses RGBA8888 to match LRU cache pixel format — zero format conversion.
static SDL_Surface* sw_frame_surface = NULL;          // 384×224 RGBA8888 compositing target
static SDL_Texture* sw_frame_upload_tex = NULL;       // Streaming texture for single-upload to GPU

// ⚡ Dirty tile tracking — 16×16 tile grid over 384×224 framebuffer.
// Only tiles touched by current OR previous frame need clearing/redrawing.
// Saves memset + compositing cost on static screens (menus, pause).
enum {
    DT_SIZE  = 16,
    DT_COLS  = 24,   // 384 / 16
    DT_ROWS  = 14,   // 224 / 16
    DT_TOTAL = DT_COLS * DT_ROWS,  // 336
};
static uint8_t  dt_current[DT_TOTAL];    // tiles covered this frame
static uint8_t  dt_previous[DT_TOTAL];   // tiles covered last frame
static uint32_t dt_prev_clear_color = 0; // previous frame's RGBA8888 clear color
static bool     dt_prev_clear_valid = false;  // whether dt_prev_clear_color is initialized

// ⚡ Mark all tiles overlapping a screen-space rect as dirty in dt_current[].
static void dt_mark_rect(float fx, float fy, float fw, float fh) {
    int c0 = (int)SDL_floorf(fx) / DT_SIZE;
    int r0 = (int)SDL_floorf(fy) / DT_SIZE;
    int c1 = (int)SDL_ceilf(fx + fw - 1.0f) / DT_SIZE;
    int r1 = (int)SDL_ceilf(fy + fh - 1.0f) / DT_SIZE;
    if (c0 < 0) c0 = 0;
    if (c0 > DT_COLS - 1) c0 = DT_COLS - 1;
    if (r0 < 0) r0 = 0;
    if (r0 > DT_ROWS - 1) r0 = DT_ROWS - 1;
    if (c1 < 0) c1 = 0;
    if (c1 > DT_COLS - 1) c1 = DT_COLS - 1;
    if (r1 < 0) r1 = 0;
    if (r1 > DT_ROWS - 1) r1 = DT_ROWS - 1;
    for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
            dt_current[r * DT_COLS + c] = 1;
        }
    }
}

// ⚡ FNV-1a hash for palette color data.
static uint32_t fnv1a_hash(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t h = 0x811c9dc5u;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 0x01000193u;
    }
    return h;
}

static SDL_Texture* textures_to_destroy[TEXTURES_TO_DESTROY_MAX] = { NULL };
static int textures_to_destroy_count = 0;


// (RenderTask AoS replaced by SoA arrays)
static int render_task_count = 0;
static int render_task_order[RENDER_TASK_MAX]; // ⚡ Sorted indices for indirect sort

// ⚡ Sortedness tracking — count inversions during draw_quad to decide sort strategy
static int sort_inversions = 0;
static float last_submitted_z = -1e30f;

// ⚡ Cached texture binding — skip redundant SetTexture lookups
static unsigned int last_set_texture_th = 0;
static SDL_Texture* last_set_texture = NULL;

// Pre-allocated batch buffers for optimized rendering
static SDL_Vertex batch_vertices[RENDER_TASK_MAX * 4];
static int batch_indices[RENDER_TASK_MAX * 6];
static bool batch_buffers_initialized = false;

// ⚡ Radix sort scratch buffers (used when sort_inversions > INSERTION_SORT_THRESHOLD)
static uint32_t radix_keys[RENDER_TASK_MAX];
static int      radix_scratch[RENDER_TASK_MAX];


// ⚡ Epsilon for float comparison in rect detection (matches MiSTer's rect_task_epsilon)
static const float sw_rect_epsilon = 0.001f;
static inline bool sw_nearly_equal(float a, float b) {
    return SDL_fabsf(a - b) <= sw_rect_epsilon;
}

// Forward declarations for rect detection/normalization (used in RenderFrame before definition)
static bool is_axis_aligned_rect(const SDL_Vertex verts[4]);
static void normalize_rect_verts(SDL_Vertex verts[4]);

// Debugging and statistics
static bool draw_rect_borders = false;
static bool dump_textures = false;
static int debug_texture_index = 0;

// --- Per-Pixel Blending Utilities (ported from MiSTer FPGA build) ---
// Operate on RGBA8888 layout matching SDL_PIXELFORMAT_RGBA8888 (LRU cache format).
// Channel layout as uint32: R<<24 | G<<16 | B<<8 | A

// ⚡ Exact divide by 255 using shifts only — no UDIV instruction.
// Correct for all x ∈ [0, 65534], which covers all u8×u8 products.
#define DIV255(x) (((x) + 1u + (((x) >> 8) & 0xFFu)) >> 8)

// ⚡ Color modulate: channel-wise multiply two RGBA8888 pixels.
static inline uint32_t sw_modulate_rgba8888(uint32_t pixel, uint32_t color) {
    const uint32_t src_r = (pixel >> 24) & 0xFFu;
    const uint32_t src_g = (pixel >> 16) & 0xFFu;
    const uint32_t src_b = (pixel >>  8) & 0xFFu;
    const uint32_t src_a = pixel & 0xFFu;
    const uint32_t mod_r = (color >> 24) & 0xFFu;
    const uint32_t mod_g = (color >> 16) & 0xFFu;
    const uint32_t mod_b = (color >>  8) & 0xFFu;
    const uint32_t mod_a = color & 0xFFu;
    return (((src_r * mod_r + 127u) / 255u) << 24) |
           (((src_g * mod_g + 127u) / 255u) << 16) |
           (((src_b * mod_b + 127u) / 255u) <<  8) |
            ((src_a * mod_a + 127u) / 255u);
}

// ⚡ Alpha composite (src-over): early-out for α=0/255, full Porter-Duff otherwise.
// Both pixels are RGBA8888: R<<24 | G<<16 | B<<8 | A
static inline uint32_t sw_blend_rgba8888(uint32_t dst_pixel, uint32_t src_pixel) {
    const uint32_t src_a = src_pixel & 0xFFu;
    if (src_a == 0u) return dst_pixel;
    if (src_a == 255u) return src_pixel;

    const uint32_t dst_a = dst_pixel & 0xFFu;
    const uint32_t inv_src_a = 255u - src_a;
    const uint32_t out_a = src_a + ((dst_a * inv_src_a + 127u) / 255u);
    if (out_a == 0u) return 0u;

    const uint32_t src_r = (src_pixel >> 24) & 0xFFu;
    const uint32_t src_g = (src_pixel >> 16) & 0xFFu;
    const uint32_t src_b = (src_pixel >>  8) & 0xFFu;
    const uint32_t dst_r = (dst_pixel >> 24) & 0xFFu;
    const uint32_t dst_g = (dst_pixel >> 16) & 0xFFu;
    const uint32_t dst_b = (dst_pixel >>  8) & 0xFFu;

    // Premultiplied src-over compositing, then un-premultiply
    const uint32_t out_r = ((src_r * src_a + dst_r * dst_a * inv_src_a / 255u) + out_a / 2u) / out_a;
    const uint32_t out_g = ((src_g * src_a + dst_g * dst_a * inv_src_a / 255u) + out_a / 2u) / out_a;
    const uint32_t out_b = ((src_b * src_a + dst_b * dst_a * inv_src_a / 255u) + out_a / 2u) / out_a;
    return (out_r << 24) | (out_g << 16) | (out_b << 8) | out_a;
}

// ⚡ Convert engine ARGB8888 color to RGBA8888 for software-frame path.
// Engine: A<<24|R<<16|G<<8|B  →  RGBA: R<<24|G<<16|B<<8|A
static inline uint32_t sw_argb_to_rgba(uint32_t argb) {
    return (argb << 8) | (argb >> 24);
}

// ⚡ Clamp integer to [lo, hi]
static inline int sw_clamp(int val, int lo, int hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

// ⚡ Pre-multiplied solid blend (ported from MiSTer's blend_solid_argb8888).
// Pre-computes src channel premul values ONCE, then applies per-pixel.
// Fast path for dst_a==255 (opaque destination — the common case).
// All channels in RGBA8888 layout: R<<24|G<<16|B<<8|A
static inline uint32_t sw_blend_solid_rgba8888(uint32_t dst_pixel,
                                               uint32_t src_a,
                                               uint32_t inv_src_a,
                                               uint32_t src_r_premul,
                                               uint32_t src_g_premul,
                                               uint32_t src_b_premul) {
    const uint32_t dst_a = dst_pixel & 0xFFu;
    const uint32_t dst_r = (dst_pixel >> 24) & 0xFFu;
    const uint32_t dst_g = (dst_pixel >> 16) & 0xFFu;
    const uint32_t dst_b = (dst_pixel >>  8) & 0xFFu;

    if (dst_a == 255u) {
        // Fast path: opaque destination — no alpha compositing needed
        const uint32_t out_r = (src_r_premul + (dst_r * inv_src_a) + 127u) / 255u;
        const uint32_t out_g = (src_g_premul + (dst_g * inv_src_a) + 127u) / 255u;
        const uint32_t out_b = (src_b_premul + (dst_b * inv_src_a) + 127u) / 255u;
        return (out_r << 24) | (out_g << 16) | (out_b << 8) | 0xFFu;
    }

    const uint32_t out_a = src_a + ((dst_a * inv_src_a + 127u) / 255u);
    if (out_a == 0u) return 0u;

    const uint32_t dst_r_premul = dst_r * dst_a;
    const uint32_t dst_g_premul = dst_g * dst_a;
    const uint32_t dst_b_premul = dst_b * dst_a;
    const uint32_t out_r = ((src_r_premul + ((dst_r_premul * inv_src_a + 127u) / 255u)) + (out_a / 2u)) / out_a;
    const uint32_t out_g = ((src_g_premul + ((dst_g_premul * inv_src_a + 127u) / 255u)) + (out_a / 2u)) / out_a;
    const uint32_t out_b = ((src_b_premul + ((dst_b_premul * inv_src_a + 127u) / 255u)) + (out_a / 2u)) / out_a;
    return (out_r << 24) | (out_g << 16) | (out_b << 8) | out_a;
}

// --- PlayStation 2 Graphics Synthesizer CLUT index shuffle ---
// The PS2 GS stores 256-color CLUTs in a non-linear memory order.
// This LUT maps linear index (0-255) to the shuffled GS index.
static const Uint8 ps2_clut_shuffle[256] = {
    0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23,
    8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 48, 49, 50, 51, 52, 53, 54, 55,
    40, 41, 42, 43, 44, 45, 46, 47, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 80, 81, 82, 83, 84, 85, 86, 87,
    72, 73, 74, 75, 76, 77, 78, 79, 88, 89, 90, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 112, 113, 114, 115, 116, 117, 118, 119,
    104, 105, 106, 107, 108, 109, 110, 111, 120, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 133, 134, 135, 144, 145, 146, 147, 148, 149, 150, 151,
    136, 137, 138, 139, 140, 141, 142, 143, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167, 176, 177, 178, 179, 180, 181, 182, 183,
    168, 169, 170, 171, 172, 173, 174, 175, 184, 185, 186, 187, 188, 189, 190, 191,
    192, 193, 194, 195, 196, 197, 198, 199, 208, 209, 210, 211, 212, 213, 214, 215,
    200, 201, 202, 203, 204, 205, 206, 207, 216, 217, 218, 219, 220, 221, 222, 223,
    224, 225, 226, 227, 228, 229, 230, 231, 240, 241, 242, 243, 244, 245, 246, 247,
    232, 233, 234, 235, 236, 237, 238, 239, 248, 249, 250, 251, 252, 253, 254, 255
};

// --- Color Reading Functions ---

static void read_rgba32_color(Uint32 pixel, SDL_Color* color) {
    color->b = pixel & 0xFF;
    color->g = (pixel >> 8) & 0xFF;
    color->r = (pixel >> 16) & 0xFF;
    color->a = (pixel >> 24) & 0xFF;
}

// ⚡ LUT-based byte-to-float conversion — eliminates 4 float divisions per call.
// Pre-computed at compile time; used by read_rgba32_fcolor in the draw hot path.
static const float rgba8_to_float[256] = {
    0.0f/255, 1.0f/255, 2.0f/255, 3.0f/255, 4.0f/255, 5.0f/255, 6.0f/255, 7.0f/255,
    8.0f/255, 9.0f/255, 10.0f/255, 11.0f/255, 12.0f/255, 13.0f/255, 14.0f/255, 15.0f/255,
    16.0f/255, 17.0f/255, 18.0f/255, 19.0f/255, 20.0f/255, 21.0f/255, 22.0f/255, 23.0f/255,
    24.0f/255, 25.0f/255, 26.0f/255, 27.0f/255, 28.0f/255, 29.0f/255, 30.0f/255, 31.0f/255,
    32.0f/255, 33.0f/255, 34.0f/255, 35.0f/255, 36.0f/255, 37.0f/255, 38.0f/255, 39.0f/255,
    40.0f/255, 41.0f/255, 42.0f/255, 43.0f/255, 44.0f/255, 45.0f/255, 46.0f/255, 47.0f/255,
    48.0f/255, 49.0f/255, 50.0f/255, 51.0f/255, 52.0f/255, 53.0f/255, 54.0f/255, 55.0f/255,
    56.0f/255, 57.0f/255, 58.0f/255, 59.0f/255, 60.0f/255, 61.0f/255, 62.0f/255, 63.0f/255,
    64.0f/255, 65.0f/255, 66.0f/255, 67.0f/255, 68.0f/255, 69.0f/255, 70.0f/255, 71.0f/255,
    72.0f/255, 73.0f/255, 74.0f/255, 75.0f/255, 76.0f/255, 77.0f/255, 78.0f/255, 79.0f/255,
    80.0f/255, 81.0f/255, 82.0f/255, 83.0f/255, 84.0f/255, 85.0f/255, 86.0f/255, 87.0f/255,
    88.0f/255, 89.0f/255, 90.0f/255, 91.0f/255, 92.0f/255, 93.0f/255, 94.0f/255, 95.0f/255,
    96.0f/255, 97.0f/255, 98.0f/255, 99.0f/255, 100.0f/255, 101.0f/255, 102.0f/255, 103.0f/255,
    104.0f/255, 105.0f/255, 106.0f/255, 107.0f/255, 108.0f/255, 109.0f/255, 110.0f/255, 111.0f/255,
    112.0f/255, 113.0f/255, 114.0f/255, 115.0f/255, 116.0f/255, 117.0f/255, 118.0f/255, 119.0f/255,
    120.0f/255, 121.0f/255, 122.0f/255, 123.0f/255, 124.0f/255, 125.0f/255, 126.0f/255, 127.0f/255,
    128.0f/255, 129.0f/255, 130.0f/255, 131.0f/255, 132.0f/255, 133.0f/255, 134.0f/255, 135.0f/255,
    136.0f/255, 137.0f/255, 138.0f/255, 139.0f/255, 140.0f/255, 141.0f/255, 142.0f/255, 143.0f/255,
    144.0f/255, 145.0f/255, 146.0f/255, 147.0f/255, 148.0f/255, 149.0f/255, 150.0f/255, 151.0f/255,
    152.0f/255, 153.0f/255, 154.0f/255, 155.0f/255, 156.0f/255, 157.0f/255, 158.0f/255, 159.0f/255,
    160.0f/255, 161.0f/255, 162.0f/255, 163.0f/255, 164.0f/255, 165.0f/255, 166.0f/255, 167.0f/255,
    168.0f/255, 169.0f/255, 170.0f/255, 171.0f/255, 172.0f/255, 173.0f/255, 174.0f/255, 175.0f/255,
    176.0f/255, 177.0f/255, 178.0f/255, 179.0f/255, 180.0f/255, 181.0f/255, 182.0f/255, 183.0f/255,
    184.0f/255, 185.0f/255, 186.0f/255, 187.0f/255, 188.0f/255, 189.0f/255, 190.0f/255, 191.0f/255,
    192.0f/255, 193.0f/255, 194.0f/255, 195.0f/255, 196.0f/255, 197.0f/255, 198.0f/255, 199.0f/255,
    200.0f/255, 201.0f/255, 202.0f/255, 203.0f/255, 204.0f/255, 205.0f/255, 206.0f/255, 207.0f/255,
    208.0f/255, 209.0f/255, 210.0f/255, 211.0f/255, 212.0f/255, 213.0f/255, 214.0f/255, 215.0f/255,
    216.0f/255, 217.0f/255, 218.0f/255, 219.0f/255, 220.0f/255, 221.0f/255, 222.0f/255, 223.0f/255,
    224.0f/255, 225.0f/255, 226.0f/255, 227.0f/255, 228.0f/255, 229.0f/255, 230.0f/255, 231.0f/255,
    232.0f/255, 233.0f/255, 234.0f/255, 235.0f/255, 236.0f/255, 237.0f/255, 238.0f/255, 239.0f/255,
    240.0f/255, 241.0f/255, 242.0f/255, 243.0f/255, 244.0f/255, 245.0f/255, 246.0f/255, 247.0f/255,
    248.0f/255, 249.0f/255, 250.0f/255, 251.0f/255, 252.0f/255, 253.0f/255, 254.0f/255, 255.0f/255,
};

// ⚡ Single-entry color cache — exploits repeated vertex colors in hot path.
static Uint32 cached_fcolor_pixel = 0xDEADBEEF; // sentinel: impossible initial value
static SDL_FColor cached_fcolor_value;

static void read_rgba32_fcolor(Uint32 pixel, SDL_FColor* fcolor) {
    if (pixel == cached_fcolor_pixel) {
        *fcolor = cached_fcolor_value;
        return;
    }
    fcolor->b = rgba8_to_float[pixel & 0xFF];
    fcolor->g = rgba8_to_float[(pixel >> 8) & 0xFF];
    fcolor->r = rgba8_to_float[(pixel >> 16) & 0xFF];
    fcolor->a = rgba8_to_float[(pixel >> 24) & 0xFF];
    cached_fcolor_pixel = pixel;
    cached_fcolor_value = *fcolor;
}

// ⚡ LUT for 5-bit to 8-bit color conversion (16-bit PSMCT16 palettes)
// Used in read_rgba16_color to avoid 3 multiplications and 3 divisions per pixel.
static const Uint8 color5_to_8[32] = {
      0,   8,  16,  24,  32,  41,  49,  57,
     65,  74,  82,  90,  98, 106, 115, 123,
    131, 139, 148, 156, 164, 172, 180, 189,
    197, 205, 213, 222, 230, 238, 246, 255
};

static void read_rgba16_color(Uint16 pixel, SDL_Color* color) {
    color->r = color5_to_8[pixel & 0x1F];
    color->g = color5_to_8[(pixel >> 5) & 0x1F];
    color->b = color5_to_8[(pixel >> 10) & 0x1F];
    color->a = (pixel & 0x8000) ? 255 : 0;
}

static void read_color(const void* pixels, int index, size_t color_size, SDL_Color* color) {
    switch (color_size) {
    case 2: {
        const Uint16* rgba16_colors = (const Uint16*)pixels;
        read_rgba16_color(rgba16_colors[index], color);
        break;
    }
    case 4: {
        const Uint32* rgba32_colors = (const Uint32*)pixels;
        read_rgba32_color(rgba32_colors[index], color);
        break;
    }
    }
}

#define LERP_FLOAT(a, b, x) ((a) * (1.0f - (x)) + (b) * (x))

static void lerp_fcolors(SDL_FColor* dest, const SDL_FColor* a, const SDL_FColor* b, float x) {
    dest->r = LERP_FLOAT(a->r, b->r, x);
    dest->g = LERP_FLOAT(a->g, b->g, x);
    dest->b = LERP_FLOAT(a->b, b->b, x);
    dest->a = LERP_FLOAT(a->a, b->a, x);
}

// --- Texture Debugging ---

static void save_texture(const SDL_Surface* surface, const SDL_Palette* palette) {
    if (surface == NULL || palette == NULL) {
        SDL_Log("Cannot save texture: NULL surface or palette");
        return;
    }

    char filename[128];
    snprintf(filename, sizeof(filename), "textures/%d.tga", debug_texture_index);

    const Uint8* pixels = (const Uint8*)surface->pixels;
    const int width = surface->w;
    const int height = surface->h;

    FILE* f = fopen(filename, "wb");
    if (!f) {
        SDL_Log("Failed to open file for writing: %s", filename);
        return;
    }

    uint8_t header[18] = { 0 };
    header[2] = 2; // uncompressed RGB
    header[12] = width & 0xFF;
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;
    header[15] = (height >> 8) & 0xFF;
    header[16] = 32;   // bits per pixel
    header[17] = 0x20; // top-left origin

    fwrite(header, 1, 18, f);

    // Write pixels in BGRA format
    const int pixel_count = width * height;
    for (int i = 0; i < pixel_count; ++i) {
        Uint8 index;

        if (palette->ncolors == 16) {
            const Uint8 byte = pixels[i / 2];
            if (i & 1) {
                index = byte >> 4;
            } else {
                index = byte & 0x0F;
            }
        } else {
            index = pixels[i];
        }

        const SDL_Color* color = &palette->colors[index];
        const Uint8 bgr[] = { color->b, color->g, color->r, color->a };
        fwrite(bgr, 1, 4, f);
    }

    fclose(f);
    debug_texture_index = (debug_texture_index + 1) % 10000;
}

void SDLGameRendererSDL_DumpTextures(void) {
    SDL_CreateDirectory("textures");
    debug_texture_index = 0;
    int count = 0;

    // Dump each indexed texture for every cached palette slot
    for (int ti = 0; ti < FL_TEXTURE_MAX; ti++) {
        SDL_Surface* surf = surfaces[ti];
        if (!surf || !SDL_ISPIXELFORMAT_INDEXED(surf->format))
            continue;
        for (int s = 0; s < IDX_PAL_SLOTS; s++) {
            int ph = idx_pal_handle[ti][s];
            if (ph <= 0 || ph > FL_PALETTE_MAX) continue;
            SDL_Palette* pal = palettes[ph - 1];
            if (!pal) continue;
            save_texture(surf, pal);
            count++;
        }
    }

    SDL_Log("[TextureDump] Wrote %d texture(s) to textures/", count);
}


// --- Texture Stack Management ---

static SDL_Texture* textures[RENDER_TASK_MAX] = { NULL };
static int texture_count = 0;

static void push_texture(SDL_Texture* texture) {
    if (texture_count >= RENDER_TASK_MAX) {
        fatal_error("Texture stack overflow in push_texture");
    }
    textures[texture_count] = texture;
    texture_count += 1;
}

static SDL_Texture* get_texture(void) {
    if (texture_count == 0) { fatal_error("No textures to get"); }
    return textures[texture_count - 1];
}

// --- Deferred Texture Destruction ---

static void push_texture_to_destroy(SDL_Texture* texture) {
    if (textures_to_destroy_count >= TEXTURES_TO_DESTROY_MAX) {
        SDL_Log("Warning: textures_to_destroy buffer full, destroying texture immediately");
        SDL_DestroyTexture(texture);
        return;
    }
    textures_to_destroy[textures_to_destroy_count] = texture;
    textures_to_destroy_count += 1;
}

static void destroy_textures(void) {
    for (int i = 0; i < texture_count; i++) {
        textures[i] = NULL;
    }
    texture_count = 0;

    for (int i = 0; i < textures_to_destroy_count; i++) {
        SDL_DestroyTexture(textures_to_destroy[i]);
        textures_to_destroy[i] = NULL;
    }
    textures_to_destroy_count = 0;
}

// ⚡ Bake: convert indexed surface+palette → RGBA SDL_Texture, store in LRU slot.
// Called only on cache miss. Direct pixel conversion avoids SDL_BlitSurface overhead.
static SDL_Texture* bake_idx_tex(SDL_Renderer* renderer, int ti, int palette_handle) {
    SDL_Surface* surf = surfaces[ti];
    SDL_Palette* pal = palettes[palette_handle - 1];
    if (!surf || !pal) return NULL;

    // Find LRU eviction slot (prefer empty slots first)
    int evict = 0;
    uint8_t worst_age = 0;
    const uint8_t clock = idx_pal_lru_clock[ti];
    for (int s = 0; s < IDX_PAL_SLOTS; s++) {
        if (idx_pal_handle[ti][s] == 0) { evict = s; goto found; } // empty
        const uint8_t age = (uint8_t)(clock - idx_pal_lru[ti][s]);
        if (age > worst_age) { worst_age = age; evict = s; }
    }
    found:;

    // Reuse existing SDL_Texture if present in this slot (update pixels in place)
    SDL_Texture* tex = idx_pal_tex[ti][evict];
    if (tex == NULL) {
        tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                SDL_TEXTUREACCESS_STREAMING, surf->w, surf->h);
        if (!tex) return NULL;
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        idx_pal_tex[ti][evict] = tex;
    }

    // Direct palette→RGBA conversion into scratch buffer (no SDL_BlitSurface)
    const int pixel_count = surf->w * surf->h;
    const SDL_Color* colors = pal->colors;
    SDL_assert(pixel_count <= RGBA_SCRATCH_MAX);

    if (surf->format == SDL_PIXELFORMAT_INDEX8) {
        const uint8_t* src = (const uint8_t*)surf->pixels;
        for (int i = 0; i < pixel_count; i++) {
            const SDL_Color c = colors[src[i]];
            rgba_scratch[i] = ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16)
                             | ((uint32_t)c.b <<  8) | c.a;
        }
    } else {
        // PSMT4: 4-bit packed indices
        const uint8_t* src = (const uint8_t*)surf->pixels;
        for (int i = 0; i < pixel_count; i++) {
            const uint8_t idx4 = (i & 1) ? (src[i >> 1] >> 4) : (src[i >> 1] & 0xF);
            const SDL_Color c = colors[idx4];
            rgba_scratch[i] = ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16)
                             | ((uint32_t)c.b <<  8) | c.a;
        }
    }
    SDL_UpdateTexture(tex, NULL, rgba_scratch, surf->w * 4);

    // ⚡ Persist pixel data for software-frame path (zero per-frame bake cost).
    // Free old buffer if evicting a slot with different dimensions.
    if (idx_pal_pixels[ti][evict] != NULL &&
        (idx_pal_pixels_w[ti][evict] != surf->w || idx_pal_pixels_h[ti][evict] != surf->h)) {
        SDL_free(idx_pal_pixels[ti][evict]);
        idx_pal_pixels[ti][evict] = NULL;
    }
    if (idx_pal_pixels[ti][evict] == NULL) {
        idx_pal_pixels[ti][evict] = (uint32_t*)SDL_malloc(pixel_count * sizeof(uint32_t));
    }
    if (idx_pal_pixels[ti][evict] != NULL) {
        SDL_memcpy(idx_pal_pixels[ti][evict], rgba_scratch, pixel_count * sizeof(uint32_t));
        idx_pal_pixels_w[ti][evict] = surf->w;
        idx_pal_pixels_h[ti][evict] = surf->h;
    }

    idx_pal_handle[ti][evict] = palette_handle;
    idx_pal_hash[ti][evict]   = palette_hash[palette_handle - 1];
    idx_pal_lru[ti][evict]    = idx_pal_lru_clock[ti]++;
    return tex;
}

// ⚡ Lookup: return pre-baked texture for (ti, palette_handle), baking on miss.
static SDL_Texture* lookup_idx_tex(SDL_Renderer* renderer, int ti, int palette_handle) {
    if (palette_handle <= 0 || palette_handle > FL_PALETTE_MAX) return NULL;
    const uint32_t want_hash = palette_hash[palette_handle - 1];
    for (int s = 0; s < IDX_PAL_SLOTS; s++) {
        if (idx_pal_handle[ti][s] == palette_handle
                && idx_pal_hash[ti][s] == want_hash) {
            idx_pal_lru[ti][s] = idx_pal_lru_clock[ti]++; // touch LRU
            return idx_pal_tex[ti][s];
        }
    }
    return bake_idx_tex(renderer, ti, palette_handle);
}

// --- Software-Frame Lifecycle ---

static bool ensure_sw_frame_surface(void) {
    if (sw_frame_surface != NULL) return true;
    sw_frame_surface = SDL_CreateSurface(cps3_width, cps3_height, SDL_PIXELFORMAT_RGBA8888);
    return sw_frame_surface != NULL;
}

static bool ensure_sw_frame_upload_texture(void) {
    if (sw_frame_upload_tex != NULL) return true;
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    sw_frame_upload_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_STREAMING, cps3_width, cps3_height);
    if (!sw_frame_upload_tex) return false;
    SDL_SetTextureScaleMode(sw_frame_upload_tex, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(sw_frame_upload_tex, SDL_BLENDMODE_NONE);
    return true;
}

// ⚡ Look up cached RGBA8888 pixels from the LRU cache for a given (tex, palette).
// Returns pointer to persistent pixel data populated during bake_idx_tex.
// Zero per-frame cost on cache hit (pixel data was baked on texture/palette change).
static const uint32_t* sw_lookup_cached_pixels(int ti, int palette_handle, int* out_w, int* out_h) {
    if (palette_handle <= 0 || palette_handle > FL_PALETTE_MAX) return NULL;
    const uint32_t want_hash = palette_hash[palette_handle - 1];
    for (int s = 0; s < IDX_PAL_SLOTS; s++) {
        if (idx_pal_handle[ti][s] == palette_handle
                && idx_pal_hash[ti][s] == want_hash
                && idx_pal_pixels[ti][s] != NULL) {
            *out_w = idx_pal_pixels_w[ti][s];
            *out_h = idx_pal_pixels_h[ti][s];
            return idx_pal_pixels[ti][s];
        }
    }
    // Cache miss — trigger a bake (which will also populate idx_pal_pixels)
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    SDL_Texture* tex = lookup_idx_tex(renderer, ti, palette_handle);
    if (!tex) return NULL;
    // After bake, the pixel data should be in the cache
    for (int s = 0; s < IDX_PAL_SLOTS; s++) {
        if (idx_pal_handle[ti][s] == palette_handle
                && idx_pal_pixels[ti][s] != NULL) {
            *out_w = idx_pal_pixels_w[ti][s];
            *out_h = idx_pal_pixels_h[ti][s];
            return idx_pal_pixels[ti][s];
        }
    }
    return NULL;
}

// ⚡ Ensure non-indexed texture has RGBA8888 pixel cache populated.
// Converts ABGR8888/ABGR1555 surface to RGBA8888 lazily on first access.
// Returns cached pixel data or NULL on failure.
static const uint32_t* sw_ensure_nonidx_pixels(int ti, int* out_w, int* out_h) {
    if (nonidx_pixels[ti] != NULL) {
        *out_w = nonidx_pixels_w[ti];
        *out_h = nonidx_pixels_h[ti];
        return nonidx_pixels[ti];
    }
    SDL_Surface* surf = surfaces[ti];
    if (!surf) return NULL;
    const int pixel_count = surf->w * surf->h;
    if (pixel_count > RGBA_SCRATCH_MAX) return NULL;

    if (surf->format == SDL_PIXELFORMAT_ABGR8888) {
        const uint32_t* src = (const uint32_t*)surf->pixels;
        for (int i = 0; i < pixel_count; i++) {
            const uint32_t p = src[i];
            rgba_scratch[i] = ((p & 0xFFu) << 24) |
                              ((p & 0xFF00u) << 8) |
                              (((p >> 16) & 0xFFu) << 8) |
                              ((p >> 24) & 0xFFu);
        }
    } else if (surf->format == SDL_PIXELFORMAT_ABGR1555) {
        const uint16_t* src = (const uint16_t*)surf->pixels;
        for (int i = 0; i < pixel_count; i++) {
            const uint16_t p = src[i];
            const uint32_t r = color5_to_8[p & 0x1Fu];
            const uint32_t g = color5_to_8[(p >> 5) & 0x1Fu];
            const uint32_t b = color5_to_8[(p >> 10) & 0x1Fu];
            const uint32_t a = (p & 0x8000u) ? 255u : 0u;
            rgba_scratch[i] = (r << 24) | (g << 16) | (b << 8) | a;
        }
    } else {
        return NULL;
    }

    nonidx_pixels[ti] = (uint32_t*)SDL_malloc(pixel_count * sizeof(uint32_t));
    if (!nonidx_pixels[ti]) return NULL;
    SDL_memcpy(nonidx_pixels[ti], rgba_scratch, pixel_count * sizeof(uint32_t));
    nonidx_pixels_w[ti] = surf->w;
    nonidx_pixels_h[ti] = surf->h;
    *out_w = surf->w;
    *out_h = surf->h;
    return nonidx_pixels[ti];
}

// ⚡ Software-rasterize a textured rect task into the software-frame surface.
static bool sw_raster_textured(int task_idx) {
    const unsigned int th = task_th[task_idx];
    const int tex_handle = LO_16_BITS(th);
    const int pal_handle = HI_16_BITS(th);
    if (tex_handle <= 0 || tex_handle > FL_TEXTURE_MAX) return false;
    const int ti = tex_handle - 1;

    int src_tex_w, src_tex_h;
    const uint32_t* src_pixels = NULL;

    if (pal_handle > 0) {
        src_pixels = sw_lookup_cached_pixels(ti, pal_handle, &src_tex_w, &src_tex_h);
    } else {
        src_pixels = sw_ensure_nonidx_pixels(ti, &src_tex_w, &src_tex_h);
    }
    if (!src_pixels) return false;

    const SDL_FRect* dst_r = &task_dst_rect[task_idx];
    const SDL_FRect* src_uv = &task_src_rect[task_idx];
    const SDL_FlipMode flip = task_flip[task_idx];
    const uint32_t color = task_color32[task_idx];

    // Convert UV to pixel coords
    const int src_x = (int)SDL_roundf(src_uv->x * (float)src_tex_w);
    const int src_y = (int)SDL_roundf(src_uv->y * (float)src_tex_h);
    const int src_w = (int)SDL_roundf(src_uv->w * (float)src_tex_w);
    const int src_h = (int)SDL_roundf(src_uv->h * (float)src_tex_h);
    if (src_w <= 0 || src_h <= 0) return true; // degenerate — skip

    const int dst_x = (int)SDL_roundf(dst_r->x);
    const int dst_y = (int)SDL_roundf(dst_r->y);
    const int dst_w = (int)SDL_roundf(dst_r->w);
    const int dst_h = (int)SDL_roundf(dst_r->h);
    if (dst_w <= 0 || dst_h <= 0) return true; // degenerate — skip

    // Clamp destination to surface bounds
    const int dst_x0 = sw_clamp(dst_x, 0, cps3_width);
    const int dst_y0 = sw_clamp(dst_y, 0, cps3_height);
    const int dst_x1 = sw_clamp(dst_x + dst_w, 0, cps3_width);
    const int dst_y1 = sw_clamp(dst_y + dst_h, 0, cps3_height);
    if (dst_x1 <= dst_x0 || dst_y1 <= dst_y0) return true; // fully clipped

    uint32_t* dst_pixels = (uint32_t*)sw_frame_surface->pixels;
    const int dst_pitch = sw_frame_surface->pitch / (int)sizeof(uint32_t);
    const bool flip_h = (flip & SDL_FLIP_HORIZONTAL) != 0;
    const bool flip_v = (flip & SDL_FLIP_VERTICAL) != 0;
    const bool has_color_mod = (color != 0xFFFFFFFFu);

    if (src_w == dst_w && src_h == dst_h) {
        // ⚡ Exact copy path: 1:1 pixel mapping with flip/clip/color-mod support
        const int clip_left = dst_x0 - dst_x;
        const int clip_top = dst_y0 - dst_y;
        const int src_x_step = flip_h ? -1 : 1;
        const int src_y_step = flip_v ? -1 : 1;
        const int src_start_x = flip_h ? (src_x + src_w - 1 - clip_left) : (src_x + clip_left);
        const int src_start_y = flip_v ? (src_y + src_h - 1 - clip_top) : (src_y + clip_top);

        for (int row = 0; row < (dst_y1 - dst_y0); row++) {
            const int sy = src_start_y + row * src_y_step;
            if (sy < 0 || sy >= src_tex_h) continue;
            const uint32_t* src_row = src_pixels + sy * src_tex_w;
            uint32_t* dst_row = dst_pixels + (dst_y0 + row) * dst_pitch + dst_x0;
            int sx = src_start_x;
            for (int col = 0; col < (dst_x1 - dst_x0); col++) {
                if (sx >= 0 && sx < src_tex_w) {
                    uint32_t px = src_row[sx];
                    if (has_color_mod) px = sw_modulate_rgba8888(px, color);
                    dst_row[col] = sw_blend_rgba8888(dst_row[col], px);
                }
                sx += src_x_step;
            }
        }
    } else {
        // ⚡ Scaled copy path: pre-computed LUT eliminates float UV math per pixel.
        // Ported from MiSTer's populate_scaled_lookup_table + try_fast_copy_fast_textured_task.
        const int visible_w = dst_x1 - dst_x0;
        const int visible_h = dst_y1 - dst_y0;

        // Pre-compute per-destination-pixel source coordinate lookup tables
        int src_x_lut[384]; // max cps3_width
        int src_y_lut[224]; // max cps3_height
        for (int i = 0; i < visible_w; i++) {
            const int dst_off = (dst_x0 + i) - dst_x;
            const int src_off = (((dst_off * 2) + 1) * src_w) / (dst_w * 2);
            src_x_lut[i] = sw_clamp(flip_h ? (src_x + src_w - 1 - src_off) : (src_x + src_off),
                                    0, src_tex_w - 1);
        }
        for (int i = 0; i < visible_h; i++) {
            const int dst_off = (dst_y0 + i) - dst_y;
            const int src_off = (((dst_off * 2) + 1) * src_h) / (dst_h * 2);
            src_y_lut[i] = sw_clamp(flip_v ? (src_y + src_h - 1 - src_off) : (src_y + src_off),
                                    0, src_tex_h - 1);
        }

        for (int row = 0; row < visible_h; row++) {
            const uint32_t* src_row = src_pixels + src_y_lut[row] * src_tex_w;
            uint32_t* dst_row = dst_pixels + (dst_y0 + row) * dst_pitch + dst_x0;
            for (int col = 0; col < visible_w; col++) {
                uint32_t px = src_row[src_x_lut[col]];
                if (has_color_mod) px = sw_modulate_rgba8888(px, color);
                dst_row[col] = sw_blend_rgba8888(dst_row[col], px);
            }
        }
    }
    return true;
}

// ⚡ Software-rasterize a solid color rect task.
// task_dst_rect and task_color32 must be populated before calling.
static bool sw_raster_solid(int task_idx) {
    const SDL_FRect* dst_r = &task_dst_rect[task_idx];
    const uint32_t color = task_color32[task_idx]; // RGBA8888 format
    const uint32_t src_a = color & 0xFFu; // Alpha is low byte in RGBA8888
    if (src_a == 0u) return true; // fully transparent — skip

    const int x0 = sw_clamp((int)SDL_floorf(dst_r->x), 0, cps3_width);
    const int y0 = sw_clamp((int)SDL_floorf(dst_r->y), 0, cps3_height);
    const int x1 = sw_clamp((int)SDL_ceilf(dst_r->x + dst_r->w), 0, cps3_width);
    const int y1 = sw_clamp((int)SDL_ceilf(dst_r->y + dst_r->h), 0, cps3_height);
    if (x1 <= x0 || y1 <= y0) return true;

    uint32_t* dst_pixels = (uint32_t*)sw_frame_surface->pixels;
    const int dst_pitch = sw_frame_surface->pitch / (int)sizeof(uint32_t);
    const int fill_w = x1 - x0;

    if (src_a == 255u) {
        // Opaque fill — direct write
        for (int y = y0; y < y1; y++) {
            uint32_t* row = dst_pixels + y * dst_pitch + x0;
            for (int i = 0; i < fill_w; i++) row[i] = color;
        }
    } else {
        // ⚡ Semi-transparent — pre-multiplied solid blend (MiSTer optimization).
        // Pre-compute src channel premul values ONCE outside the pixel loop.
        const uint32_t inv_src_a = 255u - src_a;
        const uint32_t src_r = (color >> 24) & 0xFFu;
        const uint32_t src_g = (color >> 16) & 0xFFu;
        const uint32_t src_b = (color >>  8) & 0xFFu;
        const uint32_t src_r_premul = src_r * src_a;
        const uint32_t src_g_premul = src_g * src_a;
        const uint32_t src_b_premul = src_b * src_a;
        for (int y = y0; y < y1; y++) {
            uint32_t* row = dst_pixels + y * dst_pitch + x0;
            for (int i = 0; i < fill_w; i++) {
                row[i] = sw_blend_solid_rgba8888(row[i], src_a, inv_src_a,
                                                 src_r_premul, src_g_premul, src_b_premul);
            }
        }
    }
    return true;
}

// ⚡ Scanline triangle rasterizer with affine UV interpolation.
// Rasterizes one triangle (3 vertices with position, tex_coord) into sw_frame_surface.
// src_pixels is RGBA8888, tex dimensions are src_w × src_h.
typedef struct SwTriVert {
    float x, y, u, v;
} SwTriVert;

static void sw_raster_triangle(const SwTriVert* v0, const SwTriVert* v1, const SwTriVert* v2,
                                const uint32_t* src_pixels, int src_w, int src_h,
                                uint32_t color, uint32_t* dst_pixels, int dst_pitch,
                                int clip_w, int clip_h) {
    // Sort vertices by Y (top to bottom)
    const SwTriVert* top = v0;
    const SwTriVert* mid = v1;
    const SwTriVert* bot = v2;
    if (mid->y < top->y) { const SwTriVert* t = top; top = mid; mid = t; }
    if (bot->y < top->y) { const SwTriVert* t = top; top = bot; bot = t; }
    if (bot->y < mid->y) { const SwTriVert* t = mid; mid = bot; bot = t; }

    const float total_dy = bot->y - top->y;
    if (total_dy < 0.5f) return; // Degenerate triangle

    const bool has_color_mod = (color != 0xFFFFFFFFu);

    // Long edge slopes (top→bot, spans entire triangle height)
    const float inv_total_dy = 1.0f / total_dy;
    const float dx_long = (bot->x - top->x) * inv_total_dy;
    const float du_long = (bot->u - top->u) * inv_total_dy;
    const float dv_long = (bot->v - top->v) * inv_total_dy;

    // Upper half: top → mid
    const float upper_dy = mid->y - top->y;
    if (upper_dy >= 0.5f) {
        const float inv_upper_dy = 1.0f / upper_dy;
        const float dx_short = (mid->x - top->x) * inv_upper_dy;
        const float du_short = (mid->u - top->u) * inv_upper_dy;
        const float dv_short = (mid->v - top->v) * inv_upper_dy;

        int y_start = sw_clamp((int)SDL_ceilf(top->y), 0, clip_h);
        int y_end   = sw_clamp((int)SDL_ceilf(mid->y), 0, clip_h);
        for (int y = y_start; y < y_end; y++) {
            float dt = (float)y - top->y;
            float xa = top->x + dx_long  * dt, ua = top->u + du_long  * dt, va = top->v + dv_long  * dt;
            float xb = top->x + dx_short * dt, ub = top->u + du_short * dt, vb = top->v + dv_short * dt;
            if (xa > xb) { float tmp; tmp=xa; xa=xb; xb=tmp; tmp=ua; ua=ub; ub=tmp; tmp=va; va=vb; vb=tmp; }
            int x0 = sw_clamp((int)SDL_ceilf(xa), 0, clip_w);
            int x1 = sw_clamp((int)SDL_ceilf(xb), 0, clip_w);
            float span = xb - xa;
            if (span < 0.5f) continue;
            float inv_span = 1.0f / span;
            uint32_t* row = dst_pixels + y * dst_pitch;
            for (int x = x0; x < x1; x++) {
                float frac = ((float)x - xa) * inv_span;
                int tx = sw_clamp((int)((ua + (ub - ua) * frac) * src_w), 0, src_w - 1);
                int ty = sw_clamp((int)((va + (vb - va) * frac) * src_h), 0, src_h - 1);
                uint32_t texel = src_pixels[ty * src_w + tx];
                if (has_color_mod) texel = sw_modulate_rgba8888(texel, color);
                row[x] = sw_blend_rgba8888(row[x], texel);
            }
        }
    }

    // Lower half: mid → bot
    const float lower_dy = bot->y - mid->y;
    if (lower_dy >= 0.5f) {
        const float inv_lower_dy = 1.0f / lower_dy;
        const float dx_short = (bot->x - mid->x) * inv_lower_dy;
        const float du_short = (bot->u - mid->u) * inv_lower_dy;
        const float dv_short = (bot->v - mid->v) * inv_lower_dy;

        int y_start = sw_clamp((int)SDL_ceilf(mid->y), 0, clip_h);
        int y_end   = sw_clamp((int)SDL_ceilf(bot->y), 0, clip_h);
        for (int y = y_start; y < y_end; y++) {
            float t_long = (float)y - top->y;
            float t_short = (float)y - mid->y;
            float xa = top->x + dx_long  * t_long,  ua = top->u + du_long  * t_long,  va = top->v + dv_long  * t_long;
            float xb = mid->x + dx_short * t_short, ub = mid->u + du_short * t_short, vb = mid->v + dv_short * t_short;
            if (xa > xb) { float tmp; tmp=xa; xa=xb; xb=tmp; tmp=ua; ua=ub; ub=tmp; tmp=va; va=vb; vb=tmp; }
            int x0 = sw_clamp((int)SDL_ceilf(xa), 0, clip_w);
            int x1 = sw_clamp((int)SDL_ceilf(xb), 0, clip_w);
            float span = xb - xa;
            if (span < 0.5f) continue;
            float inv_span = 1.0f / span;
            uint32_t* row = dst_pixels + y * dst_pitch;
            for (int x = x0; x < x1; x++) {
                float frac = ((float)x - xa) * inv_span;
                int tx = sw_clamp((int)((ua + (ub - ua) * frac) * src_w), 0, src_w - 1);
                int ty = sw_clamp((int)((va + (vb - va) * frac) * src_h), 0, src_h - 1);
                uint32_t texel = src_pixels[ty * src_w + tx];
                if (has_color_mod) texel = sw_modulate_rgba8888(texel, color);
                row[x] = sw_blend_rgba8888(row[x], texel);
            }
        }
    }
}

// ⚡ Software-rasterize a non-rect quad (2 triangles) into sw_frame_surface.
// Split quad indices {0,1,2,3} into triangles {0,1,2} and {1,2,3}.
static bool sw_raster_quad(int task_idx) {
    const unsigned int th = task_th[task_idx];
    const int tex_handle = LO_16_BITS(th);
    const int pal_handle = HI_16_BITS(th);
    if (tex_handle <= 0 || tex_handle > FL_TEXTURE_MAX) return false;
    const int ti = tex_handle - 1;

    int src_w, src_h;
    const uint32_t* src_pixels = NULL;
    if (pal_handle > 0) {
        src_pixels = sw_lookup_cached_pixels(ti, pal_handle, &src_w, &src_h);
    } else {
        src_pixels = sw_ensure_nonidx_pixels(ti, &src_w, &src_h);
    }
    if (!src_pixels) return false;

    // Convert vertex color to RGBA8888 for modulation
    const SDL_FColor* fc = &task_verts[task_idx][0].color;
    const uint32_t color = ((uint32_t)(fc->r * 255.0f + 0.5f) << 24) |
                           ((uint32_t)(fc->g * 255.0f + 0.5f) << 16) |
                           ((uint32_t)(fc->b * 255.0f + 0.5f) <<  8) |
                            (uint32_t)(fc->a * 255.0f + 0.5f);

    // Build SwTriVert array from SDL_Vertex data
    SwTriVert verts[4];
    for (int i = 0; i < 4; i++) {
        verts[i].x = task_verts[task_idx][i].position.x;
        verts[i].y = task_verts[task_idx][i].position.y;
        verts[i].u = task_verts[task_idx][i].tex_coord.x;
        verts[i].v = task_verts[task_idx][i].tex_coord.y;
    }

    uint32_t* dst_pixels = (uint32_t*)sw_frame_surface->pixels;
    const int dst_pitch = sw_frame_surface->pitch / (int)sizeof(uint32_t);

    // Rasterize two triangles: {0,1,2} and {1,2,3}
    sw_raster_triangle(&verts[0], &verts[1], &verts[2],
                       src_pixels, src_w, src_h, color,
                       dst_pixels, dst_pitch, cps3_width, cps3_height);
    sw_raster_triangle(&verts[1], &verts[2], &verts[3],
                       src_pixels, src_w, src_h, color,
                       dst_pixels, dst_pitch, cps3_width, cps3_height);
    return true;
}

// ⚡ Software-rasterize a non-rect SOLID quad (flat color fill via triangle rasterizer).
// Uses a 1×1 white pixel as texture source, with the solid color as the modulation color.
static const uint32_t sw_white_pixel = 0xFFFFFFFFu; // RGBA white
static bool sw_raster_solid_quad(int task_idx) {
    // Convert vertex color to RGBA8888
    const SDL_FColor* fc = &task_verts[task_idx][0].color;
    const uint32_t color = ((uint32_t)(fc->r * 255.0f + 0.5f) << 24) |
                           ((uint32_t)(fc->g * 255.0f + 0.5f) << 16) |
                           ((uint32_t)(fc->b * 255.0f + 0.5f) <<  8) |
                            (uint32_t)(fc->a * 255.0f + 0.5f);

    // Build SwTriVert with UV=(0,0) — all sample the single white pixel
    SwTriVert verts[4];
    for (int i = 0; i < 4; i++) {
        verts[i].x = task_verts[task_idx][i].position.x;
        verts[i].y = task_verts[task_idx][i].position.y;
        verts[i].u = 0.0f;
        verts[i].v = 0.0f;
    }

    uint32_t* dst_pixels = (uint32_t*)sw_frame_surface->pixels;
    const int dst_pitch = sw_frame_surface->pitch / (int)sizeof(uint32_t);

    sw_raster_triangle(&verts[0], &verts[1], &verts[2],
                       &sw_white_pixel, 1, 1, color,
                       dst_pixels, dst_pitch, cps3_width, cps3_height);
    sw_raster_triangle(&verts[1], &verts[2], &verts[3],
                       &sw_white_pixel, 1, 1, color,
                       dst_pixels, dst_pitch, cps3_width, cps3_height);
    return true;
}

// ⚡ Software-frame render: composite all tasks into sw_frame_surface, upload as one texture.
// Returns true if the entire frame was software-composited; false = fallback to draw calls.
static bool sw_render_frame(void) {
    TRACE_ZONE_N("SDL2D:SwFrame");

    // ⚡ BENCHMARK TOGGLE: set to true to force hardware path
    static const bool sw_frame_disabled = true;
    if (sw_frame_disabled) { TRACE_ZONE_END(); return false; }

    if (!ensure_sw_frame_surface() || !ensure_sw_frame_upload_texture()) {
        TRACE_ZONE_END();
        return false;
    }

    // ⚡ Pixel budget heuristic: bail to hardware if total destination pixel work
    // exceeds a threshold. Large-rect screens (VS portraits, backgrounds) are faster
    // on GPU than per-pixel CPU blending. Gameplay sprites (many 16×16 tiles) stay fast.
    // Budget = 2× framebuffer area (384×224 = 86,016 → threshold ~172K pixels).
    {
        uint64_t total_pixels = 0;
        const uint64_t pixel_budget = UINT64_MAX; // disabled for debugging
        for (int i = 0; i < render_task_count; i++) {
            const int idx = render_task_order[i];
            if (task_is_rect[idx]) {
                const SDL_FRect* r = &task_dst_rect[idx];
                total_pixels += (uint64_t)(r->w > 0 ? r->w : 0) * (uint64_t)(r->h > 0 ? r->h : 0);
            } else {
                // Non-rect: use AABB area as estimate
                const SDL_Vertex* v = task_verts[idx];
                float minx = v[0].position.x, miny = v[0].position.y;
                float maxx = minx, maxy = miny;
                for (int k = 1; k < 4; k++) {
                    if (v[k].position.x < minx) minx = v[k].position.x;
                    if (v[k].position.x > maxx) maxx = v[k].position.x;
                    if (v[k].position.y < miny) miny = v[k].position.y;
                    if (v[k].position.y > maxy) maxy = v[k].position.y;
                }
                total_pixels += (uint64_t)(maxx - minx) * (uint64_t)(maxy - miny);
            }
            if (total_pixels > pixel_budget) {
                TRACE_ZONE_END();
                return false;
            }
        }
    }

    // Compute RGBA8888 clear color for this frame
    const Uint8 cr = (flPs2State.FrameClearColor >> 16) & 0xFF;
    const Uint8 cg = (flPs2State.FrameClearColor >>  8) & 0xFF;
    const Uint8 cb = flPs2State.FrameClearColor & 0xFF;
    const Uint8 ca = flPs2State.FrameClearColor >> 24;
    const uint32_t clear_rgba = (ca != SDL_ALPHA_TRANSPARENT)
        ? ((uint32_t)cr << 24) | ((uint32_t)cg << 16) | ((uint32_t)cb << 8) | ca
        : 0x000000FFu; // opaque black fallback (R=0,G=0,B=0,A=255)

    // ⚡ Phase 0: Build current-frame tile coverage from all task rects.
    SDL_memset(dt_current, 0, sizeof(dt_current));
    for (int i = 0; i < render_task_count; i++) {
        const int idx = render_task_order[i];
        if (task_is_rect[idx]) {
            const SDL_FRect* r = &task_dst_rect[idx];
            dt_mark_rect(r->x, r->y, r->w, r->h);
        } else {
            // Non-rect: compute AABB from vertex positions
            const SDL_Vertex* v = task_verts[idx];
            float minx = v[0].position.x, miny = v[0].position.y;
            float maxx = minx, maxy = miny;
            for (int k = 1; k < 4; k++) {
                if (v[k].position.x < minx) minx = v[k].position.x;
                if (v[k].position.x > maxx) maxx = v[k].position.x;
                if (v[k].position.y < miny) miny = v[k].position.y;
                if (v[k].position.y > maxy) maxy = v[k].position.y;
            }
            dt_mark_rect(minx, miny, maxx - minx, maxy - miny);
        }
    }

    // ⚡ Compute dirty tile union (current | previous) and selectively clear.
    // If clear color changed, force all tiles dirty.
    const bool clear_color_changed = !dt_prev_clear_valid || (clear_rgba != dt_prev_clear_color);
    int dirty_count = 0;
    uint32_t* dst_pixels = (uint32_t*)sw_frame_surface->pixels;
    const int dst_pitch = sw_frame_surface->pitch / (int)sizeof(uint32_t);

    for (int t = 0; t < DT_TOTAL; t++) {
        if (clear_color_changed || dt_current[t] || dt_previous[t]) {
            dirty_count++;
            // Clear this 16×16 tile to the clear color
            const int col = t % DT_COLS;
            const int row = t / DT_COLS;
            const int px = col * DT_SIZE;
            const int py = row * DT_SIZE;
            for (int y = py; y < py + DT_SIZE; y++) {
                uint32_t* row_ptr = dst_pixels + y * dst_pitch + px;
                SDL_memset4(row_ptr, clear_rgba, DT_SIZE);
            }
        }
    }
    dt_prev_clear_color = clear_rgba;
    dt_prev_clear_valid = true;
    (void)dirty_count; // reserved for Tracy TRACE_PLOT_INT("DirtyTiles", dirty_count)

    // Phase 1: Eligibility check + rasterize
    for (int i = 0; i < render_task_count; i++) {
        const int idx = render_task_order[i];
        // ⚡ Deferred texture: task_texture may be NULL (FlushBatch path).
        // Use task_th (tex+palette handle) to detect textured vs solid tasks.
        const bool is_textured = (LO_16_BITS(task_th[idx]) > 0);
        const bool is_rect = task_is_rect[idx];

        if (is_rect && is_textured) {
            if (!sw_raster_textured(idx)) {
                TRACE_ZONE_END();
                return false;
            }
        } else if (is_rect && !is_textured) {
            // Solid rect — dst_rect and color32 were populated at enqueue time
            if (!sw_raster_solid(idx)) {
                TRACE_ZONE_END();
                return false;
            }
        } else if (!is_rect && is_textured) {
            // ⚡ Non-rect textured geometry — scanline triangle rasterizer
            if (!sw_raster_quad(idx)) {
                TRACE_ZONE_END();
                return false;
            }
        } else {
            // ⚡ Non-rect solid geometry — triangle rasterizer with flat color
            if (!sw_raster_solid_quad(idx)) {
                TRACE_ZONE_END();
                return false;
            }
        }
    }

    // Phase 2: Upload to GPU as a single texture
    if (!SDL_UpdateTexture(sw_frame_upload_tex, NULL,
                           sw_frame_surface->pixels, sw_frame_surface->pitch)) {
        TRACE_ZONE_END();
        return false;
    }

    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    SDL_SetRenderTarget(renderer, cps3_canvas);
    const SDL_FRect dst = { 0.0f, 0.0f, (float)cps3_width, (float)cps3_height };
    SDL_RenderTexture(renderer, sw_frame_upload_tex, NULL, &dst);

    TRACE_ZONE_END();
    return true;
}

// --- Render Task Management ---

static void clear_render_tasks(void) {
    // ⚡ Only reset count — no need to zero ~800KB of static data every frame
    render_task_count = 0;
    // ⚡ Reset sortedness tracking for next frame
    sort_inversions = 0;
    last_submitted_z = -1e30f;
    // ⚡ Rotate dirty tile maps: current → previous for next frame
    SDL_memcpy(dt_previous, dt_current, sizeof(dt_previous));
}

// --- Render Task Sorting ---
// Sort by z-depth first, then original submission order for stable layering.

// ⚡ Insertion sort for near-sorted index arrays — O(n) when already sorted.
// The game submits sprites roughly in z-order, so most frames have few inversions.
#define INSERTION_SORT_THRESHOLD 16
static void insertion_sort_render_task_indices(int* order, int count) {
    for (int i = 1; i < count; i++) {
        const int key = order[i];
        const float key_z = task_z[key];
        int j = i - 1;
        while (j >= 0) {
            const int oj = order[j];
            const float oj_z = task_z[oj];
            // Compare: sort by z ascending, then by index descending (reverse submission order)
            if (oj_z < key_z || (oj_z == key_z && oj > key))
                break;
            order[j + 1] = oj;
            j--;
        }
        order[j + 1] = key;
    }
}

// ⚡ Texture sub-sort: within each group of equal-Z sprites, sort by texture+palette
// handle to maximize same-texture batching. Draw order within the same Z is
// visually irrelevant, so this is always safe. Uses insertion sort since Z-groups
// are typically small (2-50 sprites).
static void texture_subsort_equal_z_groups(int* order, int count) {
    int run_start = 0;
    while (run_start < count) {
        const float z = task_z[order[run_start]];
        int run_end = run_start + 1;
        while (run_end < count && task_z[order[run_end]] == z)
            run_end++;

        // Insertion sort the run [run_start, run_end) by task_th
        // (combined texture+palette handle — breaks batches on palette changes)
        const int run_len = run_end - run_start;
        if (run_len > 1) {
            for (int i = run_start + 1; i < run_end; i++) {
                const int key = order[i];
                const unsigned int key_th = task_th[key];
                int j = i - 1;
                while (j >= run_start && task_th[order[j]] > key_th) {
                    order[j + 1] = order[j];
                    j--;
                }
                order[j + 1] = key;
            }
        }
        run_start = run_end;
    }
}

// --- Public API ---

SDL_Texture* SDLGameRendererSDL_GetCanvas(void) {
    return cps3_canvas;
}

void SDLGameRendererSDL_Init(void) {
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    cps3_canvas =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, cps3_width, cps3_height);
    if (!cps3_canvas) {
        fatal_error("Failed to create cps3_canvas texture: %s", SDL_GetError());
    }
    SDL_SetTextureScaleMode(cps3_canvas, SDL_SCALEMODE_NEAREST);

    // Pre-initialize index buffer (constant for all quads: two triangles per quad)
    if (!batch_buffers_initialized) {
        for (int i = 0; i < RENDER_TASK_MAX; i++) {
            const int idx_offset = i * 6;
            const int vert_offset = i * 4;
            batch_indices[idx_offset + 0] = vert_offset + 0;
            batch_indices[idx_offset + 1] = vert_offset + 1;
            batch_indices[idx_offset + 2] = vert_offset + 2;
            batch_indices[idx_offset + 3] = vert_offset + 1;
            batch_indices[idx_offset + 4] = vert_offset + 2;
            batch_indices[idx_offset + 5] = vert_offset + 3;
        }
        batch_buffers_initialized = true;
    }
}

void SDLGameRendererSDL_Shutdown(void) {
    // Destroy all cached non-indexed textures
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        if (texture_cache[i] != NULL) {
            SDL_DestroyTexture(texture_cache[i]);
            texture_cache[i] = NULL;
        }
    }

    // Destroy all multi-slot indexed texture cache entries
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        for (int s = 0; s < IDX_PAL_SLOTS; s++) {
            if (idx_pal_tex[i][s] != NULL) {
                SDL_DestroyTexture(idx_pal_tex[i][s]);
                idx_pal_tex[i][s] = NULL;
            }
            if (idx_pal_pixels[i][s] != NULL) {
                SDL_free(idx_pal_pixels[i][s]);
                idx_pal_pixels[i][s] = NULL;
            }
            idx_pal_handle[i][s] = 0;
            idx_pal_hash[i][s]   = 0;
        }
        idx_pal_lru_clock[i] = 0;
    }

    // Destroy all surfaces and non-indexed pixel caches
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        if (surfaces[i] != NULL) {
            SDL_DestroySurface(surfaces[i]);
            surfaces[i] = NULL;
        }
        if (nonidx_pixels[i] != NULL) {
            SDL_free(nonidx_pixels[i]);
            nonidx_pixels[i] = NULL;
        }
    }

    // Destroy all palettes
    for (int i = 0; i < FL_PALETTE_MAX; i++) {
        if (palettes[i] != NULL) {
            SDL_DestroyPalette(palettes[i]);
            palettes[i] = NULL;
        }
    }

    // Destroy deferred textures
    destroy_textures();

    // Destroy canvas
    if (cps3_canvas != NULL) {
        SDL_DestroyTexture(cps3_canvas);
        cps3_canvas = NULL;
    }

    // Destroy software-frame resources
    if (sw_frame_surface != NULL) {
        SDL_DestroySurface(sw_frame_surface);
        sw_frame_surface = NULL;
    }
    if (sw_frame_upload_tex != NULL) {
        SDL_DestroyTexture(sw_frame_upload_tex);
        sw_frame_upload_tex = NULL;
    }

    clear_render_tasks();
    batch_buffers_initialized = false;
}

void SDLGameRendererSDL_BeginFrame(void) {
    TRACE_SUB_BEGIN("SDL2D:BeginFrame");
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();

    // Clear canvas
    const Uint8 r = (flPs2State.FrameClearColor >> 16) & 0xFF;
    const Uint8 g = (flPs2State.FrameClearColor >> 8) & 0xFF;
    const Uint8 b = flPs2State.FrameClearColor & 0xFF;
    const Uint8 a = flPs2State.FrameClearColor >> 24;

    if (a != SDL_ALPHA_TRANSPARENT) {
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    }

    SDL_SetRenderTarget(renderer, cps3_canvas);
    SDL_RenderClear(renderer);
    TRACE_SUB_END();
}

void SDLGameRendererSDL_RenderFrame(void) {
    TRACE_ZONE_N("SDL2D:RenderFrame");
    TRACE_PLOT_INT("RenderTasks", render_task_count);
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    SDL_SetRenderTarget(renderer, cps3_canvas);

    if (render_task_count == 0) {
        TRACE_ZONE_END();
        return;
    }

    // ⚡ Adaptive sort: insertion sort for near-sorted frames, O(n) radix sort otherwise.
    // Most gameplay frames submit sprites in z-order, yielding 0 inversions.
    if (sort_inversions <= INSERTION_SORT_THRESHOLD) {
        // Near-sorted: initialize identity + insertion sort (O(n) for ~0 inversions)
        for (int i = 0; i < render_task_count; i++) {
            render_task_order[i] = i;
        }
        insertion_sort_render_task_indices(render_task_order, render_task_count);
    } else {
        // ⚡ Radix sort: O(n) worst-case, replaces qsort's O(n log n) + branch mispredictions.
        // Uses float-to-sortable-uint bit trick for correct IEEE 754 ordering.
        radix_sort_render_task_indices(render_task_order, task_z, render_task_count,
                                       radix_keys, radix_scratch);
    }

    // ⚡ Texture sub-sort: within each Z-group, sort by texture pointer to
    // maximize same-texture batching. Reduces SDL_RenderGeometry draw calls
    // by 5-10× since many sprites share the same Z depth.
    texture_subsort_equal_z_groups(render_task_order, render_task_count);

    // ⚡ Software-frame path: try CPU compositing before draw calls.
    // Replaces hundreds of per-sprite draw calls with a single SDL_UpdateTexture upload.
    if (sw_render_frame()) {
        TRACE_PLOT_INT("SoftwareFrame", 1);
        TRACE_ZONE_END();
        return;
    }
    TRACE_PLOT_INT("SoftwareFrame", 0);

    // ⚡ Deferred texture resolution: FlushBatch skips SetTexture (stores task_th only),
    // so task_texture[] is NULL for batch-flushed sprites. Resolve them now on the
    // rare hardware fallback path. Only indexed textures need lookup_idx_tex;
    // non-indexed use the eagerly-created texture_cache[].
    for (int i = 0; i < render_task_count; i++) {
        const int idx = render_task_order[i];
        if (task_texture[idx] != NULL) continue; // already resolved (from draw_quad path)
        const unsigned int th = task_th[idx];
        const int tex_handle = LO_16_BITS(th);
        const int pal_handle = HI_16_BITS(th);
        if (tex_handle < 1 || tex_handle > FL_TEXTURE_MAX) continue;
        const int ti = tex_handle - 1;
        SDL_Surface* surface = surfaces[ti];
        if (!surface) continue;
        if (SDL_ISPIXELFORMAT_INDEXED(surface->format) && pal_handle > 0) {
            task_texture[idx] = lookup_idx_tex(renderer, ti, pal_handle);
        } else {
            task_texture[idx] = texture_cache[ti];
        }
    }

    // Re-bind render target (sw_render_frame may have changed it)
    SDL_SetRenderTarget(renderer, cps3_canvas);

    // Batch rendering: group consecutive tasks with same texture+palette (task_th)
    // AND same rect/geometry classification.
    // ⚡ Pre-baked RGBA textures via lookup_idx_tex — pure pointer lookup, no blit.
    int batch_start = 0;
    unsigned int current_th = task_th[render_task_order[0]];
    bool current_is_rect = task_is_rect[render_task_order[0]];
    int rect_fast_path_count = 0;

    for (int i = 0; i <= render_task_count; i++) {
        const bool should_flush =
            (i == render_task_count) ||
            (task_th[render_task_order[i]] != current_th) ||
            (task_is_rect[render_task_order[i]] != current_is_rect);

        if (should_flush) {
            const int batch_size = i - batch_start;
            if (batch_size > 0) {
                SDL_assert(batch_size <= RENDER_TASK_MAX);

                SDL_Texture* draw_texture = task_texture[render_task_order[batch_start]];
                const int batch_palette    = HI_16_BITS(current_th);
                const int batch_tex_handle = LO_16_BITS(current_th);

                // ⚡ For indexed textures: swap draw_texture for the pre-baked slot.
                // lookup_idx_tex is a pure cache hit 99.9% of the time (no blit here).
                if (draw_texture != NULL && batch_palette > 0 &&
                    batch_palette <= FL_PALETTE_MAX && batch_tex_handle > 0) {
                    SDL_Texture* cached = lookup_idx_tex(renderer, batch_tex_handle - 1, batch_palette);
                    if (cached != NULL) draw_texture = cached;
                }

                // ⚡ Rect fast path: use SDL_RenderTexture for axis-aligned rects.
                // SDL_RenderTexture can use optimized hardware blit paths vs.
                // the generic triangle rasterization of SDL_RenderGeometry.
                if (current_is_rect && draw_texture != NULL) {
                    // Query texture size once per batch (all tasks share same texture)
                    float tex_w, tex_h;
                    SDL_GetTextureSize(draw_texture, &tex_w, &tex_h);

                    for (int j = 0; j < batch_size; j++) {
                        const int task_idx = render_task_order[batch_start + j];
                        const SDL_Vertex* v = task_verts[task_idx];

                        // Compute source rect — detect flipped axes via negative dimensions
                        float src_x = v[0].tex_coord.x * tex_w;
                        float src_y = v[0].tex_coord.y * tex_h;
                        float src_w = (v[3].tex_coord.x - v[0].tex_coord.x) * tex_w;
                        float src_h = (v[3].tex_coord.y - v[0].tex_coord.y) * tex_h;

                        SDL_FlipMode flip = SDL_FLIP_NONE;
                        if (src_w < 0) { flip |= SDL_FLIP_HORIZONTAL; src_x += src_w; src_w = -src_w; }
                        if (src_h < 0) { flip |= SDL_FLIP_VERTICAL;   src_y += src_h; src_h = -src_h; }

                        const SDL_FRect src = { src_x, src_y, src_w, src_h };

                        // Destination rect from vertex positions
                        const SDL_FRect dst = {
                            v[0].position.x,
                            v[0].position.y,
                            v[3].position.x - v[0].position.x,
                            v[3].position.y - v[0].position.y
                        };

                        // Apply color + alpha modulation from uniform vertex color
                        SDL_SetTextureColorModFloat(draw_texture, v[0].color.r, v[0].color.g, v[0].color.b);
                        SDL_SetTextureAlphaModFloat(draw_texture, v[0].color.a);

                        if (flip != SDL_FLIP_NONE) {
                            SDL_RenderTextureRotated(renderer, draw_texture, &src, &dst, 0.0, NULL, flip);
                        } else {
                            SDL_RenderTexture(renderer, draw_texture, &src, &dst);
                        }
                    }
                    // ⚡ Reset color/alpha mod to prevent leaking into subsequent
                    // SDL_RenderGeometry batches that may share this texture.
                    SDL_SetTextureColorModFloat(draw_texture, 1.0f, 1.0f, 1.0f);
                    SDL_SetTextureAlphaModFloat(draw_texture, 1.0f);
                    rect_fast_path_count += batch_size;
                } else if (draw_texture != NULL && !current_is_rect) {
                    // ⚡ Retroactive rect recovery: re-examine geometry tasks to see if
                    // they actually form axis-aligned rects. Catches quads missed by
                    // is_axis_aligned_rect at enqueue time (MiSTer's try_resolve_geometry_task_as_rect_copy).
                    float tex_w, tex_h;
                    SDL_GetTextureSize(draw_texture, &tex_w, &tex_h);
                    int geo_start = 0;
                    for (int j = 0; j < batch_size; j++) {
                        const int task_idx = render_task_order[batch_start + j];
                        if (is_axis_aligned_rect(task_verts[task_idx])) {
                            // Flush pending non-rect geometry before this rect
                            if (j > geo_start) {
                                const int geo_count = j - geo_start;
                                for (int k = 0; k < geo_count; k++) {
                                    const int gi = render_task_order[batch_start + geo_start + k];
                                    memcpy(&batch_vertices[k * 4], task_verts[gi], 4 * sizeof(SDL_Vertex));
                                }
                                SDL_RenderGeometry(renderer, draw_texture, batch_vertices,
                                                   geo_count * 4, batch_indices, geo_count * 6);
                            }
                            // ⚡ Normalize vertex order so v[0]=TL, v[3]=BR
                            normalize_rect_verts(task_verts[task_idx]);
                            // Submit as rect
                            const SDL_Vertex* v = task_verts[task_idx];
                            float rx = v[0].tex_coord.x * tex_w;
                            float ry = v[0].tex_coord.y * tex_h;
                            float rw = (v[3].tex_coord.x - v[0].tex_coord.x) * tex_w;
                            float rh = (v[3].tex_coord.y - v[0].tex_coord.y) * tex_h;
                            SDL_FlipMode rflip = SDL_FLIP_NONE;
                            if (rw < 0) { rflip |= SDL_FLIP_HORIZONTAL; rx += rw; rw = -rw; }
                            if (rh < 0) { rflip |= SDL_FLIP_VERTICAL;   ry += rh; rh = -rh; }
                            const SDL_FRect rsrc = { rx, ry, rw, rh };
                            const SDL_FRect rdst = {
                                v[0].position.x, v[0].position.y,
                                v[3].position.x - v[0].position.x,
                                v[3].position.y - v[0].position.y
                            };
                            SDL_SetTextureColorModFloat(draw_texture, v[0].color.r, v[0].color.g, v[0].color.b);
                            SDL_SetTextureAlphaModFloat(draw_texture, v[0].color.a);
                            if (rflip != SDL_FLIP_NONE) {
                                SDL_RenderTextureRotated(renderer, draw_texture, &rsrc, &rdst, 0.0, NULL, rflip);
                            } else {
                                SDL_RenderTexture(renderer, draw_texture, &rsrc, &rdst);
                            }
                            SDL_SetTextureColorModFloat(draw_texture, 1.0f, 1.0f, 1.0f);
                            SDL_SetTextureAlphaModFloat(draw_texture, 1.0f);
                            rect_fast_path_count++;
                            geo_start = j + 1;
                        }
                    }
                    // Flush remaining non-rect geometry
                    if (geo_start < batch_size) {
                        const int geo_count = batch_size - geo_start;
                        for (int k = 0; k < geo_count; k++) {
                            const int gi = render_task_order[batch_start + geo_start + k];
                            memcpy(&batch_vertices[k * 4], task_verts[gi], 4 * sizeof(SDL_Vertex));
                        }
                        SDL_RenderGeometry(renderer, draw_texture, batch_vertices,
                                           geo_count * 4, batch_indices, geo_count * 6);
                    }
                } else {
                    // Standard geometry path: copy vertices to batch buffer
                    for (int j = 0; j < batch_size; j++) {
                        const int task_idx = render_task_order[batch_start + j];
                        const int vert_offset = j * 4;
                        memcpy(&batch_vertices[vert_offset], task_verts[task_idx], 4 * sizeof(SDL_Vertex));
                    }

                    // Single draw call for entire batch
                    SDL_RenderGeometry(
                        renderer, draw_texture, batch_vertices, batch_size * 4, batch_indices, batch_size * 6);
                }
            }

            if (i < render_task_count) {
                current_th = task_th[render_task_order[i]];
                current_is_rect = task_is_rect[render_task_order[i]];
                batch_start = i;
            }
        }
    }
    TRACE_PLOT_INT("RectFastPath", rect_fast_path_count);

    // Debug visualization: draw colored borders around quads
    if (draw_rect_borders) {
        const SDL_FColor red = { .r = 1.0f, .g = 0.0f, .b = 0.0f, .a = SDL_ALPHA_OPAQUE_FLOAT };
        const SDL_FColor green = { .r = 0.0f, .g = 1.0f, .b = 0.0f, .a = SDL_ALPHA_OPAQUE_FLOAT };
        SDL_FColor border_color;

        for (int i = 0; i < render_task_count; i++) {
            const int task_idx = render_task_order[i];
            const float x0 = task_verts[task_idx][0].position.x;
            const float y0 = task_verts[task_idx][0].position.y;
            const float x1 = task_verts[task_idx][3].position.x;
            const float y1 = task_verts[task_idx][3].position.y;
            const SDL_FRect border_rect = { .x = x0, .y = y0, .w = (x1 - x0), .h = (y1 - y0) };

            const float lerp_factor = (render_task_count > 1) ? (float)i / (float)(render_task_count - 1) : 0.5f;
            lerp_fcolors(&border_color, &red, &green, lerp_factor);

            SDL_SetRenderDrawColorFloat(renderer, border_color.r, border_color.g, border_color.b, border_color.a);
            SDL_RenderRect(renderer, &border_rect);
        }
    }
    TRACE_ZONE_END();
}

void SDLGameRendererSDL_EndFrame(void) {
    TRACE_SUB_BEGIN("SDL2D:EndFrame");
    destroy_textures();
    clear_render_tasks();
    TRACE_SUB_END();
}

void SDLGameRendererSDL_UnlockPalette(unsigned int ph) {
    const int palette_handle = ph;


    if ((palette_handle > 0) && (palette_handle < FL_PALETTE_MAX)) {
        SDLGameRendererSDL_DestroyPalette(palette_handle);
        SDLGameRendererSDL_CreatePalette(ph << 16);
    }
}

/**
 * ⚡ Lightweight texture invalidation — replaces the old destroy+recreate cycle.
 *
 * The SDL_Surface created by CreateTexture references the system buffer via
 * pointer (SDL_CreateSurfaceFrom).  Since the caller (ppgRenewTexChunkSeqs)
 * writes directly into that system buffer, the surface already reflects the
 * new pixel data.  We only need to invalidate the GPU-side texture caches
 * so that SetTexture will recreate them from the (now-updated) surface.
 */
void SDLGameRendererSDL_UnlockTexture(unsigned int th) {
    const int texture_index = th - 1;

    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX) {
        return;
    }

    /* Invalidate texture binding cache */

    last_set_texture_th = 0;
    last_set_texture = NULL;

    /* Invalidate non-indexed GPU texture */
    if (texture_cache[texture_index] != NULL) {
        push_texture_to_destroy(texture_cache[texture_index]);
        texture_cache[texture_index] = NULL;
    }

    /* ⚡ Invalidate all indexed palette slots for this texture — pixel data changed.
     * Slots stay allocated (SDL_Texture* reused on next bake), handles cleared.
     * Pixel cache must also be freed — stale data. */
    for (int s = 0; s < IDX_PAL_SLOTS; s++) {
        idx_pal_handle[texture_index][s] = 0;
        idx_pal_hash[texture_index][s]   = 0;
        if (idx_pal_pixels[texture_index][s] != NULL) {
            SDL_free(idx_pal_pixels[texture_index][s]);
            idx_pal_pixels[texture_index][s] = NULL;
        }
    }

    /* Invalidate non-indexed pixel cache */
    if (nonidx_pixels[texture_index] != NULL) {
        SDL_free(nonidx_pixels[texture_index]);
        nonidx_pixels[texture_index] = NULL;
    }

    /* Surface stays alive — it still references the (now-updated) system buffer. */

    /* For non-indexed formats, eagerly recreate the GPU texture. */
    SDL_Surface* surface = surfaces[texture_index];
    if (surface != NULL && !SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            texture_cache[texture_index] = texture;
        }
    }
}

void SDLGameRendererSDL_CreateTexture(unsigned int th) {
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    const int texture_index = LO_16_BITS(th) - 1;

    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX) {
        fatal_error("Texture index out of bounds in CreateTexture: %d", texture_index + 1);
    }

    const FLTexture* fl_texture = &flTexture[texture_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_texture->mem_handle);
    SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_UNKNOWN;
    int pitch = 0;

    if (surfaces[texture_index] != NULL) {
        fatal_error("Overwriting an existing texture at index %d", texture_index);
    }

    switch (fl_texture->format) {
    case SCE_GS_PSMT8:
        pixel_format = SDL_PIXELFORMAT_INDEX8;
        pitch = fl_texture->width;
        break;

    case SCE_GS_PSMT4:
        pixel_format = SDL_PIXELFORMAT_INDEX4LSB;
        pitch = (fl_texture->width + 1) / 2;
        break;

    case SCE_GS_PSMCT16:
        pixel_format = SDL_PIXELFORMAT_ABGR1555;
        pitch = fl_texture->width * 2;
        break;
    case SCE_GS_PSMCT32:
        pixel_format = SDL_PIXELFORMAT_ABGR8888;
        pitch = fl_texture->width * 4;
        break;

    default:
        fatal_error("Unhandled pixel format: %d", fl_texture->format);
        break;
    }

    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(fl_texture->width, fl_texture->height, pixel_format, (void*)pixels, pitch);
    if (!surface) {
        fatal_error("Failed to create surface from memory: %s", SDL_GetError());
    }
    surfaces[texture_index] = surface;

    // For non-indexed formats (e.g. PSMCT16), create the GPU texture eagerly.
    // Indexed textures are created lazily in SetTexture once a palette is known.
    if (!SDL_ISPIXELFORMAT_INDEXED(pixel_format)) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (!texture) {
            fatal_error("Failed to create texture from surface: %s", SDL_GetError());
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        texture_cache[texture_index] = texture;
    }
}

void SDLGameRendererSDL_DestroyTexture(unsigned int texture_handle) {
    const int texture_index = texture_handle - 1;

    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX) {
        SDL_Log("Warning: Attempted to destroy invalid texture handle: %u", texture_handle);
        return;
    }

    // ⚡ Invalidate texture binding cache when a texture is destroyed
    last_set_texture_th = 0;

    // Destroy non-indexed cached texture
    if (texture_cache[texture_index] != NULL) {
        push_texture_to_destroy(texture_cache[texture_index]);
        texture_cache[texture_index] = NULL;
    }

    // Destroy all indexed palette slots for this texture
    for (int s = 0; s < IDX_PAL_SLOTS; s++) {
        if (idx_pal_tex[texture_index][s] != NULL) {
            push_texture_to_destroy(idx_pal_tex[texture_index][s]);
            idx_pal_tex[texture_index][s] = NULL;
        }
        if (idx_pal_pixels[texture_index][s] != NULL) {
            SDL_free(idx_pal_pixels[texture_index][s]);
            idx_pal_pixels[texture_index][s] = NULL;
        }
        idx_pal_handle[texture_index][s] = 0;
        idx_pal_hash[texture_index][s]   = 0;
    }
    idx_pal_lru_clock[texture_index] = 0;

    // Invalidate non-indexed pixel cache
    if (nonidx_pixels[texture_index] != NULL) {
        SDL_free(nonidx_pixels[texture_index]);
        nonidx_pixels[texture_index] = NULL;
    }

    if (surfaces[texture_index] != NULL) {
        SDL_DestroySurface(surfaces[texture_index]);
        surfaces[texture_index] = NULL;
    }
}

void SDLGameRendererSDL_CreatePalette(unsigned int ph) {
    const int palette_index = HI_16_BITS(ph) - 1;

    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX) {
        fatal_error("Palette index out of bounds in CreatePalette: %d", palette_index + 1);
    }

    const FLTexture* fl_palette = &flPalette[palette_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_palette->mem_handle);
    const int color_count = fl_palette->width * fl_palette->height;
    SDL_Color colors[256];
    size_t color_size = 0;

    if (palettes[palette_index] != NULL) {
        fatal_error("Overwriting an existing palette at index %d", palette_index);
    }

    switch (fl_palette->format) {
    case SCE_GS_PSMCT32:
        color_size = 4;
        break;

    case SCE_GS_PSMCT16:
        color_size = 2;
        break;

    default:
        fatal_error("Unhandled palette pixel format: %d", fl_palette->format);
        break;
    }

    switch (color_count) {
    case 16:
        for (int i = 0; i < 16; i++) {
            read_color(pixels, i, color_size, &colors[i]);
        }
        break;

    case 256:
        // Apply PS2 GS CLUT shuffle for 256-color palettes
        for (int i = 0; i < 256; i++) {
            const int color_index = ps2_clut_shuffle[i];
            read_color(pixels, color_index, color_size, &colors[i]);
        }
        break;

    default:
        fatal_error("Unhandled palette dimensions: %dx%d", fl_palette->width, fl_palette->height);
        break;
    }

    SDL_Palette* palette = SDL_CreatePalette(color_count);

    if (!palette) {
        fatal_error("Failed to create SDL palette: %s", SDL_GetError());
    }

    SDL_SetPaletteColors(palette, colors, 0, color_count);
    palettes[palette_index] = palette;

    // ⚡ Hash the palette color data for skip-blit optimization.
    palette_hash[palette_index] = fnv1a_hash(colors, (size_t)color_count * sizeof(SDL_Color));
}

void SDLGameRendererSDL_DestroyPalette(unsigned int palette_handle) {
    const int palette_index = palette_handle - 1;

    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX) {
        SDL_Log("Warning: Attempted to destroy invalid palette handle: %u", palette_handle);
        return;
    }

    // ⚡ Invalidate texture binding cache
    last_set_texture_th = 0;
    last_set_texture = NULL;

    // ⚡ Zero the palette hash — this is sufficient to invalidate all cached slots.
    // lookup_idx_tex checks: want_hash = palette_hash[pi]. After zeroing, want_hash=0,
    // which can never match a baked slot (FNV-1a of real data ≠ 0) → automatic miss.
    // On UnlockPalette (Destroy+Create same handle), CreatePalette sets a new non-zero
    // hash → stale slots fail the hash check → re-baked with new palette data.
    // No O(FL_TEXTURE_MAX × IDX_PAL_SLOTS) scan needed. O(1).
    palette_hash[palette_index] = 0;

    if (palettes[palette_index] != NULL) {
        SDL_DestroyPalette(palettes[palette_index]);
        palettes[palette_index] = NULL;
    }
}

void SDLGameRendererSDL_SetTexture(unsigned int th) {
    // ⚡ Cached texture binding — skip full lookup when same texture+palette was just set
    if (th == last_set_texture_th) {
        if (texture_count > 0) {
            push_texture(textures[texture_count - 1]);
        } else if (last_set_texture != NULL) {
            push_texture(last_set_texture);
        }
        return;
    }
    last_set_texture_th = th;

    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    const int texture_handle = LO_16_BITS(th);
    const int palette_handle = HI_16_BITS(th);
    const int texture_index = texture_handle - 1;

    if (texture_handle < 1 || texture_handle > FL_TEXTURE_MAX) {
        fatal_error("Invalid texture handle in SetTexture: %d", texture_handle);
    }

    if (palette_handle > FL_PALETTE_MAX) {
        fatal_error("Invalid palette handle in SetTexture: %d", palette_handle);
    }

    SDL_Surface* surface = surfaces[texture_index];

    if (!surface) {
        return;
    }

    const SDL_Palette* palette = (palette_handle != 0) ? palettes[palette_handle - 1] : NULL;

    if (dump_textures && palette != NULL) {
        save_texture(surface, palette);
    }

    // ⚡ Indexed textures: look up pre-baked RGBA texture from multi-slot cache.
    // lookup_idx_tex returns a pre-baked SDL_Texture* — bake only happens on first
    // use of a (tex, palette) pair or after invalidation (UnlockTexture/DestroyPalette).
    if (SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        SDL_Texture* texture = (palette_handle > 0)
            ? lookup_idx_tex(renderer, texture_index, palette_handle)
            : NULL;
        if (texture == NULL) return;
        push_texture(texture);
        last_set_texture = texture;
    } else {
        // Non-indexed texture — simple 1:1 cache
        SDL_Texture* texture = texture_cache[texture_index];
        if (texture == NULL) return;
        push_texture(texture);
        last_set_texture = texture;
    }
}

// ⚡ Rect fast path: detect axis-aligned quads eligible for SDL_RenderTexture.
// Checks: (1) positions form an axis-aligned box (any winding order), (2) uniform vertex color.
// ⚡ Winding-order agnostic: computes AABB and verifies each vertex sits on a corner.
// This handles {TL,TR,BL,BR}, {TL,BL,TR,BR}, and any other permutation.
static bool is_axis_aligned_rect(const SDL_Vertex verts[4]) {
    // Compute AABB from all 4 vertices
    float xmin = verts[0].position.x, xmax = xmin;
    float ymin = verts[0].position.y, ymax = ymin;
    for (int i = 1; i < 4; i++) {
        if (verts[i].position.x < xmin) xmin = verts[i].position.x;
        if (verts[i].position.x > xmax) xmax = verts[i].position.x;
        if (verts[i].position.y < ymin) ymin = verts[i].position.y;
        if (verts[i].position.y > ymax) ymax = verts[i].position.y;
    }
    // Each vertex must sit on a corner of the AABB (epsilon-based — handles float imprecision)
    for (int i = 0; i < 4; i++) {
        const bool on_x = sw_nearly_equal(verts[i].position.x, xmin) || sw_nearly_equal(verts[i].position.x, xmax);
        const bool on_y = sw_nearly_equal(verts[i].position.y, ymin) || sw_nearly_equal(verts[i].position.y, ymax);
        if (!on_x || !on_y) return false;
    }
    // Uniform vertex color (no per-vertex interpolation needed)
    for (int i = 1; i < 4; i++) {
        if (SDL_memcmp(&verts[0].color, &verts[i].color, sizeof(SDL_FColor)) != 0) return false;
    }
    return true;
}

// ⚡ Normalize 4 vertices to canonical {TL, TR, BL, BR} order.
// After this, v[0]=TL, v[1]=TR, v[2]=BL, v[3]=BR.
// Prerequisite: is_axis_aligned_rect() returned true.
static void normalize_rect_verts(SDL_Vertex verts[4]) {
    // Compute AABB
    float xmin = verts[0].position.x, xmax = xmin;
    float ymin = verts[0].position.y, ymax = ymin;
    for (int i = 1; i < 4; i++) {
        if (verts[i].position.x < xmin) xmin = verts[i].position.x;
        if (verts[i].position.x > xmax) xmax = verts[i].position.x;
        if (verts[i].position.y < ymin) ymin = verts[i].position.y;
        if (verts[i].position.y > ymax) ymax = verts[i].position.y;
    }
    // Classify each vertex into its canonical slot
    SDL_Vertex sorted[4];
    for (int i = 0; i < 4; i++) {
        const bool is_left = sw_nearly_equal(verts[i].position.x, xmin);
        const bool is_top  = sw_nearly_equal(verts[i].position.y, ymin);
        if      ( is_left &&  is_top) sorted[0] = verts[i]; // TL
        else if (!is_left &&  is_top) sorted[1] = verts[i]; // TR
        else if ( is_left && !is_top) sorted[2] = verts[i]; // BL
        else                          sorted[3] = verts[i]; // BR
    }
    SDL_memcpy(verts, sorted, sizeof(sorted));
}

// ⚡ Write directly into the render_tasks array, avoiding a 104-byte stack
// struct allocation + copy that the old push_render_task pattern required.
static void draw_quad(const SDLGameRenderer_Vertex* vertices, bool textured) {
    if (render_task_count >= RENDER_TASK_MAX) {
        SDL_Log("Warning: render task buffer full, skipping task");
        return;
    }

    const int task_idx = render_task_count;
    task_texture[task_idx] = textured ? get_texture() : NULL;
    task_th[task_idx] = textured ? last_set_texture_th : 0;
    task_z[task_idx] = flPS2ConvScreenFZ(vertices[0].coord.z);

    // ⚡ Track sortedness
    if (task_z[task_idx] < last_submitted_z) sort_inversions++;
    last_submitted_z = task_z[task_idx];

    for (int i = 0; i < 4; i++) {
        task_verts[task_idx][i].position.x = vertices[i].coord.x;
        task_verts[task_idx][i].position.y = vertices[i].coord.y;

        if (textured) {
            task_verts[task_idx][i].tex_coord.x = vertices[i].tex_coord.s;
            task_verts[task_idx][i].tex_coord.y = vertices[i].tex_coord.t;
        } else {
            task_verts[task_idx][i].tex_coord.x = 0.0f;
            task_verts[task_idx][i].tex_coord.y = 0.0f;
        }

        read_rgba32_fcolor(vertices[i].color, &task_verts[task_idx][i].color);
    }

    // ⚡ Rect fast path: classify after vertices are written (both textured and solid)
    task_is_rect[task_idx] = is_axis_aligned_rect(task_verts[task_idx]);

    // ⚡ Normalize vertex order to canonical {TL, TR, BL, BR} so all
    // downstream consumers (rect fast path, software frame) can use v[0]/v[3].
    if (task_is_rect[task_idx]) {
        normalize_rect_verts(task_verts[task_idx]);
    }

    // ⚡ Software-frame data: populate rect/flip/color for rect tasks.
    // For non-rect tasks these are unused (sw_render_frame falls back).
    if (task_is_rect[task_idx]) {
        const SDL_Vertex* v = task_verts[task_idx];
        if (textured) {
            float src_x = v[0].tex_coord.x;
            float src_y = v[0].tex_coord.y;
            float src_w = v[3].tex_coord.x - v[0].tex_coord.x;
            float src_h = v[3].tex_coord.y - v[0].tex_coord.y;
            SDL_FlipMode flip = SDL_FLIP_NONE;
            if (src_w < 0) { flip |= SDL_FLIP_HORIZONTAL; src_x += src_w; src_w = -src_w; }
            if (src_h < 0) { flip |= SDL_FLIP_VERTICAL;   src_y += src_h; src_h = -src_h; }
            task_src_rect[task_idx] = (SDL_FRect){ src_x, src_y, src_w, src_h };
            task_flip[task_idx] = flip;
        }
        task_dst_rect[task_idx] = (SDL_FRect){
            v[0].position.x, v[0].position.y,
            v[3].position.x - v[0].position.x,
            v[3].position.y - v[0].position.y
        };
        // Store color in RGBA8888 format: R<<24|G<<16|B<<8|A
        task_color32[task_idx] = ((uint32_t)(v[0].color.r * 255.0f + 0.5f) << 24) |
                                ((uint32_t)(v[0].color.g * 255.0f + 0.5f) << 16) |
                                ((uint32_t)(v[0].color.b * 255.0f + 0.5f) <<  8) |
                                 (uint32_t)(v[0].color.a * 255.0f + 0.5f);
    }

    render_task_count++;
}

void SDLGameRendererSDL_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    s32 i;

    for (i = 0; i < 4; i++) {
        vertices[i].coord.x = sprite->v[i].x;
        vertices[i].coord.y = sprite->v[i].y;
        vertices[i].coord.z = sprite->v[i].z;
        vertices[i].coord.w = 1.0f;
        vertices[i].color = color;
        vertices[i].tex_coord = sprite->t[i];
    }

    draw_quad(vertices, true);
}

void SDLGameRendererSDL_DrawSolidQuad(const Quad* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    s32 i;

    for (i = 0; i < 4; i++) {
        vertices[i].coord.x = sprite->v[i].x;
        vertices[i].coord.y = sprite->v[i].y;
        vertices[i].coord.z = sprite->v[i].z;
        vertices[i].coord.w = 1.0f;
        vertices[i].color = color;
    }

    draw_quad(vertices, false);
}

void SDLGameRendererSDL_DrawSprite(const Sprite* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    SDL_zeroa(vertices);

    for (int i = 0; i < 4; i++) {
        vertices[i].coord.z = sprite->v[0].z;
        vertices[i].color = color;
    }

    vertices[0].coord.x = sprite->v[0].x;
    vertices[0].coord.y = sprite->v[0].y;
    vertices[3].coord.x = sprite->v[3].x;
    vertices[3].coord.y = sprite->v[3].y;
    vertices[1].coord.x = vertices[3].coord.x;
    vertices[1].coord.y = vertices[0].coord.y;
    vertices[2].coord.x = vertices[0].coord.x;
    vertices[2].coord.y = vertices[3].coord.y;

    vertices[0].tex_coord = sprite->t[0];
    vertices[3].tex_coord = sprite->t[3];
    vertices[1].tex_coord.s = vertices[3].tex_coord.s;
    vertices[1].tex_coord.t = vertices[0].tex_coord.t;
    vertices[2].tex_coord.s = vertices[0].tex_coord.s;
    vertices[2].tex_coord.t = vertices[3].tex_coord.t;

    draw_quad(vertices, true);
}

void SDLGameRendererSDL_DrawSprite2(const Sprite2* sprite2) {
    Sprite sprite;
    SDL_zero(sprite);

    sprite.v[0] = sprite2->v[0];
    sprite.v[1].x = sprite2->v[1].x;
    sprite.v[1].y = sprite2->v[0].y;
    sprite.v[2].x = sprite2->v[0].x;
    sprite.v[2].y = sprite2->v[1].y;
    sprite.v[3] = sprite2->v[1];

    sprite.t[0] = sprite2->t[0];
    sprite.t[1].s = sprite2->t[1].s;
    sprite.t[1].t = sprite2->t[0].t;
    sprite.t[2].s = sprite2->t[0].s;
    sprite.t[2].t = sprite2->t[1].t;
    sprite.t[3] = sprite2->t[1];

    for (int i = 0; i < 4; i++) {
        sprite.v[i].z = sprite2->v[0].z;
    }

    SDLGameRendererSDL_DrawSprite(&sprite, sprite2->vertex_color);
}

unsigned int SDLGameRendererSDL_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle) {
    (void)texture_handle;
    (void)palette_handle;
    // SDL2D mode has no GL textures — return 0 (callers must handle gracefully)
    return 0;
}

/**
 * ⚡ Batch sprite flush for SDL2D — inlines Sprite2 → RenderTask conversion.
 *
 * Eliminates three layers of per-sprite function calls:
 *   DrawSprite2 → DrawSprite → draw_quad
 * by writing directly into the render_tasks array.
 *
 * ⚡ Pre-sorts sprites by tex_code to minimize expensive SetTexture calls
 * (SDL_BlitSurface + SDL_UpdateTexture on palette cache miss). Z-order is
 * preserved by RenderFrame which re-sorts render tasks by Z afterward.
 */

// ⚡ Scratch buffer for indirect tex_code sort (max sprites = 0x400 = 1024)
#define SPRITE_SORT_MAX 0x400
static int sprite_sort_indices[SPRITE_SORT_MAX];

void SDLGameRendererSDL_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count) {
    TRACE_SUB_BEGIN("SDL2D:FlushBatch");

    // Phase 1: Build index array of active sprites only
    int active_count = 0;
    for (int i = 0; i < count && active_count < SPRITE_SORT_MAX; i++) {
        if (active_layers[chips[i].id]) {
            sprite_sort_indices[active_count++] = i;
        }
    }

    // Phase 2: Iterate sprites in submission order.
    // ⚡ Deferred texture: we DON'T call SetTexture here. Software-frame path
    // (which succeeds 99%+ of frames) uses task_th → sw_lookup_cached_pixels
    // and never touches task_texture. SDL_Texture resolution is deferred to the
    // rare hardware fallback in RenderFrame. This eliminates ~148 per-frame
    // lookup_idx_tex scans + push_texture stack writes.
    unsigned int last_tex_code = 0;
    int set_texture_calls = 0;

    for (int si = 0; si < active_count; si++) {
        if (render_task_count >= RENDER_TASK_MAX)
            break;

        const Sprite2* spr = &chips[sprite_sort_indices[si]];

        /* Track unique tex_code transitions for Tracy telemetry */
        unsigned int tc = spr->tex_code;
        if (tc != last_tex_code) {
            last_tex_code = tc;
            set_texture_calls++;
        }

        /* --- Inlined draw_quad: Sprite2 → RenderTask directly --- */
        const int task_idx = render_task_count;
        task_texture[task_idx] = NULL; // ⚡ Deferred — resolved in RenderFrame on hw fallback
        task_th[task_idx] = tc;
        task_z[task_idx] = flPS2ConvScreenFZ(spr->v[0].z);

        if (task_z[task_idx] < last_submitted_z) sort_inversions++;
        last_submitted_z = task_z[task_idx];

        const Uint32 color = spr->vertex_color;
        const SDL_FColor fc = {
            .b = rgba8_to_float[color & 0xFF],
            .g = rgba8_to_float[(color >> 8) & 0xFF],
            .r = rgba8_to_float[(color >> 16) & 0xFF],
            .a = rgba8_to_float[(color >> 24) & 0xFF]
        };

        /* Expand Sprite2 → 4 vertices */
        const float x0 = spr->v[0].x, y0 = spr->v[0].y;
        const float x1 = spr->v[1].x, y1 = spr->v[1].y;
        const float s0 = spr->t[0].s, t0 = spr->t[0].t;
        const float s1 = spr->t[1].s, t1 = spr->t[1].t;

        task_verts[task_idx][0].position.x = x0; task_verts[task_idx][0].position.y = y0;
        task_verts[task_idx][0].tex_coord.x = s0; task_verts[task_idx][0].tex_coord.y = t0;
        task_verts[task_idx][0].color = fc;

        task_verts[task_idx][1].position.x = x1; task_verts[task_idx][1].position.y = y0;
        task_verts[task_idx][1].tex_coord.x = s1; task_verts[task_idx][1].tex_coord.y = t0;
        task_verts[task_idx][1].color = fc;

        task_verts[task_idx][2].position.x = x0; task_verts[task_idx][2].position.y = y1;
        task_verts[task_idx][2].tex_coord.x = s0; task_verts[task_idx][2].tex_coord.y = t1;
        task_verts[task_idx][2].color = fc;

        task_verts[task_idx][3].position.x = x1; task_verts[task_idx][3].position.y = y1;
        task_verts[task_idx][3].tex_coord.x = s1; task_verts[task_idx][3].tex_coord.y = t1;
        task_verts[task_idx][3].color = fc;

        // ⚡ Sprite2 always produces axis-aligned rects with uniform color
        task_is_rect[task_idx] = true;

        // ⚡ Software-frame data: pre-compute rect/flip/color for Sprite2 tasks.
        {
            float uv_w = s1 - s0;
            float uv_h = t1 - t0;
            float uv_x = s0;
            float uv_y = t0;
            SDL_FlipMode spr_flip = SDL_FLIP_NONE;
            if (uv_w < 0) { spr_flip |= SDL_FLIP_HORIZONTAL; uv_x += uv_w; uv_w = -uv_w; }
            if (uv_h < 0) { spr_flip |= SDL_FLIP_VERTICAL;   uv_y += uv_h; uv_h = -uv_h; }
            task_src_rect[task_idx] = (SDL_FRect){ uv_x, uv_y, uv_w, uv_h };
            task_dst_rect[task_idx] = (SDL_FRect){
                x0, y0,
                x1 - x0,
                y1 - y0
            };
            task_flip[task_idx] = spr_flip;
            // Store color in RGBA8888 format: R<<24|G<<16|B<<8|A
            // Engine color is ARGB (A<<24|R<<16|G<<8|B), convert via sw_argb_to_rgba.
            task_color32[task_idx] = sw_argb_to_rgba(color);
        }

        render_task_count++;
    }
    TRACE_PLOT_INT("SetTextureCalls", set_texture_calls);
    TRACE_SUB_END();
}
