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

#ifdef ENABLE_DEBUG_HUD

void SDLAppDebugHud_NoteFrameEnd(void);
void SDLAppDebugHud_UpdateFPS(void);
void SDLAppDebugHud_Render(int win_w, int win_h, const SDL_FRect* viewport);
void SDLAppDebugHud_RenderSDL2D(int win_w, int win_h, const SDL_FRect* dst_rect);
double SDLAppDebugHud_GetFPS(void);
const float* SDLAppDebugHud_GetFPSHistory(int* out_count);
extern bool show_debug_hud;
bool SDLAppDebugHud_IsVisible(void);
void SDLAppDebugHud_Toggle(void);

#else /* !ENABLE_DEBUG_HUD — inline no-ops */

static inline void SDLAppDebugHud_NoteFrameEnd(void) {}
static inline void SDLAppDebugHud_UpdateFPS(void) {}
static inline void SDLAppDebugHud_Render(int w, int h, const SDL_FRect* v) {
    (void)w; (void)h; (void)v;
}
static inline void SDLAppDebugHud_RenderSDL2D(int w, int h, const SDL_FRect* d) {
    (void)w; (void)h; (void)d;
}
static inline double SDLAppDebugHud_GetFPS(void) { return 0.0; }
static inline const float* SDLAppDebugHud_GetFPSHistory(int* out_count) {
    if (out_count) *out_count = 0;
    return (const float*)0;
}
static const bool show_debug_hud = false;
static inline bool SDLAppDebugHud_IsVisible(void) { return false; }
static inline void SDLAppDebugHud_Toggle(void) {}

#endif /* ENABLE_DEBUG_HUD */

#ifdef __cplusplus
}
#endif

#endif // SDL_APP_DEBUG_HUD_H
