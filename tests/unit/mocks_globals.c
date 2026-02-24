#include "sf33rd/Source/Game/engine/cmb_win.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/spgauge.h"
#include "sf33rd/Source/Game/engine/stun.h"
#include "sf33rd/Source/Game/engine/vital.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "structs.h"
#include "types.h"

// Mocks for globals defined in other .c files, needed for unit tests linking game_state.c

s16 SLOW_timer;
s16 SLOW_flag;
s16 EXE_flag;

JudgeGals judge_gals[2];
JudgeCom judge_com[2];
s16 last_judge_dada[2][5];
GradeFinalData judge_final[2][2];
GradeData judge_item[2][2];
u8 ji_sat[2][384];

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

SDAT sdat[2];
VIT vit[2];

s16 win_free[2];
s16 win_rno[2];
s16 poison_flag[2];

s16 eff_hit_flag[11];

const u8* ci_pointer;
u8 ci_col;
u8 ci_timer;

UNK_1 rambod[2];
UNK_2 ramhan[2];
u16 vital_inc_timer;
u16 vital_dec_timer;
s16 sag_inc_timer[2];

u32 system_timer;
