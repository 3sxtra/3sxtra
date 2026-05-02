/**
 * @file demo01.c
 * @brief Title screen and attract-mode title sequences.
 *
 * Manages the title screen flow: loading, BGM standby, opening demo
 * playback, and screen transitions. Also handles the quick "dash" title
 * used when returning from attract demos.
 *
 * Part of the demo module.
 */

#include "common.h"
#include "game_state.h"
#include "main.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/demo/demo_states.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/opening/op_sub.h"
#include "sf33rd/Source/Game/opening/opening.h"

#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/system/sys_sub.h"

/** @brief Title screen state machine — load, play opening, fade to game. */
s16 Title() {
    s16 xx = 0;

    // njSetBackColor(0, 0, 0);

    switch (g_state.D_No[1]) {
    case TITLE_WAIT_LOAD:
        if (Check_LDREQ_Clear() != 0) {
            Standby_BGM(0x34);
            g_state.D_No[1] += 1;
            g_state.D_Timer = 20;
        }

        break;

    case TITLE_PLAY_OPENING:
        if (g_state.D_Timer != 0) {
            g_state.D_Timer -= 1;
        } else if (opening_demo()) {
            g_state.D_No[1] += 1;
            g_state.D_Timer = 40;
        }

        break;

    case TITLE_PRE_TRANSITION:
        opening_demo();

        if (--g_state.D_Timer == 0) {
            g_state.D_No[1] += 1;
            Switch_Screen_Init(1);
        }

        break;

    case TITLE_TRANSITION:
        opening_demo();

        if (Switch_Screen(1) != 0) {
            g_state.D_No[1] += 1;
            g_state.Cover_Timer = 20;
        }

        break;

    case TITLE_DONE:
        Switch_Screen(1);
        g_state.D_No[1] += 1;
        g_state.D_Timer = 2;
        break;

    default:
        Switch_Screen(1);

        if (--g_state.D_Timer == 0) {
            TexRelease(0x259);
            xx = 1;
        }

        break;
    }

    return xx;
}

/** @brief Quick title screen — skip loading, show title briefly and return. */
s16 Title_At_a_Dash() {
    s16 xx = 0;

    BGM_Stop();
    Disp_Copyright();

    switch (g_state.D_No[1]) {
    case TITLE_DASH_INIT:
        g_state.D_No[1] += 1;
        g_state.D_Timer = 30;

        if (!title_tex_flag) {
            TITLE_Init();
        }

        break;

    case TITLE_DASH_SHOW:
        if (--g_state.D_Timer == 0) {
            g_state.D_No[1] += 1;
        }

        TITLE_Move(1);
        break;

    default:
        xx = 1;
        TITLE_Move(1);
        break;
    }

    return xx;
}
