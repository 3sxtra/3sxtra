/**
 * @file sdl_app_bezel.h
 * @brief Bezel overlay rendering — GL, GPU, and SDL2D backends.
 *
 * Extracted from sdl_app.c to isolate bezel vertex building, GL draw calls,
 * GPU pipeline setup/shutdown, and per-frame render dispatch from the main
 * application lifecycle.
 */
#ifndef SDL_APP_BEZEL_H
#define SDL_APP_BEZEL_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Initialise OpenGL bezel resources (VAO, VBO). */
void SDLAppBezel_InitGL(void);

/** @brief Initialise SDL_GPU bezel resources (pipeline, sampler, buffers).
 *  @param base_path Application base path for loading SPIR-V shaders. */
void SDLAppBezel_InitGPU(const char* base_path);

/** @brief Release all bezel GPU/GL resources. */
void SDLAppBezel_Shutdown(void);

/** @brief Render bezels using the OpenGL backend.
 *  @param win_w       Window width in pixels.
 *  @param win_h       Window height in pixels.
 *  @param viewport    Letterboxed viewport rect.
 *  @param passthru    Passthrough shader program handle.
 *  @param identity    4×4 identity matrix (column-major float[16]). */
void SDLAppBezel_RenderGL(int win_w, int win_h, const SDL_FRect* viewport, unsigned int passthru,
                          const float* identity);

/** @brief Render bezels using the SDL_GPU backend.
 *  @param win_w  Window width in pixels.
 *  @param win_h  Window height in pixels. */
void SDLAppBezel_RenderGPU(int win_w, int win_h);

/** @brief Render bezels using the SDL2D backend.
 *  @param renderer  SDL_Renderer handle.
 *  @param win_w     Window width in pixels.
 *  @param win_h     Window height in pixels.
 *  @param dst_rect  Game viewport destination rect. */
void SDLAppBezel_RenderSDL2D(SDL_Renderer* renderer, int win_w, int win_h, const SDL_FRect* dst_rect);

/** @brief Mark the bezel VBO as dirty so it gets re-uploaded next frame. */
void SDLAppBezel_MarkDirty(void);

#ifdef __cplusplus
}
#endif

#endif /* SDL_APP_BEZEL_H */
