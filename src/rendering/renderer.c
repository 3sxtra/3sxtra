/**
 * @file renderer.c
 * @brief Renderer_ dispatch — routes to the active platform backend.
 */

#include "rendering/game_renderer.h"

// In 3sxtra, backends are abstracted through port/rendering/renderer.h
// which handles SDL, GL, GPU, Classic routing inside the port directory.
#include "port/rendering/renderer.h"
// Include SDL layer temporarily for FlushSprite2Batch which isn't generic yet
#include "port/sdl/renderer/sdl_game_renderer.h"

void Renderer_CreateTexture(unsigned int th) {
    SDLGameRenderer_CreateTexture(th);
}

void Renderer_DestroyTexture(unsigned int texture_handle) {
    SDLGameRenderer_DestroyTexture(texture_handle);
}

void Renderer_UnlockTexture(unsigned int th) {
    SDLGameRenderer_UnlockTexture(th);
}

void Renderer_CreatePalette(unsigned int ph) {
    SDLGameRenderer_CreatePalette(ph);
}

void Renderer_DestroyPalette(unsigned int palette_handle) {
    SDLGameRenderer_DestroyPalette(palette_handle);
}

void Renderer_UnlockPalette(unsigned int th) {
    SDLGameRenderer_UnlockPalette(th);
}

void Renderer_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    RendererVertex v[4];
    for (int i = 0; i < 4; i++) {
        v[i].x = sprite->v[i].x;
        v[i].y = sprite->v[i].y;
        v[i].z = sprite->v[i].z;
        v[i].u = sprite->t[i].s;
        v[i].v = sprite->t[i].t;
        v[i].color = color;
    }
    // We named this Renderer_DrawSpriteVtx in our fork to prevent naming collision with Sprite* API
    Renderer_DrawTexturedQuadVtx((const RendererVertex*)&v, 4);
}

void Renderer_DrawSprite(const Sprite* sprite, unsigned int color) {
    RendererVertex v[4];
    for (int i = 0; i < 4; i++) {
        v[i].x = sprite->v[i].x;
        v[i].y = sprite->v[i].y;
        v[i].z = sprite->v[i].z;
        v[i].u = sprite->t[i].s;
        v[i].v = sprite->t[i].t;
        v[i].color = color;
    }
    Renderer_DrawSpriteVtx((const RendererVertex*)&v, 4);
}

void Renderer_DrawSprite2(const Sprite2* sprite2) {
    RendererVertex v[4];
    // Sprite2 only has 2 vertices (top-left, bottom-right), we need to expand to 4 for 3sxtra
    v[0].x = sprite2->v[0].x;
    v[0].y = sprite2->v[0].y;
    v[0].z = sprite2->v[0].z;
    v[0].u = sprite2->t[0].s;
    v[0].v = sprite2->t[0].t;
    v[0].color = sprite2->vertex_color;

    v[1].x = sprite2->v[1].x;
    v[1].y = sprite2->v[0].y;
    v[1].z = sprite2->v[0].z;
    v[1].u = sprite2->t[1].s;
    v[1].v = sprite2->t[0].t;
    v[1].color = sprite2->vertex_color;

    v[2].x = sprite2->v[1].x;
    v[2].y = sprite2->v[1].y;
    v[2].z = sprite2->v[1].z;
    v[2].u = sprite2->t[1].s;
    v[2].v = sprite2->t[1].t;
    v[2].color = sprite2->vertex_color;

    v[3].x = sprite2->v[0].x;
    v[3].y = sprite2->v[1].y;
    v[3].z = sprite2->v[1].z;
    v[3].u = sprite2->t[0].s;
    v[3].v = sprite2->t[1].t;
    v[3].color = sprite2->vertex_color;

    Renderer_DrawSpriteVtx((const RendererVertex*)&v, 4);
}

void Renderer_DrawSolidQuad(const Quad* quad, unsigned int color) {
    RendererVertex v[4];
    for (int i = 0; i < 4; i++) {
        v[i].x = quad->v[i].x;
        v[i].y = quad->v[i].y;
        v[i].z = quad->v[i].z;
        v[i].u = 0;
        v[i].v = 0;
        v[i].color = color;
    }
    Renderer_DrawSolidQuadVtx((const RendererVertex*)&v, 4);
}

void Renderer_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count) {
    SDLGameRenderer_FlushSprite2Batch(chips, active_layers, count);
}
