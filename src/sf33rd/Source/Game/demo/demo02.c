/**
 * @file demo02.c
 * @brief Attract-mode gameplay demo sequences.
 *
 * Runs the in-game attract demo: selects characters and stage, starts
 * CPU-vs-CPU gameplay, and handles demo timeout/conclusion with screen
 * transitions and BGM fade-out.
 *
 * Part of the demo module.
 */

#include "common.h"
#include "game_state.h"
#include "main.h"
#include "port/menu_screen.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/demo/demo_states.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/rendering/mmtmcnt.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "port/menu_task.h"

void Setup_Demo_Arts();
static void Setup_Select_Demo_PL();

#define DEMO_PL_COUNT 4
#define DEMO_STAGE_COUNT 4
#define DEMO_ARTS_COUNT 8

/** @brief Top-level demo dispatcher — thin wrapper around MenuScreen registry. */
s32 Play_Demo() {
    struct _TASK* tp = MenuTask_GetTaskPtr();

    if (!MenuScreen_IsActive()) {
        g_state.Next_Demo = 0;
        MenuScreen_Goto(MENU_SCREEN_DEMO);
    }

    MenuScreen_Tick(tp);

    if (MenuScreen_GetPhase() == MENU_PHASE_EXIT) {
        MenuScreen_ExitToLegacy(tp);
        g_state.Next_Demo = 1;
    }

    return g_state.Next_Demo;
}

/** @brief Demo sub-sequence 0 — quick start: set up gameplay and run until timeout. */
void Demo_QuickStart() {
    g_state.Play_Game = 1;

    switch (g_state.demo_phase[1]) {
    case DEMO00_SETUP:
        Switch_Screen(1);
        Purge_texcash_of_list(3);
        Make_texcash_of_list(3);
        g_state.demo_phase[1] += 1;
        g_state.fsm[2] = 0;
        g_state.Game_pause = 0;
        g_state.Conclusion_Flag = 0;
        g_state.appear_type = APPEAR_TYPE_ANIMATED;
        g_state.Control_Time = 0x800;
        g_state.Round_Level = 7;
        g_state.Weak_PL = random_16() & 1;
        break;

    case DEMO00_COVER:
        Switch_Screen(1);
        Game_Fight();

        if (--g_state.Cover_Timer == 0) {
            g_state.demo_phase[1] += 1;
            Switch_Screen_Init(0);
            return;
        }

        break;

    case DEMO00_REVEAL:
        Game_Fight();

        if (Switch_Screen_Revival(0) != 0) {
            g_state.demo_phase[1] += 1;
            g_state.demo_timer_global = 1800;
            g_state.Stop_SG = 0;
            return;
        }

        break;

    case DEMO00_PLAY:
        Game_Fight();

        if (Debug_w[DEBUG_TIME_STOP] == 9) {
            g_state.demo_timer_global = 60;
        }

        if (--g_state.demo_timer_global == 1) {
            g_state.demo_phase[1] += 1;
            g_state.Stop_Combo = 1;
            return;
        }

        if (g_state.Conclusion_Flag) {
            g_state.demo_phase[1] += 1;
            g_state.Stop_Combo = 1;
            g_state.demo_timer_global = 90;
            return;
        }

        break;

    case DEMO00_WIND_DOWN:
        Game_Fight();

        if (--g_state.demo_timer_global == 0) {
            g_state.demo_phase[1] += 1;
            g_state.Game_pause = 1;
            g_state.Disappear_LOGO = 1;
            g_state.demo_timer_global = 16;
            return;
        }

        break;

    case DEMO00_PAUSE:
        Game_Fight();

        if (--g_state.demo_timer_global == 0) {
            g_state.demo_phase[1] += 1;
            Switch_Screen_Init(0);
            SsBgmFadeOut(0x800);
            return;
        }

        break;

    case DEMO00_FADE_OUT:
        Game_Fight();

        if (Switch_Screen(0) != 0) {
            g_state.demo_phase[1] += 1;
            g_state.Demo_Flag = 0;
            g_state.Present_Mode = 0;
            g_state.Cover_Timer = 23;
            BGM_Stop();

            if (++g_state.Select_Demo_Index > 3) {
                g_state.Select_Demo_Index = 0;
                return;
            }
        }

        break;

    default:
        Switch_Screen(1);
        g_state.Next_Demo = 1;
        break;
    }
}

