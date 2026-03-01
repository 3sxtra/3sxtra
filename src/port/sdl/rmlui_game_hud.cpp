/**
 * @file rmlui_game_hud.cpp
 * @brief RmlUi in-game fight HUD data model.
 *
 * Provides a per-frame data binding layer that reads the same game globals
 * used by the CPS3 sprite renderer, exposing them to the game_hud.rml
 * document via RmlUi's BindFunc API.
 *
 * Also defines all Phase 3 per-component toggle globals declared in
 * rmlui_phase3_toggles.h. Toggling any of these to false at runtime
 * falls back to the original CPS3 sprite rendering for that element.
 */

#include "port/sdl/rmlui_game_hud.h"
#include "port/sdl/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

/* ─── Real game headers — all types come from here ─── */
extern "C" {
#include "sf33rd/Source/Game/effect/eff76.h"   /* chkNameAkuma */
#include "sf33rd/Source/Game/engine/cmb_win.h" /* CMST_BUFF, cmst_buff[2][5], cmb_stock[2], cst_read[2] */
#include "sf33rd/Source/Game/engine/plcnt.h"   /* piyori_type[2] (PiyoriType), plw[2] */
#include "sf33rd/Source/Game/engine/spgauge.h" /* SPG_DAT, spg_dat[2] — SA gauge */
#include "sf33rd/Source/Game/engine/workuser.h" /* PLW, Super_Arts, My_char, Win_Record, Max_vitality, Mode_Type … (pulls structs.h) */
#include "sf33rd/Source/Game/training/training_state.h" /* g_training_state — combo stun */

/* VIT and SDAT are defined in structs.h (pulled by workuser.h).
   Declare the globals here since no header exposes them as externs. */
extern VIT vit[2];
extern SDAT sdat[2];

/* Timer globals (declared in count.c, no public header) */
extern s16 round_timer;
extern s8 flash_r_num;
extern s8 mugen_flag;

/* Fight-active flag */
extern u8 Play_Game;

/* Round result arrays (declared in game_globals.c, used by flash_lp.c) */
/* Values: 0=empty, 1=V, 3=P(erfect), 4=C(hip), 5=D(raw), 6=J(udgement), 7=S(A finish) */
extern u8 flash_win_type[2][4];
/* Score, Continue_Coin, Play_Type, win_type — all in workuser.h */

} // extern "C"

// ─── Character name table (SF3:3S roster, index matches My_char) ───
// Index 0 = Gill (boss), then the standard roster order.
static const char* const s_char_names[21] = { "GILL",  "ALEX",    "RYU",    "YUN",  "DUDLEY", "NECRO", "HUGO",
                                              "IBUKI", "ELENA",   "ORO",    "YANG", "KEN",    "SEAN",  "URIEN",
                                              "GOUKI", "CHUN-LI", "MAKOTO", "Q",    "TWELVE", "REMY",  "AKUMA" };
#define CHAR_NAME_COUNT 21

// ─── Toggle globals (defined here, declared extern in rmlui_phase3_toggles.h) ───
bool rmlui_hud_health = true;
bool rmlui_hud_timer = true;
bool rmlui_hud_stun = true;
bool rmlui_hud_super = true;
bool rmlui_hud_combo = true;
bool rmlui_hud_names = true;
bool rmlui_hud_faces = true;
bool rmlui_hud_wins = true;
bool rmlui_hud_score = true;
bool rmlui_hud_training_stun = true;
bool rmlui_hud_training_data = true;

bool rmlui_menu_mode = true;
bool rmlui_menu_option = true;
bool rmlui_menu_game_option = true;
bool rmlui_menu_button_config = true;
bool rmlui_menu_sound = true;
bool rmlui_menu_extra_option = true;
bool rmlui_menu_sysdir = true;
bool rmlui_menu_training = true;
bool rmlui_menu_lobby = true;
bool rmlui_menu_memory_card = true;
bool rmlui_menu_blocking_tr = true;
bool rmlui_menu_blocking_tr_opt = true;
bool rmlui_menu_replay = true;

