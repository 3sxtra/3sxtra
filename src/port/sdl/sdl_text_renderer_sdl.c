/**
 * @file sdl_text_renderer_sdl.c
 * @brief SDL2D text backend — uses SDL_RenderDebugText for simple debug text.
 *
 * SDL_RenderDebugText uses a fixed 8×8 pixel font. It's functional for debug
 * overlays but intentionally minimal. No TTF font loading, no background boxes.
 */
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_text_renderer_internal.h"

#include <SDL3/SDL.h>

// File-static state (stored but mostly unused — SDL_RenderDebugText is immediate mode)
static float s_y_offset = 0.0f;
static int s_background_enabled = 0;
static float s_bg_r = 0.0f, s_bg_g = 0.0f, s_bg_b = 0.0f, s_bg_a = 0.0f;
static float s_bg_padding = 0.0f;

void SDLTextRendererSDL_Init(const char* base_path, const char* font_path) {
    (void)base_path;
    (void)font_path;
    // SDL_RenderDebugText uses a built-in font — no init needed
    s_y_offset = 0.0f;
    s_background_enabled = 0;
}

void SDLTextRendererSDL_Shutdown(void) {
    // Nothing to clean up
}

void SDLTextRendererSDL_DrawText(const char* text, float x, float y, float scale, float r, float g, float b,
                                 float target_width, float target_height) {
    (void)target_width;
    (void)target_height;

    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    SDL_SetRenderDrawColorFloat(renderer, r, g, b, 1.0f);
    SDL_SetRenderScale(renderer, scale, scale);
    SDL_RenderDebugText(renderer, x / scale, (y + s_y_offset) / scale, text);
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
}

void SDLTextRendererSDL_Flush(void) {
    // No-op: SDL_RenderDebugText is immediate mode
}

void SDLTextRendererSDL_SetYOffset(float y_offset) {
    s_y_offset = y_offset;
}

void SDLTextRendererSDL_SetBackgroundEnabled(int enabled) {
    s_background_enabled = enabled;
    (void)s_background_enabled; // Stored but not used by SDL_RenderDebugText
}

void SDLTextRendererSDL_SetBackgroundColor(float r, float g, float b, float a) {
    s_bg_r = r;
    s_bg_g = g;
    s_bg_b = b;
    s_bg_a = a;
    (void)s_bg_r;
    (void)s_bg_g;
    (void)s_bg_b;
    (void)s_bg_a;
}

void SDLTextRendererSDL_SetBackgroundPadding(float px) {
    s_bg_padding = px;
    (void)s_bg_padding;
}
