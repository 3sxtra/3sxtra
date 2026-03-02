/**
 * @file rmlui_mode_menu.cpp
 * @brief RmlUi Mode Select screen data model.
 *
 * Replaces the CPS3 effect_61/effect_04 mode-select rendering with an
 * HTML/CSS panel. The underlying Mode_Select() state machine continues
 * to drive navigation — we just bridge the event callback back to it.
 */

#include "port/sdl/rmlui_mode_menu.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {

/* Navigation globals from menu.c */
extern short Menu_Cursor_Y[4];
extern unsigned short IO_Result;
extern short Connect_Status;

} // extern "C"

/* Network availability — compile-time check via CMake NETPLAY_ENABLED define */
static inline bool netplay_is_available(void) {
#ifdef NETPLAY_ENABLED
    return true;
#else
    return false;
#endif
}

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct ModeMenuCache {
    int menu_cursor;
    bool network_available;
    bool versus_available;
};
static ModeMenuCache s_cache = {};

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

// ─── Init ─────────────────────────────────────────────────────────
extern "C" void rmlui_mode_menu_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("mode_menu");
    if (!ctor)
        return;

    ctor.BindFunc("menu_cursor", [](Rml::Variant& v) { v = (int)Menu_Cursor_Y[0]; });
    ctor.BindFunc("network_available", [](Rml::Variant& v) { v = (bool)(netplay_is_available() != 0); });
    ctor.BindFunc("versus_available", [](Rml::Variant& v) { v = (bool)(Connect_Status != 0); });

    // Event: user clicked a menu item → feed back into the CPS3 state machine
    ctor.BindEventCallback("select_item",
                           [](Rml::DataModelHandle /*model*/, Rml::Event& /*ev*/, const Rml::VariantList& args) {
                               if (!args.empty()) {
                                   int idx = args[0].Get<int>();
                                   Menu_Cursor_Y[0] = (short)idx;
                                   IO_Result = 0x100;
                                   SDL_Log("[RmlUi ModeMenu] Item selected: %d", idx);
                               }
                           });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi ModeMenu] Data model registered");
}

// ─── Per-frame update ─────────────────────────────────────────────
extern "C" void rmlui_mode_menu_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;
    DIRTY_INT(menu_cursor, (int)Menu_Cursor_Y[0]);
    DIRTY_BOOL(network_available, netplay_is_available() != 0);
    DIRTY_BOOL(versus_available, Connect_Status != 0);
}

// ─── Show / Hide ──────────────────────────────────────────────────
extern "C" void rmlui_mode_menu_show(void) {
    rmlui_wrapper_show_game_document("mode_menu");
    // Reset cursor binding on show
    if (s_model_handle)
        s_model_handle.DirtyVariable("menu_cursor");
}

extern "C" void rmlui_mode_menu_hide(void) {
    rmlui_wrapper_hide_game_document("mode_menu");
}

// ─── Shutdown ─────────────────────────────────────────────────────
extern "C" void rmlui_mode_menu_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("mode_menu");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("mode_menu");
        s_model_registered = false;
    }
}

#undef DIRTY_INT
#undef DIRTY_BOOL
