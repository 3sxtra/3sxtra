/**
 * @file count.c
 * @brief Game clock and round timer with flash effects.
 *
 * Manages the round countdown timer, bonus-game timer, and the
 * flashing color effect when time is running low (< 30 seconds).
 *
 * Part of the ui module.
 */

#include "sf33rd/Source/Game/ui/count.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/engine/player_common_mechanics.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_data.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

#define HOJI_COUNTER_MAX 53

/* Phase 3 RmlUi bypass */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include <stdbool.h>

/* Helper: CPS3 timer render, skipped when RmlUi handles timer */
#define COUNTER_WRITE_CPS3(atr)                                                                                        \
    do {                                                                                                               \
        if (!use_rmlui || !rmlui_hud_timer)                                                                            \
            counter_write(atr);                                                                                        \
    } while (0)

/** @brief Initialize the round timer from Time_Limit (or set infinite mode). */
void count_cont_init(u8 type) {
    if (g_state.Mode_Type == MODE_NETWORK) {
        g_state.Counter_hi = 99; // Netplay: use consistent value regardless of local DIP switch settings
    } else {
        g_state.Counter_hi = Time_Limit;
    }

    if (g_state.Counter_hi == -1) {
        g_state.mugen_flag = true;
        g_state.round_timer = 1;

        if (type == 0) {
            if (!use_rmlui || !rmlui_hud_timer)
                counter_write(4);
        }
    } else {
        g_state.mugen_flag = false;
        g_state.hoji_counter = HOJI_COUNTER_MAX;
        g_state.Counter_low = g_state.hoji_counter;
        g_state.round_timer = g_state.Counter_hi;
        g_state.math_counter_hi = g_state.Counter_hi;
        g_state.math_counter_hi /= 10;
        g_state.math_counter_low = g_state.Counter_hi - (g_state.math_counter_hi * 10);

        if (type == 0) {
            if (!use_rmlui || !rmlui_hud_timer)
                counter_write(4);
        }
    }

    g_state.flash_r_num = 0;
    g_state.flash_col = 0;
    g_state.counter_color = 4;
}

/** @brief Per-frame round timer update — check guards then tick down. */
void count_cont_main() {
    if (g_state.Bonus_Game_Flag) {
        return;
    }

    if (g_state.count_end) {
        if (!use_rmlui || !rmlui_hud_timer)
            counter_write(4);
        return;
    }

    if (Debug_w[DEBUG_TIME_STOP]) {
        if (!use_rmlui || !rmlui_hud_timer)
            counter_write(g_state.counter_color);
        return;
    }

    if (g_state.Allow_a_battle_f == 0 || g_state.Demo_Time_Stop != 0) {
        if (!use_rmlui || !rmlui_hud_timer)
            counter_write(g_state.counter_color);
        return;
    }

    if (g_state.Break_Into) {
        if (!use_rmlui || !rmlui_hud_timer)
            counter_write(g_state.counter_color);
        return;
    }

    if (sa_stop_check() != 0) {
        if (!use_rmlui || !rmlui_hud_timer)
            counter_write(g_state.counter_color);
        return;
    }

    if (g_state.mugen_flag) {
        if (!use_rmlui || !rmlui_hud_timer)
            counter_write(4);
        return;
    }

    if (!g_state.EXE_flag && !g_state.Game_pause) {
        counter_control();
        return;
    }

    if (!use_rmlui || !rmlui_hud_timer)
        counter_write(g_state.counter_color);
}

/** @brief Core countdown logic — decrement timer and trigger flash effects. */
void counter_control() {
    if (g_state.Counter_hi == 0) {
        if (No_Trans == 0) {
            if (!use_rmlui || !rmlui_hud_timer)
                counter_write(g_state.counter_color);
        }
        return;
    }

    if (g_state.flash_r_num) {
        if (g_state.Counter_hi == 10 && g_state.Counter_low == g_state.hoji_counter) {
            g_state.flash_timer = 0;
            counter_flash(1);
        } else if (g_state.Counter_hi < 11) {
            counter_flash(1);
        } else {
            counter_flash(0);
        }
    } else if (g_state.Counter_hi == 30 && g_state.Counter_low == g_state.hoji_counter) {
        g_state.flash_r_num = 1;
        g_state.flash_timer = 0;
        counter_flash(0);
    }

    if (g_state.Counter_low != 0) {
        g_state.Counter_low -= 1;

        if (No_Trans == 0) {
            if (!use_rmlui || !rmlui_hud_timer)
                counter_write(g_state.counter_color);
        }

        return;
    }

    g_state.Counter_low = g_state.hoji_counter;
    g_state.Counter_hi -= 1;

    if (g_state.Counter_hi == 0) {
        g_state.counter_color = 4;
    }

    g_state.round_timer = g_state.Counter_hi;
    g_state.math_counter_hi = g_state.Counter_hi;
    g_state.math_counter_hi /= 10;
    g_state.math_counter_low = g_state.Counter_hi - (g_state.math_counter_hi * 10);

    if (No_Trans == 0) {
        if (!use_rmlui || !rmlui_hud_timer)
            counter_write(g_state.counter_color);
    }
}

