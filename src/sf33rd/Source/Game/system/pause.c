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
#include "port/sdl/rmlui_phase3_toggles.h"
extern bool use_rmlui;

#define PAUSE_JMP_COUNT 4
#define FLASH_PAUSE_JMP_COUNT 5

u8 PAUSE_X;

void Pause_Task(struct _TASK* task_ptr);

static void Pause_Check(struct _TASK* task_ptr);
static void Pause_Move(struct _TASK* task_ptr);
static void Pause_Sleep(struct _TASK* /* unused */);
static void Pause_Die(struct _TASK* /* unused */);

static void Flash_Pause(struct _TASK* task_ptr);

static void Flash_Pause_Sleep(struct _TASK* /* unused */);
static void Flash_Pause_1st(struct _TASK* task_ptr);
static void Flash_Pause_2nd(struct _TASK* task_ptr);
static void Flash_Pause_3rd(struct _TASK* /* unused */);
static void Flash_Pause_4th(struct _TASK* task_ptr);

static s32 Check_Pause_Term(u16 sw, u8 PL_id);
static void Exit_Pause(struct _TASK* task_ptr);
static void Setup_Pause(struct _TASK* task_ptr);
static void Setup_Come_Out(struct _TASK* task_ptr);
static s32 Check_Play_Status(s16 PL_id);

/** @brief Main pause task entry point — dispatches sub-states and flash effects. */
void Pause_Task(struct _TASK* task_ptr) {
    void (*Main_Jmp_Tbl[PAUSE_JMP_COUNT])(struct _TASK*) = { Pause_Check, Pause_Move, Pause_Sleep, Pause_Die };

    if (!nowSoftReset() && Mode_Type != MODE_NETWORK && Mode_Type != MODE_NORMAL_TRAINING &&
        Mode_Type != MODE_PARRY_TRAINING && Mode_Type != MODE_TRIALS) {
        if (task_ptr->r_no[0] < PAUSE_JMP_COUNT) {
            Main_Jmp_Tbl[task_ptr->r_no[0]](task_ptr);
        }
        Flash_Pause(task_ptr);
    }
}

