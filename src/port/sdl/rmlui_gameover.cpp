/**
 * @file rmlui_gameover.cpp
 * @brief RmlUi Game Over / Results Screen data model.
 *
 * Replaces CPS3's spawn_effect_76 + effect_L1 result text objects in
 * GameOver_2nd() with an RmlUi overlay showing "GAME OVER", score,
 * character name, and round wins/losses.
 *
 * Key globals (from workuser.h):
 *   Score[2][3], My_char[], Win_Record[], Player_id, Play_Type
 */

#include "port/sdl/rmlui_gameover.h"
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

struct GameOverCache {
    int gameover_score;
    Rml::String gameover_char;
    int gameover_rounds_won;
    int gameover_rounds_lost;
};
static GameOverCache s_cache = {};

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
extern "C" void rmlui_gameover_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("gameover_screen");
    if (!ctor)
        return;

    ctor.BindFunc("gameover_score", [](Rml::Variant& v) { v = (int)Score[Player_id][Play_Type]; });
    ctor.BindFunc("gameover_char", [](Rml::Variant& v) { v = Rml::String(char_name(My_char[Player_id])); });
    ctor.BindFunc("gameover_rounds_won", [](Rml::Variant& v) { v = (int)Win_Record[Player_id]; });
    ctor.BindFunc("gameover_rounds_lost", [](Rml::Variant& v) {
        /* Opponent's wins = our losses */
        v = (int)Win_Record[Player_id ^ 1];
    });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi GameOver] Data model registered");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_gameover_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    DIRTY_INT(gameover_score, (int)Score[Player_id][Play_Type]);
    DIRTY_STR(gameover_char, Rml::String(char_name(My_char[Player_id])));
    DIRTY_INT(gameover_rounds_won, (int)Win_Record[Player_id]);
    DIRTY_INT(gameover_rounds_lost, (int)Win_Record[Player_id ^ 1]);
}

// ─── Show / Hide ─────────────────────────────────────────────────
extern "C" void rmlui_gameover_show(void) {
    rmlui_wrapper_show_game_document("gameover");
}

extern "C" void rmlui_gameover_hide(void) {
    rmlui_wrapper_hide_game_document("gameover");
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_gameover_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("gameover");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("gameover_screen");
        s_model_registered = false;
    }
}

#undef DIRTY_INT
#undef DIRTY_STR
