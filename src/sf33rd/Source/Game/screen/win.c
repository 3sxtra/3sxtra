/**
 * @file win.c
 * Win Screen
 */

#include "common.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui/rmlui_win_screen.h"

#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/eff58.h"
#include "sf33rd/Source/Game/effect/eff76.h"
#include "sf33rd/Source/Game/effect/effb8.h"
#include "sf33rd/Source/Game/effect/effl1.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/rendering/mmtmcnt.h"
#include "sf33rd/Source/Game/rendering/texgroup.h"
#include "sf33rd/Source/Game/screen/sel_data.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sys_sub2.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/debug/Debug.h"

#include "main.h" /* For TASK_MENU enum */
#include "port/menu_screen.h"

void Setup_Wins_OBJ();

#define WIN_JMP_COUNT 6

u8 WIN_X;

/** @brief Main winner-screen dispatcher — runs the current phase and returns exit flag. */
s32 Winner_Scene() {
    struct _TASK* tp = &task[TASK_MENU];

    if (Break_Into) {
        return 0;
    }

    // Initialize MenuScreen
    if (!MenuScreen_IsActive() || (MenuScreen_IsActive() && tp->r_no[1] != MENU_SCREEN_WIN)) {
        WIN_X = 0;
        MenuScreen_Goto(MENU_SCREEN_WIN);
        tp->r_no[1] = MENU_SCREEN_WIN;
    }

    Scene_Cut = Cut_Cut_Cut();
    MenuScreen_Tick(tp);

    if (MenuScreen_GetPhase() == MENU_PHASE_EXIT) {
        MenuScreen_ExitToLegacy(tp);
        WIN_X = 1;
    }

    if ((Check_Exit_Check() == 0) && (Debug_w[DEBUG_TIME_STOP] == -1)) {
        WIN_X = 0;
    }

    return WIN_X;
}

/** @brief Main loser-screen dispatcher — shares phases with Winner_Scene but uses Lose_2nd/3rd. */
s32 Loser_Scene() {
    struct _TASK* tp = &task[TASK_MENU];

    if (Break_Into) {
        return 0;
    }

    // Initialize MenuScreen
    if (!MenuScreen_IsActive() || (MenuScreen_IsActive() && tp->r_no[1] != MENU_SCREEN_LOSER)) {
        WIN_X = 0;
        MenuScreen_Goto(MENU_SCREEN_LOSER);
        tp->r_no[1] = MENU_SCREEN_LOSER;
    }

    Scene_Cut = Cut_Cut_Loser();
    MenuScreen_Tick(tp);

    if (MenuScreen_GetPhase() == MENU_PHASE_EXIT) {
        MenuScreen_ExitToLegacy(tp);
        WIN_X = 1;
    }

    if ((Check_Exit_Check() == 0) && (Debug_w[DEBUG_TIME_STOP] == -1)) {
        WIN_X = 0;
    }

    return WIN_X;
}

/** @brief Spawn win-streak display objects ("1st WIN", "2nd WIN", etc.) based on current mode. */
void Setup_Wins_OBJ() {
    if (Mode_Type == MODE_VERSUS) {
        WGJ_Win = VS_Win_Record[Winner_id];
    } else {
        WGJ_Win = Win_Record[Winner_id];
    }

    if ((WGJ_Win == 0) || (Mode_Type == MODE_NETWORK)) {
        return;
    }

    effect_L1_init(0);

    if (WGJ_Win > 1) {
        spawn_effect_76(0x2F, 3, 1);
        spawn_effect_76(0x31, 3, 1);
    } else {
        spawn_effect_76(0x2E, 3, 1);
        spawn_effect_76(0x30, 3, 1);
    }
}
