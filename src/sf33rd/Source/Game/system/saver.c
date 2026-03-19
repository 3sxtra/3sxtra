/**
 * @file saver.c
 * @brief Screensaver fade-to-black after prolonged inactivity.
 *
 * Monitors controller input; after ~5 minutes (18000 frames) of no
 * button presses, gradually fades the screen to black. Any input
 * cancels the saver and fades back in.
 *
 * Part of the system module.
 * Originally from the PS2 saver module.
 */

#include "sf33rd/Source/Game/system/saver.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/reset.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

/** @brief Screensaver task states (replaces Main_Jmp_Tbl indices). */
enum SaverState {
    SAVER_STATE_INIT  = 0,
    SAVER_STATE_CHECK = 1,
    SAVER_STATE_MOVE  = 2,
    SAVER_STATE_EXIT  = 3,
    SAVER_STATE_COUNT
};

#define SAVER_IDLE_THRESHOLD 18000

/** @brief Main screensaver task entry point — dispatches sub-state unless a soft reset is active. */
void Saver_Task(struct _TASK* task_ptr) {
    if (nowSoftReset()) {
        return;
    }

    switch (task_ptr->r_no[0]) {
    case SAVER_STATE_INIT:  Saver_Init(task_ptr);  break;
    case SAVER_STATE_CHECK: Saver_Check(task_ptr); break;
    case SAVER_STATE_MOVE:  Saver_Move(task_ptr);  break;
    case SAVER_STATE_EXIT:  Saver_Exit(task_ptr);  break;
    default: break;
    }
}

/** @brief Initialize the screensaver task — reset all sub-state counters and timer. */
void Saver_Init(struct _TASK* task_ptr) {
    task_ptr->r_no[0] = 1;
    task_ptr->r_no[1] = 0;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    task_ptr->timer = 0;
}

/** @brief Check for inactivity — increment the idle timer and advance to fade when threshold is reached. */
void Saver_Check(struct _TASK* task_ptr) {
    if (Demo_Flag == 0) {
        task_ptr->timer = 0;
        return;
    }

    if ((PLsw[0][0] != 0) || (PLsw[1][0] != 0)) {
        task_ptr->timer = 0;
        return;
    }

    if ((task_ptr->timer += 1) > SAVER_IDLE_THRESHOLD) {
        task_ptr->r_no[0]++;
    }
}

/** @brief Active screensaver state — fade out the screen; reset on any input. */
void Saver_Move(struct _TASK* task_ptr) {
    if ((PLsw[0][0] != 0) || PLsw[1][0] != 0) {
        Saver_Init(task_ptr);

    } else {
        switch (task_ptr->r_no[1]) {
        case 0:
            task_ptr->r_no[1] += 1;
            task_ptr->free[0] = 0;
            FadeInit();
            /* fallthrough */

        case 1:
            FadeOut(1, 4, 0);

            if ((task_ptr->free[0]++) > 0x30) {
                task_ptr->r_no[1] += 1;
            }
            break;

        case 2:
            ToneDown(0xC8, 0);
            break;
        }
    }
}

/** @brief Screensaver exit state — fade back in and reinitialize. */
void Saver_Exit(struct _TASK* task_ptr) {
    switch (task_ptr->r_no[1]) {
    case 0:
        task_ptr->r_no[1] += 1;
        FadeInit();
        /* fallthrough */

    case 1:
        if (FadeIn(1, 0xFF, 8) != 0) {
            Saver_Init(task_ptr);
        }
    }
}
