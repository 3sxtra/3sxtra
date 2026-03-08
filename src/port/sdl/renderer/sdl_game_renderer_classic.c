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

static SDL_Surface* surfaces[FL_TEXTURE_MAX] = { NULL };
static SDL_Palette* palettes[FL_PALETTE_MAX] = { NULL };

// Flat 2D texture cache: texture_cache[texture_index][palette_handle]
// palette_handle index 0 = non-indexed texture, 1..FL_PALETTE_MAX = indexed
static SDL_Texture* texture_cache[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1] = { { NULL } };

static SDL_Texture* textures_to_destroy[TEXTURES_TO_DESTROY_MAX] = { NULL };
static int textures_to_destroy_count = 0;

static SDL_Texture* current_texture = NULL;
static int current_texture_handle = 0;
static int current_palette_handle = 0;

static RenderTask render_tasks[RENDER_TASK_MAX];
static int render_task_count = 0;

// Batch buffers for SDL_RenderGeometry
static SDL_Vertex batch_vertices[RENDER_TASK_MAX * 4];
static int batch_indices[RENDER_TASK_MAX * 6];
static bool batch_buffers_initialized = false;

// PS2 CLUT shuffle table
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

// Color reading helpers
static void read_rgba32_fcolor_classic(Uint32 pixel, SDL_FColor* fcolor) {
    fcolor->b = (float)(pixel & 0xFF) / 255.0f;
    fcolor->g = (float)((pixel >> 8) & 0xFF) / 255.0f;
    fcolor->r = (float)((pixel >> 16) & 0xFF) / 255.0f;
    fcolor->a = (float)((pixel >> 24) & 0xFF) / 255.0f;
}

static void read_rgba16_color_classic(Uint16 pixel, SDL_Color* color) {
    color->r = (Uint8)(((pixel & 0x1F) * 255 + 15) / 31);
    color->g = (Uint8)((((pixel >> 5) & 0x1F) * 255 + 15) / 31);
    color->b = (Uint8)((((pixel >> 10) & 0x1F) * 255 + 15) / 31);
    color->a = (pixel & 0x8000) ? 255 : 0;
}

static void read_color_classic(const void* pixels, int index, size_t color_size, SDL_Color* color) {
    switch (color_size) {
    case 2: {
        const Uint16* rgba16_colors = (const Uint16*)pixels;
        read_rgba16_color_classic(rgba16_colors[index], color);
        break;
    }
    case 4: {
        const Uint32* rgba32_colors = (const Uint32*)pixels;
        Uint32 pixel = rgba32_colors[index];
        color->b = pixel & 0xFF;
        color->g = (pixel >> 8) & 0xFF;
        color->r = (pixel >> 16) & 0xFF;
        color->a = (pixel >> 24) & 0xFF;
        break;
    }
    }
}

// qsort comparator: sort by Z depth, then stable by original index
static int compare_render_tasks(const void* a, const void* b) {
    const RenderTask* ta = (const RenderTask*)a;
    const RenderTask* tb = (const RenderTask*)b;
    if (ta->z < tb->z) return -1;
    if (ta->z > tb->z) return 1;
    return ta->original_index - tb->original_index;
}

static void clear_render_tasks(void) {
    render_task_count = 0;
}

// --- draw_quad: enqueue a rendering task ---
static void draw_quad(SDL_Vertex vertices[4], SDL_Texture* texture, float z) {
    TRACE_SUB_BEGIN("Classic:DrawQuad");
    if (render_task_count >= RENDER_TASK_MAX) {
        TRACE_SUB_END();
        return;
    }

    RenderTask* task = &render_tasks[render_task_count];
    task->texture = texture;
    task->z = z;
    task->original_index = render_task_count;
    memcpy(task->vertices, vertices, sizeof(SDL_Vertex) * 4);
    render_task_count++;
    TRACE_SUB_END();
}

