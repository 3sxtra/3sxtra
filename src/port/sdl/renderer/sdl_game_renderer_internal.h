/**
 * @file sdl_game_renderer_internal.h
 * @brief Backend-specific renderer vtable declarations.
 */
#ifndef SDL_GAME_RENDERER_INTERNAL_H
#define SDL_GAME_RENDERER_INTERNAL_H

#include "port/sdl/renderer/sdl_game_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

// OpenGL Backend
void SDLGameRendererGL_Init(void);
void SDLGameRendererGL_Shutdown(void);
void SDLGameRendererGL_BeginFrame(void);
void SDLGameRendererGL_RenderFrame(void);
void SDLGameRendererGL_ExecutePass(int pass_index, int viewport_x, int viewport_y, int viewport_w, int viewport_h);
void SDLGameRendererGL_EndFrame(void);
void SDLGameRendererGL_CreateTexture(unsigned int th);
void SDLGameRendererGL_DestroyTexture(unsigned int texture_handle);
void SDLGameRendererGL_UnlockTexture(unsigned int th);
void SDLGameRendererGL_CreatePalette(unsigned int ph);
void SDLGameRendererGL_DestroyPalette(unsigned int palette_handle);
void SDLGameRendererGL_UnlockPalette(unsigned int ph);
void SDLGameRendererGL_SetTexture(unsigned int th);
void* SDLGameRendererGL_CreateTransientRenderTarget(int width, int height);
void SDLGameRendererGL_DestroyTransientRenderTarget(void* handle);
void SDLGameRendererGL_BindTransientRenderTarget(void* handle);
void SDLGameRendererGL_SetBlendMode(RendererBlendMode mode);
void SDLGameRendererGL_DrawTexturedQuad(const Sprite* sprite, unsigned int color);
void SDLGameRendererGL_DrawSolidQuad(const Quad* vertices, unsigned int color);
void SDLGameRendererGL_DrawSprite(const Sprite* sprite, unsigned int color);
void SDLGameRendererGL_DrawSprite2(const Sprite2* sprite2);
unsigned int SDLGameRendererGL_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle);
void SDLGameRendererGL_DumpTextures(void);
void SDLGameRendererGL_DumpPaletteStats(void);
void SDLGameRendererGL_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count);
void SDLGameRendererGL_DrawOverlayQuad(void* texture, float x, float y, float w, float h, float z);
void SDLGameRendererGL_DrawOverlayQuadEx(void* texture, float x, float y, float w, float h, float z, int flip_x,
                                         int flip_y);
void SDLGameRendererGL_DrawOverlaySubQuadEx(void* texture, float x, float y, float w, float h, float u0, float v0,
                                            float u1, float v1, float z);

// GPU Backend
void SDLGameRendererGPU_Init(void);
void SDLGameRendererGPU_Shutdown(void);
void SDLGameRendererGPU_BeginFrame(void);
void SDLGameRendererGPU_RenderFrame(void);
void SDLGameRendererGPU_ExecutePass(int pass_index, int viewport_x, int viewport_y, int viewport_w, int viewport_h);
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
void* SDLGameRendererGPU_CreateTransientRenderTarget(int width, int height);
void SDLGameRendererGPU_DestroyTransientRenderTarget(void* handle);
void SDLGameRendererGPU_BindTransientRenderTarget(void* handle);
void SDLGameRendererGPU_SetBlendMode(RendererBlendMode mode);
void SDLGameRendererGPU_DrawTexturedQuad(const Sprite* sprite, unsigned int color);
void SDLGameRendererGPU_DrawSolidQuad(const Quad* vertices, unsigned int color);
void SDLGameRendererGPU_DrawSprite(const Sprite* sprite, unsigned int color);
void SDLGameRendererGPU_DrawSprite2(const Sprite2* sprite2);
unsigned int SDLGameRendererGPU_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle);
void SDLGameRendererGPU_DumpTextures(void);
void SDLGameRendererGPU_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count);
void SDLGameRendererGPU_DrawOverlayQuad(void* texture, float x, float y, float w, float h, float z);
void SDLGameRendererGPU_DrawOverlayQuadEx(void* texture, float x, float y, float w, float h, float z, int flip_x,
                                          int flip_y);
void SDLGameRendererGPU_DrawOverlaySubQuadEx(void* texture, float x, float y, float w, float h, float u0, float v0,
                                             float u1, float v1, float z);
void SDLGameRendererGPU_QueueDeferredBlit(SDL_GPUTexture* texture, int tex_w, int tex_h, float x, float y, float w,
                                          float h, float z, int flip_x, int flip_y);
void SDLGameRendererGPU_QueueDeferredSubBlit(SDL_GPUTexture* texture, int tex_w, int tex_h, float x, float y, float w,
                                             float h, float u0, float v0, float u1, float v1, float z);
