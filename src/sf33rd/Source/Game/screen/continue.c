/**
 * @file continue.c
 * Manages the "Continue" screen, countdown, and player input.
 */

#include "common.h"
#include "game_state.h"
#include "port/sdl/rmlui/rmlui_continue.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"

#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect_49_work_user_character_state.h"
#include "sf33rd/Source/Game/effect/effect_58_sound_se_request.h"
#include "sf33rd/Source/Game/effect/effect_76_quake.h"
#include "sf33rd/Source/Game/effect/effect_95_data_table.h"
#include "sf33rd/Source/Game/effect/effect_a9_visual_generic.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/sound/sound_effects.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/system/work_sys.h"

#include "main.h" /* For TASK_MENU enum */
#include "port/menu_task.h"
#include "port/menu_screen.h"

#define CONTINUE_JMP_COUNT 5

u8 CONTINUE_X;

/** @brief Main continue-screen dispatcher — runs the current sub-state and returns exit flag. */
s32 Continue_Scene() {
    struct _TASK* tp = MenuTask_GetTaskPtr(); // Usually the menu task handles this, or just pass a dummy if none

    // We only initialize it the first time it is called
    if (!MenuScreen_IsActive() || (MenuScreen_IsActive() && tp->r_no[1] != MENU_SCREEN_CONTINUE)) {
        CONTINUE_X = 0;
        MenuScreen_Goto(MENU_SCREEN_CONTINUE);
        // Fake r_no[1] to track the state across ticks
        tp->r_no[1] = MENU_SCREEN_CONTINUE;
    }

    // Tick the active menu screen phase
    MenuScreen_Tick(tp);

    if (MenuScreen_GetPhase() == MENU_PHASE_EXIT) {
        MenuScreen_ExitToLegacy(tp);
        CONTINUE_X = 1;
    }

    if ((Check_Exit_Check() == 0) && (Debug_w[DEBUG_TIME_STOP] == -1)) {
        CONTINUE_X = 0;
    }

    return CONTINUE_X;
}