static void push_texture_to_destroy_classic(SDL_Texture* texture) {
    if (textures_to_destroy_count >= TEXTURES_TO_DESTROY_MAX) {
        SDL_DestroyTexture(texture);
        return;
    }
    textures_to_destroy[textures_to_destroy_count++] = texture;
}

// --- Public API ---

void SDLGameRendererClassic_Init(void) {
    TRACE_SUB_BEGIN("Classic:Init");
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    cps3_canvas_classic = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_TARGET, cps3_width, cps3_height);
    SDL_SetTextureScaleMode(cps3_canvas_classic, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(cps3_canvas_classic, SDL_BLENDMODE_BLEND);

    // Pre-initialize batch index buffers (6 indices per quad: 2 triangles)
    if (!batch_buffers_initialized) {
        for (int i = 0; i < RENDER_TASK_MAX; i++) {
            int base = i * 4;
            int idx_base = i * 6;
            batch_indices[idx_base + 0] = base + 0;
            batch_indices[idx_base + 1] = base + 1;
            batch_indices[idx_base + 2] = base + 2;
            batch_indices[idx_base + 3] = base + 0;
            batch_indices[idx_base + 4] = base + 2;
            batch_indices[idx_base + 5] = base + 3;
        }
        batch_buffers_initialized = true;
    }

    SDL_Log("[Classic] Initialized — simple SDL backend for benchmarking");
    TRACE_SUB_END();
}

void SDLGameRendererClassic_Shutdown(void) {
    TRACE_SUB_BEGIN("Classic:Shutdown");
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        if (surfaces[i]) {
            SDL_DestroySurface(surfaces[i]);
            surfaces[i] = NULL;
        }
        for (int j = 0; j <= FL_PALETTE_MAX; j++) {
            if (texture_cache[i][j]) {
                SDL_DestroyTexture(texture_cache[i][j]);
                texture_cache[i][j] = NULL;
            }
        }
    }
    for (int i = 0; i < FL_PALETTE_MAX; i++) {
        if (palettes[i]) {
            SDL_DestroyPalette(palettes[i]);
            palettes[i] = NULL;
        }
    }
    if (cps3_canvas_classic) {
        SDL_DestroyTexture(cps3_canvas_classic);
        cps3_canvas_classic = NULL;
    }
    clear_render_tasks();
    batch_buffers_initialized = false;
    TRACE_SUB_END();
}

void SDLGameRendererClassic_BeginFrame(void) {
    TRACE_SUB_BEGIN("Classic:BeginFrame");
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
    TRACE_SUB_END();
}

void SDLGameRendererClassic_RenderFrame(void) {
    TRACE_ZONE_N("Classic:RenderFrame");
    TRACE_PLOT_INT("ClassicRenderTasks", render_task_count);
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    SDL_SetRenderTarget(renderer, cps3_canvas_classic);

    if (render_task_count == 0) {
        TRACE_ZONE_END();
        return;
    }

    // Simple qsort — no adaptive sorting, no radix sort
    TRACE_SUB_BEGIN("Classic:Sort");
    qsort(render_tasks, render_task_count, sizeof(RenderTask), compare_render_tasks);
    TRACE_SUB_END();

    // Batch rendering: group consecutive tasks with same texture pointer
    TRACE_SUB_BEGIN("Classic:BatchRender");
    int batch_start = 0;
    SDL_Texture* current_batch_texture = render_tasks[0].texture;
    int batch_count = 0;

    for (int i = 0; i <= render_task_count; i++) {
        bool should_flush = (i == render_task_count) ||
                            (render_tasks[i].texture != current_batch_texture);

        if (should_flush) {
            int batch_size = i - batch_start;
            if (batch_size > 0) {
                // Copy vertices to batch buffer
                for (int j = 0; j < batch_size; j++) {
                    memcpy(&batch_vertices[j * 4], render_tasks[batch_start + j].vertices,
                           4 * sizeof(SDL_Vertex));
                }

                // Single draw call for entire batch
                SDL_RenderGeometry(renderer, current_batch_texture,
                                   batch_vertices, batch_size * 4,
                                   batch_indices, batch_size * 6);
                batch_count++;
            }

            if (i < render_task_count) {
                current_batch_texture = render_tasks[i].texture;
                batch_start = i;
            }
        }
    }
    TRACE_PLOT_INT("ClassicBatches", batch_count);
    TRACE_SUB_END();

    TRACE_ZONE_END();
}

