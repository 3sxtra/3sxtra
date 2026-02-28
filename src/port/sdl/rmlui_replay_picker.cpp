/**
 * @file rmlui_replay_picker.cpp
 * @brief RmlUi Replay Picker data model.
 *
 * Replaces the CPS3 effect objects in Save_Replay() / Load_Replay()
 * with an HTML/CSS overlay showing replay file list and confirmation.
 */

#include "port/sdl/rmlui_replay_picker.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "structs.h"
#include "sf33rd/Source/Game/engine/workuser.h"
} // extern "C"

static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct ReplayPickerCache {
    int cursor;
};
static ReplayPickerCache s_cache = {};

extern "C" void rmlui_replay_picker_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("replay_picker");
    if (!ctor) return;

    ctor.BindFunc("rp_cursor", [](Rml::Variant& v){ v = (int)Menu_Cursor_Y[0]; });

    s_model_handle   = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_Log("[RmlUi ReplayPicker] Data model registered");
}

extern "C" void rmlui_replay_picker_update(void) {
    if (!s_model_registered || !s_model_handle) return;
    int cur = (int)Menu_Cursor_Y[0];
    if (cur != s_cache.cursor) {
        s_cache.cursor = cur;
        s_model_handle.DirtyVariable("rp_cursor");
    }
}

extern "C" void rmlui_replay_picker_show(void) {
    rmlui_wrapper_show_document("replay_picker");
    if (s_model_handle) s_model_handle.DirtyVariable("rp_cursor");
}

extern "C" void rmlui_replay_picker_hide(void) {
    rmlui_wrapper_hide_document("replay_picker");
}

extern "C" void rmlui_replay_picker_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("replay_picker");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx) ctx->RemoveDataModel("replay_picker");
        s_model_registered = false;
    }
}
