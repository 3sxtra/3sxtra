/**
 * @file sdl_game_renderer_gpu_internal.h
 * @brief Shared internal state for the GPU renderer sub-modules.
 *
 * Declares all GPU renderer global variables, types, macros, and inline
 * helpers that are shared between sdl_game_renderer_gpu.c,
 * sdl_game_renderer_gpu_setup.c, and sdl_game_renderer_gpu_texture.c.
 *
 * The actual variable definitions live in sdl_game_renderer_gpu.c.
 */
#ifndef SDL_GAME_RENDERER_GPU_INTERNAL_H
#define SDL_GAME_RENDERER_GPU_INTERNAL_H

#include "common.h"
#include "port/sdl/renderer/sdl_game_renderer_gpu_lz77.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include <SDL3/SDL.h>
#include <simde/x86/sse2.h>
#include <simde/x86/sse4.2.h>
#include <simde/x86/ssse3.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants / Macros ──────────────────────────────────────────────── */

#define VERTEX_TRANSFER_BUFFER_COUNT 3
#define GPU_FENCE_RING_SIZE VERTEX_TRANSFER_BUFFER_COUNT
#define COMPUTE_STORAGE_SIZE (32 * 1024 * 1024) /* 32 MB — RGBA8 texture uploads */
#define TEX_ARRAY_SIZE 512
#define TEX_ARRAY_MAX_LAYERS 256
#define MAX_VERTICES 65536
#define MAX_QUADS (MAX_VERTICES / 4)
#define PALETTE_TEX_WIDTH 256
#define PALETTE_TEX_HEIGHT (FL_PALETTE_MAX + 1)
#define DEFAULT_PALETTE_ROW FL_PALETTE_MAX
#define MAX_COMPUTE_JOBS 256

/* ─── Types ───────────────────────────────────────────────────────────── */

/** @brief Job descriptor for an R8-indexed texture → texture-array upload. */
typedef struct {
    Uint32 width;
    Uint32 height;
    Uint32 layer;
    Uint32 offset;
} TextureUploadJob;

/** @brief Job descriptor for a palette row upload to the palette atlas. */
typedef struct {
    Uint32 row;    /* palette index (row in palette atlas) */
    Uint32 offset; /* byte offset in staging buffer */
} PaletteUploadJob;

/** @brief Z-depth key for stable quad sorting. */
typedef struct {
    float z;
    int original_index; /* quad index in submission order */
} QuadSortKey;

/** @brief Per-vertex data layout for the GPU pipeline. */
typedef struct GPUVertex {
    float x, y;
    float r, g, b, a;
    float u, v;
    float layer;
    float paletteIdx; /* palette row in atlas, or -1.0 for direct RGBA */
} GPUVertex;

/* ─── Shared Global Variables (defined in sdl_game_renderer_gpu.c) ──── */

/* Core device / window / pipeline */
extern SDL_GPUDevice* device;
extern LZ77Context* s_lz77_ctx;
extern SDL_Window* gpu_window;
extern SDL_GPUCommandBuffer* current_cmd_buf;
extern SDL_GPUGraphicsPipeline* pipeline;
extern SDL_GPUTexture* s_palette_texture;
extern SDL_GPUTransferBuffer* s_palette_transfer;
extern SDL_GPUSampler* palette_sampler;
extern SDL_GPUSampler* sampler;

/* Vertex / Index buffers */
extern SDL_GPUBuffer* vertex_buffer;
extern SDL_GPUBuffer* index_buffer;
extern SDL_GPUTransferBuffer* transfer_buffers[VERTEX_TRANSFER_BUFFER_COUNT];
extern SDL_GPUTransferBuffer* index_transfer_buffer;
extern int current_transfer_idx;

/* Fence ring */
extern SDL_GPUFence* s_frame_fences[GPU_FENCE_RING_SIZE];
extern int s_fence_write_idx;

/* Compute / texture staging */
extern SDL_GPUTransferBuffer* s_compute_staging_buffer;
extern u8* s_compute_staging_ptr;
extern size_t s_compute_staging_offset;
extern int s_compute_drops_last_frame;

/* Mapped vertex pointer */
extern float* mapped_vertex_ptr;
extern unsigned int vertex_count;

/* Swapchain / canvas */
extern SDL_GPUTexture* s_swapchain_texture;
extern SDL_GPUTexture* s_canvas_texture;

/* Texture array management */
extern SDL_GPUTexture* texture_array;
extern int tex_array_free[TEX_ARRAY_MAX_LAYERS];
extern int tex_array_free_count;
extern int16_t tex_array_layer[FL_TEXTURE_MAX];

/* Per-frame texture state stacks */
extern int texture_layers[MAX_VERTICES];
extern float texture_uv_sx[MAX_VERTICES];
extern float texture_uv_sy[MAX_VERTICES];
extern float texture_palette_idx[MAX_VERTICES];
extern int texture_count;

