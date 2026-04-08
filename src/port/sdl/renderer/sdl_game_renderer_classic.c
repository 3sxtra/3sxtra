/**
 * @file sdl_game_renderer_classic.c
 * @brief SDL2D Classic backend — simple reference renderer for benchmarking.
 *
 * Reimplements the original SDL2D renderer *before* optimizations:
 *   - Flat 2D texture cache (texture_cache[FL_TEXTURE_MAX][FL_PALETTE_MAX+1])
 *   - SDL_SetSurfacePalette + SDL_CreateTextureFromSurface (SDL-internal conversion)
 *   - AoS RenderTask struct + qsort
 *   - SDL_RenderGeometry for all quads (no rect fast path, no software frame)
 *   - No FlushSprite2Batch — each sprite goes through draw_quad individually
 *
 * Instrumented with Tracy zones for A/B profiling against the optimized SDL2D.
 * Access: --renderer classic
 */
#include "common.h"
#include "port/renderer_plugin.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/app/sdl_app_config.h"
#include "port/sdl/renderer/sdl_game_renderer.h"
#include "port/sdl/renderer/sdl_game_renderer_internal.h"
#include "port/tracy_zones.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"

#include <libgraph.h>

#include <SDL3/SDL.h>

#include <stdlib.h>

#define RENDER_TASK_MAX 8192
#define TEXTURES_TO_DESTROY_MAX 1024

// partially SoA RenderTask (vertices split out for batch contiguous fast path)
typedef struct RenderTask {
    SDL_Texture* texture;
    float z;
    int original_index;
    RendererBlendMode blend_mode;
    bool is_rect;
} RenderTask;

static SDL_Vertex cl_task_verts[RENDER_TASK_MAX][4]; // ⚡ Parallel SoA array for vertices

static SDL_Texture* cps3_canvas_classic = NULL;

static const int cps3_width = 384;
static const int cps3_height = 224;

static SDL_Surface* cl_surfaces[FL_TEXTURE_MAX] = { NULL };
static SDL_Palette* cl_palettes[FL_PALETTE_MAX] = { NULL };

// Flat 2D texture cache: cl_tex_cache[texture_index][palette_handle]
// palette_handle index 0 = non-indexed texture, 1..FL_PALETTE_MAX = indexed
static SDL_Texture* cl_tex_cache[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1];

static SDL_Texture* cl_textures_to_destroy[TEXTURES_TO_DESTROY_MAX] = { NULL };
static int cl_textures_to_destroy_count = 0;

static SDL_Texture* cl_current_texture = NULL;
static unsigned int cl_current_th = 0;
static RendererBlendMode cl_current_blend_mode = RENDERER_BLEND_NORMAL;

static RenderTask cl_render_tasks[RENDER_TASK_MAX];
static int cl_render_task_count = 0;
static int cl_sort_order[RENDER_TASK_MAX]; // ⚡ Index-based sort: sort 4-byte indices not 104-byte structs

// Batch buffers for SDL_RenderGeometry
static SDL_Vertex cl_batch_vertices[RENDER_TASK_MAX * 4];
static int cl_batch_indices[RENDER_TASK_MAX * 6];
static bool cl_batch_buffers_initialized = false;

