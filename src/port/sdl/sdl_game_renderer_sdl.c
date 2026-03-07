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
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_game_renderer.h"
#include "port/sdl/sdl_game_renderer_internal.h"
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

// ⚡ Per-palette content hash — computed in CreatePalette, used to detect stale cache entries.
static uint32_t palette_hash[FL_PALETTE_MAX];

// ⚡ Shared scratch buffer for RGBA pixel conversion during bake_idx_tex.
// Large enough for the biggest texture observed (256×256 = 65536 px).
#define RGBA_SCRATCH_MAX (512 * 512)
static uint32_t rgba_scratch[RGBA_SCRATCH_MAX];

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

// Debugging and statistics
static bool draw_rect_borders = false;
static bool dump_textures = false;
static int debug_texture_index = 0;

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

// --- Render Task Management ---

static void clear_render_tasks(void) {
    // ⚡ Only reset count — no need to zero ~800KB of static data every frame
    render_task_count = 0;
    // ⚡ Reset sortedness tracking for next frame
    sort_inversions = 0;
    last_submitted_z = -1e30f;
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
            idx_pal_handle[i][s] = 0;
            idx_pal_hash[i][s]   = 0;
        }
        idx_pal_lru_clock[i] = 0;
    }

    // Destroy all surfaces
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        if (surfaces[i] != NULL) {
            SDL_DestroySurface(surfaces[i]);
            surfaces[i] = NULL;
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

    // Batch rendering: group consecutive tasks with same texture+palette (task_th).
    // ⚡ Pre-baked RGBA textures via lookup_idx_tex — pure pointer lookup, no blit.
    int batch_start = 0;
    unsigned int current_th = task_th[render_task_order[0]];

    for (int i = 0; i <= render_task_count; i++) {
        const bool should_flush =
            (i == render_task_count) || (task_th[render_task_order[i]] != current_th);

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

                // Copy vertices to batch buffer via sorted indices
                for (int j = 0; j < batch_size; j++) {
                    const int task_idx = render_task_order[batch_start + j];
                    const int vert_offset = j * 4;
                    memcpy(&batch_vertices[vert_offset], task_verts[task_idx], 4 * sizeof(SDL_Vertex));
                }

                // Single draw call for entire batch
                SDL_RenderGeometry(
                    renderer, draw_texture, batch_vertices, batch_size * 4, batch_indices, batch_size * 6);
            }

            if (i < render_task_count) {
                current_th = task_th[render_task_order[i]];
                batch_start = i;
            }
        }
    }

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
     * Slots stay allocated (SDL_Texture* reused on next bake), handles cleared. */
    for (int s = 0; s < IDX_PAL_SLOTS; s++) {
        idx_pal_handle[texture_index][s] = 0;
        idx_pal_hash[texture_index][s]   = 0;
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
        idx_pal_handle[texture_index][s] = 0;
        idx_pal_hash[texture_index][s]   = 0;
    }
    idx_pal_lru_clock[texture_index] = 0;

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

// ⚡ Write directly into the render_tasks array, avoiding a 104-byte stack
// struct allocation + copy that the old push_render_task pattern required.
static void draw_quad(const SDLGameRenderer_Vertex* vertices, bool textured) {
    if (render_task_count >= RENDER_TASK_MAX) {
        SDL_Log("Warning: render task buffer full, skipping task");
        return;
    }

    const int task_idx = render_task_count;
    task_texture[task_idx] = textured ? get_texture() : NULL;
    task_th[task_idx] = last_set_texture_th;
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

    // Phase 2: Iterate sprites in submission order — SetTexture is now a cheap
    // pointer lookup, so no pre-sort needed. RenderFrame's texture_subsort_equal_z_groups
    // handles GPU draw call batching within same-Z groups.
    unsigned int last_tex_code = 0;
    int set_texture_calls = 0;

    for (int si = 0; si < active_count; si++) {
        if (render_task_count >= RENDER_TASK_MAX)
            break;

        const Sprite2* spr = &chips[sprite_sort_indices[si]];

        /* SetTexture on tex_code change — needed for palette cache lookup */
        unsigned int tc = spr->tex_code;
        if (tc != last_tex_code) {
            last_tex_code = tc;
            SDLGameRendererSDL_SetTexture(tc);
            set_texture_calls++;
        }

        /* --- Inlined draw_quad: Sprite2 → RenderTask directly --- */
        const int task_idx = render_task_count;
        task_texture[task_idx] = last_set_texture;
        task_th[task_idx] = last_set_texture_th;
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

        render_task_count++;
    }
    TRACE_PLOT_INT("SetTextureCalls", set_texture_calls);
    TRACE_SUB_END();
}
