/**
 * @file rmlui_memory_card.cpp
 * @brief RmlUi Memory Card (Save/Load) Screen data model.
 *
 * Replaces CPS3's effect_57/61/64/66/04 objects in Memory_Card() case 0
 * with an RmlUi overlay showing the save/load menu items.
 *
 * Native menu structure (char_index 0x15..0x18):
 *   cursor 0: SAVE DATA
 *   cursor 1: LOAD DATA
 *   cursor 2: AUTO SAVE  (toggle: Convert_Buff[3][0][2], 0=OFF 1=ON)
 *   cursor 3: EXIT
 *
 * Key globals (from workuser.h):
 *   Menu_Cursor_Y[], IO_Result, Convert_Buff[4][2][12]
 */

#include "port/sdl/rmlui_memory_card.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
} // extern "C"

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct MemCardCache {
    int cursor_y;
    int auto_save;
};
static MemCardCache s_cache = {};

#define DIRTY_INT(nm, expr)                                                                                            \
    do {                                                                                                               \
        int _v = (expr);                                                                                               \
        if (_v != s_cache.nm) {                                                                                        \
            s_cache.nm = _v;                                                                                           \
            s_model_handle.DirtyVariable(#nm);                                                                         \
        }                                                                                                              \
    } while (0)

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_memory_card_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("memory_card");
    if (!ctor)
        return;

    ctor.BindFunc("cursor_y", [](Rml::Variant& v) { v = (int)Menu_Cursor_Y[0]; });

    // Auto-save toggle value: Convert_Buff[3][0][2], 0 = OFF, 1 = ON
    ctor.BindFunc("auto_save_label", [](Rml::Variant& v) {
        int val = Convert_Buff[3][0][2];
        v = Rml::String(val ? "\"ON\"" : "\"OFF\"");
    });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi MemoryCard] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_memory_card_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    DIRTY_INT(cursor_y, (int)Menu_Cursor_Y[0]);

    // Always dirty auto_save_label since it reads Convert_Buff which
    // changes from left/right input without a simple scalar diff
    int as_val = (int)Convert_Buff[3][0][2];
    if (as_val != s_cache.auto_save) {
        s_cache.auto_save = as_val;
        s_model_handle.DirtyVariable("auto_save_label");
    }
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_memory_card_show(void) {
    rmlui_wrapper_show_game_document("memory_card");
}

extern "C" void rmlui_memory_card_hide(void) {
    rmlui_wrapper_hide_game_document("memory_card");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_memory_card_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("memory_card");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("memory_card");
        s_model_registered = false;
    }
}

#undef DIRTY_INT