void SDLGameRendererClassic_EndFrame(void) {
    TRACE_SUB_BEGIN("Classic:EndFrame");
    // Destroy deferred textures
    for (int i = 0; i < textures_to_destroy_count; i++) {
        SDL_DestroyTexture(textures_to_destroy[i]);
        textures_to_destroy[i] = NULL;
    }
    textures_to_destroy_count = 0;
    clear_render_tasks();
    TRACE_SUB_END();
}

SDL_Texture* SDLGameRendererClassic_GetCanvas(void) {
    return cps3_canvas_classic;
}

// --- Texture Management ---

void SDLGameRendererClassic_CreateTexture(unsigned int th) {
    TRACE_SUB_BEGIN("Classic:CreateTexture");
    int texture_index = th - 1;
    FLTexture* tex = &flTexture[th];

    if (surfaces[texture_index] != NULL) {
        SDL_DestroySurface(surfaces[texture_index]);
        surfaces[texture_index] = NULL;
    }

    int texture_format = tex->Cfg & 0x3F;
    int width = 1 << ((tex->Cfg >> 26) & 0xF);
    int height = 1 << ((tex->Cfg >> 30) & 0xF);

    SDL_Surface* surface = NULL;
    switch (texture_format) {
    case PSMT8:
        surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_INDEX8,
                                        tex->pBuf, width);
        break;
    case PSMT4:
        surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_INDEX4MSB,
                                        tex->pBuf, width / 2);
        break;
    case PSMCT16:
        surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_ABGR1555,
                                        tex->pBuf, width * 2);
        break;
    case PSMCT32:
        surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_ABGR8888,
                                        tex->pBuf, width * 4);
        break;
    default:
        TRACE_SUB_END();
        return;
    }

    surfaces[texture_index] = surface;

    // For non-indexed formats, eagerly create SDL_Texture
    if (!SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
        SDL_Texture* sdl_texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (sdl_texture) {
            SDL_SetTextureScaleMode(sdl_texture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND);
        }
        // Store at palette index 0 (non-indexed slot)
        if (texture_cache[texture_index][0]) {
            push_texture_to_destroy_classic(texture_cache[texture_index][0]);
        }
        texture_cache[texture_index][0] = sdl_texture;
    }
    TRACE_SUB_END();
}

void SDLGameRendererClassic_DestroyTexture(unsigned int texture_handle) {
    TRACE_SUB_BEGIN("Classic:DestroyTexture");
    int texture_index = texture_handle - 1;
    if (surfaces[texture_index]) {
        SDL_DestroySurface(surfaces[texture_index]);
        surfaces[texture_index] = NULL;
    }
    for (int j = 0; j <= FL_PALETTE_MAX; j++) {
        if (texture_cache[texture_index][j]) {
            push_texture_to_destroy_classic(texture_cache[texture_index][j]);
            texture_cache[texture_index][j] = NULL;
        }
    }
    TRACE_SUB_END();
}

void SDLGameRendererClassic_UnlockTexture(unsigned int th) {
    TRACE_SUB_BEGIN("Classic:UnlockTexture");
    int texture_index = th - 1;
    // Invalidate all cached textures for this surface
    for (int j = 0; j <= FL_PALETTE_MAX; j++) {
        if (texture_cache[texture_index][j]) {
            push_texture_to_destroy_classic(texture_cache[texture_index][j]);
            texture_cache[texture_index][j] = NULL;
        }
    }
    // Recreate non-indexed texture if applicable
    SDL_Surface* surface = surfaces[texture_index];
    if (surface && !SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
        SDL_Texture* sdl_texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (sdl_texture) {
            SDL_SetTextureScaleMode(sdl_texture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND);
        }
        texture_cache[texture_index][0] = sdl_texture;
    }
    TRACE_SUB_END();
}

