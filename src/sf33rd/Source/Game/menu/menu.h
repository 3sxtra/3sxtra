/**
 * @file menu.h
 * @brief Public API for game menus — mode select, options, training, replays.
 *
 * Part of the menu module.
 */

#ifndef MENU_H
#define MENU_H

#include "sf33rd/Source/Game/effect/effect_57_header_for_menus.h"
#include "structs.h"
#include "types.h"

void Menu_Task(struct _TASK* task_ptr);
void Menu_Init(struct _TASK* task_ptr);
void Setup_Pad_or_Stick();
u16 Check_Menu_Lever(u8 PL_id, s16 type);
void Menu_Common_Init();
s32 Load_Replay_MC_Sub(struct _TASK* task_ptr, s16 PL_id);
void Setup_Replay_Sub(s16 type, MenuHeader char_type, s16 master_player);
void Setup_Save_Replay_2nd(struct _TASK* task_ptr, s16 /* unused */);
s32 Setup_Final_Cursor_Pos(s8 cursor_x, s16 dir);
void Default_Training_Data(s32 flag);

/// Re-activate TASK_MENU at the Network_Lobby input loop (RmlUI mode).
/// Called when returning from a casual room match to the network lobby,
/// where Soft_Reset_Sub() has killed the menu task.
void Menu_ReenterNetworkLobby(void);

enum MenuAtState { MENU_AT_INIT = 0, MENU_AT_SAVE_DIRECTION = 19, MENU_AT_LOAD_DIRECTION = 20 };

enum AutoSaveState { AUTO_SAVE_1ST = 0, AUTO_SAVE_2ND = 1, AUTO_SAVE_3RD = 2, AUTO_SAVE_4TH = 3 };

/** @brief G_No magic numbers for menu operations */
#define GAME_STATE_MENU 2
#define GAME_MODE_MENU_IDLE 12
#define GAME_MODE_IN_GAME 1
#define GAME_SUBMODE_REPLAY 2
#define GAME_SUBMODE_TRAINING 5
#define GAME_SUBMODE_SAVE 6

void bg_etc_write_ex(s16 type);

#endif
