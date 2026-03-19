/**
 * @file combo_stage_globals.c
 * @brief Combo system and stage/background global variable definitions.
 *
 * Combo tracking buffers, hit counters, and the stage background work area.
 * Split from game_globals.c for organizational clarity.
 */

#include "sf33rd/Source/Game/engine/cmb_win.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "types.h"

/* === Combo System === */

CMST_BUFF cmst_buff[2][5];
s16 old_cmb_flag[2];
s8 cmb_stock[2];
s8 first_attack;
s8 rever_attack[2];
s8 paring_attack[2];
s8 bonus_pts[2];
s16 hit_num;
u8 sa_kind;
u8 end_flag[2];
s16 calc_hit[2][10];
s16 score_calc[2][12];
s8 cmb_all_stock[1];
s8 sarts_finish_flag[2];
s8 last_hit_time;
s8 cmb_calc_now[2];
u8 cst_read[2];
u8 cst_write[2];

/* === Stage / Background === */

BG bg_w;

u16 att_req;
