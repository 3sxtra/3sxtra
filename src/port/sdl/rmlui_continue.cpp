/**
 * @file rmlui_continue.cpp
 * @brief RmlUi Continue Screen data model.
 *
 * Replaces CPS3's spawn_effect_76(0x3B–0x3F) text objects in
 * Setup_Continue_OBJ() with an RmlUi overlay showing the countdown
 * timer and "CONTINUE?" prompt.
 *
 * Key globals (from workuser.h):
 *   Continue_Count_Down[2], Continue_Count[2], Cont_No[4],
 *   LOSER, My_char[]
 */

#include "port/sdl/rmlui_continue.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
} // extern "C"

// ─── Character name table ────────────────────────────────────────
static const char* const s_char_names[20] = { "RYU",   "ALEX",   "YUEN",    "DUDLEY", "NECRO", "HUGO",   "IBuki",
                                              "ELENA", "ORO",    "YANG",    "KEN",    "SEAN",  "MAKOTO", "REMY",
                                              "Q",     "TWELVE", "CHUN-LI", "URIEN",  "GILL",  "AKUMA" };
#define CHAR_NAME_COUNT 20

static const char* char_name(int idx) {
    if (idx >= 0 && idx < CHAR_NAME_COUNT)
        return s_char_names[idx];
    return "???";
}

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct ContinueCache {
    int continue_count;
    bool continue_active;
    Rml::String loser_name;
};
static ContinueCache s_cache = {};

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

#define DIRTY_STR(nm, expr)                                                                                            \
    do {                                                                                                               \
        Rml::String _v = (expr);                                                                                       \
        if (_v != s_cache.nm) {                                                                                        \
            s_cache.nm = _v;                                                                                           \
            s_model_handle.DirtyVariable(#nm);                                                                         \
        }                                                                                                              \
    } while (0)

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_continue_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("continue_screen");
    if (!ctor)
        return;

    ctor.BindFunc("continue_count", [](Rml::Variant& v) { v = (int)Continue_Count_Down[LOSER]; });
    ctor.BindFunc("continue_active", [](Rml::Variant& v) { v = (bool)(Cont_No[0] < 2); });
    ctor.BindFunc("loser_name", [](Rml::Variant& v) { v = Rml::String(char_name(My_char[LOSER])); });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi Continue] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_continue_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    DIRTY_INT(continue_count, (int)Continue_Count_Down[LOSER]);
    DIRTY_BOOL(continue_active, Cont_No[0] < 2);
    DIRTY_STR(loser_name, Rml::String(char_name(My_char[LOSER])));
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_continue_show(void) {
    rmlui_wrapper_show_document("continue");
}

extern "C" void rmlui_continue_hide(void) {
    rmlui_wrapper_hide_document("continue");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_continue_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("continue");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx)
            ctx->RemoveDataModel("continue_screen");
        s_model_registered = false;
    }
}

#undef DIRTY_INT
#undef DIRTY_BOOL
#undef DIRTY_STR
