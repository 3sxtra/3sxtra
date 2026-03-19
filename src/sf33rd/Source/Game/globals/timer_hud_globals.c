/**
 * @file timer_hud_globals.c
 * @brief Round timer and HUD state global variable definitions.
 *
 * Timer counters, flash state, and select-timer work area.
 * Split from game_globals.c for organizational clarity.
 */

#include "sf33rd/Source/Game/select_timer.h"
#include "types.h"

/* === Round Timer / HUD State === */

s8 round_timer;
s8 flash_timer;
s8 flash_r_num;
s8 flash_col;
s8 math_counter_hi;
s8 math_counter_low;
u8 counter_color;
bool mugen_flag;
s8 hoji_counter;

SelectTimerState select_timer_state;
