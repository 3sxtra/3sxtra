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
#include "netplay/netplay.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/rendering/texgroup.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "structs.h"

#define RESET_JMP_COUNT 4
#define RESET_STATUS_PENDING 0x62
#define RESET_STATUS_TRIGGERED 0x63

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
    void (*Main_Jmp_Tbl[RESET_JMP_COUNT])() = { Reset_Init, Reset_Move, Reset_Wait, Reset_Sleep };
    Check_Reset_IO(task_ptr, 0);
    Check_Reset_IO(task_ptr, 1);
    if (task_ptr->r_no[0] < RESET_JMP_COUNT) {
        Main_Jmp_Tbl[task_ptr->r_no[0]](task_ptr);
    }
}

/** @brief Reset init state — advance to move state and clear reset flag. */
static void Reset_Init(struct _TASK* task_ptr) {
    task_ptr->r_no[0] += 1;
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
        task_ptr->r_no[0] = 2;
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
        task_ptr->r_no[0] += 1;
        break;
    }
}

/** @brief Reset sleep state — wait for the reset button to be released before reinitializing. */
static void Reset_Sleep(struct _TASK* task_ptr) {
    ToneDown(0xFF, 0);

    if (Pause_ID == 0) {
        if (!(p1sw_0 & 0x4000)) {
            task_ptr->r_no[0] = 0;
        }
    } else if (!(p2sw_0 & 0x4000)) {
        task_ptr->r_no[0] = 0;
    }

    if (task_ptr->r_no[0] == 0) {
        checkAdxFileLoaded();
        checkSelObjFileLoaded();
    }
}

/** @brief Evaluate soft-reset conditions across both players (respects Forbid_Reset). */
static void Check_Reset() {
    if (Forbid_Reset) {
        RESET_X = 0;
        return;
    }

    if (Netplay_IsEnabled()) {
        RESET_X = 0;
        return;
    }

    Switch_Type = 1;

    if (Check_SoftReset(0) == 0) {
        Check_SoftReset(1);
    }
}

/** @brief Check whether the given player has entered the soft-reset button sequence. */
static u8 Check_SoftReset(s16 PL_id) {
    if (Reset_Status[PL_id] == RESET_STATUS_TRIGGERED) {
        Game_pause = 0x81;
        Pause_ID = PL_id;
        return RESET_X = 1;
    }

    return RESET_X = 0;
}

/** @brief Determine the next disposal type after a reset (bootrom return vs. normal restart). */
static s32 Setup_Next_Disposal() {
    if (Reset_Bootrom) {
        return 1;
    }

    if ((G_No[0] == 1) || ((G_No[0] == 2) && (G_No[1] == 0))) {
        return 1;
    }

    return 0;
}

/** @brief Track the Start/Back button state machine for reset detection on the given player. */
static void Check_Reset_IO(struct _TASK* /* unused */, s16 PL_id) {
    u16 sw;
    u16 plsw;

    if (Switch_Type == 0) {
        if (PL_id) {
            plsw = p2sw_0;
        } else {
            plsw = p1sw_0;
        }
    } else {
        plsw = PLsw[PL_id][0];
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
