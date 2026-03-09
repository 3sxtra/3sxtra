/**
 * @file sdl_app_screenshot.c
 * @brief Screenshot capture implementation.
 *
 * Captures the current GL framebuffer to a BMP file. Uses a deferred
 * flag so the capture request (from input handling) is processed at
 * end-of-frame when the composited image is available.
 */
#include "port/sdl/app/sdl_app_screenshot.h"
#include "port/sdl/app/sdl_app.h"

// clang-format off
#include <glad/gl.h>
#include <SDL3/SDL.h>
// clang-format on

/** @brief Deferred capture flag — set by RequestCapture, cleared by ProcessPending. */
static bool should_save_screenshot = false;

/** @brief Read-back the GL framebuffer and write it to a BMP file. */
static void save_screenshot(const char* filename) {
    SDL_Window* win = SDLApp_GetWindow();
    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    SDL_Surface* surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGB24);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, surface->pixels);
    SDL_SaveBMP(surface, filename);
    SDL_DestroySurface(surface);
}

void SDLAppScreenshot_RequestCapture(void) {
    should_save_screenshot = true;
}

void SDLAppScreenshot_ProcessPending(void) {
    if (should_save_screenshot) {
        save_screenshot("screenshot.bmp");
        should_save_screenshot = false;
    }
}
