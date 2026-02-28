/**
 * @file rmlui_training_menus.cpp
 * @brief RmlUi Training Sub-Menu data models.
 *
 * Covers 6 screens using a shared data model:
 *   1. Training Mode selector (4 items)
 *   2. Normal Training pause menu (8 effect_A3 items)
 *   3. Dummy Setting (sub-menu within training)
 *   4. Training Option (sub-menu within training)
 *   5. Blocking Training pause menu (6 items)
 *   6. Blocking Training Option (sub-menu within blocking)
 *
 * Key globals: Training[0..2], Menu_Cursor_Y[], Training_Index
 */

#include "port/sdl/rmlui_training_menus.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
#include "structs.h"
} // extern "C"

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct TrainingMenuCache {
    int cursor;
    int training_index;
};
static TrainingMenuCache s_cache = {};

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_training_menus_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("training_menus");
    if (!ctor)
        return;

    ctor.BindFunc("tr_cursor", [](Rml::Variant& v) { v = (int)Menu_Cursor_Y[0]; });
    ctor.BindFunc("tr_index", [](Rml::Variant& v) { v = (int)Training_Index; });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi TrainingMenus] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_training_menus_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    int cur = (int)Menu_Cursor_Y[0];
    if (cur != s_cache.cursor) {
        s_cache.cursor = cur;
        s_model_handle.DirtyVariable("tr_cursor");
    }
    int ti = (int)Training_Index;
    if (ti != s_cache.training_index) {
        s_cache.training_index = ti;
        s_model_handle.DirtyVariable("tr_index");
    }
}

// ─── Show / Hide for each screen ─────────────────────────────────

extern "C" void rmlui_training_mode_show(void) {
    rmlui_wrapper_show_document("training_mode");
    if (s_model_handle)
        s_model_handle.DirtyVariable("tr_cursor");
}
extern "C" void rmlui_training_mode_hide(void) {
    rmlui_wrapper_hide_document("training_mode");
}

extern "C" void rmlui_normal_training_show(void) {
    rmlui_wrapper_show_document("normal_training");
    if (s_model_handle)
        s_model_handle.DirtyVariable("tr_cursor");
}
extern "C" void rmlui_normal_training_hide(void) {
    rmlui_wrapper_hide_document("normal_training");
}

extern "C" void rmlui_dummy_setting_show(void) {
    rmlui_wrapper_show_document("dummy_setting");
    if (s_model_handle)
        s_model_handle.DirtyVariable("tr_cursor");
}
extern "C" void rmlui_dummy_setting_hide(void) {
    rmlui_wrapper_hide_document("dummy_setting");
}

extern "C" void rmlui_training_option_show(void) {
    rmlui_wrapper_show_document("training_option");
    if (s_model_handle)
        s_model_handle.DirtyVariable("tr_cursor");
}
extern "C" void rmlui_training_option_hide(void) {
    rmlui_wrapper_hide_document("training_option");
}

extern "C" void rmlui_blocking_training_show(void) {
    rmlui_wrapper_show_document("blocking_training");
    if (s_model_handle)
        s_model_handle.DirtyVariable("tr_cursor");
}
extern "C" void rmlui_blocking_training_hide(void) {
    rmlui_wrapper_hide_document("blocking_training");
}

extern "C" void rmlui_blocking_tr_option_show(void) {
    rmlui_wrapper_show_document("blocking_tr_option");
    if (s_model_handle)
        s_model_handle.DirtyVariable("tr_cursor");
}
extern "C" void rmlui_blocking_tr_option_hide(void) {
    rmlui_wrapper_hide_document("blocking_tr_option");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_training_menus_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("training_mode");
        rmlui_wrapper_hide_document("normal_training");
        rmlui_wrapper_hide_document("dummy_setting");
        rmlui_wrapper_hide_document("training_option");
        rmlui_wrapper_hide_document("blocking_training");
        rmlui_wrapper_hide_document("blocking_tr_option");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx)
            ctx->RemoveDataModel("training_menus");
        s_model_registered = false;
    }
}
