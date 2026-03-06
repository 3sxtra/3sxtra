#ifndef SDL_GAME_RENDERER_H
#define SDL_GAME_RENDERER_H

#include "structs.h"
#include <SDL3/SDL.h>

typedef struct SDLGameRenderer_Vertex {
    struct {
        float x;
        float y;
        float z;
        float w;
    } coord;
    unsigned int color;
    TexCoord tex_coord;
} SDLGameRenderer_Vertex;

typedef struct Quad {
    Vec3 v[4];
} Quad;

typedef struct Sprite {
    Vec3 v[4];
    TexCoord t[4];
    unsigned int tex_code;
} Sprite;

typedef enum {
    SDL_GAME_RENDERER_BLEND_NORMAL,
    SDL_GAME_RENDERER_BLEND_ADD,
} SDLGameRenderer_BlendMode;

typedef struct Sprite2 {
    Vec3 v[2];
    TexCoord t[2];
    unsigned int vertex_color;
    unsigned int tex_code;
    unsigned int id;
} Sprite2;

extern unsigned int cps3_canvas_texture;

void SDLGameRenderer_Init();
void SDLGameRenderer_Shutdown();
void SDLGameRenderer_BeginFrame();
void SDLGameRenderer_RenderFrame();
void SDLGameRenderer_EndFrame();

void SDLGameRenderer_CreateTexture(unsigned int th);
void SDLGameRenderer_SetBlendMode(SDLGameRenderer_BlendMode mode);
void SDLGameRenderer_DestroyTexture(unsigned int texture_handle);
void SDLGameRenderer_UnlockTexture(unsigned int th);
void SDLGameRenderer_CreatePalette(unsigned int ph);
void SDLGameRenderer_DestroyPalette(unsigned int palette_handle);
void SDLGameRenderer_UnlockPalette(unsigned int ph);
void SDLGameRenderer_SetTexture(unsigned int th);
void SDLGameRenderer_DrawTexturedQuad(const Sprite* sprite, unsigned int color);
void SDLGameRenderer_DrawSolidQuad(const Quad* vertices, unsigned int color);
void SDLGameRenderer_DrawSprite(const Sprite* sprite, unsigned int color);
void SDLGameRenderer_DrawSprite2(const Sprite2* sprite2);

// ⚡ Batch flush: submits all sprites in one call with tex_code sorting (GPU) and inlined vertex setup.
// `active_layers` is a bitmask array — sprite i is only drawn if active_layers[chip[i].id] != 0.
void SDLGameRenderer_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count);

// Returns the cached GL texture ID for a given texture+palette combination.
// Used by ImGui to render game textures. Returns 0 if not found/invalid.
unsigned int SDLGameRenderer_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle);

// Dumps all currently loaded textures to textures/*.tga
void SDLGameRenderer_DumpTextures(void);

#endif
