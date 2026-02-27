/**
 * @file work_sys.h
 * @brief Extern declarations for global system state variables.
 *
 * @netplay_sync
 * Several globals declared here are saved/loaded in GameState for rollback.
 * Marked with @netplay_sync below. setup_vs_mode() in netplay.c canonicalizes
 * bg_pos, fm_pos, bg_prm, Screen_Switch, and system_timer to zero before
 * the first synced frame.
 */
#ifndef WORK_SYS_H
#define WORK_SYS_H

#include "structs.h"
#include "types.h"

extern struct _SYSTEM_W sys_w;
extern struct _VM_W vm_w;
extern _EXTRA_OPTION ck_ex_option;

/// Player 1 current inputs
extern u16 p1sw_0;

/// Player 1 previous inputs
extern u16 p1sw_1;

/// Player 2 current inputs
extern u16 p2sw_0;

/// Player 2 previous inputs
extern u16 p2sw_1;

/// Player 3 current inputs (unused)
extern u16 p3sw_0;

/// Player 3 previous inputs (unused)
extern u16 p3sw_1;

/// Player 4 current inputs (unused)
extern u16 p4sw_0;

/// Player 4 previous inputs (unused)
extern u16 p4sw_1;

extern u32 system_timer;
extern u8 Interface_Type[2];
extern s32 X_Adjust;
extern s32 Y_Adjust;
extern s32 X_Adjust_Buff[3];
extern s32 Y_Adjust_Buff[3];
extern u8 Disp_Size_H;
extern u8 Disp_Size_V;
extern u8 No_Trans;

/// Controller 1 inputs
extern u16 p1sw_buff;

/// Controller 2 inputs
extern u16 p2sw_buff;

/// Controller 3 inputs (unused)
extern u16 p3sw_buff;

/// Controller 4 inputs (unused)
extern u16 p4sw_buff;

extern u32 Interrupt_Timer;
extern s8 Gill_Appear_Flag;

/// @netplay_sync Current and previous inputs for controllers 1 and 2.
/// `PLsw[i][0]` holds current inputs for controller `i`. `PLsw[i][1]` – previous button presses.
/// Fed by advance_game() in netplay.c during rollback simulation.
extern u16 PLsw[2][2];

/// @netplay_sync Background scroll positions — zeroed by setup_vs_mode().
extern BG_POS bg_pos[8];
/// @netplay_sync Foreground scroll positions.
extern FM_POS fm_pos[8];
/// @netplay_sync Background parameters — zeroed by setup_vs_mode().
extern BackgroundParameters bg_prm[8];
extern f32 scr_sc;

extern struct _SAVE_W save_w[6];
extern Permission permission_player[6];
extern SystemDir system_dir[6];
extern _REPLAY_W Replay_w;
extern struct _REP_GAME_INFOR Rep_Game_Infor[11];
extern struct _TASK task[11];
extern MTX BgMATRIX[9];
extern TrainingData Training[3];

#endif