// PS2 CLUT shuffle table
static const Uint8 cl_ps2_clut_shuffle[256] = {
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

// --- Color reading helpers ---

static void cl_read_rgba32_fcolor(Uint32 pixel, SDL_FColor* fcolor) {
    fcolor->b = (float)(pixel & 0xFF) / 255.0f;
    fcolor->g = (float)((pixel >> 8) & 0xFF) / 255.0f;
    fcolor->r = (float)((pixel >> 16) & 0xFF) / 255.0f;
    fcolor->a = (float)((pixel >> 24) & 0xFF) / 255.0f;
}

static void cl_read_rgba16_color(Uint16 pixel, SDL_Color* color) {
    color->r = (Uint8)(((pixel & 0x1F) * 255 + 15) / 31);
    color->g = (Uint8)((((pixel >> 5) & 0x1F) * 255 + 15) / 31);
    color->b = (Uint8)((((pixel >> 10) & 0x1F) * 255 + 15) / 31);
    color->a = (pixel & 0x8000) ? 255 : 0;
}

static void cl_read_color(const void* pixels, int index, size_t color_size, SDL_Color* color) {
    if (color_size == 2) {
        const Uint16* rgba16 = (const Uint16*)pixels;
        cl_read_rgba16_color(rgba16[index], color);
    } else {
        const Uint32* rgba32 = (const Uint32*)pixels;
        Uint32 pixel = rgba32[index];
        color->b = pixel & 0xFF;
        color->g = (pixel >> 8) & 0xFF;
        color->r = (pixel >> 16) & 0xFF;
        color->a = (pixel >> 24) & 0xFF;
    }
}

// ⚡ Insertion sort on index array — O(n) for near-sorted input (which CPS3 frames are).
// Sorts by Z depth, stable by original submission order.
static void cl_insertion_sort_indices(int* order, int count) {
    for (int i = 1; i < count; i++) {
        const int key = order[i];
        const float key_z = cl_render_tasks[key].z;
        int j = i - 1;
        while (j >= 0 &&
               (cl_render_tasks[order[j]].z > key_z || (cl_render_tasks[order[j]].z == key_z && order[j] > key))) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }
}

// ⚡ Texture sub-sort: within runs of equal Z, sort by texture pointer.
// Maximizes same-texture batching → fewer SDL_RenderGeometry draw calls.
static void cl_texture_subsort_equal_z(int* order, int count) {
    int i = 0;
    while (i < count) {
        // Find end of this Z-equal run
        const float z = cl_render_tasks[order[i]].z;
        int j = i + 1;
        while (j < count && cl_render_tasks[order[j]].z == z)
            j++;
        // Stable insertion sort [i..j) by texture pointer (runs are typically small)
        for (int a = i + 1; a < j; a++) {
            const int key = order[a];
            const SDL_Texture* key_tex = cl_render_tasks[key].texture;
            int b = a - 1;
            while (b >= i && (uintptr_t)cl_render_tasks[order[b]].texture > (uintptr_t)key_tex) {
                order[b + 1] = order[b];
                b--;
            }
            order[b + 1] = key;
        }
        i = j;
    }
}

// --- draw_quad: enqueue a rendering task ---
static void cl_draw_quad(SDL_Vertex vertices[4], SDL_Texture* texture, float z) {
    if (cl_render_task_count >= RENDER_TASK_MAX)
        return;

    RenderTask* task = &cl_render_tasks[cl_render_task_count];
    task->texture = texture;
    task->z = flPS2ConvScreenFZ(z);
    task->original_index = cl_render_task_count;
    task->blend_mode = cl_current_blend_mode;
    task->is_rect = false; // Default to geometry mode
    memcpy(cl_task_verts[cl_render_task_count], vertices, sizeof(SDL_Vertex) * 4);
    cl_render_task_count++;
}

static void cl_push_texture_to_destroy(SDL_Texture* texture) {
    if (cl_textures_to_destroy_count >= TEXTURES_TO_DESTROY_MAX) {
        SDL_DestroyTexture(texture);
        return;
    }
    cl_textures_to_destroy[cl_textures_to_destroy_count++] = texture;
}

// --- Public API ---

void SDLGameRendererClassic_Init(void) {
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    cps3_canvas_classic = SDL_CreateTexture(renderer,
                                            SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_TARGET,
                                            cps3_width * g_resolution_scale,
                                            cps3_height * g_resolution_scale);
    SDL_SetTextureScaleMode(cps3_canvas_classic, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(cps3_canvas_classic, SDL_BLENDMODE_BLEND);

    // Pre-initialize batch index buffers (6 indices per quad: 2 triangles)
    if (!cl_batch_buffers_initialized) {
        for (int i = 0; i < RENDER_TASK_MAX; i++) {
            int base = i * 4;
            int idx_base = i * 6;
            cl_batch_indices[idx_base + 0] = base + 0;
            cl_batch_indices[idx_base + 1] = base + 1;
            cl_batch_indices[idx_base + 2] = base + 2;
            cl_batch_indices[idx_base + 3] = base + 1; // 1 instead of 0
            cl_batch_indices[idx_base + 4] = base + 2;
            cl_batch_indices[idx_base + 5] = base + 3;
        }
        cl_batch_buffers_initialized = true;
    }

    // Zero out the flat cache
    memset(cl_tex_cache, 0, sizeof(cl_tex_cache));

    SDL_Log("[Classic] Initialized — simple SDL backend for benchmarking");
}

void SDLGameRendererClassic_Shutdown(void) {
    // Flush deferred texture destruction first
    for (int i = 0; i < cl_textures_to_destroy_count; i++) {
        SDL_DestroyTexture(cl_textures_to_destroy[i]);
        cl_textures_to_destroy[i] = NULL;
    }
    cl_textures_to_destroy_count = 0;

    // Destroy all cached textures
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        if (cl_surfaces[i]) {
            SDL_DestroySurface(cl_surfaces[i]);
            cl_surfaces[i] = NULL;
        }
        for (int j = 0; j <= FL_PALETTE_MAX; j++) {
            if (cl_tex_cache[i][j]) {
                SDL_DestroyTexture(cl_tex_cache[i][j]);
                cl_tex_cache[i][j] = NULL;
            }
        }
    }
    for (int i = 0; i < FL_PALETTE_MAX; i++) {
        if (cl_palettes[i]) {
            SDL_DestroyPalette(cl_palettes[i]);
            cl_palettes[i] = NULL;
        }
    }
    if (cps3_canvas_classic) {
        SDL_DestroyTexture(cps3_canvas_classic);
        cps3_canvas_classic = NULL;
    }
    cl_current_texture = NULL;
    cl_current_th = 0;
    cl_render_task_count = 0;
    cl_batch_buffers_initialized = false;
}

void SDLGameRendererClassic_BeginFrame(void) {
    TRACE_ZONE_N("Classic:BeginFrame");
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();

    const Uint8 r = (flPs2State.FrameClearColor >> 16) & 0xFF;
    const Uint8 g = (flPs2State.FrameClearColor >> 8) & 0xFF;
    const Uint8 b = flPs2State.FrameClearColor & 0xFF;
    const Uint8 a = flPs2State.FrameClearColor >> 24;

    if (a != SDL_ALPHA_TRANSPARENT) {
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    }

    SDL_SetRenderTarget(renderer, cps3_canvas_classic);
    SDL_RenderClear(renderer);
    TRACE_ZONE_END();
}

void SDLGameRendererClassic_SetBlendMode(RendererBlendMode mode) {
    cl_current_blend_mode = mode;
}

void SDLGameRendererClassic_RenderFrame(void) {

    TRACE_PLOT_INT("ClassicRenderTasks", cl_render_task_count);
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    SDL_SetRenderTarget(renderer, cps3_canvas_classic);

    if (cl_render_task_count == 0) {

        return;
    }

    // ⚡ Index-based sort: sort 4-byte indices instead of 104-byte structs.
    // Insertion sort is O(n) for near-sorted input (CPS3 submits mostly in Z order).
    TRACE_SUB_BEGIN("Classic:Sort");
    for (int i = 0; i < cl_render_task_count; i++)
        cl_sort_order[i] = i;
    cl_insertion_sort_indices(cl_sort_order, cl_render_task_count);
    // ⚡ Texture sub-sort: group same-texture sprites within equal-Z runs.
    cl_texture_subsort_equal_z(cl_sort_order, cl_render_task_count);
    TRACE_SUB_END();

    // Batch rendering: group consecutive tasks with same texture pointer and blend mode.
    // Uses cl_sort_order[] indirection — structs stay in submission order.
    TRACE_SUB_BEGIN("Classic:BatchRender");
    int batch_start = 0;
    SDL_Texture* current_batch_texture = cl_render_tasks[cl_sort_order[0]].texture;
    RendererBlendMode current_batch_blend = cl_render_tasks[cl_sort_order[0]].blend_mode;
    bool current_batch_is_rect = cl_render_tasks[cl_sort_order[0]].is_rect;

    for (int i = 0; i <= cl_render_task_count; i++) {
        const int idx = (i < cl_render_task_count) ? cl_sort_order[i] : 0;
        const bool should_flush = (i == cl_render_task_count) ||
                                  (cl_render_tasks[idx].texture != current_batch_texture) ||
                                  (cl_render_tasks[idx].blend_mode != current_batch_blend) ||
                                  (cl_render_tasks[idx].is_rect != current_batch_is_rect);

        if (should_flush) {
            const int batch_size = i - batch_start;
            if (batch_size > 0) {
                SDL_BlendMode sdl_blend;
                if (current_batch_blend == RENDERER_BLEND_ADD)
                    sdl_blend = SDL_BLENDMODE_ADD;
                else if (current_batch_blend == RENDERER_BLEND_MULTIPLY)
                    sdl_blend = SDL_BLENDMODE_MUL;
                else
                    sdl_blend = SDL_BLENDMODE_BLEND;

                if (current_batch_texture)
                    SDL_SetTextureBlendMode(current_batch_texture, sdl_blend);
                else
                    SDL_SetRenderDrawBlendMode(renderer, sdl_blend);

                // ⚡ Rect fast path: use SDL_RenderTexture for axis-aligned rects.
                // SDL_RenderTexture can use optimized hardware blit paths.
                if (current_batch_is_rect && current_batch_texture != NULL) {
                    float tex_w, tex_h;
                    SDL_GetTextureSize(current_batch_texture, &tex_w, &tex_h);

                    for (int j = 0; j < batch_size; j++) {
                        const int si = cl_sort_order[batch_start + j];
                        const SDL_Vertex* v = cl_task_verts[si];

                        float src_x = v[0].tex_coord.x * tex_w;
                        float src_y = v[0].tex_coord.y * tex_h;
                        float src_w = (v[3].tex_coord.x - v[0].tex_coord.x) * tex_w;
                        float src_h = (v[3].tex_coord.y - v[0].tex_coord.y) * tex_h;

                        SDL_FlipMode flip = SDL_FLIP_NONE;
                        if (src_w < 0) {
                            flip |= SDL_FLIP_HORIZONTAL;
                            src_x += src_w;
                            src_w = -src_w;
                        }
                        if (src_h < 0) {
                            flip |= SDL_FLIP_VERTICAL;
                            src_y += src_h;
                            src_h = -src_h;
                        }

                        const SDL_FRect src = { src_x, src_y, src_w, src_h };
                        const SDL_FRect dst = { v[0].position.x,
                                                v[0].position.y,
                                                v[3].position.x - v[0].position.x,
                                                v[3].position.y - v[0].position.y };

                        SDL_SetTextureColorModFloat(current_batch_texture, v[0].color.r, v[0].color.g, v[0].color.b);
                        SDL_SetTextureAlphaModFloat(current_batch_texture, v[0].color.a);

                        if (flip != SDL_FLIP_NONE) {
                            SDL_RenderTextureRotated(renderer, current_batch_texture, &src, &dst, 0.0, NULL, flip);
                        } else {
                            SDL_RenderTexture(renderer, current_batch_texture, &src, &dst);
                        }
                    }
                    // Reset color mod
                    SDL_SetTextureColorModFloat(current_batch_texture, 1.0f, 1.0f, 1.0f);
                    SDL_SetTextureAlphaModFloat(current_batch_texture, 1.0f);
                } else {
                    // ⚡ Batch copy elimination: if tasks are sequential, pass direct array offset
                    bool contiguous = true;
                    for (int j = 1; j < batch_size; j++) {
                        if (cl_sort_order[batch_start + j] != cl_sort_order[batch_start + j - 1] + 1) {
                            contiguous = false;
                            break;
                        }
                    }

                    if (contiguous) {
                        const int first_si = cl_sort_order[batch_start];
                        SDL_RenderGeometry(renderer,
                                           current_batch_texture,
                                           (const SDL_Vertex*)cl_task_verts[first_si],
                                           batch_size * 4,
                                           cl_batch_indices,
                                           batch_size * 6);
                    } else {
                        // Sparse elements — must copy into flat buffer
                        for (int j = 0; j < batch_size; j++) {
                            const int si = cl_sort_order[batch_start + j];
                            memcpy(&cl_batch_vertices[j * 4], cl_task_verts[si], 4 * sizeof(SDL_Vertex));
                        }
                        SDL_RenderGeometry(renderer,
                                           current_batch_texture,
                                           cl_batch_vertices,
                                           batch_size * 4,
                                           cl_batch_indices,
                                           batch_size * 6);
                    }
                }
            }

            if (i < cl_render_task_count) {
                current_batch_texture = cl_render_tasks[idx].texture;
                current_batch_blend = cl_render_tasks[idx].blend_mode;
                current_batch_is_rect = cl_render_tasks[idx].is_rect;
                batch_start = i;
            }
        }
    }
    TRACE_SUB_END();
}

void SDLGameRendererClassic_EndFrame(void) {
    TRACE_ZONE_N("Classic:EndFrame");
    // Destroy deferred textures
    for (int i = 0; i < cl_textures_to_destroy_count; i++) {
        SDL_DestroyTexture(cl_textures_to_destroy[i]);
        cl_textures_to_destroy[i] = NULL;
    }
    cl_textures_to_destroy_count = 0;
    cl_render_task_count = 0;
    TRACE_ZONE_END();
}

SDL_Texture* SDLGameRendererClassic_GetCanvas(void) {
    return cps3_canvas_classic;
}

// --- Texture Management ---

void SDLGameRendererClassic_CreateTexture(unsigned int th) {
    TRACE_ZONE_N("Classic:CreateTexture");
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    const int texture_index = LO_16_BITS(th) - 1;

    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX) {
        TRACE_ZONE_END();
        return;
    }

    const FLTexture* fl_texture = &flTexture[texture_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_texture->mem_handle);
    SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_UNKNOWN;
    int pitch = 0;

    if (cl_surfaces[texture_index] != NULL) {
        SDL_DestroySurface(cl_surfaces[texture_index]);
        cl_surfaces[texture_index] = NULL;
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
        TRACE_ZONE_END();
        return;
    }

    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(fl_texture->width, fl_texture->height, pixel_format, (void*)pixels, pitch);
    if (!surface) {
        TRACE_ZONE_END();
        return;
    }
    cl_surfaces[texture_index] = surface;

    // For non-indexed formats, eagerly create SDL_Texture
    if (!SDL_ISPIXELFORMAT_INDEXED(pixel_format)) {
        SDL_Texture* sdl_texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (sdl_texture) {
            SDL_SetTextureScaleMode(sdl_texture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND);
        }
        if (cl_tex_cache[texture_index][0]) {
            cl_push_texture_to_destroy(cl_tex_cache[texture_index][0]);
        }
        cl_tex_cache[texture_index][0] = sdl_texture;
    }
    TRACE_ZONE_END();
}

