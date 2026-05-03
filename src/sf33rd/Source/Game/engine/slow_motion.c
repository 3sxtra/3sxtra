/**
 * @file slow_motion.c
 * Slow-Motion Controller
 */

#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/state_user.h"

const s8 slow_timer_to_flag[32] = { 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
                                    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };

/** @brief Initializes slow-motion state to off. */
void init_slowmo_flag() {
    g_state.execute_flag = 0;
    g_state.slowmo_flag = 0;
    g_state.slowmo_timer = 0;
}

/** @brief Triggers the conclusion slow-motion effect. */
void set_round_end_slowmo() {
    g_state.slowmo_timer = 95;
}

/** @brief Per-frame update of the execution freeze flag based on slow-motion timer. */
void set_execute_flag() {
    s16 tmw;

    if (!g_state.Game_pause) {
        if (g_state.slowmo_timer) {
            if (--g_state.slowmo_timer) {
                tmw = g_state.slowmo_timer / 8;

                if (tmw > 31) {
                    tmw = 31;
                }

                g_state.slowmo_flag = slow_timer_to_flag[tmw];
            } else {
                g_state.slowmo_flag = 0;
            }
        }

        g_state.execute_flag = g_state.Game_timer % (g_state.slowmo_flag + 1);
    }
}