/** @brief Check both players for pause/controller-disconnect input and transition accordingly. */
static void Pause_Check(struct _TASK* task_ptr) {
    PAUSE_X = 0;

    if (Check_Pause_Term(~PLsw[0][1] & PLsw[0][0], 0) == 0) {
        Check_Pause_Term(~PLsw[1][1] & PLsw[1][0], 1);
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

/** @brief Pause active state — wait for the menu system to signal exit. */
static void Pause_Move(struct _TASK* task_ptr) {
    if (Exit_Menu) {
        Exit_Pause(task_ptr);
    }
}

/** @brief Pause sleep state (no-op). */
static void Pause_Sleep(struct _TASK* /* unused */) {};

/** @brief Pause die state (no-op). */
static void Pause_Die(struct _TASK* /* unused */) {};

/** @brief Dispatch the flash-pause sub-state for displaying pause overlay messages. */
static void Flash_Pause(struct _TASK* task_ptr) {
    void (*Flash_Jmp_Tbl[FLASH_PAUSE_JMP_COUNT])(
        struct _TASK*) = { Flash_Pause_Sleep, Flash_Pause_1st, Flash_Pause_2nd, Flash_Pause_3rd, Flash_Pause_4th };

    if (Pause_Down != 0) {
        if (task_ptr->r_no[2] < FLASH_PAUSE_JMP_COUNT) {
            Flash_Jmp_Tbl[task_ptr->r_no[2]](task_ptr);
        }
    }
}

/** @brief Flash pause sleep state (no-op). */
static void Flash_Pause_Sleep(struct _TASK* /* unused */) {}

/** @brief Flash pause 1st phase — initial delay before showing the pause message. */
static void Flash_Pause_1st(struct _TASK* task_ptr) {
    if (--task_ptr->free[0] == 0) {
        task_ptr->r_no[2] = 2;
        task_ptr->free[0] = 60;
    }
}

/** @brief Flash pause 2nd phase — display the "1P PAUSE" or "2P PAUSE" text. */
static void Flash_Pause_2nd(struct _TASK* task_ptr) {
    if (--task_ptr->free[0]) {
        if (!use_rmlui || !rmlui_screen_pause) {
            if (Pause_ID == 0) {
                SSPutStr2(20, 9, 9, "1P PAUSE");
            } else {
                SSPutStr2(20, 9, 9, "2P PAUSE");
            }
        }
        return;
    }

    task_ptr->r_no[2] = 1;
    task_ptr->free[0] = 30;
}

/** @brief Flash pause 3rd phase (no-op). */
static void Flash_Pause_3rd(struct _TASK* /* unused */) {}

/** @brief Flash pause 4th phase — handle controller-disconnected state. */
static void Flash_Pause_4th(struct _TASK* task_ptr) {
    if (Interface_Type[Pause_ID] == 0) {
        dispControllerWasRemovedMessage(0x84, 0x52, 0x10);
        return;
    }

    Pause_Type = 1;
    Setup_Pause(task_ptr);
}

/** @brief Display the "Please reconnect the controller" message at the given screen position. */
void dispControllerWasRemovedMessage(s32 x, s32 y, s32 step) {
    if (use_rmlui && rmlui_screen_pause)
        return;
    SSPutStrPro(0, x, y, 9, -1, "Please reconnect");
    SSPutStrPro(0, x, (y + step), 9, -1, "the controller to");

    if (Pause_ID) {
        SSPutStrPro(0, x, (y + (step * 2)), 9, -1, "controller port 2.");
        return;
    }

    SSPutStrPro(0, x, (y + (step * 2)), 9, -1, "controller port 1.");
}

/** @brief Evaluate whether pause conditions are met for the given player/input; sets PAUSE_X on match. */
static s32 Check_Pause_Term(u16 sw, u8 PL_id) {
    if (Demo_Flag == 0) {
        return 0;
    }

    if (Allow_a_battle_f == 0 || Extra_Break != 0) {
        return 0;
    }

    if (vm_w.Access != 0 || vm_w.Request != 0) {
        return PAUSE_X = 0;
    }

    if (Exec_Wipe) {
        return 0;
    }

    Pause_ID = PL_id;

    if (Check_Play_Status(PL_id) == 0) {
        return 0;
    }

    if (sw & SWK_START) {
        Pause_Type = 1;
        return PAUSE_X = 1;
    }

    if (Present_Mode == 3) {
        if (Interface_Type[Decide_ID] == 0) {
            Pause_ID = Decide_ID;
            Pause_Type = 2;
            return PAUSE_X = 2;
        }
    } else if (Interface_Type[PL_id] == 0 && plw[PL_id].wu.pl_operator) {
        Pause_Type = 2;
        return PAUSE_X = 2;
    }

    return 0;
}

/** @brief Exit the pause state: restore audio, clear flags, and kill the menu/saver tasks. */
static void Exit_Pause(struct _TASK* task_ptr) {
    u8 ix;

    if (Present_Mode != 3 && Check_Pause_Term(0, Pause_ID ^ 1)) {
        Exit_Menu = 0;
        return;
    }

    SE_selected();
    Game_pause = 0;
    Pause = 0;
    Pause_Down = 0;

    for (ix = 0; ix < 4; ix++) {
        task_ptr->r_no[ix] = 0;
        task_ptr->free[ix] = 0;
    }

    Menu_Suicide[0] = 1;
    Menu_Suicide[1] = 1;
    Menu_Suicide[2] = 1;
    Menu_Suicide[3] = 1;
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
static void setup_pause_common(struct _TASK* task_ptr, u8 flash_phase) {
    s16 ix;

    SE_selected();
    Pause_Down = 1;
    Game_pause = 0x81;
    task_ptr->r_no[0] = 1;
    task_ptr->r_no[2] = flash_phase;
    task_ptr->free[0] = 1;
    cpReadyTask(TASK_MENU, Menu_Task);
    task[TASK_MENU].r_no[0] = 1;
    Exit_Menu = 0;

    for (ix = 0; ix < 4; ix++) {
        Menu_Suicide[ix] = 0;
    }

    Order[0x8A] = 3;
    Order_Timer[0x8A] = 1;
    effect_66_init(0x8A, 9, 2, 7, -1, -1, -0x3FFC);
    SsBgmHalfVolume(1);
    spu_all_off();
}

/** @brief Enter the standard pause state: freeze game, launch pause menu, dim BGM. */
static void Setup_Pause(struct _TASK* task_ptr) {
    setup_pause_common(task_ptr, 1);
}

/** @brief Enter the controller-disconnected pause state (similar to Setup_Pause but with phase 4 flash). */
static void Setup_Come_Out(struct _TASK* task_ptr) {
    setup_pause_common(task_ptr, 4);
}

/** @brief Check whether the player is active in the current round (always 1 in VS mode). */
static s32 Check_Play_Status(s16 PL_id) {
    if (Mode_Type != MODE_VERSUS) {
        return Round_Operator[PL_id];
    }

    return 1;
}
