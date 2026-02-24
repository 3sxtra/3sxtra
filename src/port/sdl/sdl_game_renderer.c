#include "port/sdl/sdl_game_renderer.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_game_renderer_internal.h"

void SDLGameRenderer_Init() {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_Init();
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
    } else if (r == RENDERER_SDL2D) {
        SDLGameRendererSDL_EndFrame();
    } else {
        SDLGameRendererGL_EndFrame();
    }
}

extern void SDLGameRendererGL_ResetBatchState(void);
void SDLGameRenderer_ResetBatchState() {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_OPENGL) {
        SDLGameRendererGL_ResetBatchState();
    }
    // SDL2D and GPU backends: texture_count is local, same overflow is
    // unlikely with their simpler stacks.  Add stubs if needed.
}

void SDLGameRenderer_CreateTexture(unsigned int th) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLGameRendererGPU_CreateTexture(th);
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
    } else if (r == RENDERER_SDL2D) {
        return SDLGameRendererSDL_GetCachedGLTexture(texture_handle, palette_handle);
    } else {
        return SDLGameRendererGL_GetCachedGLTexture(texture_handle, palette_handle);
    }
}
