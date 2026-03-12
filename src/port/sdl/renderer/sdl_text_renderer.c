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

    // Get the buffer pointer
    typedef struct {
        u16 x;
        u16 y;
        u32 code;
        u32 col;
    } RenderBuffer;

    RenderBuffer* buff_ptr = (RenderBuffer*)flPS2GetSystemBuffAdrs(flDebugStrHan);
    if (buff_ptr == NULL) {
        return;
    }

    // Disable background for debug text (clean overlay)
    SDLTextRenderer_SetBackgroundEnabled(0);

    // Scale factor: Upstream uses target_height / 480.0f uniformly for beautiful pixel-perfect square aspect.
    float scale = target_height / 480.0f;
    float char_scale = scale;

    // Render each character
    for (u32 i = 0; i < flDebugStrCtr; i++) {
        RenderBuffer* ch = &buff_ptr[i];

        // Skip non-printable characters
        if (ch->code < 0x20 || ch->code > 0x7F) {
            continue;
        }

        // Extract ARGB color (PS2 format: A<<24 | R<<16 | G<<8 | B)
        u8 a = (ch->col >> 24) & 0xFF;
        u8 r = (ch->col >> 16) & 0xFF;
        u8 g = (ch->col >> 8) & 0xFF;
        u8 b = ch->col & 0xFF;

        // The PS2 flPrintColor function halved all color values before storing.
        // Double them back to restore original brightness.
        r = (r < 128) ? r * 2 : 255;
        g = (g < 128) ? g * 2 : 255;
        b = (b < 128) ? b * 2 : 255;
        a = (a < 128) ? a * 2 : 255;

        // Note: the original code didn't use alpha channel of the text color?
        // We'll pass RGB converted to floats.
        float rf = r / 255.0f;
        float gf = g / 255.0f;
        float bf = b / 255.0f;

        // Create single character string
        char text[2] = { (char)ch->code, '\0' };

        // Calculate absolute position on the screen window.
        // Renderers now treat x and y as exact pixel anchors, so we scale them here.
        float px = (float)ch->x * scale;
        float py = (float)ch->y * scale;

        // Single drop shadow for contrast (2 draws vs 9 for the old 8-direction outline)
        SDLTextRenderer_DrawText(text, px + 1, py + 1, char_scale, 0.0f, 0.0f, 0.0f, target_width, target_height);
        SDLTextRenderer_DrawText(text, px, py, char_scale, rf, gf, bf, target_width, target_height);
    }

    // Restore background setting (Default is enabled for UI text usually)
    SDLTextRenderer_SetBackgroundEnabled(1);

    // Clear debug text buffer for next frame
    flDebugStrCtr = 0;
#else
    (void)target_width;
    (void)target_height;
#endif
}
