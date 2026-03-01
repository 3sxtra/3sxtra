/**
 * @file rmlui_attract_overlay.cpp
 * @brief RmlUi attract demo overlay — small logo + "PRESS START BUTTON".
 *
 * Shown during attract-mode CPU vs CPU demo fights (Loop_Demo cases 3/5).
 * Replaces the native Disp_00_0() text overlay with an RmlUi document
 * containing logo_small.png and a blinking "PRESS START BUTTON" prompt.
 */

#include "port/sdl/rmlui_attract_overlay.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

// ─── Init ─────────────────────────────────────────────────────
extern "C" void rmlui_attract_overlay_init(void) {
    SDL_Log("[RmlUi AttractOverlay] Initialized");
}

// ─── Show / Hide ──────────────────────────────────────────────
extern "C" void rmlui_attract_overlay_show(void) {
    SDL_Log("[RmlUi AttractOverlay] show() called");
    rmlui_wrapper_show_game_document("attract_overlay");
}

extern "C" void rmlui_attract_overlay_hide(void) {
    SDL_Log("[RmlUi AttractOverlay] hide() called");
    rmlui_wrapper_hide_game_document("attract_overlay");
}

// ─── Shutdown ─────────────────────────────────────────────────
extern "C" void rmlui_attract_overlay_shutdown(void) {
    rmlui_wrapper_hide_game_document("attract_overlay");
}
