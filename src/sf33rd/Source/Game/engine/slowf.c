/**
 * @file slowf.c
 * Slow-Motion Controller
 */

#include "sf33rd/Source/Game/engine/slowf.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/workuser.h"

const s8 slow_timer_to_flag[32] = { 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
                                    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };

/** @brief Initializes slow-motion state to off. */
void init_slow_flag() {
    g_state.EXE_flag = 0;
    g_state.SLOW_flag = 0;
    g_state.SLOW_timer = 0;
}

/** @brief Triggers the conclusion slow-motion effect. */
void set_conclusion_slow() {
    g_state.SLOW_timer = 95;
}

/** @brief Per-frame update of the execution freeze flag based on slow-motion timer. */
void set_EXE_flag() {
    s16 tmw;

    if (!g_state.Game_pause) {
        if (g_state.SLOW_timer) {
            if (--g_state.SLOW_timer) {
                tmw = g_state.SLOW_timer / 8;

                if (tmw > 31) {
                    tmw = 31;
                }

                g_state.SLOW_flag = slow_timer_to_flag[tmw];
            } else {
                g_state.SLOW_flag = 0;
            }
        }

        g_state.EXE_flag = g_state.Game_timer % (g_state.SLOW_flag + 1);
    }
}
