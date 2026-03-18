/**
 * @file ms_saver.c
 * @brief MenuScreen registry integration for the Screensaver.
 *
 * Implements the on_enter, on_tick, and on_exit callbacks for
 * MENU_SCREEN_SAVER.
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/reset.h"
#include "sf33rd/Source/Game/system/saver.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

#define SAVER_IDLE_THRESHOLD 18000

static void ms_saver_enter(struct _TASK* tp) {
    tp->free[0] = 1; /* Match Saver_Init -> r_no[0] = 1 (Saver_Check) */
    tp->free[1] = 0; /* Sub-phase */
    tp->timer = 0;   /* Idle timer */
}

static void ms_saver_tick(struct _TASK* tp) {
    switch (tp->free[0]) {
    case 1: /* Saver_Check */
        if (Demo_Flag == 0) {
            tp->timer = 0;
            return;
        }

        if ((PLsw[0][0] != 0) || (PLsw[1][0] != 0)) {
            tp->timer = 0;
            return;
        }

        if ((tp->timer += 1) > SAVER_IDLE_THRESHOLD) {
            tp->free[0] = 2; /* Move to Saver_Move */
        }
        break;

    case 2: /* Saver_Move */
        if ((PLsw[0][0] != 0) || PLsw[1][0] != 0) {
            /* Input detected -> exit screensaver */
            tp->free[0] = 1;
            tp->free[1] = 0;
            tp->timer = 0;
        } else {
            switch (tp->free[1]) {
            case 0:
                tp->free[1] += 1;
                tp->free[2] = 0; /* Use free[2] for fade counter instead of free[0] to avoid conflict */
                FadeInit();
                /* fallthrough */

            case 1:
                FadeOut(1, 4, 0);

                if ((tp->free[2]++) > 0x30) {
                    tp->free[1] += 1;
                }
                break;

            case 2:
                ToneDown(0xC8, 0);
                break;
            }
        }
        break;

    case 3: /* Saver_Exit */
        switch (tp->free[1]) {
        case 0:
            tp->free[1] += 1;
            FadeInit();
            /* fallthrough */

        case 1:
            if (FadeIn(1, 0xFF, 8) != 0) {
                /* Reset back to Saver_Check */
                tp->free[0] = 1;
                tp->free[1] = 0;
                tp->timer = 0;
            }
            break;
        }
        break;
    }
}

static void ms_saver_exit(struct _TASK* tp) {
    /* No-op */
}

__attribute__((constructor)) static void register_ms_saver() {
    extern MenuScreen g_screens[];

    g_screens[MENU_SCREEN_SAVER] = (MenuScreen) { .name = "saver",
                                                  .id = MENU_SCREEN_SAVER,
                                                  .parent = MENU_SCREEN_NONE,
                                                  .on_enter = ms_saver_enter,
                                                  .on_tick = ms_saver_tick,
                                                  .on_exit = ms_saver_exit,
                                                  .cursor_max = 0,
                                                  .cancel_item = -1,
                                                  .rmlui_show = NULL,
                                                  .rmlui_hide = NULL,
                                                  .header_type = (MenuHeader)-1,
                                                  .effect_slot = 0 };
}
