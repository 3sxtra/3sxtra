/**
 * @file rmlui_title_screen.cpp
 * @brief RmlUi Title Screen data model.
 *
 * Replaces CPS3's SSPutStr calls in Disp_00_0() with an RmlUi overlay
 * showing "PRESS START BUTTON" with a CSS blink animation.
 * The blinking is handled entirely by CSS @keyframes — no need to
 * mirror the E_No[1] timer-based blink cycle from Entry_00().
 */

#include "port/sdl/rmlui_title_screen.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {

/* Game state globals — G_No[4] for 2P prompt visibility */
#include "sf33rd/Source/Game/engine/workuser.h"

} // extern "C"

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct TitleCache {
    bool show_2p;
};
static TitleCache s_cache = {};

// ─── Init ─────────────────────────────────────────────────────────
extern "C" void rmlui_title_screen_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("title_screen");
    if (!ctor) return;

    ctor.BindFunc("show_2p_prompt", [](Rml::Variant& v){
        v = (G_No[1] == 3 || G_No[1] == 5);
    });

    s_model_handle   = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi TitleScreen] Data model registered");
}

// ─── Per-frame update ─────────────────────────────────────────────
extern "C" void rmlui_title_screen_update(void) {
    if (!s_model_registered || !s_model_handle) return;

    bool show = (G_No[1] == 3 || G_No[1] == 5);
    if (show != s_cache.show_2p) {
        s_cache.show_2p = show;
        s_model_handle.DirtyVariable("show_2p_prompt");
    }
}

// ─── Show / Hide ──────────────────────────────────────────────────
extern "C" void rmlui_title_screen_show(void) {
    rmlui_wrapper_show_document("title");
}

extern "C" void rmlui_title_screen_hide(void) {
    rmlui_wrapper_hide_document("title");
}

// ─── Shutdown ─────────────────────────────────────────────────────
extern "C" void rmlui_title_screen_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("title");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx) ctx->RemoveDataModel("title_screen");
        s_model_registered = false;
    }
}
