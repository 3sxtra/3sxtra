/**
 * @file game_renderer_vtable.h
 * @brief Function-pointer vtable for backend dispatch.
 *
 * Replaces the 300-line if/else chain in sdl_game_renderer.c with a
 * single const pointer that is set once at init time.  Each backend
 * (GL, GPU, SDL2D, Classic) provides a static const instance.
 */
#ifndef GAME_RENDERER_VTABLE_H
#define GAME_RENDERER_VTABLE_H

#include "port/rendering/renderer.h"
#include "rendering/primitives.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GameRendererVtable {
    /* ---- Lifecycle (6) ---- */
    void (*Init)(void);
    void (*Shutdown)(void);
    void (*BeginFrame)(void);
    void (*RenderFrame)(void);
    void (*ExecutePass)(int pass_index, int vp_x, int vp_y, int vp_w, int vp_h);
    void (*EndFrame)(void);

    /* ---- Texture management (7) ---- */
    void (*CreateTexture)(unsigned int th);
    void (*DestroyTexture)(unsigned int texture_handle);
    void (*UnlockTexture)(unsigned int th);
    void (*CreatePalette)(unsigned int ph);
    void (*DestroyPalette)(unsigned int palette_handle);
    void (*UnlockPalette)(unsigned int ph);
    void (*SetTexture)(unsigned int th);

    /* ---- FrameGraph Transient Resources (3) ---- */
    void* (*CreateTransientRenderTarget)(int width, int height);
    void (*DestroyTransientRenderTarget)(void* handle);
    void (*BindTransientRenderTarget)(void* handle);

    /* ---- State (1) ---- */
    void (*SetBlendMode)(RendererBlendMode mode);

    /* ---- Drawing (5) ---- */
    void (*DrawTexturedQuad)(const Sprite* sprite, unsigned int color);
    void (*DrawSolidQuad)(const Quad* vertices, unsigned int color);
    void (*DrawSprite)(const Sprite* sprite, unsigned int color);
    void (*DrawSprite2)(const Sprite2* sprite2);
    void (*FlushSprite2Batch)(Sprite2* chips, const unsigned char* active_layers, int count);

    /* ---- Debug (2) ---- */
    unsigned int (*GetCachedGLTexture)(unsigned int texture_handle, unsigned int palette_handle);
    void (*DumpTextures)(void);

    /* ---- Overlays (3) ---- */
    void (*DrawOverlayQuad)(void* texture, float x, float y, float w, float h, float z);
    void (*DrawOverlayQuadEx)(void* texture, float x, float y, float w, float h, float z, int flip_x, int flip_y);
    void (*DrawOverlaySubQuadEx)(void* texture, float x, float y, float w, float h, float u0, float v0, float u1,
                                 float v1, float z);
} GameRendererVtable;

/** Active backend vtable — set once by GameRendererVtable_Init(). */
extern const GameRendererVtable* g_game_renderer;

/** Pick the vtable matching the current RendererBackend. */
void GameRendererVtable_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* GAME_RENDERER_VTABLE_H */
