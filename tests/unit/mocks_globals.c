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

u8 rf_b2_flag;
s32 b2_curr_no;
s32 test_pl_no;
s32 test_mes_no;
s32 test_in;
s32 old_mes_no2;
s32 old_mes_no3;
s32 old_mes_no_pl;
s32 mes_timer;
s32 bg_pos[4];
s32 fm_pos[4];
s32 bg_prm[12];
s32 Gill_Appear_Flag;
char cmd_sel[2];
char no_sa[2];
s32 Hnc_Num;
s32 end_w[8];
s32 scr_sc;
s32 X_Adjust;
s32 Y_Adjust;
f32 BgMATRIX[16];
s32 vm_w[8];
s32 ck_ex_option[10];
s32 X_Adjust_Buff[4];
s32 Y_Adjust_Buff[4];
u16 PLsw[2][2];
u8 task[100];
u16 Screen_Switch = 0;
u16 Screen_Switch_Buffer = 0;
u8 rw_num;
u8 rw_bg_flag[4];
u8 tokusyu_stage;
s32 rw_gbix[13];
s8 stage_flash;
s8 stage_ftimer;
s32 yang_ix_plus;
s8 yang_ix;
s8 yang_timer;
u8 ending_flag;
BackgroundParameters end_prm[8];
u8 gouki_end_gbix[16];
const u32 *rw3col_ptr;
u8 bg_disp_off;
s32 bgPalCodeOffset[8];
RW_DATA rw_dat[20];
s32 FadeLimit;
s32 WipeLimit;
u8 Appear_car_stop;
u8 Appear_hv;
u8 Appear_free;
u8 Appear_flag;
s32 app_counter;
s32 appear_work;
u8 Appear_end;
s32 y_sitei_pos;
u8 y_sitei_flag;
s32 c_number;
s32 c_kakikae;
s32 g_number;
s32 g_kakikae;
u8 nosekae;
s32 scrn_adgjust_y;
s32 scrn_adgjust_x;
s32 zoom_add;
s32 ls_cnt1;
u8 bg_app;
u8 sa_pa_flag;
u8 aku_flag;
u8 seraph_flag;
u8 akebono_flag;
s32 bg_mvxy[8];
s32 chase_time_y;
s32 chase_time_x;
s32 chase_y;
s32 chase_x;
u8 demo_car_flag;
s32 ideal_w;
u8 bg_app_stop;
u8 bg_stop;
s32 base_y_pos;
s32 etcBgPalCnvTable[20];
s32 etcBgGixCnvTable[20];
#ifndef MOCK_SUPPRESS_CONFLICTS
s32 test_flag;
s32 ixbfw_cut;
bool pcon_dp_flag;
s32 Game_timer;
s32 wcp;
u8 My_char[2];
PLW plw[2];
s32 Max_vitality;
s32 att_req;
BG bg_w;
s32 bs2_floor;
#endif

bool use_rmlui = false;
void* rmlui_screen_trials = NULL;

// Effect globals needed by game_state.c (gather_state / load_state).
// Only defined when mocks_netplay.c is NOT linked (it provides its own copies).
#ifndef MOCK_HAS_NETPLAY
#include "sf33rd/Source/Game/effect/effect.h"
s16 exec_tm[8];
uintptr_t frw[EFFECT_MAX][448];
s16 frwctr;
s16 frwctr_min;
s16 frwque[EFFECT_MAX];
s16 head_ix[8];
s16 tail_ix[8];

// Stubs for functions called by save_state/load_state_from_event in game_state.c
int Netplay_GetPlayerHandle(void) { return 0; }
int Netplay_GetBattleStartFrame(void) { return -1; }
#endif