void SDLGameRendererClassic_DestroyTexture(unsigned int texture_handle) {
    const int texture_index = texture_handle - 1;
    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX)
        return;

    if (cl_surfaces[texture_index]) {
        SDL_DestroySurface(cl_surfaces[texture_index]);
        cl_surfaces[texture_index] = NULL;
    }
    for (int j = 0; j <= FL_PALETTE_MAX; j++) {
        if (cl_tex_cache[texture_index][j]) {
            cl_push_texture_to_destroy(cl_tex_cache[texture_index][j]);
            cl_tex_cache[texture_index][j] = NULL;
        }
    }
}

void SDLGameRendererClassic_UnlockTexture(unsigned int th) {
    const int texture_index = LO_16_BITS(th) - 1;
    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX)
        return;

    // Invalidate all cached textures for this surface
    for (int j = 0; j <= FL_PALETTE_MAX; j++) {
        if (cl_tex_cache[texture_index][j]) {
            cl_push_texture_to_destroy(cl_tex_cache[texture_index][j]);
            cl_tex_cache[texture_index][j] = NULL;
        }
    }
    // Recreate non-indexed texture if applicable
    SDL_Surface* surface = cl_surfaces[texture_index];
    if (surface && !SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
        SDL_Texture* sdl_texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (sdl_texture) {
            SDL_SetTextureScaleMode(sdl_texture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND);
        }
        cl_tex_cache[texture_index][0] = sdl_texture;
    }
}

