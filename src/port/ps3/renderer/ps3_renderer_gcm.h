#ifndef PS3_RENDERER_GCM_H
#define PS3_RENDERER_GCM_H

#include "rendering/game_renderer.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include <cell/gcm.h>
#include <stdbool.h>
#include <stdint.h>

#define RENDER_TASK_MAX 8192
#define CELL_GCM_MAX_TEXTURES 1024
#define CELL_GCM_MAX_PALETTES 1088

typedef struct {
    float x, y, z;
    float u, v;
    unsigned int color; // ARGB (swapped to RGBA during draw_quad)
} GcmVertex;

typedef struct {
    uint32_t texture_handle;
    int vertex_offset;
    float z;
    int original_index;
    int index;
} GcmRenderTask;

void CRS_Renderer_Init(void);
void CRS_Renderer_BeginFrame(void);
void CRS_Renderer_RenderFrame(void);
void CRS_Renderer_EndFrame(void);
void CRS_Renderer_CreateTexture(unsigned int th);
void CRS_Renderer_DestroyTexture(unsigned int th);
void CRS_Renderer_UnlockTexture(unsigned int th);
void CRS_Renderer_CreatePalette(unsigned int ph);
void CRS_Renderer_DestroyPalette(unsigned int ph);
void CRS_Renderer_UnlockPalette(unsigned int ph);
void CRS_Renderer_SetTexture(unsigned int th);
void CRS_Renderer_DrawTexturedQuad(const Sprite* sprite, unsigned int color);
void CRS_Renderer_DrawSolidQuad(const Quad* vertices, unsigned int color);
void CRS_Renderer_DrawSprite(const Sprite* sprite, unsigned int color);
void CRS_Renderer_DrawSprite2(const Sprite2* sprite2);

#endif // PS3_RENDERER_GCM_H