/* Palette upload tracking */
extern bool s_palette_uploaded[FL_PALETTE_MAX];
extern int s_pal_upload_dirty_indices[FL_PALETTE_MAX];
extern int s_pal_upload_dirty_count;

/* Back-to-back SetTexture cache */
extern unsigned int s_last_set_texture_handle;

/* CPU-side surfaces and palettes */
extern SDL_Surface* surfaces[FL_TEXTURE_MAX];
extern SDL_Palette* palettes[FL_PALETTE_MAX];

/* Dirty flags / indices */
extern bool texture_dirty_flags[FL_TEXTURE_MAX];
extern bool palette_dirty_flags[FL_PALETTE_MAX];
extern int dirty_texture_indices[FL_TEXTURE_MAX];
extern int dirty_texture_count;
extern int dirty_palette_indices[FL_PALETTE_MAX];
extern int dirty_palette_count;

/* Hash-based dirty detection */
extern uint32_t palette_hash[FL_PALETTE_MAX];
extern uint32_t texture_hash[FL_TEXTURE_MAX];

/* Texture upload job queue */
extern TextureUploadJob s_tex_upload_jobs[MAX_COMPUTE_JOBS];
extern int s_tex_upload_count;

/* Palette upload job queue */
extern PaletteUploadJob s_pal_upload_jobs[MAX_COMPUTE_JOBS];
extern int s_pal_upload_count;

/* Z-depth sorting */
extern QuadSortKey quad_sort_keys[MAX_QUADS];
extern unsigned int quad_count;
extern QuadSortKey quad_sort_temp[MAX_QUADS];

/* ─── Inline Helpers ──────────────────────────────────────────────────── */

/**
 * @brief CRC32 hardware hash — 4 bytes/cycle via SIMDe.
 *
 * Used for texture and palette dirty detection.
 */
static inline uint32_t hash_memory(const void* ptr, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    const uint32_t* data32 = (const uint32_t*)ptr;
    size_t count32 = len / 4;
    for (size_t i = 0; i < count32; i++) {
        crc = simde_mm_crc32_u32(crc, data32[i]);
    }
    const uint8_t* tail = (const uint8_t*)(data32 + count32);
    size_t remaining = len & 3;
    for (size_t i = 0; i < remaining; i++) {
        crc = simde_mm_crc32_u8(crc, tail[i]);
    }
    return crc ^ 0xFFFFFFFFu;
}

/** @brief Read an RGBA32 pixel into an SDL_Color (R in low byte). */
static inline void read_rgba32_color(Uint32 pixel, SDL_Color* color) {
    color->r = (pixel >> 0) & 0xFF;
    color->g = (pixel >> 8) & 0xFF;
    color->b = (pixel >> 16) & 0xFF;
    color->a = (pixel >> 24) & 0xFF;
}

/** @brief Read an RGBA16 (ABGR1555) pixel into an SDL_Color. */
static inline void read_rgba16_color(Uint16 pixel, SDL_Color* color) {
    color->r = (pixel & 0x1F) * 255 / 31;
    color->g = ((pixel >> 5) & 0x1F) * 255 / 31;
    color->b = ((pixel >> 10) & 0x1F) * 255 / 31;
    color->a = (pixel & 0x8000) ? 255 : 0;
}

/* ─── PS2 CLUT Shuffle Table ──────────────────────────────────────────── */

/**
 * @brief PS2 GS CLUT shuffle LUT — maps linear index (0-255) to shuffled GS index.
 *
 * The PS2 GS stores 256-color CLUTs in a non-linear memory order.
 * Defined in the header so both setup and texture modules can use it.
 */
static const Uint8 ps2_clut_shuffle[256] = {
    0,   1,   2,   3,   4,   5,   6,   7,   16,  17,  18,  19,  20,  21,  22,  23,  8,   9,   10,  11,  12,  13,
    14,  15,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  48,  49,  50,  51,
    52,  53,  54,  55,  40,  41,  42,  43,  44,  45,  46,  47,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,
    66,  67,  68,  69,  70,  71,  80,  81,  82,  83,  84,  85,  86,  87,  72,  73,  74,  75,  76,  77,  78,  79,
    88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 112, 113, 114, 115, 116, 117,
    118, 119, 104, 105, 106, 107, 108, 109, 110, 111, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131,
    132, 133, 134, 135, 144, 145, 146, 147, 148, 149, 150, 151, 136, 137, 138, 139, 140, 141, 142, 143, 152, 153,
    154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 176, 177, 178, 179, 180, 181, 182, 183,
    168, 169, 170, 171, 172, 173, 174, 175, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197,
    198, 199, 208, 209, 210, 211, 212, 213, 214, 215, 200, 201, 202, 203, 204, 205, 206, 207, 216, 217, 218, 219,
    220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 240, 241, 242, 243, 244, 245, 246, 247, 232, 233,
    234, 235, 236, 237, 238, 239, 248, 249, 250, 251, 252, 253, 254, 255
};

#ifdef __cplusplus
}
#endif

#endif /* SDL_GAME_RENDERER_GPU_INTERNAL_H */
