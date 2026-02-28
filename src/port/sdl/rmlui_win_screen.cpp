/**
 * @file rmlui_win_screen.cpp
 * @brief RmlUi Winner/Loser Screen data model.
 *
 * Replaces CPS3's spawn_effect_76 text objects in Win_2nd()/Lose_2nd()
 * with an RmlUi overlay showing winner name, score, win streak, and
 * a subdued loser variant.
 *
 * Key globals (from workuser.h):
 *   Winner_id, WGJ_Score, WGJ_Win, Win_Record[], VS_Win_Record[],
 *   Continue_Coin[], Score[][3], My_char[], Mode_Type, Play_Type
 */

#include "port/sdl/rmlui_win_screen.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/engine/workuser.h"
} // extern "C"

// ─── Character name table (shared with game_hud) ────────────────
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

struct WinCache {
    Rml::String winner_name;
    int winner_score;
    int winner_wins;
    bool is_loser;
    bool is_versus;
    Rml::String streak_text;
};
static WinCache s_cache = {};

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
extern "C" void rmlui_win_screen_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("win_screen");
    if (!ctor)
        return;

    ctor.BindFunc("winner_name", [](Rml::Variant& v) { v = Rml::String(char_name(My_char[Winner_id])); });
    ctor.BindFunc("winner_score", [](Rml::Variant& v) { v = (int)WGJ_Score; });
    ctor.BindFunc("winner_wins", [](Rml::Variant& v) { v = (int)WGJ_Win; });
    ctor.BindFunc("is_loser", [](Rml::Variant& v) {
        v = false; // Updated per-frame via dirty check
    });
    ctor.BindFunc("is_versus", [](Rml::Variant& v) { v = (bool)(Mode_Type == MODE_VERSUS); });
    ctor.BindFunc("streak_text", [](Rml::Variant& v) {
        if (WGJ_Win > 1)
            v = Rml::String("2nd WIN+");
        else if (WGJ_Win == 1)
            v = Rml::String("1st WIN");
        else
            v = Rml::String("");
    });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi WinScreen] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_win_screen_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    DIRTY_STR(winner_name, Rml::String(char_name(My_char[Winner_id])));
    DIRTY_INT(winner_score, (int)WGJ_Score);
    DIRTY_INT(winner_wins, (int)WGJ_Win);
    DIRTY_BOOL(is_versus, Mode_Type == MODE_VERSUS);

    Rml::String streak;
    if (WGJ_Win > 1)
        streak = "2nd WIN+";
    else if (WGJ_Win == 1)
        streak = "1st WIN";
    else
        streak = "";
    DIRTY_STR(streak_text, streak);
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_win_screen_show(void) {
    /* Reset the loser flag — Win_2nd uses show(), Lose_2nd doesn't set it */
    s_cache.is_loser = false;
    if (s_model_handle)
        s_model_handle.DirtyVariable("is_loser");
    rmlui_wrapper_show_document("win");
}

extern "C" void rmlui_win_screen_hide(void) {
    rmlui_wrapper_hide_document("win");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_win_screen_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("win");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx)
            ctx->RemoveDataModel("win_screen");
        s_model_registered = false;
    }
}

#undef DIRTY_INT
#undef DIRTY_BOOL
#undef DIRTY_STR