// ⚡ Opt6: LZ77 GPU compute decompression
int SDLGameRendererGPU_LZ77Available(void);
int SDLGameRendererGPU_LZ77Enqueue(const u8* compressed, u32 comp_size, u32 decomp_size, int texture_handle,
                                   int palette_handle, u32 code, u32 tile_dim);

// SDL2D Backend (SDL_Renderer software/accelerated 2D)
void SDLGameRendererSDL_Init(void);
void SDLGameRendererSDL_Shutdown(void);
void SDLGameRendererSDL_BeginFrame(void);
void SDLGameRendererSDL_RenderFrame(void);
void SDLGameRendererSDL_ExecutePass(int pass_index, int viewport_x, int viewport_y, int viewport_w, int viewport_h);
void SDLGameRendererSDL_EndFrame(void);
void SDLGameRendererSDL_CreateTexture(unsigned int th);
void SDLGameRendererSDL_DestroyTexture(unsigned int texture_handle);
void SDLGameRendererSDL_UnlockTexture(unsigned int th);
void SDLGameRendererSDL_CreatePalette(unsigned int ph);
void SDLGameRendererSDL_DestroyPalette(unsigned int palette_handle);
void SDLGameRendererSDL_UnlockPalette(unsigned int ph);
void SDLGameRendererSDL_SetTexture(unsigned int th);
void* SDLGameRendererSDL_CreateTransientRenderTarget(int width, int height);
void SDLGameRendererSDL_DestroyTransientRenderTarget(void* handle);
void SDLGameRendererSDL_BindTransientRenderTarget(void* handle);
void SDLGameRendererSDL_SetBlendMode(RendererBlendMode mode);
void SDLGameRendererSDL_DrawTexturedQuad(const Sprite* sprite, unsigned int color);
void SDLGameRendererSDL_DrawSolidQuad(const Quad* vertices, unsigned int color);
void SDLGameRendererSDL_DrawSprite(const Sprite* sprite, unsigned int color);
void SDLGameRendererSDL_DrawSprite2(const Sprite2* sprite2);
unsigned int SDLGameRendererSDL_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle);
void SDLGameRendererSDL_DumpTextures(void);
void SDLGameRendererSDL_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count);
SDL_Texture* SDLGameRendererSDL_GetCanvas(void);
void SDLGameRendererSDL_DrawOverlayQuad(void* texture, float x, float y, float w, float h, float z);
void SDLGameRendererSDL_DrawOverlayQuadEx(void* texture, float x, float y, float w, float h, float z, int flip_x,
                                          int flip_y);
void SDLGameRendererSDL_DrawOverlaySubQuadEx(void* texture, float x, float y, float w, float h, float u0, float v0,
                                             float u1, float v1, float z);

// SDL2D Classic Backend (simple reference renderer for benchmarking)
void SDLGameRendererClassic_Init(void);
void SDLGameRendererClassic_Shutdown(void);
void SDLGameRendererClassic_BeginFrame(void);
void SDLGameRendererClassic_RenderFrame(void);
void SDLGameRendererClassic_EndFrame(void);
void SDLGameRendererClassic_CreateTexture(unsigned int th);
void SDLGameRendererClassic_DestroyTexture(unsigned int texture_handle);
void SDLGameRendererClassic_UnlockTexture(unsigned int th);
void SDLGameRendererClassic_CreatePalette(unsigned int ph);
void SDLGameRendererClassic_DestroyPalette(unsigned int palette_handle);
void SDLGameRendererClassic_UnlockPalette(unsigned int ph);
void SDLGameRendererClassic_SetTexture(unsigned int th);
void SDLGameRendererClassic_SetBlendMode(RendererBlendMode mode);
void SDLGameRendererClassic_DrawTexturedQuad(const Sprite* sprite, unsigned int color);
void SDLGameRendererClassic_DrawSolidQuad(const Quad* vertices, unsigned int color);
void SDLGameRendererClassic_DrawSprite(const Sprite* sprite, unsigned int color);
void SDLGameRendererClassic_DrawSprite2(const Sprite2* sprite2);
unsigned int SDLGameRendererClassic_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle);
void SDLGameRendererClassic_DumpTextures(void);
void SDLGameRendererClassic_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count);
SDL_Texture* SDLGameRendererClassic_GetCanvas(void);
void SDLGameRendererClassic_DrawOverlayQuad(void* texture, float x, float y, float w, float h, float z);
void SDLGameRendererClassic_DrawOverlayQuadEx(void* texture, float x, float y, float w, float h, float z, int flip_x,
                                              int flip_y);
void SDLGameRendererClassic_DrawOverlaySubQuadEx(void* texture, float x, float y, float w, float h, float u0,
                                                 float v0, float u1, float v1, float z);

#ifdef __cplusplus
}
#endif

#endif
