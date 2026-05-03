/**
 * @file sys_sub.h
 * @brief Public API for the system state and management hub.
 *
 * Umbrella header — includes all sys_sub domain headers for
 * backward compatibility with existing consumers.
 */
#ifndef SYS_SUB_H
#define SYS_SUB_H

#include "structs.h"
#include "types.h"

#include <stdbool.h>

/* Domain headers — split from the original monolithic sys_sub.h */
#include "sf33rd/Source/Game/system/sys_replay.h"
#include "sf33rd/Source/Game/system/sys_ranking.h"
#include "sf33rd/Source/Game/system/sys_options.h"
#include "sf33rd/Source/Game/system/sys_score.h"

/* === Core wipe/transition === */
void Switch_Screen_Init(s32 /* unused */);
s32 Switch_Screen(u8 Wipe_Type);
s32 Switch_Screen_Revival(u8 Wipe_Type);

/* === Input remapping === */
u16 Remap_Buttons(u16 sw, const _PAD_INFOR* pad_info);
u16 Convert_User_Setting(s16 PL_id);

/* === Player/session state === */
void Clear_Personal_Data(s16 PL_id);
s16 Check_Count_Cut(s16 PL_id, s16 Limit);
void Disp_Personal_Count(s16 PL_id, s8 counter);
void Setup_Play_Type(void);
void Set_Training_Hitbox_Display(bool enabled);
bool Is_Training_Hitbox_Display_Enabled(void);
void Clear_Flash_No(void);

/// @brief Check if a player wants to skip an animation
/// @return `true` if animation should be skipped, `false` otherwise
bool Cut_Cut_Cut(void);

s32 Button_Cut_EX(s16* pTimer, s16 limit);
s32 Setup_Target_PL(void);
void Setup_Final_Grade(void);
void Clear_Win_Type(void);
void Clear_Disp_Ranking(s16 PL_id);
void Setup_ID(void);

/* === Decompression === */
void Meltw(u16* s, u16* d, s32 file_ptr);

/* === Background === */
void Setup_BG(s16 BG_INDEX, s16 X, s16 Y);
void Setup_Virtual_BG(s16 BG_INDEX, s16 X, s16 Y);
void BG_move(void);
void BG_move_Ex(u8 ix);
void Basic_Sub(void);
void Basic_Sub_Ex(void);
s32 Check_PL_Load(void);
void BG_Draw_System(void);

/* === Demo/input data === */
u16 Check_Demo_Data(s16 PL_id);

/* === System clear/init === */
void System_all_clear_Level_B(void);
s16 Cut_Cut_C_Timer(void);
void Switch_Priority_76(void);
s32 Cut_Cut_Sub(s16 xx);
bool Cut_Cut_Loser(void);
void njWaitVSync_with_N(void);

/* === Soft reset === */
void Soft_Reset_Sub(void);

/* === Task/menu === */
void cpRevivalTask(void);
s32 Check_Menu_Task(void);

/* === Time/difficulty === */
void Setup_Limit_Time(void);
void Setup_Training_Difficulty(void);

/* === Flash === */
void Clear_Flash_Init(s16 level);
s16 Clear_Flash_Sub(void);

/* === RNG === */
void All_Clear_Random_ix(void);
void All_Clear_Timer(void);
void All_Clear_ETC(void);
void Setup_Net_Random_ix(void);

/* === Fade === */
s32 Request_Fade(u16 fade_code);
s32 Check_Fade_Complete_SP(void);
s32 Check_Fade_Complete(void);

/* === Misc === */
void All_Clear_Suicide(void);
s32 Flash_Violent(State_Other* /* unused */, s32 /* unused */);

#endif
