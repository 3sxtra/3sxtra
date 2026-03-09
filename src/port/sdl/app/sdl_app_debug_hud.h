/**
 * @file sdl_app_debug_hud.h
 * @brief Debug HUD overlay: FPS measurement, history tracking, and on-screen display.
 *
 * Provides frame-time measurement, rolling FPS computation, an unbounded FPS
 * history buffer (for netplay graphs), and text-based debug overlay rendering
 * on GL, GPU, and SDL2D backends.
 */
#ifndef SDL_APP_DEBUG_HUD_H
#define SDL_APP_DEBUG_HUD_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Record the current timestamp for FPS calculation (call once per frame). */
void SDLAppDebugHud_NoteFrameEnd(void);

/** @brief Recompute rolling FPS from the frame-time ring buffer and append to history. */
void SDLAppDebugHud_UpdateFPS(void);

/** @brief Render the debug HUD text overlay (GL/GPU path).
 *  @param win_w    Window width in pixels.
 *  @param win_h    Window height in pixels.
 *  @param viewport Letterboxed viewport rect for positioning. */
void SDLAppDebugHud_Render(int win_w, int win_h, const SDL_FRect* viewport);

/** @brief Render a simplified debug HUD for the SDL2D backend.
 *  @param win_w    Window width in pixels.
 *  @param win_h    Window height in pixels.
 *  @param dst_rect Destination blit rect (game canvas area). */
void SDLAppDebugHud_RenderSDL2D(int win_w, int win_h, const SDL_FRect* dst_rect);

/** @brief Get the current measured FPS value. */
double SDLAppDebugHud_GetFPS(void);

/** @brief Get the FPS history buffer.
 *  @param[out] out_count  Number of entries in the history.
 *  @return Pointer to the history array (owned by this module, do not free). */
const float* SDLAppDebugHud_GetFPSHistory(int* out_count);

/** @brief Debug HUD visibility flag (extern for RmlUi mods menu binding). */
extern bool show_debug_hud;

/** @brief Return whether the debug HUD is currently visible. */
bool SDLAppDebugHud_IsVisible(void);

/** @brief Toggle debug HUD visibility and persist to config. */
void SDLAppDebugHud_Toggle(void);

#ifdef __cplusplus
}
#endif

#endif // SDL_APP_DEBUG_HUD_H