bool rmlui_screen_title = true;
bool rmlui_screen_winner = true;
bool rmlui_screen_continue = true;
bool rmlui_screen_gameover = true;
bool rmlui_screen_select = true;
bool rmlui_screen_vs_result = true;
bool rmlui_screen_pause = true;
bool rmlui_screen_entry_text = true;
bool rmlui_screen_trials = true;
bool rmlui_screen_copyright = true;
bool rmlui_screen_name_entry = true;
bool rmlui_screen_exit_confirm = true;
bool rmlui_screen_attract_overlay = true;

// ─── Data model state ───────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

// Cached snapshot for dirty detection
struct HudSnapshot {
    int p1_health, p2_health;
    int p1_drain, p2_drain;
    int p1_hp_color, p2_hp_color;
    int round_timer;
    bool timer_flash;
    bool timer_infinite;
    int p1_stun, p2_stun;
    int p1_stun_max, p2_stun_max;
    bool p1_stun_active, p2_stun_active;
    int p1_sa_stocks, p2_sa_stocks;
    int p1_sa_stocks_max, p2_sa_stocks_max;
    int p1_sa_fill, p2_sa_fill;
    int p1_sa_fill_max, p2_sa_fill_max;
    bool p1_sa_active, p2_sa_active;
    bool p1_sa_max, p2_sa_max;
    int p1_sa_pct, p2_sa_pct;
    Rml::String p1_stun_width, p2_stun_width;
    Rml::String p1_sa_width, p2_sa_width;
    int p1_combo_count, p2_combo_count;
    int p1_combo_kind, p2_combo_kind;
    bool p1_combo_active, p2_combo_active;
    int p1_combo_pts, p2_combo_pts;
    bool p1_combo_pts_flag, p2_combo_pts_flag;
    Rml::String p1_name, p2_name;
    int p1_wins, p2_wins;
    bool is_fight_active;
    int p1_combo_stun, p2_combo_stun;
    bool training_stun_active;
    int p1_score, p2_score;
    int p1_parry_count, p2_parry_count;
    Rml::String p1_sa_type, p2_sa_type;
    bool p1_is_human, p2_is_human;
    // Round result tracking
    int rounds_to_win;
    int p1_r0, p1_r1, p1_r2, p1_r3;
    int p2_r0, p2_r1, p2_r2, p2_r3;
    Rml::String p1_r0_lbl, p1_r1_lbl, p1_r2_lbl, p1_r3_lbl;
    Rml::String p2_r0_lbl, p2_r1_lbl, p2_r2_lbl, p2_r3_lbl;
    int p1_round_wins, p2_round_wins;
};
static HudSnapshot s_cache = {};

// ─── Dirty-check macros ──────────────────────────────────────────
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

// ─── Helper: safe char name lookup ──────────────────────────────
static const char* char_name(int idx) {
    if (idx >= 0 && idx < CHAR_NAME_COUNT)
        return s_char_names[idx];
    return "???";
}

