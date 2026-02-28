/**
 * @file rmlui_memory_card.cpp
 * @brief RmlUi Memory Card (Save/Load) Screen data model.
 *
 * Replaces CPS3's effect_57/61/64/66/04 objects in Memory_Card() case 0
 * with an RmlUi overlay showing save/load file slots.
 *
 * Key globals (from workuser.h):
 *   Menu_Cursor_Y[], Menu_Cursor_X[], IO_Result, vm_w
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
    int  cursor_y;
    int  cursor_x;
    int  io_result;
};
static MemCardCache s_cache = {};

#define DIRTY_INT(nm, expr) do { \
    int _v = (expr); \
    if (_v != s_cache.nm) { s_cache.nm = _v; s_model_handle.DirtyVariable(#nm); } \
} while(0)

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_memory_card_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("memory_card");
    if (!ctor) return;

    ctor.BindFunc("cursor_y", [](Rml::Variant& v){
        v = (int)Menu_Cursor_Y[0];
    });
    ctor.BindFunc("cursor_x", [](Rml::Variant& v){
        v = (int)Menu_Cursor_X[0];
    });
    ctor.BindFunc("io_result", [](Rml::Variant& v){
        v = (int)IO_Result;
    });

    s_model_handle   = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi MemoryCard] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_memory_card_update(void) {
    if (!s_model_registered || !s_model_handle) return;

    DIRTY_INT(cursor_y, (int)Menu_Cursor_Y[0]);
    DIRTY_INT(cursor_x, (int)Menu_Cursor_X[0]);
    DIRTY_INT(io_result, (int)IO_Result);
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_memory_card_show(void) {
    rmlui_wrapper_show_document("memory_card");
}

extern "C" void rmlui_memory_card_hide(void) {
    rmlui_wrapper_hide_document("memory_card");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_memory_card_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("memory_card");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx) ctx->RemoveDataModel("memory_card");
        s_model_registered = false;
    }
}

#undef DIRTY_INT
