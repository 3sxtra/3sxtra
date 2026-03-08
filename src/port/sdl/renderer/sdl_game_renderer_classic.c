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

#define RENDER_TASK_MAX 8192
#define TEXTURES_TO_DESTROY_MAX 1024

// AoS RenderTask — simple and readable (no SoA optimizations)
typedef struct RenderTask {
    SDL_Texture* texture;
    SDL_Vertex vertices[4];
    float z;
    int original_index;
} RenderTask;

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

static RenderTask cl_render_tasks[RENDER_TASK_MAX];
static int cl_render_task_count = 0;

// Batch buffers for SDL_RenderGeometry
static SDL_Vertex cl_batch_vertices[RENDER_TASK_MAX * 4];
static int cl_batch_indices[RENDER_TASK_MAX * 6];
static bool cl_batch_buffers_initialized = false;

// PS2 CLUT shuffle table
static const Uint8 cl_ps2_clut_shuffle[256] = {
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

// qsort comparator: sort by Z depth, then stable by original index
static int cl_compare_render_tasks(const void* a, const void* b) {
    const RenderTask* ta = (const RenderTask*)a;
    const RenderTask* tb = (const RenderTask*)b;
    if (ta->z < tb->z) return -1;
    if (ta->z > tb->z) return 1;
    return ta->original_index - tb->original_index;
}

// --- draw_quad: enqueue a rendering task ---
static void cl_draw_quad(SDL_Vertex vertices[4], SDL_Texture* texture, float z) {
    if (cl_render_task_count >= RENDER_TASK_MAX) return;

    RenderTask* task = &cl_render_tasks[cl_render_task_count];
    task->texture = texture;
    task->z = z;
    task->original_index = cl_render_task_count;
    memcpy(task->vertices, vertices, sizeof(SDL_Vertex) * 4);
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
    cps3_canvas_classic = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_TARGET, cps3_width, cps3_height);
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
            cl_batch_indices[idx_base + 3] = base + 0;
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

void SDLGameRendererClassic_RenderFrame(void) {
    TRACE_ZONE_N("Classic:RenderFrame");
    TRACE_PLOT_INT("ClassicRenderTasks", cl_render_task_count);
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    SDL_SetRenderTarget(renderer, cps3_canvas_classic);

    if (cl_render_task_count == 0) {
        TRACE_ZONE_END();
        return;
    }

    // Simple qsort — no adaptive sorting, no radix sort
    TRACE_SUB_BEGIN("Classic:Sort");
    qsort(cl_render_tasks, cl_render_task_count, sizeof(RenderTask), cl_compare_render_tasks);
    TRACE_SUB_END();

    // Batch rendering: group consecutive tasks with same texture pointer
    TRACE_SUB_BEGIN("Classic:BatchRender");
    int batch_start = 0;
    SDL_Texture* current_batch_texture = cl_render_tasks[0].texture;

    for (int i = 0; i <= cl_render_task_count; i++) {
        bool should_flush = (i == cl_render_task_count) ||
                            (cl_render_tasks[i].texture != current_batch_texture);

        if (should_flush) {
            int batch_size = i - batch_start;
            if (batch_size > 0) {
                for (int j = 0; j < batch_size; j++) {
                    memcpy(&cl_batch_vertices[j * 4], cl_render_tasks[batch_start + j].vertices,
                           4 * sizeof(SDL_Vertex));
                }
                SDL_RenderGeometry(renderer, current_batch_texture,
                                   cl_batch_vertices, batch_size * 4,
                                   cl_batch_indices, batch_size * 6);
            }

            if (i < cl_render_task_count) {
                current_batch_texture = cl_render_tasks[i].texture;
                batch_start = i;
            }
        }
    }
    TRACE_SUB_END();

    TRACE_ZONE_END();
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

    SDL_Surface* surface = SDL_CreateSurfaceFrom(fl_texture->width, fl_texture->height,
                                                  pixel_format, (void*)pixels, pitch);
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
    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX) return;

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
    if (texture_index < 0 || texture_index >= FL_TEXTURE_MAX) return;

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
    TRACE_ZONE_N("Classic:CreatePalette");
    const int palette_index = LO_16_BITS(ph) - 1;
    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX) {
        TRACE_ZONE_END();
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
    case SCE_GS_PSMCT32: color_size = 4; break;
    case SCE_GS_PSMCT16: color_size = 2; break;
    default:
        TRACE_ZONE_END();
        return;
    }

    SDL_Palette* sdl_pal = SDL_CreatePalette(color_count);
    if (!sdl_pal) {
        TRACE_ZONE_END();
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
    TRACE_ZONE_END();
}