// --- Palette Management ---

void SDLGameRendererClassic_CreatePalette(unsigned int ph) {

    const int palette_index = HI_16_BITS(ph) - 1;
    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX) {

        return;
    }

    const FLTexture* fl_palette = &flPalette[palette_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_palette->mem_handle);
    const int color_count = fl_palette->width * fl_palette->height;
    size_t color_size = 0;

    if (cl_palettes[palette_index] != NULL) {
        SDL_DestroyPalette(cl_palettes[palette_index]);
        cl_palettes[palette_index] = NULL;
    }

    switch (fl_palette->format) {
    case SCE_GS_PSMCT32:
        color_size = 4;
        break;
    case SCE_GS_PSMCT16:
        color_size = 2;
        break;
    default:

        return;
    }

    SDL_Palette* sdl_pal = SDL_CreatePalette(color_count);
    if (!sdl_pal) {

        return;
    }

    SDL_Color colors[256];
    if (color_count == 256) {
        for (int i = 0; i < 256; i++) {
            cl_read_color(pixels, cl_ps2_clut_shuffle[i], color_size, &colors[i]);
        }
    } else {
        for (int i = 0; i < color_count && i < 256; i++) {
            cl_read_color(pixels, i, color_size, &colors[i]);
        }
    }
    SDL_SetPaletteColors(sdl_pal, colors, 0, color_count);
    cl_palettes[palette_index] = sdl_pal;
}

