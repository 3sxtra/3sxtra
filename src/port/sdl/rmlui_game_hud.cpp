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
#include "sf33rd/Source/Game/engine/spgauge.h"  /* SPG_DAT, spg_dat[2] — SA gauge */
#include "sf33rd/Source/Game/engine/workuser.h"  /* PLW, Super_Arts, My_char, Win_Record, Max_vitality, Mode_Type … (pulls structs.h) */
#include "sf33rd/Source/Game/training/training_state.h"  /* g_training_state — combo stun */
#include "sf33rd/Source/Game/engine/plcnt.h"     /* piyori_type[2] (PiyoriType), plw[2] */
#include "sf33rd/Source/Game/engine/cmb_win.h"   /* CMST_BUFF, cmst_buff[2][5], cmb_stock[2], cst_read[2] */

/* VIT and SDAT are defined in structs.h (pulled by workuser.h).
   Declare the globals here since no header exposes them as externs. */
extern VIT  vit[2];
extern SDAT sdat[2];

/* Timer globals (declared in count.c, no public header) */
extern s16 round_timer;
extern s8  flash_r_num;
extern s8  mugen_flag;

/* Fight-active flag */
extern u8 Play_Game;

} // extern "C"


// ─── Character name table (SF3:3S roster, index matches My_char) ───
static const char* const s_char_names[20] = {
    "RYU",     "ALEX",   "YUEN",   "DUDLEY", "NECRO",
    "HUGO",    "IBuki",  "ELENA",  "ORO",    "YANG",
    "KEN",     "SEAN",   "MAKOTO", "REMY",   "Q",
    "TWELVE",  "CHUN-LI","URIEN",  "GILL",   "AKUMA"
};
#define CHAR_NAME_COUNT 20

// ─── Toggle globals (defined here, declared extern in rmlui_phase3_toggles.h) ───
bool rmlui_hud_health    = true;
bool rmlui_hud_timer     = true;
bool rmlui_hud_stun      = true;
bool rmlui_hud_super     = true;
bool rmlui_hud_combo     = true;
bool rmlui_hud_names     = true;
bool rmlui_hud_faces     = true;
bool rmlui_hud_wins      = true;
bool rmlui_hud_training_stun = true;

bool rmlui_menu_mode          = true;
bool rmlui_menu_option        = true;
bool rmlui_menu_game_option   = true;
bool rmlui_menu_button_config = true;
bool rmlui_menu_sound         = true;
bool rmlui_menu_extra_option  = true;
bool rmlui_menu_sysdir        = true;
bool rmlui_menu_training      = true;
bool rmlui_menu_lobby         = true;
bool rmlui_menu_memory_card   = true;
bool rmlui_menu_blocking_tr   = true;
bool rmlui_menu_blocking_tr_opt = true;

bool rmlui_screen_title    = true;
bool rmlui_screen_winner   = true;
bool rmlui_screen_continue = true;
bool rmlui_screen_gameover = true;
bool rmlui_screen_select   = true;
bool rmlui_screen_vs_result= true;
bool rmlui_screen_pause      = true;
bool rmlui_screen_entry_text = true;
bool rmlui_screen_trials     = true;
bool rmlui_screen_copyright  = true;
bool rmlui_screen_name_entry = true;

// ─── Data model state ───────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

// Cached snapshot for dirty detection
struct HudSnapshot {
    int  p1_health, p2_health;
    int  p1_drain,  p2_drain;
    int  p1_hp_color, p2_hp_color;
    int  round_timer_val;
    bool timer_flash;
    bool timer_infinite;
    int  p1_stun, p2_stun;
    int  p1_stun_max, p2_stun_max;
    bool p1_stun_active, p2_stun_active;
    int  p1_sa_stocks,   p2_sa_stocks;
    int  p1_sa_stocks_max, p2_sa_stocks_max;
    int  p1_sa_fill,     p2_sa_fill;
    int  p1_sa_fill_max, p2_sa_fill_max;
    bool p1_sa_active,   p2_sa_active;
    bool p1_sa_max,      p2_sa_max;
    int  p1_combo_count, p2_combo_count;
    int  p1_combo_kind,  p2_combo_kind;
    bool p1_combo_active, p2_combo_active;
    Rml::String p1_name, p2_name;
    int  p1_wins, p2_wins;
    bool is_fight_active;
    int  p1_combo_stun, p2_combo_stun;
    bool training_stun_active;
};
static HudSnapshot s_cache = {};

