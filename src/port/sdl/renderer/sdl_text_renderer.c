/**
 * @file sdl_text_renderer.c
 * @brief Text renderer dispatch layer.
 *
 * Routes text rendering calls to the active backend (GL, SDL_GPU, or SDL2D)
 * based on the current renderer selection. Also handles PS2 debug text
 * buffer rendering for the DEBUG build.
 */
#include "port/sdl/renderer/sdl_text_renderer.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/renderer/sdl_text_renderer_internal.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "types.h"

void SDLTextRenderer_Init(const char* base_path, const char* font_path) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLTextRendererGPU_Init(base_path, font_path);
    } else if (is_sdl2d_backend(r)) {
        SDLTextRendererSDL_Init(base_path, font_path);
    } else {
        SDLTextRendererGL_Init(base_path, font_path);
    }
}

void SDLTextRenderer_Shutdown(void) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLTextRendererGPU_Shutdown();
    } else if (is_sdl2d_backend(r)) {
        SDLTextRendererSDL_Shutdown();
    } else {
        SDLTextRendererGL_Shutdown();
    }
}

void SDLTextRenderer_DrawText(const char* text, float x, float y, float scale, float r, float g, float b,
                              float target_width, float target_height) {
    RendererBackend rend = SDLApp_GetRenderer();
    if (rend == RENDERER_SDLGPU) {
        SDLTextRendererGPU_DrawText(text, x, y, scale, r, g, b, target_width, target_height);
    } else if (is_sdl2d_backend(rend)) {
        SDLTextRendererSDL_DrawText(text, x, y, scale, r, g, b, target_width, target_height);
    } else {
        SDLTextRendererGL_DrawText(text, x, y, scale, r, g, b, target_width, target_height);
    }
}

void SDLTextRenderer_Flush(void) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLTextRendererGPU_Flush();
    } else if (is_sdl2d_backend(r)) {
        SDLTextRendererSDL_Flush();
    } else {
        SDLTextRendererGL_Flush();
    }
}

void SDLTextRenderer_SetYOffset(float y_offset) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLTextRendererGPU_SetYOffset(y_offset);
    } else if (is_sdl2d_backend(r)) {
        SDLTextRendererSDL_SetYOffset(y_offset);
    } else {
        SDLTextRendererGL_SetYOffset(y_offset);
    }
}

void SDLTextRenderer_SetBackgroundEnabled(int enabled) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLTextRendererGPU_SetBackgroundEnabled(enabled);
    } else if (is_sdl2d_backend(r)) {
        SDLTextRendererSDL_SetBackgroundEnabled(enabled);
    } else {
        SDLTextRendererGL_SetBackgroundEnabled(enabled);
    }
}

void SDLTextRenderer_SetBackgroundColor(float r, float g, float b, float a) {
    RendererBackend rend = SDLApp_GetRenderer();
    if (rend == RENDERER_SDLGPU) {
        SDLTextRendererGPU_SetBackgroundColor(r, g, b, a);
    } else if (is_sdl2d_backend(rend)) {
        SDLTextRendererSDL_SetBackgroundColor(r, g, b, a);
    } else {
        SDLTextRendererGL_SetBackgroundColor(r, g, b, a);
    }
}

void SDLTextRenderer_SetBackgroundPadding(float px) {
    RendererBackend r = SDLApp_GetRenderer();
    if (r == RENDERER_SDLGPU) {
        SDLTextRendererGPU_SetBackgroundPadding(px);
    } else if (is_sdl2d_backend(r)) {
        SDLTextRendererSDL_SetBackgroundPadding(px);
    } else {
        SDLTextRendererGL_SetBackgroundPadding(px);
    }
}

void SDLTextRenderer_DrawDebugBuffer(float target_width, float target_height) {
#if DEBUG

    if (flDebugStrCtr == 0) {
        return;
    }

    void* buff_ptr = flPS2GetSystemBuffAdrs(flDebugStrHan);
    if (buff_ptr == NULL) {
        return;
    }

    RendererBackend r = SDLApp_GetRenderer();

    // ⚡ Pi4: GL backend uses batched draw (2 glDrawArrays calls total).
    // Other backends still use the per-character approach.
    if (!is_sdl2d_backend(r) && r != RENDERER_SDLGPU) {
        float scale = target_height / 480.0f;
        SDLTextRendererGL_DrawDebugChars(buff_ptr, (int)flDebugStrCtr, scale, target_width, target_height);
        flDebugStrCtr = 0;
        return;
    }

    // Fallback for GPU/SDL2D: per-character rendering
    typedef struct {
        u16 x;
        u16 y;
        u32 code;
        u32 col;
    } RenderBuffer;

    RenderBuffer* rb = (RenderBuffer*)buff_ptr;
    SDLTextRenderer_SetBackgroundEnabled(0);
    float scale = target_height / 480.0f;

    for (u32 i = 0; i < flDebugStrCtr; i++) {
        RenderBuffer* ch = &rb[i];
        if (ch->code < 0x20 || ch->code > 0x7F)
            continue;

        u8 cr = (ch->col >> 16) & 0xFF;
        u8 cg = (ch->col >> 8) & 0xFF;
        u8 cb = ch->col & 0xFF;
        cr = (cr < 128) ? cr * 2 : 255;
        cg = (cg < 128) ? cg * 2 : 255;
        cb = (cb < 128) ? cb * 2 : 255;

        float px = (float)ch->x * scale;
        float py = (float)ch->y * scale;
        char text[2] = { (char)ch->code, '\0' };

        SDLTextRenderer_DrawText(text, px + 1, py + 1, scale, 0.0f, 0.0f, 0.0f, target_width, target_height);
        SDLTextRenderer_DrawText(
            text, px, py, scale, cr / 255.0f, cg / 255.0f, cb / 255.0f, target_width, target_height);
    }

    SDLTextRenderer_SetBackgroundEnabled(1);
    flDebugStrCtr = 0;
#else
    (void)target_width;
    (void)target_height;
#endif
}
