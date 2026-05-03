/**
 * @file game_globals.c
 * @brief DECOMPOSED — see globals/ subdirectory.
 *
 * This file formerly held ~300 global variable definitions for the game module.
 * As of March 2026, those definitions have been split into domain-specific
 * source files under src/sf33rd/Source/Game/globals/:
 *
 *   player_globals.c      — Player state (PLW, super arts, appearance)
 *   timer_hud_globals.c   — Round timer, flash state, HUD counters
 *   score_globals.c       — Scores, bonuses, order arrays
 *   match_globals.c       — Round/match control, char select, AI, system timers
 *   combo_stage_globals.c — Combo system, stage/background work area
 *
 * The commented-out duplicate definitions (slowf, grade, spgauge, stun, vital,
 * win_pl, ta_sub, eff56, plcnt) that were already removed are not replicated
 * in the new files — they remain documented here for historical reference.
 */

/*
 * === Historical: Removed Duplicates ===
 *
 * The following globals were commented out because they are defined in their
 * respective subsystem .c files. Kept here as documentation only.
 */

// Removed duplicates found in slowf.c
/*
s16 SLOW_timer;
s16 SLOW_flag;
s16 EXE_flag;
*/

// Removed duplicates found in grade.c
/*
JudgeGals judge_gals[2];
JudgeCom judge_com[2];
s16 last_judge_dada[2][5];
GradeFinalData judge_final[2][2];
GradeData judge_item[2][2];
u8 ji_sat[2][384];
*/

// Removed duplicates found in spgauge.c
/*
s8 Old_Stop_SG;
s8 Exec_Wipe_F;
s8 time_clear[2];
s16 spg_number;
s16 spg_work;
s16 spg_offset;
s8 time_num;
s8 time_timer;
s8 time_flag[2];
s16 col;
s8 time_operate[2];
s8 sast_now[2];
s8 max2[2];
s8 max_rno2[2];
SPG_DAT spg_dat[2];
*/

// Removed duplicate found in stun.c
// SDAT sdat[2];

// Removed duplicate found in vital.c
// VIT vit[2];

// Removed duplicates found in win_pl.c
/*
s16 win_free[2];
s16 win_rno[2];
s16 poison_flag[2];
*/

// Removed duplicate found in ta_sub.c
// s16 eff_hit_flag[11];

// Removed duplicates found in eff56.c
/*
const u8* ci_pointer;
u8 ci_col;
u8 ci_timer;
*/

// Removed duplicates found in plcnt.c
/*
CollisionHurtbox rambod[2];
CollisionHurtboxExt ramhan[2];
u16 vital_inc_timer;
u16 vital_dec_timer;
s16 sag_inc_timer[2];
*/
