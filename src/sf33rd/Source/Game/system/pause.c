/**
 * @file pause.c
 * @brief Game pause/unpause state machine.
 *
 * Handles detecting the pause button, entering/exiting pause state,
 * displaying the "1P PAUSE" / "2P PAUSE" flash messages, and
 * controller-disconnected notifications.
 *
 * Part of the system module.
 * Originally from the PS2 pause module.
 */

#include "sf33rd/Source/Game/system/pause.h"
#include "game_state.h"
#include "port/menu_task.h"
#include "port/task_api.h"
#include "common.h"
#include "main.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/effect/eff66.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/menu/menu.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/system/reset.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

/* RmlUi Phase 3 bypass */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"

/** @brief Top-level pause task states (replaces Main_Jmp_Tbl indices). */
typedef enum {
    PAUSE_CHECK = 0, /**< Polling both players for pause/disconnect input */
    PAUSE_MOVE = 1,  /**< g_state.Pause active � waiting for menu to signal exit  */
    PAUSE_SLEEP = 2, /**< Sleeping (no-op)                                */
    PAUSE_DIE = 3,   /**< Dead (no-op)                                    */
    PAUSE_STATE_COUNT
} PauseState;

/** @brief Flash overlay sub-states (replaces Flash_Jmp_Tbl indices). */
typedef enum {
    FLASH_SLEEP = 0, /**< Idle � no flash overlay active           */
    FLASH_1ST = 1,   /**< Initial delay before showing pause text  */
    FLASH_2ND = 2,   /**< Displaying "1P PAUSE" / "2P PAUSE" text  */
    FLASH_3RD = 3,   /**< No-op (killed / gap state)               */
    FLASH_4TH = 4,   /**< Controller-disconnected state            */
    FLASH_STATE_COUNT
} FlashPauseState;

#define PAUSE_HOLD_FRAMES 150 /**< Start must be held this many frames to pause (2.5s � 60fps) */

u8 PAUSE_X;
static u16 start_hold_frames[2]; /**< Per-player frame counter for Start hold */

void Pause_Task(struct _TASK* task_ptr);

static void Pause_Check(struct _TASK* task_ptr);
static void Pause_Move(struct _TASK* task_ptr);
static void Pause_Sleep(struct _TASK* unused1);
static void Pause_Die(struct _TASK* unused1);

static void Flash_Pause(struct _TASK* task_ptr);

static void Flash_Pause_Sleep(struct _TASK* unused1);
static void Flash_Pause_1st(struct _TASK* task_ptr);
static void Flash_Pause_2nd(struct _TASK* task_ptr);
static void Flash_Pause_3rd(struct _TASK* unused1);
static void Flash_Pause_4th(struct _TASK* task_ptr);

static s32 Check_Pause_Term(u16 sw, u8 PL_id);
static void Exit_Pause(struct _TASK* task_ptr);
static void Setup_Pause(struct _TASK* task_ptr);
static void Setup_Come_Out(struct _TASK* task_ptr);
static s32 Check_Play_Status(s16 PL_id);

/** @brief Main pause task entry point � dispatches sub-states and flash effects. */
void Pause_Task(struct _TASK* task_ptr) {
    if (!nowSoftReset() && g_state.Mode_Type != MODE_NETWORK && g_state.Mode_Type != MODE_NORMAL_TRAINING &&
        g_state.Mode_Type != MODE_PARRY_TRAINING && g_state.Mode_Type != MODE_TRIALS) {
        switch ((PauseState)task_ptr->r_no[0]) {
        case PAUSE_CHECK:
            Pause_Check(task_ptr);
            break;
        case PAUSE_MOVE:
            Pause_Move(task_ptr);
            break;
        case PAUSE_SLEEP:
            Pause_Sleep(task_ptr);
            break;
        case PAUSE_DIE:
            Pause_Die(task_ptr);
            break;
        default:
            break;
        }
        Flash_Pause(task_ptr);
    }
}

/** @brief Check both players for pause/controller-disconnect input and transition accordingly. */
static void Pause_Check(struct _TASK* task_ptr) {
    PAUSE_X = 0;

    if (Check_Pause_Term(g_state.PLsw[0][0], 0) == 0) {
        Check_Pause_Term(g_state.PLsw[1][0], 1);
    }

    switch (PAUSE_X) {
    case 1:
        Setup_Pause(task_ptr);
        break;

    case 2:
        Setup_Come_Out(task_ptr);
        break;
    }
}