/** @brief Render the round timer digits on the HUD. */
void counter_write(u8 atr) {
    u8 i;

    if (omop_cockpit != 0) {
        if (omop_round_timer == 0) {
            for (i = 0; i < 4; i++) {
                scfont_sqput(i + 22, 1, 9, 2, 31, 2, 1, 3, 2);
            }
        } else if (!g_state.mugen_flag) {
            scfont_sqput(22, 0, atr, 2, g_state.math_counter_hi << 1, 2, 2, 4, 2);
            scfont_sqput(24, 0, atr, 2, g_state.math_counter_low << 1, 2, 2, 4, 2);
        } else {
            scfont_sqput(22, 0, 4, 2, 28, 28, 4, 4, 2);
        }

        scfont_sqput(21, 1, 9, 0, 12, 6, 1, 4, 2);
        scfont_sqput(26, 1, 137, 0, 12, 6, 1, 4, 2);
        scfont_sqput(22, 4, 9, 0, 3, 18, 4, 1, 2);
    }
}

/** @brief Render the bonus-game timer digits (larger style). */
void bcounter_write() {
    if (!No_Trans) {
        scfont_put(21, 4, 0x8F, 2, 20, 6, 2);
        scfont_sqput(22, 2, 15, 2, g_state.math_counter_hi << 1, 6, 2, 3, 2);
        scfont_sqput(24, 2, 15, 2, g_state.math_counter_low << 1, 6, 2, 3, 2);
        scfont_put(26, 4, 15, 2, 20, 6, 2);
    }
}

#define FLASH_TIMER_COUNT 2
#define FLASH_COLOR_COUNT 4

/** @brief Cycle through flash colors when time is low. */
void counter_flash(s8 Flash_Num) {
    if (Flash_Num < 0 || Flash_Num >= FLASH_TIMER_COUNT) {
        return;
    }

    g_state.flash_timer--;

    if (g_state.flash_timer < 0) {
        g_state.flash_timer = flash_timer_tbl[Flash_Num];

        if (g_state.flash_col >= 0 && g_state.flash_col < FLASH_COLOR_COUNT) {
            g_state.counter_color = flash_color_tbl[g_state.flash_col];
        }

        g_state.flash_col++;

        if (g_state.flash_col == FLASH_COLOR_COUNT) {
            g_state.flash_col = 0;
        }
    }
}

/** @brief Initialize the bonus-game countdown (50 seconds). */
void bcount_cont_init() {
    g_state.Counter_hi = 50;
    g_state.hoji_counter = HOJI_COUNTER_MAX;
    g_state.Counter_low = g_state.hoji_counter;
    g_state.round_timer = g_state.Counter_hi;
    g_state.math_counter_hi = 5;
    g_state.math_counter_low = 0;
    bcounter_write();
    g_state.Time_Stop = 0;
}

/** @brief Per-frame bonus timer update — check guards then tick down. */
void bcount_cont_main() {
    if (g_state.Break_Into != 0 || sa_stop_check() || g_state.Time_Stop != 0 || g_state.Allow_a_battle_f == 0) {
        return;
    }

    if (!Debug_w[DEBUG_TIME_STOP] && !g_state.EXE_flag && !g_state.Game_pause) {
        bcounter_control();
    }
}

/** @brief Core bonus countdown — decrement and trigger time-over. */
void bcounter_control() {
    if (g_state.Counter_hi == 0) {
        return;
    }

    if (g_state.Counter_low != 0) {
        g_state.Counter_low -= 1;
        return;
    }

    g_state.hoji_counter = HOJI_COUNTER_MAX;
    g_state.Counter_low = g_state.hoji_counter;
    g_state.Counter_hi -= 1;
    g_state.round_timer = g_state.Counter_hi;
    g_state.math_counter_hi = g_state.Counter_hi;
    g_state.math_counter_hi /= 10;
    g_state.math_counter_low = g_state.Counter_hi - (g_state.math_counter_hi * 10);

    if (g_state.Counter_hi == 0) {
        g_state.math_counter_hi = g_state.math_counter_low = 0;
        g_state.Allow_a_battle_f = 0;
        g_state.Time_Over = true;
    }
}

/** @brief Decrement bonus timer by 1 (or force to 0 if kind != 0). */
s16 bcounter_down(u8 kind) {
    if (g_state.Counter_hi == 0) {
        g_state.math_counter_hi = g_state.math_counter_low = 0;
        return 0;
    }

    g_state.Counter_hi -= 1;

    if (kind) {
        g_state.Counter_hi = 0;
    }

    g_state.math_counter_hi = g_state.Counter_hi;
    g_state.math_counter_hi /= 10;
    g_state.math_counter_low = g_state.Counter_hi - (g_state.math_counter_hi * 10);

    if (g_state.Counter_hi == 0) {
        g_state.math_counter_hi = g_state.math_counter_low = 0;
    }

    return g_state.Counter_hi;
}
