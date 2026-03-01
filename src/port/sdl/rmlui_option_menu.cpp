/**
 * @file rmlui_option_menu.cpp
 * @brief RmlUi Option Menu screen data model.
 *
 * Replaces the CPS3 effect_61/effect_04 option-select rendering with an
 * HTML/CSS panel. The underlying Option_Select() state machine continues
 * to drive navigation — we just bridge the event callback back to it.
 */

#include "port/sdl/rmlui_option_menu.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {

/* Game globals — Menu_Cursor_Y, IO_Result, save_w, Present_Mode */
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "structs.h"

} // extern "C"

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct OptionMenuCache {
    int cursor;
    bool extra_available;
};
static OptionMenuCache s_cache = {};

#define DIRTY_INT(nm, expr)                                                                                            \
    do {                                                                                                               \
        int _v = (expr);                                                                                               \
        if (_v != s_cache.nm) {                                                                                        \
            s_cache.nm = _v;                                                                                           \
            s_model_handle.DirtyVariable(#nm);                                                                         \
        }                                                                                                              \
    } while (0)

#define DIRTY_BOOL(nm, expr)                                                                                           \
    do {                                                                                                               \
        bool _v = (bool)(expr);                                                                                        \
        if (_v != s_cache.nm) {                                                                                        \
            s_cache.nm = _v;                                                                                           \
            s_model_handle.DirtyVariable(#nm);                                                                         \
        }                                                                                                              \
    } while (0)

static inline bool extra_option_available(void) {
    return save_w[Present_Mode].Extra_Option != 0 || save_w[Present_Mode].Unlock_All != 0;
}

// ─── Init ─────────────────────────────────────────────────────────
extern "C" void rmlui_option_menu_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("option_menu");
    if (!ctor)
        return;

    ctor.BindFunc("option_cursor", [](Rml::Variant& v) { v = (int)Menu_Cursor_Y[0]; });
    ctor.BindFunc("extra_option_available", [](Rml::Variant& v) { v = extra_option_available(); });

    // Event: user clicked a menu item → feed back into the CPS3 state machine
    ctor.BindEventCallback("select_item",
                           [](Rml::DataModelHandle /*model*/, Rml::Event& /*ev*/, const Rml::VariantList& args) {
                               if (!args.empty()) {
                                   int idx = args[0].Get<int>();
                                   Menu_Cursor_Y[0] = (short)idx;
                                   IO_Result = 0x100;
                                   SDL_Log("[RmlUi OptionMenu] Item selected: %d", idx);
                               }
                           });

    // Event: cancel (back button) → IO_Result = 0x200
    ctor.BindEventCallback("cancel",
                           [](Rml::DataModelHandle /*model*/, Rml::Event& /*ev*/, const Rml::VariantList& /*args*/) {
                               IO_Result = 0x200;
                               SDL_Log("[RmlUi OptionMenu] Cancel pressed");
                           });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi OptionMenu] Data model registered");
}

// ─── Per-frame update ─────────────────────────────────────────────
extern "C" void rmlui_option_menu_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;
    DIRTY_INT(cursor, (int)Menu_Cursor_Y[0]);
    DIRTY_BOOL(extra_available, extra_option_available());
}

// ─── Show / Hide ──────────────────────────────────────────────────
extern "C" void rmlui_option_menu_show(void) {
    rmlui_wrapper_show_game_document("option_menu");
    if (s_model_handle)
        s_model_handle.DirtyVariable("option_cursor");
}

extern "C" void rmlui_option_menu_hide(void) {
    rmlui_wrapper_hide_game_document("option_menu");
}

// ─── Shutdown ─────────────────────────────────────────────────────
extern "C" void rmlui_option_menu_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("option_menu");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("option_menu");
        s_model_registered = false;
    }
}

#undef DIRTY_INT
#undef DIRTY_BOOL