/** @brief Demo sub-sequence 1 — full attract: character select then gameplay. */
void Demo_FullAttract() {
    if (g_state.demo_phase[1] >= 2) {
        g_state.Play_Game = 1;
    }

    switch (g_state.demo_phase[1]) {
    case DEMO01_SETUP:
        Switch_Screen(1);
        g_state.demo_phase[1] += 1;
        g_state.Game_pause = 0;
        g_state.Demo_Time_Stop = 0;
        Before_Select_Sub();
        Setup_Select_Demo_PL();
        Setup_Demo_Arts();
        g_state.Weak_PL = random_16() & 1;
        Clear_Break_Com(0);
        grade_check_work_1st_init(0, 0);
        grade_check_work_1st_init(0, 1);
        Clear_Break_Com(1);
        grade_check_work_1st_init(1, 0);
        grade_check_work_1st_init(1, 1);
        Game_CharSelect();
        break;

    case DEMO01_SELECT:
        Game_CharSelect();

        if (g_state.Demo_Time_Stop) {
            g_state.demo_phase[1] += 1;
            g_state.fsm[2] = 0;
            return;
        }

        break;

    case DEMO01_COVER:
        Switch_Screen(1);
        Game_Fight();

        if (--g_state.Cover_Timer == 0) {
            g_state.demo_phase[1] += 1;
            Switch_Screen_Init(0);
            return;
        }

        break;

    case DEMO01_REVEAL:
        Game_Fight();

        if (Switch_Screen_Revival(0) != 0) {
            g_state.demo_phase[1] += 1;
            g_state.demo_timer_global = 1200;
            g_state.Stop_SG = 0;
            return;
        }

        break;

    case DEMO01_PLAY:
        Game_Fight();

        if (--g_state.demo_timer_global == 1) {
            g_state.Stop_Combo = 1;
            g_state.Disappear_LOGO = 1;
            return;
        }

        if (!g_state.demo_timer_global) {
            g_state.demo_phase[1] += 1;
            g_state.demo_timer_global = 16;
            g_state.Demo_Time_Stop = 1;
            g_state.Game_pause = 1;
            return;
        }

        break;

    case DEMO01_PAUSE:
        Game_Fight();

        if (--g_state.demo_timer_global == 0) {
            g_state.demo_phase[1] += 1;
            Switch_Screen_Init(0);
            SsBgmFadeOut(0x800);
            return;
        }

        break;

    case DEMO01_FADE_OUT:
        Game_Fight();

        if (Switch_Screen(0) != 0) {
            g_state.demo_phase[1] += 1;
            g_state.Cover_Timer = 23;
            BGM_Stop();
            return;
        }

        break;

    default:
        g_state.Next_Demo = 1;
        break;
    }
}

const s8 Demo_PL_Play_Data[4][2] = { { 15, 19 }, { 11, 18 }, { 2, 16 }, { 12, 8 } };
const u8 Arts_Rnd_Demo_Data[8] = { 0, 0, 0, 1, 1, 1, 2, 2 };
const s8 Demo_Stage_Play_Data[4][2] = { { 15, 19 }, { 11, 18 }, { 2, 16 }, { 12, 8 } };
const s8 Demo_PL_Data[4] = { 0, 1, 0, 1 };

/** @brief Select demo characters from a predefined roster (with debug overrides). */
void Setup_Demo_PL() {
    if (g_state.Demo_PL_Index < 0 || g_state.Demo_PL_Index >= DEMO_PL_COUNT) {
        g_state.Demo_PL_Index = 0;
    }
    g_state.My_char[0] = Demo_PL_Play_Data[g_state.Demo_PL_Index][0];
    g_state.My_char[1] = Demo_PL_Play_Data[g_state.Demo_PL_Index][1];

    if (Debug_w[DEBUG_MY_CHAR_PL1]) {
        g_state.My_char[0] = Debug_w[DEBUG_MY_CHAR_PL1] - 1;
    }

    if (Debug_w[DEBUG_MY_CHAR_PL2]) {
        g_state.My_char[1] = Debug_w[DEBUG_MY_CHAR_PL2] - 1;
    }

    init_omop();
}

/** @brief Assign random super arts and default colors for demo players. */
void Setup_Demo_Arts() {
    g_state.Super_Arts[0] = Arts_Rnd_Demo_Data[random_16() & 7];
    g_state.Super_Arts[1] = Arts_Rnd_Demo_Data[random_16() & 7];
    g_state.Player_Color[0] = 0;
    g_state.Player_Color[1] = 0;
}

/** @brief Select a demo stage from the predefined roster and advance the index. */
void Setup_Demo_Stage() {
    s16 rnd = random_16() & 1;

    if (g_state.Demo_Stage_Index < 0 || g_state.Demo_Stage_Index >= DEMO_STAGE_COUNT) {
        g_state.Demo_Stage_Index = 0;
    }
    g_state.bg_w.area = 0;
    g_state.bg_w.stage = Demo_Stage_Play_Data[g_state.Demo_Stage_Index][rnd];
    g_state.Demo_Stage_Index += 1;

    if (++g_state.Demo_PL_Index > 3) {
        g_state.Demo_PL_Index = 0;
        g_state.Demo_Stage_Index = 0;
    }
}

/** @brief Configure which player is human-controlled in the current demo. */
static void Setup_Select_Demo_PL() {
    g_state.plw[0].wu.pl_operator = 0;
    g_state.plw[1].wu.pl_operator = 0;
    g_state.Operator_Status[0] = 0;
    g_state.Operator_Status[1] = 0;
    if (g_state.Select_Demo_Index < 0 || g_state.Select_Demo_Index >= DEMO_PL_COUNT) {
        g_state.Select_Demo_Index = 0;
    }
    g_state.plw[Demo_PL_Data[g_state.Select_Demo_Index]].wu.pl_operator = 1;
    g_state.Operator_Status[Demo_PL_Data[g_state.Select_Demo_Index]] = 1;
}
