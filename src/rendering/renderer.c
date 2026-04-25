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
    PortRenderer_DrawTexturedQuad(sprite, color);
}

void Renderer_DrawSprite(const Sprite* sprite, unsigned int color) {
    PortRenderer_DrawSprite(sprite, color);
}

void Renderer_DrawSprite2(const Sprite2* sprite2) {
    PortRenderer_DrawSprite2(sprite2);
}

void Renderer_DrawSolidQuad(const Quad* quad, unsigned int color) {
    PortRenderer_DrawSolidQuad(quad, color);
}

void Renderer_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count) {
    SDLGameRenderer_FlushSprite2Batch(chips, active_layers, count);
}
