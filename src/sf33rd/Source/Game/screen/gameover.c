/**
 * @file gameover.c
 * Game Over screen
 */

#include "common.h"
#include "game_state.h"
#include "port/sdl/rmlui/rmlui_gameover.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"

#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/eff58.h"
#include "sf33rd/Source/Game/effect/eff76.h"
#include "sf33rd/Source/Game/effect/effa9.h"
#include "sf33rd/Source/Game/effect/effl1.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/screen/sel_data.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

#include "main.h" /* For TASK_MENU enum */
#include "port/menu_task.h"
#include "port/menu_screen.h"

#define GAMEOVER_JMP_COUNT 3

u8 GAME_OVER_X;

/** @brief Main game-over dispatcher — runs the current sub-state and returns exit flag. */
s16 Game_Over() {
    struct _TASK* tp = MenuTask_GetTaskPtr(); // Menu task usually handles UI

    // Initialize MenuScreen
    if (!MenuScreen_IsActive() || (MenuScreen_IsActive() && tp->r_no[1] != MENU_SCREEN_GAMEOVER)) {
        GAME_OVER_X = 0;
        MenuScreen_Goto(MENU_SCREEN_GAMEOVER);
        tp->r_no[1] = MENU_SCREEN_GAMEOVER;
    }

    g_state.Scene_Cut = Cut_Cut_Loser();
    MenuScreen_Tick(tp);

    if (MenuScreen_GetPhase() == MENU_PHASE_EXIT) {
        MenuScreen_ExitToLegacy(tp);
        GAME_OVER_X = 1;
    }

    if ((Check_Exit_Check() == 0) && (Debug_w[DEBUG_TIME_STOP] == -1)) {
        GAME_OVER_X = 0;
    }

    if (g_state.Break_Into) {
        return 0;
    }

    return GAME_OVER_X;
}