void SDLGameRendererClassic_DestroyPalette(unsigned int palette_handle) {
    const int pi = palette_handle - 1;
    if (pi < 0 || pi >= FL_PALETTE_MAX)
        return;

    if (cl_palettes[pi]) {
        SDL_DestroyPalette(cl_palettes[pi]);
        cl_palettes[pi] = NULL;
    }
    // Invalidate all texture cache entries for this palette
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        if (cl_tex_cache[i][pi + 1]) {
            cl_push_texture_to_destroy(cl_tex_cache[i][pi + 1]);
            cl_tex_cache[i][pi + 1] = NULL;
        }
    }
}

void SDLGameRendererClassic_UnlockPalette(unsigned int ph) {
    const int pi = ph - 1;
    if (pi < 0 || pi >= FL_PALETTE_MAX)
        return;

    // Re-create palette from updated data (shift to HI_16_BITS format for CreatePalette)
    SDLGameRendererClassic_CreatePalette(ph << 16);
    // Invalidate all texture cache entries using this palette
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        if (cl_tex_cache[i][pi + 1]) {
            cl_push_texture_to_destroy(cl_tex_cache[i][pi + 1]);
            cl_tex_cache[i][pi + 1] = NULL;
        }
    }
}

// --- SetTexture: use SDL's built-in palette+surface → texture conversion ---

void SDLGameRendererClassic_SetTexture(unsigned int th) {
    // Fast path: skip all work (including Tracy) when texture handle unchanged
    if (th == cl_current_th && cl_current_texture != NULL)
        return;

    const int texture_handle = LO_16_BITS(th);
    const int palette_handle = HI_16_BITS(th);
    const int texture_index = texture_handle - 1;

    if (texture_handle < 1 || texture_handle > FL_TEXTURE_MAX) {
        cl_current_texture = NULL;
        cl_current_th = th;
        return;
    }

    SDL_Surface* surface = cl_surfaces[texture_index];
    if (!surface) {
        cl_current_texture = NULL;
        cl_current_th = th;
        return;
    }
    cl_current_th = th;

    /* ── Plugin texture override ── */
    if (RENDERER_HAS_PLUGIN() && g_renderer_plugin->TryOverrideTexture) {
        void* override_tex =
            g_renderer_plugin->TryOverrideTexture((unsigned int)texture_handle, (unsigned int)palette_handle);
        if (override_tex != NULL) {
            SDL_Texture* sdl_tex = (SDL_Texture*)override_tex;
            SDL_SetTextureScaleMode(sdl_tex, SDL_SCALEMODE_LINEAR);
            cl_current_texture = sdl_tex;
            return;
        }
    }

    if (SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        // Check flat cache first (no Tracy overhead for cache hits)
        if (palette_handle > 0 && palette_handle <= FL_PALETTE_MAX &&
            cl_tex_cache[texture_index][palette_handle] != NULL) {
            cl_current_texture = cl_tex_cache[texture_index][palette_handle];
            return;
        }

        // Cache miss: instrument only this slow path
        TRACE_ZONE_N("Classic:SetTexture");
        if (palette_handle > 0 && palette_handle <= FL_PALETTE_MAX) {
            if (cl_palettes[palette_handle - 1] != NULL) {
                SDL_SetSurfacePalette(surface, cl_palettes[palette_handle - 1]);
                SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
                SDL_Texture* sdl_texture = SDL_CreateTextureFromSurface(renderer, surface);
                if (sdl_texture) {
                    SDL_SetTextureScaleMode(sdl_texture, SDL_SCALEMODE_NEAREST);
                    SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND);
                    cl_tex_cache[texture_index][palette_handle] = sdl_texture;
                    cl_current_texture = sdl_texture;
                } else {
                    SDL_Log("Classic SetTexture: SDL_CreateTextureFromSurface failed for texture %d, palette %d: %s",
                            texture_handle,
                            palette_handle,
                            SDL_GetError());
                }
            } else {
                SDL_Log("Classic SetTexture: Palette %d is NULL for texture %d", palette_handle, texture_handle);
            }
        } else {
            SDL_Log(
                "Classic SetTexture: Invalid palette_handle %d for indexed texture %d", palette_handle, texture_handle);
        }
        TRACE_ZONE_END();
    } else {
        // Non-indexed: use eagerly created texture at slot 0
        cl_current_texture = cl_tex_cache[texture_index][0];
    }
}

// --- Drawing ---

void SDLGameRendererClassic_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    SDL_Vertex vertices[4];
    SDL_FColor fcolor;
    cl_read_rgba32_fcolor(color, &fcolor);

    const float scale = (float)g_resolution_scale;
    for (int i = 0; i < 4; i++) {
        vertices[i].position.x = sprite->v[i].x * scale;
        vertices[i].position.y = sprite->v[i].y * scale;
        vertices[i].color = fcolor;
        vertices[i].tex_coord.x = sprite->t[i].s;
        vertices[i].tex_coord.y = sprite->t[i].t;
    }

    cl_draw_quad(vertices, cl_current_texture, sprite->v[0].z);
}

void SDLGameRendererClassic_DrawSolidQuad(const Quad* quad, unsigned int color) {
    SDL_Vertex vertices[4];
    SDL_FColor fcolor;
    cl_read_rgba32_fcolor(color, &fcolor);

    const float scale = (float)g_resolution_scale;
    for (int i = 0; i < 4; i++) {
        vertices[i].position.x = quad->v[i].x * scale;
        vertices[i].position.y = quad->v[i].y * scale;
        vertices[i].color = fcolor;
        vertices[i].tex_coord.x = 0.0f;
        vertices[i].tex_coord.y = 0.0f;
    }

    cl_draw_quad(vertices, NULL, quad->v[0].z);
}

