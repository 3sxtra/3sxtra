#ifndef SDL_GAME_RENDERER_INTERNAL_H
#define SDL_GAME_RENDERER_INTERNAL_H

#include "port/sdl/sdl_game_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

// OpenGL Backend
void SDLGameRendererGL_Init(void);
void SDLGameRendererGL_Shutdown(void);
void SDLGameRendererGL_BeginFrame(void);
void SDLGameRendererGL_RenderFrame(void);
void SDLGameRendererGL_EndFrame(void);
void SDLGameRendererGL_CreateTexture(unsigned int th);
void SDLGameRendererGL_DestroyTexture(unsigned int texture_handle);
void SDLGameRendererGL_UnlockTexture(unsigned int th);
void SDLGameRendererGL_CreatePalette(unsigned int ph);
void SDLGameRendererGL_DestroyPalette(unsigned int palette_handle);
void SDLGameRendererGL_UnlockPalette(unsigned int ph);
void SDLGameRendererGL_SetTexture(unsigned int th);
void SDLGameRendererGL_DrawTexturedQuad(const Sprite* sprite, unsigned int color);
void SDLGameRendererGL_DrawSolidQuad(const Quad* vertices, unsigned int color);
void SDLGameRendererGL_DrawSprite(const Sprite* sprite, unsigned int color);
void SDLGameRendererGL_DrawSprite2(const Sprite2* sprite2);
unsigned int SDLGameRendererGL_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle);

// GPU Backend
void SDLGameRendererGPU_Init(void);
void SDLGameRendererGPU_Shutdown(void);
void SDLGameRendererGPU_BeginFrame(void);
void SDLGameRendererGPU_RenderFrame(void);
void SDLGameRendererGPU_EndFrame(void);
SDL_GPUCommandBuffer* SDLGameRendererGPU_GetCommandBuffer(void);
SDL_GPUTexture* SDLGameRendererGPU_GetSwapchainTexture(void);
SDL_GPUTexture* SDLGameRendererGPU_GetCanvasTexture(void); // New
void SDLGameRendererGPU_CreateTexture(unsigned int th);
void SDLGameRendererGPU_DestroyTexture(unsigned int texture_handle);
void SDLGameRendererGPU_UnlockTexture(unsigned int th);
void SDLGameRendererGPU_CreatePalette(unsigned int ph);
void SDLGameRendererGPU_DestroyPalette(unsigned int palette_handle);
void SDLGameRendererGPU_UnlockPalette(unsigned int ph);
void SDLGameRendererGPU_SetTexture(unsigned int th);
void SDLGameRendererGPU_DrawTexturedQuad(const Sprite* sprite, unsigned int color);
void SDLGameRendererGPU_DrawSolidQuad(const Quad* vertices, unsigned int color);
void SDLGameRendererGPU_DrawSprite(const Sprite* sprite, unsigned int color);
void SDLGameRendererGPU_DrawSprite2(const Sprite2* sprite2);
unsigned int SDLGameRendererGPU_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle);

// SDL2D Backend (SDL_Renderer software/accelerated 2D)
void SDLGameRendererSDL_Init(void);
void SDLGameRendererSDL_Shutdown(void);
void SDLGameRendererSDL_BeginFrame(void);
void SDLGameRendererSDL_RenderFrame(void);
void SDLGameRendererSDL_EndFrame(void);
void SDLGameRendererSDL_CreateTexture(unsigned int th);
void SDLGameRendererSDL_DestroyTexture(unsigned int texture_handle);
void SDLGameRendererSDL_UnlockTexture(unsigned int th);
void SDLGameRendererSDL_CreatePalette(unsigned int ph);
void SDLGameRendererSDL_DestroyPalette(unsigned int palette_handle);
void SDLGameRendererSDL_UnlockPalette(unsigned int ph);
void SDLGameRendererSDL_SetTexture(unsigned int th);
void SDLGameRendererSDL_DrawTexturedQuad(const Sprite* sprite, unsigned int color);
void SDLGameRendererSDL_DrawSolidQuad(const Quad* vertices, unsigned int color);
void SDLGameRendererSDL_DrawSprite(const Sprite* sprite, unsigned int color);
void SDLGameRendererSDL_DrawSprite2(const Sprite2* sprite2);
unsigned int SDLGameRendererSDL_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle);
SDL_Texture* SDLGameRendererSDL_GetCanvas(void);

#ifdef __cplusplus
}
#endif

#endif
