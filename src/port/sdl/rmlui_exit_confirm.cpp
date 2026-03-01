/**
 * @file rmlui_exit_confirm.cpp
 * @brief RmlUi Exit Confirmation screen data model.
 *
 * Replaces the CPS3 sprite-based "Select Game" button rendering in
 * toSelectGame() with an HTML/CSS confirmation panel.  The underlying
 * toSelectGame() state machine continues to drive all input and
 * transitions — this component only provides the visual overlay.
 */

#include "port/sdl/rmlui_exit_confirm.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

// ─── Init ─────────────────────────────────────────────────────────
extern "C" void rmlui_exit_confirm_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("exit_confirm");
    if (!ctor)
        return;

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi ExitConfirm] Data model registered");
}

// ─── Per-frame update ─────────────────────────────────────────────
extern "C" void rmlui_exit_confirm_update(void) {
    /* No dynamic bindings — static confirmation panel. */
    (void)s_model_handle;
}

// ─── Show / Hide ──────────────────────────────────────────────────
extern "C" void rmlui_exit_confirm_show(void) {
    rmlui_wrapper_show_game_document("exit_confirm");
}

extern "C" void rmlui_exit_confirm_hide(void) {
    rmlui_wrapper_hide_game_document("exit_confirm");
}

// ─── Shutdown ─────────────────────────────────────────────────────
extern "C" void rmlui_exit_confirm_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("exit_confirm");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("exit_confirm");
        s_model_registered = false;
    }
}