// DrawSprite: v[0]=TL corner, v[3]=BR corner (matching optimized backend)
// Vertex order: 0=TL, 1=TR, 2=BL, 3=BR
// Index buffer: 0,1,2 (TL,TR,BL) + 1,2,3 (TR,BL,BR)
void SDLGameRendererClassic_DrawSprite(const Sprite* sprite, unsigned int color) {
    SDL_Vertex vertices[4];
    SDL_FColor fcolor;
    cl_read_rgba32_fcolor(color, &fcolor);

    for (int i = 0; i < 4; i++) {
        vertices[i].color = fcolor;
    }

    const float scale = (float)g_resolution_scale;

    // TL = v[0]
    vertices[0].position.x = sprite->v[0].x * scale;
    vertices[0].position.y = sprite->v[0].y * scale;
    vertices[0].tex_coord.x = sprite->t[0].s;
    vertices[0].tex_coord.y = sprite->t[0].t;

    // BR = v[3]
    vertices[3].position.x = sprite->v[3].x * scale;
    vertices[3].position.y = sprite->v[3].y * scale;
    vertices[3].tex_coord.x = sprite->t[3].s;
    vertices[3].tex_coord.y = sprite->t[3].t;

    // TR = derived from v[3].x, v[0].y
    vertices[1].position.x = vertices[3].position.x;
    vertices[1].position.y = vertices[0].position.y;
    vertices[1].tex_coord.x = vertices[3].tex_coord.x;
    vertices[1].tex_coord.y = vertices[0].tex_coord.y;

    // BL = derived from v[0].x, v[3].y
    vertices[2].position.x = vertices[0].position.x;
    vertices[2].position.y = vertices[3].position.y;
    vertices[2].tex_coord.x = vertices[0].tex_coord.x;
    vertices[2].tex_coord.y = vertices[3].tex_coord.y;

    cl_draw_quad(vertices, cl_current_texture, sprite->v[0].z);
}

// DrawSprite2: match optimized backend's conversion exactly
void SDLGameRendererClassic_DrawSprite2(const Sprite2* sprite2) {
    Sprite sprite;
    SDL_zero(sprite);

    // Copy corners: v[0]=TL, v[3]=BR (from sprite2's 2-corner format)
    sprite.v[0] = sprite2->v[0];
    sprite.v[1].x = sprite2->v[1].x;
    sprite.v[1].y = sprite2->v[0].y;
    sprite.v[2].x = sprite2->v[0].x;
    sprite.v[2].y = sprite2->v[1].y;
    sprite.v[3] = sprite2->v[1];

    // Expand 2-corner texcoords to 4 corners
    sprite.t[0] = sprite2->t[0];
    sprite.t[1].s = sprite2->t[1].s;
    sprite.t[1].t = sprite2->t[0].t;
    sprite.t[2].s = sprite2->t[0].s;
    sprite.t[2].t = sprite2->t[1].t;
    sprite.t[3] = sprite2->t[1];

    // All 4 verts share same z
    for (int i = 0; i < 4; i++) {
        sprite.v[i].z = sprite2->v[0].z;
    }

    SDLGameRendererClassic_DrawSprite(&sprite, sprite2->vertex_color);
}

