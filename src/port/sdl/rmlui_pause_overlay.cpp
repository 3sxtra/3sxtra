/**
 * @file rmlui_pause_overlay.cpp
 * @brief RmlUi pause text overlay data model.
 *
 * Drives the pause.rml document showing "1P PAUSE" / "2P PAUSE" text
 * with a CSS blink animation, and a controller-disconnected message.
 *
 * Key globals: Pause_Down, Pause_ID, Interface_Type[], Pause_Type
 */

#include "port/sdl/rmlui_pause_overlay.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/pause.h"
} // extern "C"

// ─── Data model ──────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct PauseCache {
    int pause_player;
    bool pause_visible;
    bool ctrl_disconnected;
    int disconnect_port;
};
static PauseCache s_cache = {};

// ─── Init ────────────────────────────────────────────────────
extern "C" void rmlui_pause_overlay_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("pause_overlay");
    if (!ctor)
        return;

    ctor.BindFunc("pause_player", [](Rml::Variant& v) { v = (int)Pause_ID; });
    ctor.BindFunc("pause_visible", [](Rml::Variant& v) { v = (bool)(Pause_Down != 0); });
    ctor.BindFunc("pause_label", [](Rml::Variant& v) { v = Rml::String(Pause_ID == 0 ? "1P PAUSE" : "2P PAUSE"); });
    ctor.BindFunc("ctrl_disconnected", [](Rml::Variant& v) { v = (bool)(Pause_Down != 0 && Pause_Type == 2); });
    ctor.BindFunc("disconnect_port", [](Rml::Variant& v) { v = (int)(Pause_ID + 1); });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi PauseOverlay] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────
extern "C" void rmlui_pause_overlay_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    bool visible = (Pause_Down != 0);
    if (visible != s_cache.pause_visible) {
        s_cache.pause_visible = visible;
        s_model_handle.DirtyVariable("pause_visible");
        s_model_handle.DirtyVariable("pause_label");
        if (visible)
            rmlui_wrapper_show_game_document("pause");
        else
            rmlui_wrapper_hide_game_document("pause");
    }

    int pid = (int)Pause_ID;
    if (pid != s_cache.pause_player) {
        s_cache.pause_player = pid;
        s_model_handle.DirtyVariable("pause_player");
        s_model_handle.DirtyVariable("pause_label");
        s_model_handle.DirtyVariable("disconnect_port");
    }

    bool disc = (Pause_Down != 0 && Pause_Type == 2);
    if (disc != s_cache.ctrl_disconnected) {
        s_cache.ctrl_disconnected = disc;
        s_model_handle.DirtyVariable("ctrl_disconnected");
    }
}

// ─── Shutdown ────────────────────────────────────────────────
extern "C" void rmlui_pause_overlay_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("pause");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("pause_overlay");
        s_model_registered = false;
    }
}