/** @brief g_state.Pause active state � wait for the menu system to signal exit. */
static void Pause_Move(struct _TASK* task_ptr) {
    if (g_state.Exit_Menu) {
        Exit_Pause(task_ptr);
    }
}

/** @brief g_state.Pause sleep state (no-op). */
static void Pause_Sleep(struct _TASK* unused1) {};

/** @brief g_state.Pause die state (no-op). */
static void Pause_Die(struct _TASK* unused1) {};

/** @brief Dispatch the flash-pause sub-state for displaying pause overlay messages. */
static void Flash_Pause(struct _TASK* task_ptr) {
    if (g_state.Pause_Down != 0) {
        switch ((FlashPauseState)task_ptr->r_no[2]) {
        case FLASH_SLEEP:
            Flash_Pause_Sleep(task_ptr);
            break;
        case FLASH_1ST:
            Flash_Pause_1st(task_ptr);
            break;
        case FLASH_2ND:
            Flash_Pause_2nd(task_ptr);
            break;
        case FLASH_3RD:
            Flash_Pause_3rd(task_ptr);
            break;
        case FLASH_4TH:
            Flash_Pause_4th(task_ptr);
            break;
        default:
            break;
        }
    }
}

/** @brief Flash pause sleep state (no-op). */
static void Flash_Pause_Sleep(struct _TASK* unused1) {}

/** @brief Flash pause 1st phase � initial delay before showing the pause message. */
static void Flash_Pause_1st(struct _TASK* task_ptr) {
    if (--task_ptr->free[0] == 0) {
        task_ptr->r_no[2] = FLASH_2ND;
        task_ptr->free[0] = 60;
    }
}

/** @brief Flash pause 2nd phase � display the "1P PAUSE" or "2P PAUSE" text. */
static void Flash_Pause_2nd(struct _TASK* task_ptr) {
    if (--task_ptr->free[0]) {
        if (!use_rmlui || !rmlui_screen_pause) {
            if (g_state.Pause_ID == 0) {
                SSPutStr2(20, 9, 9, "1P PAUSE");
            } else {
                SSPutStr2(20, 9, 9, "2P PAUSE");
            }
        }
        return;
    }

    task_ptr->r_no[2] = FLASH_1ST;
    task_ptr->free[0] = 30;
}

/** @brief Flash pause 3rd phase (no-op). */
static void Flash_Pause_3rd(struct _TASK* unused1) {}

/** @brief Flash pause 4th phase � handle controller-disconnected state. */
static void Flash_Pause_4th(struct _TASK* task_ptr) {
    if (Interface_Type[g_state.Pause_ID] == 0) {
        dispControllerWasRemovedMessage(0x84, 0x52, 0x10);
        return;
    }

    g_state.Pause_Type = 1;
    Setup_Pause(task_ptr);
}

/** @brief Display the "Please reconnect the controller" message at the given screen position. */
void dispControllerWasRemovedMessage(s32 x, s32 y, s32 step) {
    if (use_rmlui && rmlui_screen_pause)
        return;
    SSPutStrPro(0, x, y, 9, -1, "Please reconnect");
    SSPutStrPro(0, x, (y + step), 9, -1, "the controller to");

    if (g_state.Pause_ID) {
        SSPutStrPro(0, x, (y + (step * 2)), 9, -1, "controller port 2.");
        return;
    }

    SSPutStrPro(0, x, (y + (step * 2)), 9, -1, "controller port 1.");
}

/** @brief Evaluate whether pause conditions are met for the given player/input; sets PAUSE_X on match. */
static s32 Check_Pause_Term(u16 sw, u8 PL_id) {
    if (g_state.Demo_Flag == 0) {
        return 0;
    }

    if (g_state.Allow_a_battle_f == 0 || g_state.Extra_Break != 0) {
        return 0;
    }

    if (g_state.vm_w.Access != 0 || g_state.vm_w.Request != 0) {
        return PAUSE_X = 0;
    }

    if (g_state.Exec_Wipe) {
        return 0;
    }

    g_state.Pause_ID = PL_id;

    if (Check_Play_Status(PL_id) == 0) {
        return 0;
    }

    if (sw & SWK_START) {
        if (++start_hold_frames[PL_id] >= PAUSE_HOLD_FRAMES) {
            start_hold_frames[PL_id] = 0;
            g_state.Pause_Type = 1;
            return PAUSE_X = 1;
        }
        return 0;
    } else {
        start_hold_frames[PL_id] = 0;
    }

    // This skips checking controller connection status during gameplay testing
    if (configuration.test.enabled) {
        return 0;
    }

    if (g_state.Present_Mode == 3) {
        if (Interface_Type[g_state.Decide_ID] == 0) {
            g_state.Pause_ID = g_state.Decide_ID;
            g_state.Pause_Type = 2;
            return PAUSE_X = 2;
        }
    } else if (Interface_Type[PL_id] == 0 && g_state.plw[PL_id].wu.pl_operator) {
        g_state.Pause_Type = 2;
        return PAUSE_X = 2;
    }

    return 0;
}