// --- Palette Management ---

void SDLGameRendererClassic_CreatePalette(unsigned int ph) {
    TRACE_SUB_BEGIN("Classic:CreatePalette");
    FLTexture* pal = &flPalette[ph];
    int color_fmt = pal->Cfg & 0x3F;
    size_t color_size = (color_fmt == PSMCT32) ? 4 : 2;
    int ncolors = (color_fmt == PSMCT32) ? 256 :
                  (color_size == 2) ? 256 : 16;

    // CPS3 palette format: PSMT8 = 256 colors, PSMT4 = 16 colors
    ncolors = (pal->Cfg >> 26) == 0 ? 16 : 256;

    if (palettes[ph - 1]) {
        SDL_DestroyPalette(palettes[ph - 1]);
    }

    SDL_Palette* sdl_pal = SDL_CreatePalette(ncolors);
    if (!sdl_pal) {
        TRACE_SUB_END();
        return;
    }

    SDL_Color colors[256];
    for (int i = 0; i < ncolors; i++) {
        int idx = (ncolors == 256) ? ps2_clut_shuffle[i] : i;
        read_color_classic(pal->pBuf, idx, color_size, &colors[i]);
    }
    SDL_SetPaletteColors(sdl_pal, colors, 0, ncolors);

    palettes[ph - 1] = sdl_pal;
    TRACE_SUB_END();
}

void SDLGameRendererClassic_DestroyPalette(unsigned int palette_handle) {
    TRACE_SUB_BEGIN("Classic:DestroyPalette");
    int pi = palette_handle - 1;
    if (palettes[pi]) {
        SDL_DestroyPalette(palettes[pi]);
        palettes[pi] = NULL;
    }
    // Invalidate all texture cache entries for this palette
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        if (texture_cache[i][palette_handle]) {
            push_texture_to_destroy_classic(texture_cache[i][palette_handle]);
            texture_cache[i][palette_handle] = NULL;
        }
    }
    TRACE_SUB_END();
}

void SDLGameRendererClassic_UnlockPalette(unsigned int ph) {
    TRACE_SUB_BEGIN("Classic:UnlockPalette");
    // Re-create palette from updated data
    SDLGameRendererClassic_CreatePalette(ph);
    // Invalidate all texture cache entries using this palette
    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        if (texture_cache[i][ph]) {
            push_texture_to_destroy_classic(texture_cache[i][ph]);
            texture_cache[i][ph] = NULL;
        }
    }
    TRACE_SUB_END();
}

// --- SetTexture: use SDL's built-in palette+surface → texture conversion ---

void SDLGameRendererClassic_SetTexture(unsigned int th) {
    TRACE_SUB_BEGIN("Classic:SetTexture");
    FLTexture* tex = &flTexture[th];
    int texture_index = th - 1;
    int palette_handle = tex->Clut;

    SDL_Surface* surface = surfaces[texture_index];
    if (!surface) {
        current_texture = NULL;
        TRACE_SUB_END();
        return;
    }
    current_texture_handle = th;
    current_palette_handle = palette_handle;

    if (SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        // Check flat cache first
        if (palette_handle > 0 && palette_handle <= FL_PALETTE_MAX &&
            texture_cache[texture_index][palette_handle] != NULL) {
            current_texture = texture_cache[texture_index][palette_handle];
            TRACE_SUB_END();
            return;
        }

        // Cache miss: use SDL_SetSurfacePalette + SDL_CreateTextureFromSurface
        if (palette_handle > 0 && palette_handle <= FL_PALETTE_MAX &&
            palettes[palette_handle - 1] != NULL) {
            SDL_SetSurfacePalette(surface, palettes[palette_handle - 1]);
            SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
            SDL_Texture* sdl_texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (sdl_texture) {
                SDL_SetTextureScaleMode(sdl_texture, SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND);
                texture_cache[texture_index][palette_handle] = sdl_texture;
                current_texture = sdl_texture;
            } else {
                current_texture = NULL;
            }
        } else {
            current_texture = NULL;
        }
    } else {
        // Non-indexed: use eagerly created texture
        current_texture = texture_cache[texture_index][0];
    }
    TRACE_SUB_END();
}

