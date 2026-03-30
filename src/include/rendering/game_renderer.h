#ifndef RENDERING_GAME_RENDERER_H
#define RENDERING_GAME_RENDERER_H

#include "rendering/primitives.h"

void Renderer_CreateTexture(unsigned int th);
void Renderer_DestroyTexture(unsigned int texture_handle);
void Renderer_UnlockTexture(unsigned int th);
void Renderer_CreatePalette(unsigned int ph);
void Renderer_DestroyPalette(unsigned int palette_handle);
void Renderer_UnlockPalette(unsigned int th);
void Renderer_SetTexture(int th);
void Renderer_DrawTexturedQuad(const Sprite* sprite, unsigned int color);
void Renderer_DrawSprite(const Sprite* sprite, unsigned int color);
void Renderer_DrawSprite2(const Sprite2* sprite2);
void Renderer_DrawSolidQuad(const Quad* quad, unsigned int color);
void Renderer_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count);

#endif