/** @brief Exit the pause state: restore audio, clear flags, and kill the menu/saver tasks. */
static void Exit_Pause(struct _TASK* task_ptr) {
    u8 ix;

    if (g_state.Present_Mode != 3 && Check_Pause_Term(0, g_state.Pause_ID ^ 1)) {
        g_state.Exit_Menu = 0;
        return;
    }

    SE_selected();
    g_state.Game_pause = 0;
    g_state.Pause = 0;
    g_state.Pause_Down = 0;
    start_hold_frames[0] = 0;
    start_hold_frames[1] = 0;

    for (ix = 0; ix < 4; ix++) {
        task_ptr->r_no[ix] = 0;
        task_ptr->free[ix] = 0;
    }

    g_state.Menu_Suicide[0] = 1;
    g_state.Menu_Suicide[1] = 1;
    g_state.Menu_Suicide[2] = 1;
    g_state.Menu_Suicide[3] = 1;
    pulpul_request_again();
    cpExitTask(TASK_SAVER);
    cpExitTask(TASK_MENU);
    SsBgmHalfVolume(0);
}

/**
 * @brief Common pause-entry setup shared by Setup_Pause and Setup_Come_Out.
 *
 * @param task_ptr    The pause task.
 * @param flash_phase Flash sub-state: 1 = standard pause text, 4 = controller-disconnected.
 */
static void setup_pause_common(struct _TASK* task_ptr, FlashPauseState flash_phase) {
    s16 ix;

    SE_selected();
    g_state.Pause_Down = 1;
    g_state.Game_pause = 0x81;
    task_ptr->r_no[0] = PAUSE_MOVE;
    task_ptr->r_no[2] = (u8)flash_phase;
    task_ptr->free[0] = 1;
    cpReadyTask(TASK_MENU, Menu_Task);
    /* MTP_IDLE (1) tells the menu task to re-enter at its "idle" handler.
     * Note: value 1 is also used internally by After_Title() as a jump-table
     * index, but when written from here it means "resume from pause exit." */
    MenuTask_SetPhase(MTP_IDLE);
    g_state.Exit_Menu = 0;

    for (ix = 0; ix < 4; ix++) {
        g_state.Menu_Suicide[ix] = 0;
    }

    g_state.Order[0x8A] = 3;
    g_state.Order_Timer[0x8A] = 1;
    effect_66_init(0x8A, 9, 2, 7, -1, -1, -0x3FFC);
    SsBgmHalfVolume(1);
    spu_all_off();
}

/** @brief Enter the standard pause state: freeze game, launch pause menu, dim BGM. */
static void Setup_Pause(struct _TASK* task_ptr) {
    setup_pause_common(task_ptr, FLASH_1ST);
}

/** @brief Enter the controller-disconnected pause state (similar to Setup_Pause but with phase 4 flash). */
static void Setup_Come_Out(struct _TASK* task_ptr) {
    setup_pause_common(task_ptr, FLASH_4TH);
}

/* ══════════════════════════════════════════════════════════════════════════�
 *  Accessor functions � decouple menu_input.c from TASK_PAUSE internals
 * ══════════════════════════════════════════════════════════════════════════�
 */

/** @brief Set the flash overlay sub-state (used by menu_input.c). */
void Pause_SetFlashPhase(u8 phase) {
    PauseTask_SetPhase(phase);
}

/** @brief Set the flash overlay timer (used by menu_input.c). */
void Pause_SetFlashTimer(u8 timer) {
    PauseTask_SetTimer(timer);
}

/** @brief Kill the flash overlay by setting it to the no-op state (used by menu_input.c exit path). */
void Pause_KillFlash(void) {
    PauseTask_SetPhase(FLASH_3RD);
}

/** @brief Check whether the player is active in the current round (always 1 in VS mode). */
static s32 Check_Play_Status(s16 PL_id) {
    if (g_state.Mode_Type != MODE_VERSUS) {
        return g_state.Round_Operator[PL_id];
    }

    return 1;
}
