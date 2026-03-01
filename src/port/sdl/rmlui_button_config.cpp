/**
 * @file rmlui_button_config.cpp
 * @brief RmlUi Button Config data model.
 *
 * Replaces the CPS3 effect_23 button mapping display and effect_66 highlight
 * boxes in Button_Config(). Reads Key_Disp_Work[PL][12] for current mappings.
 */

#include "port/sdl/rmlui_button_config.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
#include "structs.h"
} // extern "C"

static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct ButtonConfigCache {
    int cursor;
};
static ButtonConfigCache s_cache = {};

extern "C" void rmlui_button_config_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("button_config");
    if (!ctor)
        return;

    ctor.BindFunc("bc_cursor", [](Rml::Variant& v) { v = (int)Menu_Cursor_Y[0]; });

    // Button names for the 9 action rows
    static const char* btn_names[] = { "LP", "MP", "HP", "LK", "MK", "HK", "LP+LK", "MP+MK", "DEFAULT" };
    for (int i = 0; i < 9; i++) {
        char name[32];
        snprintf(name, sizeof(name), "bc_label_%d", i);
        int idx = i;
        ctor.BindFunc(Rml::String(name), [idx](Rml::Variant& v) { v = Rml::String(btn_names[idx]); });
    }

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    SDL_Log("[RmlUi ButtonConfig] Data model registered");
}

extern "C" void rmlui_button_config_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;
    int cur = (int)Menu_Cursor_Y[0];
    if (cur != s_cache.cursor) {
        s_cache.cursor = cur;
        s_model_handle.DirtyVariable("bc_cursor");
    }
}

extern "C" void rmlui_button_config_show(void) {
    rmlui_wrapper_show_game_document("button_config");
    if (s_model_handle)
        s_model_handle.DirtyVariable("bc_cursor");
}

extern "C" void rmlui_button_config_hide(void) {
    rmlui_wrapper_hide_game_document("button_config");
}

extern "C" void rmlui_button_config_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("button_config");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("button_config");
        s_model_registered = false;
    }
}
