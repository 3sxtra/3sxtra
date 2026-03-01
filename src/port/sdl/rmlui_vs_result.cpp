/**
 * @file rmlui_vs_result.cpp
 * @brief RmlUi VS Result Screen data model.
 *
 * Replaces CPS3's effect_A0/effect_91/effect_66 objects in VS_Result()
 * case 1 with an RmlUi overlay showing P1/P2 win counts, win percentages,
 * and character names.
 *
 * The show() function receives pre-computed values from VS_Result() so
 * the percentage calculation logic stays in menu.c untouched.
 */

#include "port/sdl/rmlui_vs_result.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/effect/eff76.h"   /* chkNameAkuma */
#include "sf33rd/Source/Game/engine/workuser.h"
} // extern "C"

// ─── Character name table (SF3:3S roster, index matches My_char) ───
static const char* const s_char_names[21] = { "GILL",  "ALEX",    "RYU",    "YUN",  "DUDLEY", "NECRO", "HUGO",
                                              "IBUKI", "ELENA",   "ORO",    "YANG", "KEN",    "SEAN",  "URIEN",
                                              "GOUKI", "CHUN-LI", "MAKOTO", "Q",    "TWELVE", "REMY",  "AKUMA" };
#define CHAR_NAME_COUNT 21

static const char* safe_char_name(int idx) {
    if (idx >= 0 && idx < CHAR_NAME_COUNT)
        return s_char_names[idx];
    return "???";
}
static const char* char_name(int my_char_id) {
    return safe_char_name(my_char_id + chkNameAkuma(my_char_id, 6));
}

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct VSResultCache {
    int p1_wins, p2_wins;
    int p1_pct, p2_pct;
    Rml::String p1_char, p2_char;
};
static VSResultCache s_cache = {};

#define DIRTY_INT(nm, expr)                                                                                            \
    do {                                                                                                               \
        int _v = (expr);                                                                                               \
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
extern "C" void rmlui_vs_result_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("vs_result");
    if (!ctor)
        return;

    ctor.BindFunc("p1_wins", [](Rml::Variant& v) { v = s_cache.p1_wins; });
    ctor.BindFunc("p2_wins", [](Rml::Variant& v) { v = s_cache.p2_wins; });
    ctor.BindFunc("p1_pct", [](Rml::Variant& v) { v = s_cache.p1_pct; });
    ctor.BindFunc("p2_pct", [](Rml::Variant& v) { v = s_cache.p2_pct; });
    ctor.BindFunc("p1_char", [](Rml::Variant& v) { v = Rml::String(char_name(My_char[0])); });
    ctor.BindFunc("p2_char", [](Rml::Variant& v) { v = Rml::String(char_name(My_char[1])); });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi VSResult] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_vs_result_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    DIRTY_INT(p1_wins, (int)VS_Win_Record[0]);
    DIRTY_INT(p2_wins, (int)VS_Win_Record[1]);
    DIRTY_STR(p1_char, Rml::String(char_name(My_char[0])));
    DIRTY_STR(p2_char, Rml::String(char_name(My_char[1])));
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_vs_result_show(int p1_wins, int p2_wins, int p1_pct, int p2_pct) {
    s_cache.p1_wins = p1_wins;
    s_cache.p2_wins = p2_wins;
    s_cache.p1_pct = p1_pct;
    s_cache.p2_pct = p2_pct;
    if (s_model_handle) {
        s_model_handle.DirtyVariable("p1_wins");
        s_model_handle.DirtyVariable("p2_wins");
        s_model_handle.DirtyVariable("p1_pct");
        s_model_handle.DirtyVariable("p2_pct");
    }
    rmlui_wrapper_show_game_document("vs_result");
}

extern "C" void rmlui_vs_result_hide(void) {
    rmlui_wrapper_hide_game_document("vs_result");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_vs_result_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("vs_result");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("vs_result");
        s_model_registered = false;
    }
}

#undef DIRTY_INT
#undef DIRTY_STR