// ─── Dirty-check macros ──────────────────────────────────────────
#define DIRTY_INT(nm, expr) do { \
    int _v = (expr); \
    if (_v != s_cache.nm) { s_cache.nm = _v; s_model_handle.DirtyVariable(#nm); } \
} while(0)

#define DIRTY_BOOL(nm, expr) do { \
    bool _v = (bool)(expr); \
    if (_v != s_cache.nm) { s_cache.nm = _v; s_model_handle.DirtyVariable(#nm); } \
} while(0)

#define DIRTY_STR(nm, expr) do { \
    Rml::String _v = (expr); \
    if (_v != s_cache.nm) { s_cache.nm = _v; s_model_handle.DirtyVariable(#nm); } \
} while(0)

// ─── Helper: safe char name lookup ──────────────────────────────
static const char* char_name(int idx) {
    if (idx >= 0 && idx < CHAR_NAME_COUNT) return s_char_names[idx];
    return "???";
}

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_game_hud_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
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
    ctor.BindFunc("p1_health", [](Rml::Variant& v){ v = (int)plw[0].wu.vital_new; });
    ctor.BindFunc("p2_health", [](Rml::Variant& v){ v = (int)plw[1].wu.vital_new; });
    ctor.BindFunc("p1_drain",  [](Rml::Variant& v){ v = (int)vit[0].cred; });
    ctor.BindFunc("p2_drain",  [](Rml::Variant& v){ v = (int)vit[1].cred; });
    ctor.BindFunc("p1_hp_color",[](Rml::Variant& v){ v = (int)vit[0].colnum; });
    ctor.BindFunc("p2_hp_color",[](Rml::Variant& v){ v = (int)vit[1].colnum; });
    ctor.BindFunc("health_max", [](Rml::Variant& v){ v = (int)Max_vitality; });

    // ── Timer ──
    ctor.BindFunc("round_timer",   [](Rml::Variant& v){ v = (int)round_timer; });
    ctor.BindFunc("timer_flash",   [](Rml::Variant& v){ v = (bool)(flash_r_num != 0); });
    ctor.BindFunc("timer_infinite",[](Rml::Variant& v){ v = (bool)(mugen_flag != 0); });

    // ── Stun ──
    ctor.BindFunc("p1_stun",        [](Rml::Variant& v){ v = (int)sdat[0].cstn; });
    ctor.BindFunc("p2_stun",        [](Rml::Variant& v){ v = (int)sdat[1].cstn; });
    ctor.BindFunc("p1_stun_max",    [](Rml::Variant& v){ v = (int)piyori_type[0].genkai; });
    ctor.BindFunc("p2_stun_max",    [](Rml::Variant& v){ v = (int)piyori_type[1].genkai; });
    ctor.BindFunc("p1_stun_active", [](Rml::Variant& v){ v = (bool)(sdat[0].sflag != 0); });
    ctor.BindFunc("p2_stun_active", [](Rml::Variant& v){ v = (bool)(sdat[1].sflag != 0); });

    // ── Super Art Gauge ──
    // spg_dat[N].max  = number of full stocks stored
    // Super_Arts[N]   = selected SA variant (1/2/3) = max stocks possible
    // spg_dat[N].time = per-stock fill level
    // spg_dat[N].sa_flag nonzero while SA timer is running
    ctor.BindFunc("p1_sa_stocks",     [](Rml::Variant& v){ v = (int)spg_dat[0].max; });
    ctor.BindFunc("p2_sa_stocks",     [](Rml::Variant& v){ v = (int)spg_dat[1].max; });
    ctor.BindFunc("p1_sa_stocks_max", [](Rml::Variant& v){ v = (int)Super_Arts[0]; });
    ctor.BindFunc("p2_sa_stocks_max", [](Rml::Variant& v){ v = (int)Super_Arts[1]; });
    ctor.BindFunc("p1_sa_fill",       [](Rml::Variant& v){ v = (int)spg_dat[0].time; });
    ctor.BindFunc("p2_sa_fill",       [](Rml::Variant& v){ v = (int)spg_dat[1].time; });
    ctor.BindFunc("p1_sa_fill_max",   [](Rml::Variant& v){ v = (int)spg_dat[0].spg_dotlen; });
    ctor.BindFunc("p2_sa_fill_max",   [](Rml::Variant& v){ v = (int)spg_dat[1].spg_dotlen; });
    ctor.BindFunc("p1_sa_active",     [](Rml::Variant& v){ v = (bool)(spg_dat[0].sa_flag != 0); });
    ctor.BindFunc("p2_sa_active",     [](Rml::Variant& v){ v = (bool)(spg_dat[1].sa_flag != 0); });
    ctor.BindFunc("p1_sa_max",        [](Rml::Variant& v){ v = (bool)(spg_dat[0].max >= Super_Arts[0] && Super_Arts[0] > 0); });
    ctor.BindFunc("p2_sa_max",        [](Rml::Variant& v){ v = (bool)(spg_dat[1].max >= Super_Arts[1] && Super_Arts[1] > 0); });

    // ── Combo ──
    ctor.BindFunc("p1_combo_count", [](Rml::Variant& v){
        auto& b = cmst_buff[0][cst_read[0]];
        v = (int)(b.hit_hi * 10 + b.hit_low);
    });
    ctor.BindFunc("p2_combo_count", [](Rml::Variant& v){
        auto& b = cmst_buff[1][cst_read[1]];
        v = (int)(b.hit_hi * 10 + b.hit_low);
    });
    ctor.BindFunc("p1_combo_kind", [](Rml::Variant& v){ v = (int)cmst_buff[0][cst_read[0]].kind; });
    ctor.BindFunc("p2_combo_kind", [](Rml::Variant& v){ v = (int)cmst_buff[1][cst_read[1]].kind; });
    ctor.BindFunc("p1_combo_active",[](Rml::Variant& v){ v = (bool)(cmb_stock[0] > 0); });
    ctor.BindFunc("p2_combo_active",[](Rml::Variant& v){ v = (bool)(cmb_stock[1] > 0); });

    // ── Names & Wins ──
    ctor.BindFunc("p1_name", [](Rml::Variant& v){ v = Rml::String(char_name(My_char[0])); });
    ctor.BindFunc("p2_name", [](Rml::Variant& v){ v = Rml::String(char_name(My_char[1])); });
    ctor.BindFunc("p1_wins", [](Rml::Variant& v){
        v = (int)((Mode_Type == MODE_VERSUS) ? VS_Win_Record[0] : Win_Record[0]);
    });
    ctor.BindFunc("p2_wins", [](Rml::Variant& v){
        v = (int)((Mode_Type == MODE_VERSUS) ? VS_Win_Record[1] : Win_Record[1]);
    });

    // ── HUD visibility ──
    ctor.BindFunc("is_fight_active", [](Rml::Variant& v){ v = (bool)(Play_Game == 1); });

    // ── Training Stun Counter ──
    ctor.BindFunc("p1_combo_stun", [](Rml::Variant& v){ v = (int)g_training_state.p1.combo_stun; });
    ctor.BindFunc("p2_combo_stun", [](Rml::Variant& v){ v = (int)g_training_state.p2.combo_stun; });
    ctor.BindFunc("training_stun_active", [](Rml::Variant& v){
        v = (bool)(Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_TRIALS);
    });

    s_model_handle  = ctor.GetModelHandle();
    s_model_registered = true;

    // Pre-load the HUD document (hidden initially; shown when is_fight_active is true)
    rmlui_wrapper_show_document("game_hud");

    SDL_Log("[RmlUi HUD] Data model registered (36 bindings)");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_game_hud_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    DIRTY_INT(p1_health,      (int)plw[0].wu.vital_new);
    DIRTY_INT(p2_health,      (int)plw[1].wu.vital_new);
    DIRTY_INT(p1_drain,       (int)vit[0].cred);
    DIRTY_INT(p2_drain,       (int)vit[1].cred);
    DIRTY_INT(p1_hp_color,    (int)vit[0].colnum);
    DIRTY_INT(p2_hp_color,    (int)vit[1].colnum);
    DIRTY_INT(round_timer_val,(int)round_timer);
    DIRTY_BOOL(timer_flash,   flash_r_num != 0);
    DIRTY_BOOL(timer_infinite, mugen_flag != 0);
    DIRTY_INT(p1_stun,        (int)sdat[0].cstn);
    DIRTY_INT(p2_stun,        (int)sdat[1].cstn);
    DIRTY_INT(p1_stun_max,    (int)piyori_type[0].genkai);
    DIRTY_INT(p2_stun_max,    (int)piyori_type[1].genkai);
    DIRTY_BOOL(p1_stun_active, sdat[0].sflag != 0);
    DIRTY_BOOL(p2_stun_active, sdat[1].sflag != 0);
    DIRTY_INT(p1_sa_stocks,   (int)spg_dat[0].max);
    DIRTY_INT(p2_sa_stocks,   (int)spg_dat[1].max);
    DIRTY_INT(p1_sa_fill,     (int)spg_dat[0].time);
    DIRTY_INT(p2_sa_fill,     (int)spg_dat[1].time);
    DIRTY_INT(p1_sa_fill_max, (int)spg_dat[0].spg_dotlen);
    DIRTY_INT(p2_sa_fill_max, (int)spg_dat[1].spg_dotlen);
    DIRTY_BOOL(p1_sa_active,  spg_dat[0].sa_flag != 0);
    DIRTY_BOOL(p2_sa_active,  spg_dat[1].sa_flag != 0);
    DIRTY_BOOL(p1_sa_max,     Super_Arts[0] > 0 && spg_dat[0].max >= Super_Arts[0]);
    DIRTY_BOOL(p2_sa_max,     Super_Arts[1] > 0 && spg_dat[1].max >= Super_Arts[1]);
    DIRTY_BOOL(p1_combo_active, cmb_stock[0] > 0);
    DIRTY_BOOL(p2_combo_active, cmb_stock[1] > 0);
    if (cmb_stock[0] > 0) {
        DIRTY_INT(p1_combo_count, cmst_buff[0][cst_read[0]].hit_hi*10 + cmst_buff[0][cst_read[0]].hit_low);
        DIRTY_INT(p1_combo_kind,  (int)cmst_buff[0][cst_read[0]].kind);
    }
    if (cmb_stock[1] > 0) {
        DIRTY_INT(p2_combo_count, cmst_buff[1][cst_read[1]].hit_hi*10 + cmst_buff[1][cst_read[1]].hit_low);
        DIRTY_INT(p2_combo_kind,  (int)cmst_buff[1][cst_read[1]].kind);
    }
    DIRTY_STR(p1_name, Rml::String(char_name(My_char[0])));
    DIRTY_STR(p2_name, Rml::String(char_name(My_char[1])));
    DIRTY_INT(p1_wins, (Mode_Type==MODE_VERSUS)?(int)VS_Win_Record[0]:(int)Win_Record[0]);
    DIRTY_INT(p2_wins, (Mode_Type==MODE_VERSUS)?(int)VS_Win_Record[1]:(int)Win_Record[1]);
    DIRTY_BOOL(is_fight_active, Play_Game == 1);
    DIRTY_INT(p1_combo_stun, (int)g_training_state.p1.combo_stun);
    DIRTY_INT(p2_combo_stun, (int)g_training_state.p2.combo_stun);
    DIRTY_BOOL(training_stun_active, Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_TRIALS);
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_game_hud_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_document("game_hud");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx) ctx->RemoveDataModel("game_hud");
        s_model_registered = false;
    }
    SDL_Log("[RmlUi HUD] Shut down");
}

#undef DIRTY_INT
#undef DIRTY_BOOL
#undef DIRTY_STR
