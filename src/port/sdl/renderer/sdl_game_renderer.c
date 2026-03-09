/**
 * @file sdl_game_renderer.c
 * @brief Game renderer backend dispatch (GPU / GL / SDL2D).
 */
#include "port/sdl/renderer/sdl_game_renderer.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/renderer/sdl_game_renderer_internal.h"

void SDLGameRenderer_Init() {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_Init();
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_Init();
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_Init();
    } else {
        SDLGameRendererGL_Init();
    }
}

void SDLGameRenderer_Shutdown() {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_Shutdown();
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_Shutdown();
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_Shutdown();
    } else {
        SDLGameRendererGL_Shutdown();
    }
}

void SDLGameRenderer_BeginFrame() {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_BeginFrame();
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_BeginFrame();
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_BeginFrame();
    } else {
        SDLGameRendererGL_BeginFrame();
    }
}

void SDLGameRenderer_RenderFrame() {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_RenderFrame();
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_RenderFrame();
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_RenderFrame();
    } else {
        SDLGameRendererGL_RenderFrame();
    }
}

void SDLGameRenderer_EndFrame() {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_EndFrame();
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_EndFrame();
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_EndFrame();
    } else {
        SDLGameRendererGL_EndFrame();
    }
}

void SDLGameRenderer_ResetBatchState() {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_OPENGL) {
        SDLGameRendererGL_ResetBatchState();
    }
    // SDL2D, Classic, and GPU backends: texture_count is local, same overflow is
    // unlikely with their simpler stacks.  Add stubs if needed.
}

void SDLGameRenderer_CreateTexture(unsigned int th) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_CreateTexture(th);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_CreateTexture(th);
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_CreateTexture(th);
    } else {
        SDLGameRendererGL_CreateTexture(th);
    }
}

void SDLGameRenderer_DestroyTexture(unsigned int texture_handle) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_DestroyTexture(texture_handle);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_DestroyTexture(texture_handle);
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_DestroyTexture(texture_handle);
    } else {
        SDLGameRendererGL_DestroyTexture(texture_handle);
    }
}

void SDLGameRenderer_UnlockTexture(unsigned int th) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_UnlockTexture(th);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_UnlockTexture(th);
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_UnlockTexture(th);
    } else {
        SDLGameRendererGL_UnlockTexture(th);
    }
}

void SDLGameRenderer_CreatePalette(unsigned int ph) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_CreatePalette(ph);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_CreatePalette(ph);
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_CreatePalette(ph);
    } else {
        SDLGameRendererGL_CreatePalette(ph);
    }
}

void SDLGameRenderer_DestroyPalette(unsigned int palette_handle) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_DestroyPalette(palette_handle);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_DestroyPalette(palette_handle);
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_DestroyPalette(palette_handle);
    } else {
        SDLGameRendererGL_DestroyPalette(palette_handle);
    }
}

void SDLGameRenderer_UnlockPalette(unsigned int ph) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_UnlockPalette(ph);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_UnlockPalette(ph);
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_UnlockPalette(ph);
    } else {
        SDLGameRendererGL_UnlockPalette(ph);
    }
}

void SDLGameRenderer_SetTexture(unsigned int th) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_SetTexture(th);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_SetTexture(th);
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_SetTexture(th);
    } else {
        SDLGameRendererGL_SetTexture(th);
    }
}

void SDLGameRenderer_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_DrawTexturedQuad(sprite, color);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_DrawTexturedQuad(sprite, color);
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_DrawTexturedQuad(sprite, color);
    } else {
        SDLGameRendererGL_DrawTexturedQuad(sprite, color);
    }
}

void SDLGameRenderer_DrawSolidQuad(const Quad* vertices, unsigned int color) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_DrawSolidQuad(vertices, color);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_DrawSolidQuad(vertices, color);
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_DrawSolidQuad(vertices, color);
    } else {
        SDLGameRendererGL_DrawSolidQuad(vertices, color);
    }
}

void SDLGameRenderer_DrawSprite(const Sprite* sprite, unsigned int color) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_DrawSprite(sprite, color);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_DrawSprite(sprite, color);
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_DrawSprite(sprite, color);
    } else {
        SDLGameRendererGL_DrawSprite(sprite, color);
    }
}

void SDLGameRenderer_DrawSprite2(const Sprite2* sprite2) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_DrawSprite2(sprite2);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_DrawSprite2(sprite2);
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_DrawSprite2(sprite2);
    } else {
        SDLGameRendererGL_DrawSprite2(sprite2);
    }
}

unsigned int SDLGameRenderer_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        return SDLGameRendererGPU_GetCachedGLTexture(texture_handle, palette_handle);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        return SDLGameRendererClassic_GetCachedGLTexture(texture_handle, palette_handle);
    } else if (r == RENDERER_SDL2D) {
        return SDLGameRendererSDL_GetCachedGLTexture(texture_handle, palette_handle);
    } else {
        return SDLGameRendererGL_GetCachedGLTexture(texture_handle, palette_handle);
    }
}

void SDLGameRenderer_DumpTextures(void) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_DumpTextures();
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_DumpTextures();
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_DumpTextures();
    } else {
        SDLGameRendererGL_DumpTextures();
        SDLGameRendererGL_DumpPaletteStats();
    }
}

void SDLGameRenderer_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_FlushSprite2Batch(chips, active_layers, count);
    } else if (r == RENDERER_SDL2D_CLASSIC) {
        SDLGameRendererClassic_FlushSprite2Batch(chips, active_layers, count);
    } else if (r == RENDERER_OPENGL) {
        SDLGameRendererGL_FlushSprite2Batch(chips, active_layers, count);
    } else {
        SDLGameRendererSDL_FlushSprite2Batch(chips, active_layers, count);
    }
}

// ⚡ Opt6: LZ77 GPU compute dispatch — only available on GPU backend
int Renderer_LZ77Available(void) {
    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        return SDLGameRendererGPU_LZ77Available();
    }
    return 0;
}

int Renderer_LZ77Enqueue(const u8* compressed, u32 comp_size, u32 decomp_size, int texture_handle, int palette_handle,
                         u32 code, u32 tile_dim) {
    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        return SDLGameRendererGPU_LZ77Enqueue(
            compressed, comp_size, decomp_size, texture_handle, palette_handle, code, tile_dim);
    }
    return 0;
}
