/**
 * @file work_sys.c
 * @brief Global system state variable definitions.
 *
 * Defines all shared system-level variables (player input, timers,
 * screen adjustments, game mode, save data, etc.) used throughout
 * the game. Corresponding extern declarations live in work_sys.h.
 *
 * Part of the system module.
 */

#include "common.h"
#include "game_state.h"
#include <stddef.h>

s8 Time_Limit;

// sbss

struct _SYSTEM_W sys_w;
TrainingData Training[3];
u16 p1sw_0;
u16 p1sw_1;
u16 p2sw_0;
u16 p2sw_1;
u16 p3sw_0;
u16 p3sw_1;
u16 p4sw_0;
u16 p4sw_1;
u8 Interface_Type[2];
u8 Disp_Size_H;
u8 Disp_Size_V;
u8 No_Trans;
u16 p1sw_buff;
u16 p2sw_buff;
u16 p3sw_buff;
u16 p4sw_buff;
u32 Interrupt_Timer;

#include "sf33rd/Source/Game/system/work_sys.h"

MTX BgMATRIX[9];
struct _TASK task[11];
struct _REP_GAME_INFOR Rep_Game_Infor[11];
_REPLAY_W Replay_w;
SystemDir system_dir[6];
Permission permission_player[6];
struct _SAVE_W save_w[SAVEW_COUNT];