void SDLGameRendererClassic_DestroyPalette(unsigned int palette_handle) {
    const int pi = LO_16_BITS(palette_handle) - 1;
    if (pi < 0 || pi >= FL_PALETTE_MAX) return;

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
    const int pi = LO_16_BITS(ph) - 1;
    if (pi < 0 || pi >= FL_PALETTE_MAX) return;

    // Re-create palette from updated data
    SDLGameRendererClassic_CreatePalette(ph);
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
    TRACE_ZONE_N("Classic:SetTexture");
    const int texture_handle = LO_16_BITS(th);
    const int palette_handle = HI_16_BITS(th);
    const int texture_index = texture_handle - 1;

    if (texture_handle < 1 || texture_handle > FL_TEXTURE_MAX) {
        cl_current_texture = NULL;
        cl_current_th = th;
        TRACE_ZONE_END();
        return;
    }

    SDL_Surface* surface = cl_surfaces[texture_index];
    if (!surface) {
        cl_current_texture = NULL;
        cl_current_th = th;
        TRACE_ZONE_END();
        return;
    }
    cl_current_th = th;

    if (SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        // Check flat cache first
        if (palette_handle > 0 && palette_handle <= FL_PALETTE_MAX &&
            cl_tex_cache[texture_index][palette_handle] != NULL) {
            cl_current_texture = cl_tex_cache[texture_index][palette_handle];
            TRACE_ZONE_END();
            return;
        }

        // Cache miss: use SDL_SetSurfacePalette + SDL_CreateTextureFromSurface
        if (palette_handle > 0 && palette_handle <= FL_PALETTE_MAX &&
            cl_palettes[palette_handle - 1] != NULL) {
            SDL_SetSurfacePalette(surface, cl_palettes[palette_handle - 1]);
            SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
            SDL_Texture* sdl_texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (sdl_texture) {
                SDL_SetTextureScaleMode(sdl_texture, SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND);
                cl_tex_cache[texture_index][palette_handle] = sdl_texture;
                cl_current_texture = sdl_texture;
            } else {
                cl_current_texture = NULL;
            }
        } else {
            cl_current_texture = NULL;
        }
    } else {
        // Non-indexed: use eagerly created texture at slot 0
        cl_current_texture = cl_tex_cache[texture_index][0];
    }
    TRACE_ZONE_END();
}

// --- Drawing ---

void SDLGameRendererClassic_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    SDL_Vertex vertices[4];
    SDL_FColor fcolor;
    cl_read_rgba32_fcolor(color, &fcolor);

    for (int i = 0; i < 4; i++) {
        vertices[i].position.x = sprite->v[i].x;
        vertices[i].position.y = sprite->v[i].y;
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

    for (int i = 0; i < 4; i++) {
        vertices[i].position.x = quad->v[i].x;
        vertices[i].position.y = quad->v[i].y;
        vertices[i].color = fcolor;
        vertices[i].tex_coord.x = 0.0f;
        vertices[i].tex_coord.y = 0.0f;
    }

    cl_draw_quad(vertices, NULL, quad->v[0].z);
}

void SDLGameRendererClassic_DrawSprite(const Sprite* sprite, unsigned int color) {
    SDL_Vertex vertices[4];
    SDL_FColor fcolor;
    cl_read_rgba32_fcolor(color, &fcolor);

    // TL
    vertices[0].position.x = sprite->v[0].x;
    vertices[0].position.y = sprite->v[0].y;
    vertices[0].color = fcolor;
    vertices[0].tex_coord.x = sprite->t[0].s;
    vertices[0].tex_coord.y = sprite->t[0].t;

    // TR
    vertices[1].position.x = sprite->v[1].x;
    vertices[1].position.y = sprite->v[0].y;
    vertices[1].color = fcolor;
    vertices[1].tex_coord.x = sprite->t[1].s;
    vertices[1].tex_coord.y = sprite->t[0].t;

    // BR
    vertices[2].position.x = sprite->v[1].x;
    vertices[2].position.y = sprite->v[1].y;
    vertices[2].color = fcolor;
    vertices[2].tex_coord.x = sprite->t[1].s;
    vertices[2].tex_coord.y = sprite->t[1].t;

    // BL
    vertices[3].position.x = sprite->v[0].x;
    vertices[3].position.y = sprite->v[1].y;
    vertices[3].color = fcolor;
    vertices[3].tex_coord.x = sprite->t[0].s;
    vertices[3].tex_coord.y = sprite->t[1].t;

    cl_draw_quad(vertices, cl_current_texture, sprite->v[0].z);
}

void SDLGameRendererClassic_DrawSprite2(const Sprite2* sprite2) {
    // Convert Sprite2 to Sprite, then call DrawSprite
    Sprite sprite;
    sprite.tex_code = sprite2->tex_code;
    sprite.v[0].x = sprite2->v[0].x + sprite2->modelX;
    sprite.v[0].y = sprite2->v[0].y + sprite2->modelY;
    sprite.v[0].z = sprite2->v[0].z;
    sprite.v[1].x = sprite2->v[1].x + sprite2->modelX;
    sprite.v[1].y = sprite2->v[1].y + sprite2->modelY;
    sprite.v[1].z = sprite2->v[1].z;
    sprite.t[0] = sprite2->t[0];
    sprite.t[1] = sprite2->t[1];

    // Set texture from tex_code
    if (sprite2->tex_code > 0) {
        SDLGameRendererClassic_SetTexture(sprite2->tex_code);
    }

    SDLGameRendererClassic_DrawSprite(&sprite, sprite2->vertex_color);
}

// FlushSprite2Batch — no optimization, just loop through and draw each sprite
void SDLGameRendererClassic_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count) {
    TRACE_ZONE_N("Classic:FlushBatch");
    TRACE_ZONE_VALUE(count);

    for (int i = 0; i < count; i++) {
        if (active_layers && !active_layers[chips[i].id]) {
            continue;
        }
        SDLGameRendererClassic_DrawSprite2(&chips[i]);
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
