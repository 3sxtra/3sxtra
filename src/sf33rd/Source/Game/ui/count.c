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
#include "common.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/engine/pls01.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_data.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

/** @brief Initialize the round timer from Time_Limit (or set infinite mode). */
void count_cont_init(u8 type) {
    if (Mode_Type == MODE_NETWORK) {
        Counter_hi = 99; // Netplay: use consistent value regardless of local DIP switch settings
    } else {
        Counter_hi = Time_Limit;
    }

    if (Counter_hi == -1) {
        mugen_flag = true;
        round_timer = 1;

        if (type == 0) {
            counter_write(4);
        }
    } else {
        mugen_flag = false;
        hoji_counter = 60;
        Counter_low = hoji_counter;
        round_timer = Counter_hi;
        math_counter_hi = Counter_hi;
        math_counter_hi /= 10;
        math_counter_low = Counter_hi - (math_counter_hi * 10);

        if (type == 0) {
            counter_write(4);
        }
    }

    flash_r_num = 0;
    flash_col = 0;
    counter_color = 4;
}

/** @brief Per-frame round timer update — check guards then tick down. */
void count_cont_main() {
    if (Bonus_Game_Flag) {
        return;
    }

    if (count_end) {
        counter_write(4);
        return;
    }

    if (Debug_w[DEBUG_TIME_STOP]) {
        counter_write(counter_color);
        return;
    }

    if (Allow_a_battle_f == 0 || Demo_Time_Stop != 0) {
        counter_write(counter_color);
        return;
    }

    if (Break_Into) {
        counter_write(counter_color);
        return;
    }

    if (sa_stop_check() != 0) {
        counter_write(counter_color);
        return;
    }

    if (mugen_flag) {
        counter_write(4);
        return;
    }

    if (!EXE_flag && !Game_pause) {
        counter_control();
        return;
    }

    counter_write(counter_color);
}

/** @brief Core countdown logic — decrement timer and trigger flash effects. */
void counter_control() {
    if (Counter_hi == 0) {
        if (No_Trans == 0) {
            counter_write(counter_color);
        }
        return;
    }

    if (flash_r_num) {
        if (Counter_hi == 10 && Counter_low == hoji_counter) {
            flash_timer = 0;
            counter_flash(1);
        } else if (Counter_hi < 11) {
            counter_flash(1);
        } else {
            counter_flash(0);
        }
    } else if (Counter_hi == 30 && Counter_low == hoji_counter) {
        flash_r_num = 1;
        flash_timer = 0;
        counter_flash(0);
    }

    if (Counter_low != 0) {
        Counter_low -= 1;

        if (No_Trans == 0) {
            counter_write(counter_color);
        }

        return;
    }

    Counter_low = hoji_counter;
    Counter_hi -= 1;

    if (Counter_hi == 0) {
        counter_color = 4;
    }

    round_timer = Counter_hi;
    math_counter_hi = Counter_hi;
    math_counter_hi /= 10;
    math_counter_low = Counter_hi - (math_counter_hi * 10);

    if (No_Trans == 0) {
        counter_write(counter_color);
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
        } else if (!mugen_flag) {
            scfont_sqput(22, 0, atr, 2, math_counter_hi << 1, 2, 2, 4, 2);
            scfont_sqput(24, 0, atr, 2, math_counter_low << 1, 2, 2, 4, 2);
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
        scfont_sqput(22, 2, 15, 2, math_counter_hi << 1, 6, 2, 3, 2);
        scfont_sqput(24, 2, 15, 2, math_counter_low << 1, 6, 2, 3, 2);
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

    flash_timer--;

    if (flash_timer < 0) {
        flash_timer = flash_timer_tbl[Flash_Num];

        if (flash_col >= 0 && flash_col < FLASH_COLOR_COUNT) {
            counter_color = flash_color_tbl[flash_col];
        }

        flash_col++;

        if (flash_col == FLASH_COLOR_COUNT) {
            flash_col = 0;
        }
    }
}

/** @brief Initialize the bonus-game countdown (50 seconds). */
void bcount_cont_init() {
    Counter_hi = 50;
    hoji_counter = 60;
    Counter_low = hoji_counter;
    round_timer = Counter_hi;
    math_counter_hi = 5;
    math_counter_low = 0;
    bcounter_write();
    Time_Stop = 0;
}

/** @brief Per-frame bonus timer update — check guards then tick down. */
void bcount_cont_main() {
    if (Break_Into != 0 || sa_stop_check() || Time_Stop != 0 || Allow_a_battle_f == 0) {
        return;
    }

    if (!Debug_w[DEBUG_TIME_STOP] && !EXE_flag && !Game_pause) {
        bcounter_control();
    }
}

/** @brief Core bonus countdown — decrement and trigger time-over. */
void bcounter_control() {
    if (Counter_hi == 0) {
        return;
    }

    if (Counter_low != 0) {
        Counter_low -= 1;
        return;
    }

    hoji_counter = 60;
    Counter_low = hoji_counter;
    Counter_hi -= 1;
    round_timer = Counter_hi;
    math_counter_hi = Counter_hi;
    math_counter_hi /= 10;
    math_counter_low = Counter_hi - (math_counter_hi * 10);

    if (Counter_hi == 0) {
        math_counter_hi = math_counter_low = 0;
        Allow_a_battle_f = 0;
        Time_Over = true;
    }
}

/** @brief Decrement bonus timer by 1 (or force to 0 if kind != 0). */
s16 bcounter_down(u8 kind) {
    if (Counter_hi == 0) {
        math_counter_hi = math_counter_low = 0;
        return 0;
    }

    Counter_hi -= 1;

    if (kind) {
        Counter_hi = 0;
    }

    math_counter_hi = Counter_hi;
    math_counter_hi /= 10;
    math_counter_low = Counter_hi - (math_counter_hi * 10);

    if (Counter_hi == 0) {
        math_counter_hi = math_counter_low = 0;
    }

    return Counter_hi;
}
