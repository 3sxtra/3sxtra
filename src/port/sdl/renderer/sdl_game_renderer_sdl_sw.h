/**
 * @file sdl_game_renderer_sdl_sw.h
 * @brief Extracted CPU-side software rasterizer for the SDL2D renderer.
 */
#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the software rasterizer system.
 */
void SWRaster_Init(void);

/**
 * @brief Shuts down the software rasterizer system.
 */
void SWRaster_Shutdown(void);

/** @brief Linearly interpolate between two SDL_FColor values. */
void lerp_fcolors(SDL_FColor* dest, const SDL_FColor* a, const SDL_FColor* b, float x);

/** @brief Context for the software rasterizer — holds geometry, texture callbacks, and canvas info. */
typedef struct {
    int count;
    const int* order;
    const bool* is_rect;
    const unsigned int* th;
    const SDL_Vertex (*verts)[4];
    const SDL_FRect* src_rect;
    const SDL_FRect* dst_rect;
    const SDL_FlipMode* flip;
    const uint32_t* color32;

    int canvas_w;
    int canvas_h;
    SDL_Texture* canvas;

    const uint32_t* (*lookup_cached_pixels)(int ti, int palette_handle, int* out_w, int* out_h);
    const uint32_t* (*ensure_nonidx_pixels)(int ti, int* out_w, int* out_h);

    uint32_t frame_clear_color;
} SWRaster_Context;

/**
 * @brief Renders a frame using the software rasterizer.
 *
 * @param ctx The context structure containing geometry and texture callbacks.
 * @return true on success, false on failure.
 */
bool SWRaster_RenderFrame(const SWRaster_Context* ctx);
