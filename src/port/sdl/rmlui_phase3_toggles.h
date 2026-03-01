#pragma once
/**
 * @file rmlui_phase3_toggles.h
 * @brief Per-component toggle globals for Phase 3 CPS3 → RmlUi bypass.
 *
 * Each toggle is initially true (RmlUi mode enables all overlays).
 * Users can disable individual components per session from the mods menu,
 * causing the CPS3 fallback renderer to activate for that element.
 *
 * All globals are defined in rmlui_game_hud.cpp.
 * Include this header (plus <stdbool.h>) in any C file that needs bypass checks.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Fight HUD ─────────────────────────────────────────────────── */
extern bool rmlui_hud_health;        /** HP bar + drain bar */
extern bool rmlui_hud_timer;         /** Round countdown timer */
extern bool rmlui_hud_stun;          /** Stun gauge */
extern bool rmlui_hud_super;         /** Super Art (SA) gauge */
extern bool rmlui_hud_combo;         /** Combo window */
extern bool rmlui_hud_names;         /** Player name text */
extern bool rmlui_hud_faces;         /** Character portraits */
extern bool rmlui_hud_wins;          /** Win-pip indicators */
extern bool rmlui_hud_score;         /** Score digits (Arcade/VS) */
extern bool rmlui_hud_training_stun; /** Training combo stun counter */
extern bool rmlui_hud_training_data; /** Training damage/combo data overlay */

/* ── Menu screens ───────────────────────────────────────────────── */
extern bool rmlui_menu_mode;            /** Mode Select (Arcade/VS/Training/…) */
extern bool rmlui_menu_option;          /** Option Menu dispatcher */
extern bool rmlui_menu_game_option;     /** Game Option screen */
extern bool rmlui_menu_button_config;   /** Button Config */
extern bool rmlui_menu_sound;           /** Sound Test / Screen Adjust */
extern bool rmlui_menu_extra_option;    /** Extra Option (4 pages) */
extern bool rmlui_menu_sysdir;          /** System Direction dipswitch */
extern bool rmlui_menu_training;        /** Training Mode selector */
extern bool rmlui_menu_lobby;           /** Network Lobby */
extern bool rmlui_menu_memory_card;     /** Memory Card (Save/Load) */
extern bool rmlui_menu_blocking_tr;     /** Blocking Training pause menu */
extern bool rmlui_menu_blocking_tr_opt; /** Blocking Training option screen */

/* ── Screen overlays ────────────────────────────────────────────── */
extern bool rmlui_screen_title;        /** Title / "PRESS START" */
extern bool rmlui_screen_winner;       /** Winner/Loser banner */
extern bool rmlui_screen_continue;     /** Continue countdown */
extern bool rmlui_screen_gameover;     /** Game Over / Results */
extern bool rmlui_screen_select;       /** Character Select text overlay */
extern bool rmlui_screen_vs_result;    /** VS Result tally screen */
extern bool rmlui_screen_pause;        /** Pause text overlay */
extern bool rmlui_screen_entry_text;   /** Arcade-flow text (CONTINUE/GAME OVER/PRESS START) */
extern bool rmlui_screen_trials;       /** Trial mode HUD overlay */
extern bool rmlui_screen_copyright;    /** Copyright text overlay */
extern bool rmlui_screen_name_entry;   /** Name entry / ranking screen */
extern bool rmlui_screen_exit_confirm; /** Exit confirmation screen */
extern bool rmlui_screen_attract_overlay; /** Attract demo overlay (small logo + PRESS START) */

#ifdef __cplusplus
}
#endif
