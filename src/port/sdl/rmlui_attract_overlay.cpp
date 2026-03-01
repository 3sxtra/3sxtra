/**
 * @file rmlui_attract_overlay.cpp
 * @brief RmlUi attract demo overlay — small logo + "PRESS START BUTTON".
 *
 * Shown during attract-mode CPU vs CPU demo fights (Loop_Demo cases 3/5).
 * Replaces the native Disp_00_0() text overlay with an RmlUi document
 * containing logo_small.png and a blinking "PRESS START BUTTON" prompt.
 *
 * The logo starts hidden and appears only when rmlui_attract_overlay_show_logo()
 * is called (triggered from SF3_logo() in sc_sub.c when the native logo
 * animation reaches the fully-revealed state).
 */

#include "port/sdl/rmlui_attract_overlay.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;
static bool s_show_logo = false;

// ─── Init ─────────────────────────────────────────────────────
extern "C" void rmlui_attract_overlay_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("attract_overlay");
    if (!ctor)
        return;

    ctor.BindFunc("show_logo", [](Rml::Variant& v) { v = s_show_logo; });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi AttractOverlay] Data model registered");
}

// ─── Show / Hide ──────────────────────────────────────────────
extern "C" void rmlui_attract_overlay_show(void) {
    SDL_Log("[RmlUi AttractOverlay] show() called");
    s_show_logo = false; /* Logo starts hidden; revealed by show_logo() */
    if (s_model_registered && s_model_handle)
        s_model_handle.DirtyVariable("show_logo");
    rmlui_wrapper_show_game_document("attract_overlay");
}

extern "C" void rmlui_attract_overlay_hide(void) {
    SDL_Log("[RmlUi AttractOverlay] hide() called");
    s_show_logo = false;
    if (s_model_registered && s_model_handle)
        s_model_handle.DirtyVariable("show_logo");
    rmlui_wrapper_hide_game_document("attract_overlay");
}

// ─── Logo visibility ─────────────────────────────────────────
extern "C" void rmlui_attract_overlay_show_logo(void) {
    if (s_show_logo)
        return; /* Already visible */
    SDL_Log("[RmlUi AttractOverlay] show_logo() — revealing HD logo");
    s_show_logo = true;
    if (s_model_registered && s_model_handle)
        s_model_handle.DirtyVariable("show_logo");
}

extern "C" void rmlui_attract_overlay_hide_logo(void) {
    if (!s_show_logo)
        return;
    SDL_Log("[RmlUi AttractOverlay] hide_logo() — hiding HD logo");
    s_show_logo = false;
    if (s_model_registered && s_model_handle)
        s_model_handle.DirtyVariable("show_logo");
}

// ─── Shutdown ─────────────────────────────────────────────────
extern "C" void rmlui_attract_overlay_shutdown(void) {
    rmlui_wrapper_hide_game_document("attract_overlay");
    if (s_model_registered) {
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("attract_overlay");
        s_model_registered = false;
    }
}