// ⚡ Inlined FlushSprite2Batch — Sprite2 → RenderTask directly in one loop.
// Eliminates the 3-deep call chain (DrawSprite2 → DrawSprite → draw_quad)
// and 2 intermediate vertex/sprite copies per sprite.
void SDLGameRendererClassic_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count) {
    TRACE_ZONE_N("Classic:FlushBatch");
    TRACE_ZONE_VALUE(count);

    // ⚡ Pre-computed u8→float LUT (avoids per-channel division by 255.0f)
    static const float to_float[256] = {
        0.0f / 255,   1.0f / 255,   2.0f / 255,   3.0f / 255,   4.0f / 255,   5.0f / 255,   6.0f / 255,   7.0f / 255,
        8.0f / 255,   9.0f / 255,   10.0f / 255,  11.0f / 255,  12.0f / 255,  13.0f / 255,  14.0f / 255,  15.0f / 255,
        16.0f / 255,  17.0f / 255,  18.0f / 255,  19.0f / 255,  20.0f / 255,  21.0f / 255,  22.0f / 255,  23.0f / 255,
        24.0f / 255,  25.0f / 255,  26.0f / 255,  27.0f / 255,  28.0f / 255,  29.0f / 255,  30.0f / 255,  31.0f / 255,
        32.0f / 255,  33.0f / 255,  34.0f / 255,  35.0f / 255,  36.0f / 255,  37.0f / 255,  38.0f / 255,  39.0f / 255,
        40.0f / 255,  41.0f / 255,  42.0f / 255,  43.0f / 255,  44.0f / 255,  45.0f / 255,  46.0f / 255,  47.0f / 255,
        48.0f / 255,  49.0f / 255,  50.0f / 255,  51.0f / 255,  52.0f / 255,  53.0f / 255,  54.0f / 255,  55.0f / 255,
        56.0f / 255,  57.0f / 255,  58.0f / 255,  59.0f / 255,  60.0f / 255,  61.0f / 255,  62.0f / 255,  63.0f / 255,
        64.0f / 255,  65.0f / 255,  66.0f / 255,  67.0f / 255,  68.0f / 255,  69.0f / 255,  70.0f / 255,  71.0f / 255,
        72.0f / 255,  73.0f / 255,  74.0f / 255,  75.0f / 255,  76.0f / 255,  77.0f / 255,  78.0f / 255,  79.0f / 255,
        80.0f / 255,  81.0f / 255,  82.0f / 255,  83.0f / 255,  84.0f / 255,  85.0f / 255,  86.0f / 255,  87.0f / 255,
        88.0f / 255,  89.0f / 255,  90.0f / 255,  91.0f / 255,  92.0f / 255,  93.0f / 255,  94.0f / 255,  95.0f / 255,
        96.0f / 255,  97.0f / 255,  98.0f / 255,  99.0f / 255,  100.0f / 255, 101.0f / 255, 102.0f / 255, 103.0f / 255,
        104.0f / 255, 105.0f / 255, 106.0f / 255, 107.0f / 255, 108.0f / 255, 109.0f / 255, 110.0f / 255, 111.0f / 255,
        112.0f / 255, 113.0f / 255, 114.0f / 255, 115.0f / 255, 116.0f / 255, 117.0f / 255, 118.0f / 255, 119.0f / 255,
        120.0f / 255, 121.0f / 255, 122.0f / 255, 123.0f / 255, 124.0f / 255, 125.0f / 255, 126.0f / 255, 127.0f / 255,
        128.0f / 255, 129.0f / 255, 130.0f / 255, 131.0f / 255, 132.0f / 255, 133.0f / 255, 134.0f / 255, 135.0f / 255,
        136.0f / 255, 137.0f / 255, 138.0f / 255, 139.0f / 255, 140.0f / 255, 141.0f / 255, 142.0f / 255, 143.0f / 255,
        144.0f / 255, 145.0f / 255, 146.0f / 255, 147.0f / 255, 148.0f / 255, 149.0f / 255, 150.0f / 255, 151.0f / 255,
        152.0f / 255, 153.0f / 255, 154.0f / 255, 155.0f / 255, 156.0f / 255, 157.0f / 255, 158.0f / 255, 159.0f / 255,
        160.0f / 255, 161.0f / 255, 162.0f / 255, 163.0f / 255, 164.0f / 255, 165.0f / 255, 166.0f / 255, 167.0f / 255,
        168.0f / 255, 169.0f / 255, 170.0f / 255, 171.0f / 255, 172.0f / 255, 173.0f / 255, 174.0f / 255, 175.0f / 255,
        176.0f / 255, 177.0f / 255, 178.0f / 255, 179.0f / 255, 180.0f / 255, 181.0f / 255, 182.0f / 255, 183.0f / 255,
        184.0f / 255, 185.0f / 255, 186.0f / 255, 187.0f / 255, 188.0f / 255, 189.0f / 255, 190.0f / 255, 191.0f / 255,
        192.0f / 255, 193.0f / 255, 194.0f / 255, 195.0f / 255, 196.0f / 255, 197.0f / 255, 198.0f / 255, 199.0f / 255,
        200.0f / 255, 201.0f / 255, 202.0f / 255, 203.0f / 255, 204.0f / 255, 205.0f / 255, 206.0f / 255, 207.0f / 255,
        208.0f / 255, 209.0f / 255, 210.0f / 255, 211.0f / 255, 212.0f / 255, 213.0f / 255, 214.0f / 255, 215.0f / 255,
        216.0f / 255, 217.0f / 255, 218.0f / 255, 219.0f / 255, 220.0f / 255, 221.0f / 255, 222.0f / 255, 223.0f / 255,
        224.0f / 255, 225.0f / 255, 226.0f / 255, 227.0f / 255, 228.0f / 255, 229.0f / 255, 230.0f / 255, 231.0f / 255,
        232.0f / 255, 233.0f / 255, 234.0f / 255, 235.0f / 255, 236.0f / 255, 237.0f / 255, 238.0f / 255, 239.0f / 255,
        240.0f / 255, 241.0f / 255, 242.0f / 255, 243.0f / 255, 244.0f / 255, 245.0f / 255, 246.0f / 255, 247.0f / 255,
        248.0f / 255, 249.0f / 255, 250.0f / 255, 251.0f / 255, 252.0f / 255, 253.0f / 255, 254.0f / 255, 255.0f / 255,
    };

    unsigned int last_tex_code = 0;
    const float scale = (float)g_resolution_scale;

    for (int i = 0; i < count; i++) {
        if (active_layers && !active_layers[chips[i].id])
            continue;
        if (cl_render_task_count >= RENDER_TASK_MAX)
            break;

        const Sprite2* spr = &chips[i];

        // SetTexture on tex_code change (still goes through engine pipeline)
        const unsigned int tc = spr->tex_code;
        if (tc != last_tex_code) {
            last_tex_code = tc;
            flSetRenderState(FLRENDER_TEXSTAGE0, tc);
        }

        // --- Inlined: Sprite2 → RenderTask (was DrawSprite2 → DrawSprite → draw_quad) ---
        RenderTask* task = &cl_render_tasks[cl_render_task_count];
        task->texture = cl_current_texture;
        task->z = flPS2ConvScreenFZ(spr->v[0].z);
        task->original_index = cl_render_task_count;
        task->blend_mode = cl_current_blend_mode;

        // Color: u8→float via LUT (was cl_read_rgba32_fcolor with 4 divisions)
        const Uint32 color = spr->vertex_color;
        const SDL_FColor fc = { .b = to_float[color & 0xFF],
                                .g = to_float[(color >> 8) & 0xFF],
                                .r = to_float[(color >> 16) & 0xFF],
                                .a = to_float[(color >> 24) & 0xFF] };

        // Expand Sprite2 2-corner → 4 vertices directly into task
        // Sprite2: v[0]=TL, v[1]=BR; t[0]=TL UV, t[1]=BR UV
        const float x0 = spr->v[0].x * scale;
        const float y0 = spr->v[0].y * scale;
        const float x1 = spr->v[1].x * scale;
        const float y1 = spr->v[1].y * scale;
        const float s0 = spr->t[0].s, t0 = spr->t[0].t;
        const float s1 = spr->t[1].s, t1 = spr->t[1].t;

        // TL
        cl_task_verts[cl_render_task_count][0].position.x = x0;
        cl_task_verts[cl_render_task_count][0].position.y = y0;
        cl_task_verts[cl_render_task_count][0].tex_coord.x = s0;
        cl_task_verts[cl_render_task_count][0].tex_coord.y = t0;
        cl_task_verts[cl_render_task_count][0].color = fc;
        // TR
        cl_task_verts[cl_render_task_count][1].position.x = x1;
        cl_task_verts[cl_render_task_count][1].position.y = y0;
        cl_task_verts[cl_render_task_count][1].tex_coord.x = s1;
        cl_task_verts[cl_render_task_count][1].tex_coord.y = t0;
        cl_task_verts[cl_render_task_count][1].color = fc;
        // BL
        cl_task_verts[cl_render_task_count][2].position.x = x0;
        cl_task_verts[cl_render_task_count][2].position.y = y1;
        cl_task_verts[cl_render_task_count][2].tex_coord.x = s0;
        cl_task_verts[cl_render_task_count][2].tex_coord.y = t1;
        cl_task_verts[cl_render_task_count][2].color = fc;
        // BR
        cl_task_verts[cl_render_task_count][3].position.x = x1;
        cl_task_verts[cl_render_task_count][3].position.y = y1;
        cl_task_verts[cl_render_task_count][3].tex_coord.x = s1;
        cl_task_verts[cl_render_task_count][3].tex_coord.y = t1;
        cl_task_verts[cl_render_task_count][3].color = fc;

        task->is_rect = true; // ⚡ Inlined SPR2 are always axis-aligned rects

        cl_render_task_count++;
    }

    TRACE_ZONE_END();
}