// ─── Helper: win-type value → display label ─────────────────────
static const char* win_type_label(int wt) {
    switch (wt) {
    case 1:
        return "V"; // Normal victory
    case 3:
        return "P"; // Perfect
    case 4:
        return "C"; // Chip / special
    case 5:
        return "D"; // Draw
    case 6:
        return "J"; // Judgement
    case 7:
        return "S"; // Super Art finish
    default:
        return ""; // Empty / unplayed
    }
}

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_game_hud_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx) {
        SDL_Log("[RmlUi HUD] No context available");
        return;
    }

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("game_hud");
    if (!ctor) {
        SDL_Log("[RmlUi HUD] Failed to create data model");
        return;
    }

    // ── Health ──
    ctor.BindFunc("p1_health", [](Rml::Variant& v) { v = (int)plw[0].wu.vital_new; });
    ctor.BindFunc("p2_health", [](Rml::Variant& v) { v = (int)plw[1].wu.vital_new; });
    ctor.BindFunc("p1_drain", [](Rml::Variant& v) { v = (int)vit[0].cred; });
    ctor.BindFunc("p2_drain", [](Rml::Variant& v) { v = (int)vit[1].cred; });
    ctor.BindFunc("p1_hp_color", [](Rml::Variant& v) { v = (int)vit[0].colnum; });
    ctor.BindFunc("p2_hp_color", [](Rml::Variant& v) { v = (int)vit[1].colnum; });
    ctor.BindFunc("health_max", [](Rml::Variant& v) { v = (int)Max_vitality; });

    // ── Timer ──
    ctor.BindFunc("round_timer", [](Rml::Variant& v) { v = (int)round_timer; });
    ctor.BindFunc("timer_flash", [](Rml::Variant& v) { v = (bool)(flash_r_num != 0); });
    ctor.BindFunc("timer_infinite", [](Rml::Variant& v) { v = (bool)(mugen_flag != 0); });

    // ── Stun ──
    ctor.BindFunc("p1_stun", [](Rml::Variant& v) { v = (int)sdat[0].cstn; });
    ctor.BindFunc("p2_stun", [](Rml::Variant& v) { v = (int)sdat[1].cstn; });
    ctor.BindFunc("p1_stun_max", [](Rml::Variant& v) { v = (int)piyori_type[0].genkai; });
    ctor.BindFunc("p2_stun_max", [](Rml::Variant& v) { v = (int)piyori_type[1].genkai; });
    ctor.BindFunc("p1_stun_active", [](Rml::Variant& v) { v = (bool)(sdat[0].sflag != 0); });
    ctor.BindFunc("p2_stun_active", [](Rml::Variant& v) { v = (bool)(sdat[1].sflag != 0); });

    // ── Super Art Gauge ──
    // spg_dat[N].max  = number of full stocks stored
    // Super_Arts[N]   = selected SA variant (1/2/3) = max stocks possible
    // spg_dat[N].time = per-stock fill level
    // spg_dat[N].sa_flag nonzero while SA timer is running
    ctor.BindFunc("p1_sa_stocks", [](Rml::Variant& v) { v = (int)spg_dat[0].spg_level; });
    ctor.BindFunc("p2_sa_stocks", [](Rml::Variant& v) { v = (int)spg_dat[1].spg_level; });
    ctor.BindFunc("p1_sa_stocks_max", [](Rml::Variant& v) { v = (int)spg_dat[0].spg_maxlevel; });
    ctor.BindFunc("p2_sa_stocks_max", [](Rml::Variant& v) { v = (int)spg_dat[1].spg_maxlevel; });
    ctor.BindFunc("p1_sa_fill", [](Rml::Variant& v) { v = (int)spg_dat[0].time; });
    ctor.BindFunc("p2_sa_fill", [](Rml::Variant& v) { v = (int)spg_dat[1].time; });
    ctor.BindFunc("p1_sa_fill_max", [](Rml::Variant& v) { v = (int)spg_dat[0].spg_dotlen; });
    ctor.BindFunc("p2_sa_fill_max", [](Rml::Variant& v) { v = (int)spg_dat[1].spg_dotlen; });
    ctor.BindFunc("p1_sa_active", [](Rml::Variant& v) { v = (bool)(spg_dat[0].sa_flag != 0); });
    ctor.BindFunc("p2_sa_active", [](Rml::Variant& v) { v = (bool)(spg_dat[1].sa_flag != 0); });
    ctor.BindFunc("p1_sa_max", [](Rml::Variant& v) {
        v = (bool)(spg_dat[0].spg_maxlevel > 0 && spg_dat[0].spg_level >= spg_dat[0].spg_maxlevel);
    });
    ctor.BindFunc("p2_sa_max", [](Rml::Variant& v) {
        v = (bool)(spg_dat[1].spg_maxlevel > 0 && spg_dat[1].spg_level >= spg_dat[1].spg_maxlevel);
    });
    // Pre-computed SA fill percentage (0-100) for all stocks combined
    ctor.BindFunc("p1_sa_pct", [](Rml::Variant& v) {
        int dotlen = spg_dat[0].spg_dotlen > 0 ? spg_dat[0].spg_dotlen : 1;
        int max_stocks = spg_dat[0].spg_maxlevel > 0 ? spg_dat[0].spg_maxlevel : 1;
        int pct = (spg_dat[0].spg_level * dotlen + spg_dat[0].current_spg) * 100 / (max_stocks * dotlen);
        v = (pct > 100) ? 100 : pct;
    });
    ctor.BindFunc("p2_sa_pct", [](Rml::Variant& v) {
        int dotlen = spg_dat[1].spg_dotlen > 0 ? spg_dat[1].spg_dotlen : 1;
        int max_stocks = spg_dat[1].spg_maxlevel > 0 ? spg_dat[1].spg_maxlevel : 1;
        int pct = (spg_dat[1].spg_level * dotlen + spg_dat[1].current_spg) * 100 / (max_stocks * dotlen);
        v = (pct > 100) ? 100 : pct;
    });
    ctor.BindFunc("p1_stun_width", [](Rml::Variant& v) {
        char b[32];
        snprintf(b, sizeof(b), "%ddp", piyori_type[0].genkai);
        v = Rml::String(b);
    });
    ctor.BindFunc("p2_stun_width", [](Rml::Variant& v) {
        char b[32];
        snprintf(b, sizeof(b), "%ddp", piyori_type[1].genkai);
        v = Rml::String(b);
    });
    ctor.BindFunc("p1_sa_width", [](Rml::Variant& v) {
        char b[32];
        snprintf(b, sizeof(b), "%ddp", spg_dat[0].spg_dotlen);
        v = Rml::String(b);
    });
    ctor.BindFunc("p2_sa_width", [](Rml::Variant& v) {
        char b[32];
        snprintf(b, sizeof(b), "%ddp", spg_dat[1].spg_dotlen);
        v = Rml::String(b);
    });

    // ── Combo ──
    ctor.BindFunc("p1_combo_count", [](Rml::Variant& v) {
        auto& b = cmst_buff[0][cst_read[0]];
        v = (int)(b.hit_hi * 10 + b.hit_low);
    });
    ctor.BindFunc("p2_combo_count", [](Rml::Variant& v) {
        auto& b = cmst_buff[1][cst_read[1]];
        v = (int)(b.hit_hi * 10 + b.hit_low);
    });
    ctor.BindFunc("p1_combo_kind", [](Rml::Variant& v) { v = (int)cmst_buff[0][cst_read[0]].kind; });
    ctor.BindFunc("p2_combo_kind", [](Rml::Variant& v) { v = (int)cmst_buff[1][cst_read[1]].kind; });
    ctor.BindFunc("p1_combo_active", [](Rml::Variant& v) { v = (bool)(cmb_stock[0] > 0); });
    ctor.BindFunc("p2_combo_active", [](Rml::Variant& v) { v = (bool)(cmb_stock[1] > 0); });

    // ── Combo Points ──
    ctor.BindFunc("p1_combo_pts",
                  [](Rml::Variant& v) { v = (cmb_stock[0] > 0) ? (int)cmst_buff[0][cst_read[0]].pts : 0; });
    ctor.BindFunc("p2_combo_pts",
                  [](Rml::Variant& v) { v = (cmb_stock[1] > 0) ? (int)cmst_buff[1][cst_read[1]].pts : 0; });
    ctor.BindFunc("p1_combo_pts_flag",
                  [](Rml::Variant& v) { v = (bool)(cmb_stock[0] > 0 && cmst_buff[0][cst_read[0]].pts_flag); });
    ctor.BindFunc("p2_combo_pts_flag",
                  [](Rml::Variant& v) { v = (bool)(cmb_stock[1] > 0 && cmst_buff[1][cst_read[1]].pts_flag); });

    // ── Names & Wins ──
    ctor.BindFunc("p1_name", [](Rml::Variant& v) { v = Rml::String(char_name(My_char[0])); });
    ctor.BindFunc("p2_name", [](Rml::Variant& v) { v = Rml::String(char_name(My_char[1])); });
    ctor.BindFunc("p1_wins",
                  [](Rml::Variant& v) { v = (int)((Mode_Type == MODE_VERSUS) ? VS_Win_Record[0] : Win_Record[0]); });
    ctor.BindFunc("p2_wins",
                  [](Rml::Variant& v) { v = (int)((Mode_Type == MODE_VERSUS) ? VS_Win_Record[1] : Win_Record[1]); });

    // ── Round results (per-round bubbles) ──
    ctor.BindFunc("rounds_to_win",
                  [](Rml::Variant& v) { v = (int)(save_w[Present_Mode].Battle_Number[Play_Type] + 1); });
    ctor.BindFunc("p1_round_wins", [](Rml::Variant& v) { v = (int)PL_Wins[0]; });
    ctor.BindFunc("p2_round_wins", [](Rml::Variant& v) { v = (int)PL_Wins[1]; });
    // Per-round result type (int 0-7)
    ctor.BindFunc("p1_r0", [](Rml::Variant& v) { v = (int)flash_win_type[0][0]; });
    ctor.BindFunc("p1_r1", [](Rml::Variant& v) { v = (int)flash_win_type[0][1]; });
    ctor.BindFunc("p1_r2", [](Rml::Variant& v) { v = (int)flash_win_type[0][2]; });
    ctor.BindFunc("p1_r3", [](Rml::Variant& v) { v = (int)flash_win_type[0][3]; });
    ctor.BindFunc("p2_r0", [](Rml::Variant& v) { v = (int)flash_win_type[1][0]; });
    ctor.BindFunc("p2_r1", [](Rml::Variant& v) { v = (int)flash_win_type[1][1]; });
    ctor.BindFunc("p2_r2", [](Rml::Variant& v) { v = (int)flash_win_type[1][2]; });
    ctor.BindFunc("p2_r3", [](Rml::Variant& v) { v = (int)flash_win_type[1][3]; });
    // Per-round result label (string: V/P/C/D/J/S/"")
    ctor.BindFunc("p1_r0_lbl", [](Rml::Variant& v) { v = Rml::String(win_type_label(flash_win_type[0][0])); });
    ctor.BindFunc("p1_r1_lbl", [](Rml::Variant& v) { v = Rml::String(win_type_label(flash_win_type[0][1])); });
    ctor.BindFunc("p1_r2_lbl", [](Rml::Variant& v) { v = Rml::String(win_type_label(flash_win_type[0][2])); });
    ctor.BindFunc("p1_r3_lbl", [](Rml::Variant& v) { v = Rml::String(win_type_label(flash_win_type[0][3])); });
    ctor.BindFunc("p2_r0_lbl", [](Rml::Variant& v) { v = Rml::String(win_type_label(flash_win_type[1][0])); });
    ctor.BindFunc("p2_r1_lbl", [](Rml::Variant& v) { v = Rml::String(win_type_label(flash_win_type[1][1])); });
    ctor.BindFunc("p2_r2_lbl", [](Rml::Variant& v) { v = Rml::String(win_type_label(flash_win_type[1][2])); });
    ctor.BindFunc("p2_r3_lbl", [](Rml::Variant& v) { v = Rml::String(win_type_label(flash_win_type[1][3])); });

    // ── Score ──
    ctor.BindFunc("p1_score", [](Rml::Variant& v) { v = (int)(Score[0][Play_Type] + Continue_Coin[0]); });
    ctor.BindFunc("p2_score", [](Rml::Variant& v) { v = (int)(Score[1][Play_Type] + Continue_Coin[1]); });

    // ── Operator status (human vs CPU) ──
    ctor.BindFunc("p1_is_human", [](Rml::Variant& v) { v = (bool)(Operator_Status[0] != 0); });
    ctor.BindFunc("p2_is_human", [](Rml::Variant& v) { v = (bool)(Operator_Status[1] != 0); });

    // ── SA Type Numeral ──
    static const char* const sa_numerals[3] = { "I", "II", "III" };
    ctor.BindFunc("p1_sa_type", [](Rml::Variant& v) {
        int idx = Super_Arts[0];
        v = Rml::String((idx >= 0 && idx < 3) ? sa_numerals[idx] : "");
    });
    ctor.BindFunc("p2_sa_type", [](Rml::Variant& v) {
        int idx = Super_Arts[1];
        v = Rml::String((idx >= 0 && idx < 3) ? sa_numerals[idx] : "");
    });

    // ── Parry Counter ──
    ctor.BindFunc("p1_parry_count", [](Rml::Variant& v) { v = (int)paring_counter[0]; });
    ctor.BindFunc("p2_parry_count", [](Rml::Variant& v) { v = (int)paring_counter[1]; });

    // ── HUD visibility ──
    ctor.BindFunc("is_fight_active", [](Rml::Variant& v) { v = (bool)(Play_Game == 1); });

    // ── Training Stun Counter ──
    ctor.BindFunc("p1_combo_stun", [](Rml::Variant& v) { v = (int)g_training_state.p1.combo_stun; });
    ctor.BindFunc("p2_combo_stun", [](Rml::Variant& v) { v = (int)g_training_state.p2.combo_stun; });
    ctor.BindFunc("training_stun_active",
                  [](Rml::Variant& v) { v = (bool)(Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_TRIALS); });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    // Pre-load the HUD document (hidden initially; shown when is_fight_active is true)
    rmlui_wrapper_show_game_document("game_hud");

    SDL_Log("[RmlUi HUD] Data model registered (52 bindings)");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_game_hud_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    // Game01() calls rmlui_wrapper_hide_all_game_documents() between rounds,
    // which hides our HUD doc. Re-show it when a fight is active.
    if (Play_Game == 1 && !rmlui_wrapper_is_game_document_visible("game_hud")) {
        rmlui_wrapper_show_game_document("game_hud");
    }



    DIRTY_INT(p1_health, (int)plw[0].wu.vital_new);
    DIRTY_INT(p2_health, (int)plw[1].wu.vital_new);
    DIRTY_INT(p1_drain, (int)vit[0].cred);
    DIRTY_INT(p2_drain, (int)vit[1].cred);
    DIRTY_INT(p1_hp_color, (int)vit[0].colnum);
    DIRTY_INT(p2_hp_color, (int)vit[1].colnum);
    DIRTY_INT(round_timer, (int)round_timer);
    DIRTY_BOOL(timer_flash, flash_r_num != 0);
    DIRTY_BOOL(timer_infinite, mugen_flag != 0);
    DIRTY_INT(p1_stun, (int)sdat[0].cstn);
    DIRTY_INT(p2_stun, (int)sdat[1].cstn);
    DIRTY_INT(p1_stun_max, (int)piyori_type[0].genkai);
    DIRTY_INT(p2_stun_max, (int)piyori_type[1].genkai);
    DIRTY_BOOL(p1_stun_active, sdat[0].sflag != 0);
    DIRTY_BOOL(p2_stun_active, sdat[1].sflag != 0);
    DIRTY_INT(p1_sa_stocks, (int)spg_dat[0].spg_level);
    DIRTY_INT(p2_sa_stocks, (int)spg_dat[1].spg_level);
    DIRTY_INT(p1_sa_fill, (int)spg_dat[0].time);
    DIRTY_INT(p2_sa_fill, (int)spg_dat[1].time);
    DIRTY_INT(p1_sa_fill_max, (int)spg_dat[0].spg_dotlen);
    DIRTY_INT(p2_sa_fill_max, (int)spg_dat[1].spg_dotlen);
    DIRTY_BOOL(p1_sa_active, spg_dat[0].sa_flag != 0);
    DIRTY_BOOL(p2_sa_active, spg_dat[1].sa_flag != 0);
    DIRTY_BOOL(p1_sa_max, spg_dat[0].spg_maxlevel > 0 && spg_dat[0].spg_level >= spg_dat[0].spg_maxlevel);
    DIRTY_BOOL(p2_sa_max, spg_dat[1].spg_maxlevel > 0 && spg_dat[1].spg_level >= spg_dat[1].spg_maxlevel);
    DIRTY_INT(p1_sa_stocks_max, (int)spg_dat[0].spg_maxlevel);
    DIRTY_INT(p2_sa_stocks_max, (int)spg_dat[1].spg_maxlevel);
    { // SA fill percentage (0-100) for all stocks combined
        int dl0 = spg_dat[0].spg_dotlen > 0 ? spg_dat[0].spg_dotlen : 1;
        int max0 = spg_dat[0].spg_maxlevel > 0 ? spg_dat[0].spg_maxlevel : 1;
        int pct0 = (spg_dat[0].spg_level * dl0 + spg_dat[0].current_spg) * 100 / (max0 * dl0);
        DIRTY_INT(p1_sa_pct, pct0 > 100 ? 100 : pct0);

        int dl1 = spg_dat[1].spg_dotlen > 0 ? spg_dat[1].spg_dotlen : 1;
        int max1 = spg_dat[1].spg_maxlevel > 0 ? spg_dat[1].spg_maxlevel : 1;
        int pct1 = (spg_dat[1].spg_level * dl1 + spg_dat[1].current_spg) * 100 / (max1 * dl1);
        DIRTY_INT(p2_sa_pct, pct1 > 100 ? 100 : pct1);
    }

    char dwb[32];
    snprintf(dwb, sizeof(dwb), "%ddp", piyori_type[0].genkai);
    DIRTY_STR(p1_stun_width, Rml::String(dwb));
    snprintf(dwb, sizeof(dwb), "%ddp", piyori_type[1].genkai);
    DIRTY_STR(p2_stun_width, Rml::String(dwb));
    snprintf(dwb, sizeof(dwb), "%ddp", spg_dat[0].spg_dotlen);
    DIRTY_STR(p1_sa_width, Rml::String(dwb));
    snprintf(dwb, sizeof(dwb), "%ddp", spg_dat[1].spg_dotlen);
    DIRTY_STR(p2_sa_width, Rml::String(dwb));
    DIRTY_BOOL(p1_combo_active, cmb_stock[0] > 0);
    DIRTY_BOOL(p2_combo_active, cmb_stock[1] > 0);
    if (cmb_stock[0] > 0) {
        DIRTY_INT(p1_combo_count, cmst_buff[0][cst_read[0]].hit_hi * 10 + cmst_buff[0][cst_read[0]].hit_low);
        DIRTY_INT(p1_combo_kind, (int)cmst_buff[0][cst_read[0]].kind);
        DIRTY_INT(p1_combo_pts, (int)cmst_buff[0][cst_read[0]].pts);
        DIRTY_BOOL(p1_combo_pts_flag, cmst_buff[0][cst_read[0]].pts_flag != 0);
    } else {
        DIRTY_INT(p1_combo_pts, 0);
        DIRTY_BOOL(p1_combo_pts_flag, false);
    }
    if (cmb_stock[1] > 0) {
        DIRTY_INT(p2_combo_count, cmst_buff[1][cst_read[1]].hit_hi * 10 + cmst_buff[1][cst_read[1]].hit_low);
        DIRTY_INT(p2_combo_kind, (int)cmst_buff[1][cst_read[1]].kind);
        DIRTY_INT(p2_combo_pts, (int)cmst_buff[1][cst_read[1]].pts);
        DIRTY_BOOL(p2_combo_pts_flag, cmst_buff[1][cst_read[1]].pts_flag != 0);
    } else {
        DIRTY_INT(p2_combo_pts, 0);
        DIRTY_BOOL(p2_combo_pts_flag, false);
    }
    DIRTY_STR(p1_name, Rml::String(char_name(My_char[0] + chkNameAkuma(My_char[0], 6))));
    DIRTY_STR(p2_name, Rml::String(char_name(My_char[1] + chkNameAkuma(My_char[1], 6))));
    DIRTY_INT(p1_wins, (Mode_Type == MODE_VERSUS) ? (int)VS_Win_Record[0] : (int)Win_Record[0]);
    DIRTY_INT(p2_wins, (Mode_Type == MODE_VERSUS) ? (int)VS_Win_Record[1] : (int)Win_Record[1]);
    DIRTY_BOOL(is_fight_active, Play_Game == 1);
    DIRTY_INT(p1_combo_stun, (int)g_training_state.p1.combo_stun);
    DIRTY_INT(p2_combo_stun, (int)g_training_state.p2.combo_stun);
    DIRTY_BOOL(training_stun_active, Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_TRIALS);

    // ── Score ──
    DIRTY_INT(p1_score, (int)(Score[0][Play_Type] + Continue_Coin[0]));
    DIRTY_INT(p2_score, (int)(Score[1][Play_Type] + Continue_Coin[1]));
    DIRTY_BOOL(p1_is_human, Operator_Status[0] != 0);
    DIRTY_BOOL(p2_is_human, Operator_Status[1] != 0);

    // ── Parry counter ──
    DIRTY_INT(p1_parry_count, (int)paring_counter[0]);
    DIRTY_INT(p2_parry_count, (int)paring_counter[1]);

    // ── SA type numeral ──
    static const char* const sa_nums[3] = { "I", "II", "III" };
    int sa0 = Super_Arts[0];
    int sa1 = Super_Arts[1];
    DIRTY_STR(p1_sa_type, Rml::String((sa0 >= 0 && sa0 < 3) ? sa_nums[sa0] : ""));
    DIRTY_STR(p2_sa_type, Rml::String((sa1 >= 0 && sa1 < 3) ? sa_nums[sa1] : ""));
    // ── Round results ──
    DIRTY_INT(rounds_to_win, (int)(save_w[Present_Mode].Battle_Number[Play_Type] + 1));
    DIRTY_INT(p1_round_wins, (int)PL_Wins[0]);
    DIRTY_INT(p2_round_wins, (int)PL_Wins[1]);
    DIRTY_INT(p1_r0, (int)flash_win_type[0][0]);
    DIRTY_INT(p1_r1, (int)flash_win_type[0][1]);
    DIRTY_INT(p1_r2, (int)flash_win_type[0][2]);
    DIRTY_INT(p1_r3, (int)flash_win_type[0][3]);
    DIRTY_INT(p2_r0, (int)flash_win_type[1][0]);
    DIRTY_INT(p2_r1, (int)flash_win_type[1][1]);
    DIRTY_INT(p2_r2, (int)flash_win_type[1][2]);
    DIRTY_INT(p2_r3, (int)flash_win_type[1][3]);
    DIRTY_STR(p1_r0_lbl, Rml::String(win_type_label(flash_win_type[0][0])));
    DIRTY_STR(p1_r1_lbl, Rml::String(win_type_label(flash_win_type[0][1])));
    DIRTY_STR(p1_r2_lbl, Rml::String(win_type_label(flash_win_type[0][2])));
    DIRTY_STR(p1_r3_lbl, Rml::String(win_type_label(flash_win_type[0][3])));
    DIRTY_STR(p2_r0_lbl, Rml::String(win_type_label(flash_win_type[1][0])));
    DIRTY_STR(p2_r1_lbl, Rml::String(win_type_label(flash_win_type[1][1])));
    DIRTY_STR(p2_r2_lbl, Rml::String(win_type_label(flash_win_type[1][2])));
    DIRTY_STR(p2_r3_lbl, Rml::String(win_type_label(flash_win_type[1][3])));
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_game_hud_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("game_hud");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("game_hud");
        s_model_registered = false;
    }
    SDL_Log("[RmlUi HUD] Shut down");
}

#undef DIRTY_INT
#undef DIRTY_BOOL
#undef DIRTY_STR
