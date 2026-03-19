/**
 * @file workuser_combat.h
 * @brief Extern declarations for gameplay globals - Combat and Round state
 */
#ifndef WORKUSER_COMBAT_H
#define WORKUSER_COMBAT_H

#include "types.h"
#include "game_state.h"
#include "sf33rd/Source/Game/engine/cmd_data.h"
#include "sf33rd/Source/Game/select_timer.h"
#include <stdbool.h>

extern s8 Winner_id;
extern s8 Loser_id;
extern s8 Break_Into;
extern s8 Forbid_Break;
extern s8 Request_Break[2];
extern s8 Continue_Count[2];
extern s8 WINNER;
extern s8 LOSER;
extern s8 Champion;
extern s8 Perfect_Flag;
extern s8 Counter_Attack[2];
extern s8 Attack_Flag[2];
extern s8 Guard_Flag[2];
extern s8 Stop_Combo;
extern u8 Stock_Hit_Flag[2];
extern u8 Continue_Coin[2];
extern s8 Continue_Menu[2];
extern u8 Type_of_Attack[2];
extern s8 Attack_Count_No0[2];
extern s8 sa_gauge_flash[2];
extern s8 aiuchi_flag;
extern u8 paring_counter[2];
extern u8 paring_bonus_r[2];
extern u8 paring_ctr_vs[2][2];
extern u8 paring_ctr_ori[2];
extern u8 last_parry_red[2];
extern u8 Attack_Count_Buff[2][4];
extern u8 Attack_Count_Index[2];
extern u8 CC_Value[2];
extern u8 Continue_Coin2[2];
extern u8 Perfect_Counter[2];
extern u8 Straight_Counter[2];
extern s8 Break_Into_CPU;
extern s8 Introduce_Break_Into[2];
extern u8 Pause_Hit_Marks;
extern u8 Extra_Break;
extern u8 Battle_Q[2];
extern u8 Continue_Count_Down[2];
extern u8 Continue_Cut[2];
extern u8 Straight_Flag[2];
extern s8 Vital_Handicap[6][2];
extern u16 Guard_Type[2];
extern s16 Attack_Counter[2];
extern s16 Last_Attack_Counter[2];
extern s16 Guard_Counter[2];
extern u16 vital_stop_flag[2];
extern u16 gauge_stop_flag[2];

#endif // WORKUSER_COMBAT_H
