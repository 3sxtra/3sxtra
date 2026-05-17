/**
 * @file reset.c
 * @brief Soft-reset detection and execution state machine.
 *
 * Monitors Start+Back button combinations on both controllers to detect
 * a soft-reset request. When triggered, stops audio, breaks pending loads,
 * and reinitializes the game to the title screen.
 *
 * Part of the system module.
 * Originally from the PS2 reset module.
 */

#include "sf33rd/Source/Game/system/reset.h"
#include "game_state.h"
#include "netplay/netplay.h"
#include "port/input.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/io/afs_loader.h"
#include "sf33rd/Source/Game/rendering/texture_group.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/hud_subroutines.h"
#include "structs.h"

#define RESET_STATUS_PENDING 0x62
#define RESET_STATUS_TRIGGERED 0x63

/** @brief Top-level reset task states (replaces Main_Jmp_Tbl indices). */
typedef enum {
    RESET_INIT = 0,  /**< Initialize — advance to move state      */
    RESET_MOVE = 1,  /**< Monitor for reset button combination     */
    RESET_WAIT = 2,  /**< Wait for pending loads, then soft-reset  */
    RESET_SLEEP = 3, /**< Wait for buttons released before reinit  */
    RESET_STATE_COUNT
} ResetState;

u8 Reset_Status[2];
u8 RESET_X;

static void Reset_Init(struct _TASK* task_ptr);
static void Reset_Move(struct _TASK* task_ptr);
static void Reset_Wait(struct _TASK* task_ptr);
static void Reset_Sleep(struct _TASK* task_ptr);
static void Check_Reset();
static u8 Check_SoftReset(s16 PL_id);
static s32 Setup_Next_Disposal();
static void Check_Reset_IO(struct _TASK* /* unused */, s16 PL_id);

/** @brief Main reset task entry point — processes I/O for both players, then dispatches sub-state. */
void Reset_Task(struct _TASK* task_ptr) {
    Check_Reset_IO(task_ptr, 0);
    Check_Reset_IO(task_ptr, 1);
    switch ((ResetState)task_ptr->r_no[0]) {
    case RESET_INIT:
        Reset_Init(task_ptr);
        break;
    case RESET_MOVE:
        Reset_Move(task_ptr);
        break;
    case RESET_WAIT:
        Reset_Wait(task_ptr);
        break;
    case RESET_SLEEP:
        Reset_Sleep(task_ptr);
        break;
    default:
        break;
    }
}

/** @brief Reset init state — advance to move state and clear reset flag. */
static void Reset_Init(struct _TASK* task_ptr) {
    task_ptr->r_no[0] = RESET_MOVE;
    RESET_X = 0;
}

/** @brief Return whether a soft reset is currently in progress. */
u8 nowSoftReset() {
    return RESET_X != 0;
}

/** @brief Reset move state — check for reset input and initiate the reset sequence if detected. */
static void Reset_Move(struct _TASK* task_ptr) {
    RESET_X = 0;
    Check_Reset();

    if (RESET_X) {
        ToneDown(0xFF, 0);
        sound_all_off();
        task_ptr->r_no[0] = RESET_WAIT;
        task_ptr->free[0] = Setup_Next_Disposal();
        task_ptr->r_no[1] = 0;
        Request_LDREQ_Break();
        effect_work_init();
    }
}

/** @brief Reset wait state — stop audio and execute soft-reset once loads have completed. */
static void Reset_Wait(struct _TASK* task_ptr) {
    ToneDown(0xFF, 0);

    switch (task_ptr->r_no[1]) {
    case 0:
        sound_all_off();

        if (Check_LDREQ_Break() == 0) {
            task_ptr->r_no[1] += 1;
        }

        break;

    case 1:
        Soft_Reset_Sub();
        task_ptr->r_no[0] = RESET_SLEEP;
        break;
    }
}

/** @brief Reset sleep state — wait for the reset button to be released before reinitializing. */
static void Reset_Sleep(struct _TASK* task_ptr) {
    ToneDown(0xFF, 0);

    if (g_state.Pause_ID == 0) {
        if (!(p1sw_0 & 0x4000)) {
            task_ptr->r_no[0] = RESET_INIT;
        }
    } else if (!(p2sw_0 & 0x4000)) {
        task_ptr->r_no[0] = RESET_INIT;
    }

    if (task_ptr->r_no[0] == RESET_INIT) {
        checkAdxFileLoaded();
        checkSelObjFileLoaded();
    }
}

/** @brief Evaluate soft-reset conditions across both players (respects g_state.Forbid_Reset). */
static void Check_Reset() {
    if (g_state.Forbid_Reset) {
        RESET_X = 0;
        return;
    }

    if (Netplay_IsEnabled()) {
        RESET_X = 0;
        return;
    }

    g_state.Switch_Type = 1;

    if (Check_SoftReset(0) == 0) {
        Check_SoftReset(1);
    }
}

/** @brief Check whether the given player has entered the soft-reset button sequence. */
static u8 Check_SoftReset(s16 PL_id) {
    if (Reset_Status[PL_id] == RESET_STATUS_TRIGGERED) {
        g_state.Game_pause = 0x81;
        g_state.Pause_ID = PL_id;
        return RESET_X = 1;
    }

    return RESET_X = 0;
}

/** @brief Determine the next disposal type after a reset (bootrom return vs. normal restart). */
static s32 Setup_Next_Disposal() {
    if (g_state.Reset_Bootrom) {
        return 1;
    }

    if ((g_state.fsm[0] == 1) || ((g_state.fsm[0] == 2) && (g_state.fsm[1] == 0))) {
        return 1;
    }

    return 0;
}

/** @brief Track the Start/Back button state machine for reset detection on the given player. */
static void Check_Reset_IO(struct _TASK* /* unused */, s16 PL_id) {
    u16 sw;
    u16 plsw;

    if (g_state.Switch_Type == 0) {
        if (PL_id) {
            plsw = p2sw_0;
        } else {
            plsw = p1sw_0;
        }
    } else {
        plsw = g_state.PLsw[PL_id][0];
    }

    sw = plsw & (SWK_START | SWK_BACK);

    if (sw == 0) {
        Reset_Status[PL_id] = 0;
        return;
    }

    switch (Reset_Status[PL_id]) {
    case 0:
        if (sw == (SWK_START | SWK_BACK)) {
            Reset_Status[PL_id] = RESET_STATUS_TRIGGERED;
            break;
        }

        if (sw & SWK_START) {
            Reset_Status[PL_id] = RESET_STATUS_PENDING;
        }

        break;

    case RESET_STATUS_PENDING:
        if (!(sw & SWK_START)) {
            Reset_Status[PL_id] = 0;
        }

        break;

    default:
        if (plsw != (SWK_START | SWK_BACK)) {
            Reset_Status[PL_id] = 0;
        }

        break;
    }
}