unsigned int SDLGameRendererClassic_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle) {
    (void)texture_handle;
    (void)palette_handle;
    return 0; // Not applicable for SDL2D
}

void SDLGameRendererClassic_DumpTextures(void) {
    SDL_Log("[Classic] Texture dump not implemented");
}

/* ─── Overlay Sprite Enqueue (Classic) ──────────────────────────────── */

void SDLGameRendererClassic_DrawOverlaySpriteEx(SDL_Texture* texture, float x, float y, float w, float h, float z,
                                                int flip_x, int flip_y) {
    if (cl_render_task_count >= RENDER_TASK_MAX || texture == NULL)
        return;

    const float s = (float)g_resolution_scale;
    float sx = x * s, sy = y * s, sw = w * s, sh = h * s;

    float u0 = flip_x ? 1.0f : 0.0f;
    float u1 = flip_x ? 0.0f : 1.0f;
    float v0 = flip_y ? 1.0f : 0.0f;
    float v1 = flip_y ? 0.0f : 1.0f;
    const SDL_FColor white = { 1.0f, 1.0f, 1.0f, 1.0f };

    SDL_Vertex verts[4];
    verts[0] = (SDL_Vertex) { { sx, sy }, white, { u0, v0 } };
    verts[1] = (SDL_Vertex) { { sx + sw, sy }, white, { u1, v0 } };
    verts[2] = (SDL_Vertex) { { sx, sy + sh }, white, { u0, v1 } };
    verts[3] = (SDL_Vertex) { { sx + sw, sy + sh }, white, { u1, v1 } };

    cl_draw_quad(verts, texture, z);
    /* Adjust z: cl_draw_quad calls flPS2ConvScreenFZ internally, but we
     * already have a converted z from TextureUtil_DrawQuad. Override. */
    cl_render_tasks[cl_render_task_count - 1].z = z;
}

void SDLGameRendererClassic_DrawOverlaySubSprite(SDL_Texture* texture, float x, float y, float w, float h, float u0,
                                                 float v0, float u1, float v1, float z) {
    if (cl_render_task_count >= RENDER_TASK_MAX || texture == NULL)
        return;

    const float s = (float)g_resolution_scale;
    float sx = x * s, sy = y * s, sw = w * s, sh = h * s;

    const SDL_FColor white = { 1.0f, 1.0f, 1.0f, 1.0f };

    SDL_Vertex verts[4];
    verts[0] = (SDL_Vertex) { { sx, sy }, white, { u0, v0 } };
    verts[1] = (SDL_Vertex) { { sx + sw, sy }, white, { u1, v0 } };
    verts[2] = (SDL_Vertex) { { sx, sy + sh }, white, { u0, v1 } };
    verts[3] = (SDL_Vertex) { { sx + sw, sy + sh }, white, { u1, v1 } };

    cl_draw_quad(verts, texture, z);
    cl_render_tasks[cl_render_task_count - 1].z = z;
}

void SDLGameRendererClassic_DrawOverlaySprite(SDL_Texture* texture, float x, float y, float w, float h, float z) {
    SDLGameRendererClassic_DrawOverlaySpriteEx(texture, x, y, w, h, z, 0, 0);
}
