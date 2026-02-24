/**
 * @file menu_internal.h
 * @brief Internal declarations shared between menu.c, menu_draw.c, and menu_input.c.
 *
 * NOT part of the public API — include only from menu_*.c files.
 */

#ifndef MENU_INTERNAL_H
#define MENU_INTERNAL_H

#include "sf33rd/Source/Game/menu/menu.h"
#include "structs.h"
#include "types.h"

/* === Shared types === */

typedef void (*MenuFunc)(struct _TASK*);

typedef struct {
    s16 pos_x;
    s8* menu;
} LetterData;

/* === Dispatch table sizes === */
#define MENU_JMP_COUNT 14
#define AT_JMP_COUNT 22
#define IN_GAME_JMP_COUNT 5
#define AUTO_SAVE_JMP_COUNT 4
#define TRAINING_JMP_COUNT 8
#define TRAINING_LETTER_COUNT 6
#define MENU_DELAY_COUNT 6

/* === Shared state (defined in menu.c) === */
extern u8 r_no_plus;
extern u8 control_player;
extern u8 control_pl_rno;
extern const LetterData training_letter_data[TRAINING_LETTER_COUNT];
extern const u8 Menu_Deley_Time[MENU_DELAY_COUNT];
extern const u8 Game_Option_Index_Data[10];
extern const u8 Sound_Data_Max[3][6];
extern const u8 Menu_Max_Data_Tr[2][2][6];

/* === Functions in menu.c called from other menu_*.c files === */
s32 Exit_Sub(struct _TASK* task_ptr, s16 cursor_ix, s16 next_routine);
s32 Menu_Sub_case1(struct _TASK* task_ptr);

/* === Functions in menu_draw.c === */
void imgSelectGameButton(void);
void Setup_Win_Lose_OBJ(void);
void Setup_Button_Sub(s16 x, s16 y, s16 master_player);
void Flash_1P_or_2P(struct _TASK* task_ptr);
void Training_Disp_Sub(struct _TASK* task_ptr);

/* === Functions in menu_input.c === */
u16 MC_Move_Sub(u16 sw, s16 cursor_id, s16 menu_max, s16 cansel_menu);
void System_Dir_Move_Sub(s16 PL_id);
void System_Dir_Move_Sub_LR(u16 sw, s16 cursor_id);
void Dir_Move_Sub(struct _TASK* task_ptr, s16 PL_id);
u16 Dir_Move_Sub2(u16 sw);
void Dir_Move_Sub_LR(u16 sw, s16 unused);
void Setup_Next_Page(struct _TASK* task_ptr, u8 unused);
void Save_Direction(struct _TASK* task_ptr);
void Load_Direction(struct _TASK* task_ptr);
void Load_Replay_Sub(struct _TASK* task_ptr);
u16 Game_Option_Sub(s16 PL_id);
u16 GO_Move_Sub_LR(u16 sw, s16 cursor_id);
void Button_Config_Sub(s16 PL_id);
void Button_Move_Sub_LR(u16 sw, s16 cursor_id);
void Button_Exit_Check(struct _TASK* task_ptr, s16 PL_id);
void Return_Option_Mode_Sub(struct _TASK* task_ptr);
void Screen_Adjust_Sub(s16 PL_id);
void Screen_Exit_Check(struct _TASK* task_ptr, s16 PL_id);
void Screen_Move_Sub_LR(u16 sw);
void Setup_Sound_Mode(u8 last_mode);
u16 Sound_Cursor_Sub(s16 PL_id);
u16 SD_Move_Sub_LR(u16 sw);
void Memory_Card_Sub(s16 PL_id);
void Save_Load_Menu(struct _TASK* task_ptr);
void Go_Back_MC(struct _TASK* task_ptr);
u16 Memory_Card_Move_Sub_LR(u16 sw, s16 cursor_id);
u16 After_VS_Move_Sub(u16 sw, s16 cursor_id, s16 menu_max);
s32 VS_Result_Move_Sub(struct _TASK* task_ptr, s16 PL_id);
s32 VS_Result_Select_Sub(struct _TASK* task_ptr, s16 PL_id);
void Setup_Save_Replay_1st(struct _TASK* task_ptr);
s32 Save_Replay_MC_Sub(struct _TASK* task_ptr, s16 unused);
void Exit_Replay_Save(struct _TASK* task_ptr);
void Return_VS_Result_Sub(struct _TASK* task_ptr);
void Decide_PL(s16 PL_id);
void Control_Player_Tr(void);
void Next_Be_Tr_Menu(struct _TASK* task_ptr);
s32 Check_Pause_Term_Tr(s16 PL_id);
s32 Pause_Check_Tr(s16 PL_id);
void Setup_Tr_Pause(struct _TASK* task_ptr);
s32 Pause_in_Normal_Tr(struct _TASK* task_ptr);
s32 Pause_1st_Sub(struct _TASK* task_ptr);
void Menu_Select(struct _TASK* task_ptr);
void Return_Pause_Sub(struct _TASK* task_ptr);
s32 Check_Pad_in_Pause(struct _TASK* task_ptr);
void Pad_Come_Out(struct _TASK* task_ptr);
s32 Yes_No_Cursor_Move_Sub(struct _TASK* task_ptr);
void Button_Config_in_Game(struct _TASK* task_ptr);
void Button_Exit_Check_in_Game(struct _TASK* task_ptr, s16 PL_id);
void Yes_No_Cursor_Exit_Training(struct _TASK* task_ptr, s16 cursor_id);
void Button_Config_Tr(struct _TASK* task_ptr);
void Button_Exit_Check_in_Tr(struct _TASK* task_ptr, s16 PL_id);
void Dummy_Move_Sub(struct _TASK* task_ptr, s16 PL_id, s16 id, s16 type, s16 max);
void Dummy_Move_Sub_LR(u16 sw, s16 id, s16 type, s16 cursor_id);
void Ex_Move_Sub_LR(u16 sw, s16 PL_id);
void Check_Skip_Recording(void);
void Check_Skip_Replay(s16 ix);
void Setup_NTr_Data(s16 ix);
void Training_Init_Sub(struct _TASK* task_ptr);
void Training_Exit_Sub(struct _TASK* task_ptr);
void Default_Training_Option(void);
void Back_to_Mode_Select(struct _TASK* task_ptr);

#endif /* MENU_INTERNAL_H */