// --- Drawing ---

void SDLGameRendererClassic_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    TRACE_SUB_BEGIN("Classic:DrawTexturedQuad");
    SDL_Vertex vertices[4];
    SDL_FColor fcolor;
    read_rgba32_fcolor_classic(color, &fcolor);

    for (int i = 0; i < 4; i++) {
        vertices[i].position.x = sprite->v[i].x;
        vertices[i].position.y = sprite->v[i].y;
        vertices[i].color = fcolor;
        vertices[i].tex_coord.x = sprite->t[i].u;
        vertices[i].tex_coord.y = sprite->t[i].v;
    }

    draw_quad(vertices, current_texture, sprite->v[0].z);
    TRACE_SUB_END();
}

void SDLGameRendererClassic_DrawSolidQuad(const Quad* quad, unsigned int color) {
    TRACE_SUB_BEGIN("Classic:DrawSolidQuad");
    SDL_Vertex vertices[4];
    SDL_FColor fcolor;
    read_rgba32_fcolor_classic(color, &fcolor);

    for (int i = 0; i < 4; i++) {
        vertices[i].position.x = quad->v[i].x;
        vertices[i].position.y = quad->v[i].y;
        vertices[i].color = fcolor;
        vertices[i].tex_coord.x = 0.0f;
        vertices[i].tex_coord.y = 0.0f;
    }

    draw_quad(vertices, NULL, quad->v[0].z);
    TRACE_SUB_END();
}

void SDLGameRendererClassic_DrawSprite(const Sprite* sprite, unsigned int color) {
    TRACE_SUB_BEGIN("Classic:DrawSprite");
    SDL_Vertex vertices[4];
    SDL_FColor fcolor;
    read_rgba32_fcolor_classic(color, &fcolor);

    // Arrange sprite data into quad: TL=0, TR=1, BL=2, BR=3
    // Sprite v[0]=TL, v[1]=TR texture, v[2]=BL, v[3]=BR
    vertices[0].position.x = sprite->v[0].x;
    vertices[0].position.y = sprite->v[0].y;
    vertices[0].color = fcolor;
    vertices[0].tex_coord.x = sprite->t[0].u;
    vertices[0].tex_coord.y = sprite->t[0].v;

    vertices[1].position.x = sprite->v[1].x;
    vertices[1].position.y = sprite->v[0].y;
    vertices[1].color = fcolor;
    vertices[1].tex_coord.x = sprite->t[1].u;
    vertices[1].tex_coord.y = sprite->t[0].v;

    vertices[2].position.x = sprite->v[0].x;
    vertices[2].position.y = sprite->v[1].y;
    vertices[2].color = fcolor;
    vertices[2].tex_coord.x = sprite->t[0].u;
    vertices[2].tex_coord.y = sprite->t[1].v;

    vertices[3].position.x = sprite->v[1].x;
    vertices[3].position.y = sprite->v[1].y;
    vertices[3].color = fcolor;
    vertices[3].tex_coord.x = sprite->t[1].u;
    vertices[3].tex_coord.y = sprite->t[1].v;

    draw_quad(vertices, current_texture, sprite->v[0].z);
    TRACE_SUB_END();
}

void SDLGameRendererClassic_DrawSprite2(const Sprite2* sprite2) {
    TRACE_SUB_BEGIN("Classic:DrawSprite2");
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
    unsigned int tex_code = sprite2->tex_code;
    if (tex_code > 0) {
        SDLGameRendererClassic_SetTexture(tex_code);
    }

    SDLGameRendererClassic_DrawSprite(&sprite, sprite2->vertex_color);
    TRACE_SUB_END();
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
