/**
 * @file select_timer.c
 * @brief Character select screen countdown timer implementation.
 *
 * Implements BCD-based countdown for the character/super-art selection screen.
 * The timer decrements once per second (60 frames), using BCD subtraction
 * inherited from the CPS3 arcade hardware.
 *
 * Part of the game flow module.
 * Originally from the PS2 game module.
 */

#include "sf33rd/Source/Game/select_timer.h"
#include "constants.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "types.h"

#include <SDL3/SDL.h>

#include <stdbool.h>

static s16 s_bcd_carry = 0;

/**
 * @brief BCD (Binary-Coded Decimal) subtraction: b - a.
 *
 * Performs packed-BCD subtraction with borrow, mirroring the 68000 SBCD instruction
 * used on the original CPS3 arcade hardware.
 *
 * @param a Value to subtract
 * @param b Value to subtract from
 * @return BCD result of (b - a)
 */
static u8 sbcd(u8 a, u8 b) {
    s16 c;
    s16 d;

    if ((d = (b & 0xF) - (a & 0xF) - (s_bcd_carry & 1)) < 0) {
        d += 10;
        d |= 16;
    }

    c = (b & 0xF0) - (a & 0xF0) - (d & 0xF0);
    d &= 0xF;

    if ((d |= c) < 0) {
        d += 160;
        s_bcd_carry = 1;
    } else {
        s_bcd_carry = 0;
    }

    return d;
}

/** @brief Pause the timer if Time_Stop indicates sleep mode. */
static void check_sleep() {
    if (Time_Stop == 2) {
        select_timer_state.step = 0;
    }
}

/** @brief Initialize the select timer for a new selection phase. */
void SelectTimer_Init() {
    select_timer_state.is_running = true;
    select_timer_state.step = 0;
}

/** @brief Clear and stop the select timer. */
void SelectTimer_Finish() {
    SDL_zero(select_timer_state);
}

/**
 * @brief Run one frame of the select timer state machine.
 *
 * Steps: 0=waiting for Time_Stop to clear, 1=counting down each second,
 * 2=reached zero (30-frame grace period), 3=timeout fired.
 */
void SelectTimer_Run() {
    if (Present_Mode == 4 || Present_Mode == 5) {
        return;
    }

    if (Debug_w[DEBUG_TIME_STOP]) {
        return;
    }

    if (Break_Into) {
        return;
    }

    switch (select_timer_state.step) {
    case 0:
        if (Time_Stop == 0) {
            select_timer_state.step = 1;
        }

        break;

    case 1:
        check_sleep();

        if (--Unit_Of_Timer) {
            break;
        }

        Unit_Of_Timer = UNIT_OF_TIMER_MAX;
        s_bcd_carry = 0;
        Select_Timer = sbcd(1, Select_Timer);

        if (Select_Timer == 0) {
            select_timer_state.step = 2;
            select_timer_state.timer = 30;
        }

        break;

    case 2:
        check_sleep();

        if (Select_Timer) {
            select_timer_state.step = 1;
            Unit_Of_Timer = UNIT_OF_TIMER_MAX;
        } else {
            select_timer_state.timer -= 1;

            if (select_timer_state.timer == 0) {
                Time_Over = true;
                select_timer_state.step = 3;
            }
        }

        break;

    case 3:
        check_sleep();
        Time_Over = true;

        if (Select_Timer) {
            select_timer_state.step = 1;
            Unit_Of_Timer = UNIT_OF_TIMER_MAX;
        }

        break;

    default:
        select_timer_state.is_running = false;
        break;
    }
}
