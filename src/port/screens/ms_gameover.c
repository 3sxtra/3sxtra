/**
 * @file ms_gameover.c
 * @brief MenuScreen registry integration for the Game Over screen.
 *
 * Implements the on_enter, on_tick, and on_exit callbacks for
 * MENU_SCREEN_GAMEOVER.
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/effect/eff58.h"
#include "sf33rd/Source/Game/effect/eff76.h"
#include "sf33rd/Source/Game/effect/effa9.h"
#include "sf33rd/Source/Game/effect/effl1.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/screen/sel_data.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

#include "port/sdl/rmlui/rmlui_gameover.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"

extern u8 GAME_OVER_X;

static void Setup_Result_OBJ(void);

static void ms_gameover_enter(struct _TASK* tp) {
    tp->free[0] = 0; /* Match GO_No[0] phase */
    tp->free[1] = 0; /* Match GO_No[1] phase */

    /* Same as GameOver_1st case 0 */
    Unsubstantial_BG[3] = 1;
    Target_BG_X[3] = bg_w.bgw[3].wxy[0].disp.pos + 466;
    Offset_BG_X[3] = 0;
    Target_BG_X[1] = bg_w.bgw[1].wxy[0].disp.pos + 458;
    Offset_BG_X[1] = 0;
    bg_mvxy.a[0].sp = 0xE0000;
    bg_mvxy.d[0].sp = 0;
    if (use_rmlui && rmlui_screen_gameover) {
        rmlui_gameover_show_banner();
    } else {
        effect_A9_init(0x20, 5, 0x12, 0);
    }
    BGM_Request(59);
    Next_Step = 0;

    effect_58_init(0xC, 1, 3);
    effect_58_init(0xC, 1, 1);
    effect_58_init(0xF, 5, 2);
    effect_58_init(0x10, 5, 2);

    if (Break_Com[WINNER][0]) {
        if (!use_rmlui || !rmlui_screen_gameover) {
            spawn_effect_76(0x38, 3, 1);
        }
        /* Break_Com skip means we go straight to exit after some time */
        tp->free[1] = 0xFF; /* Marker to skip to phase transition logic */
    } else {
        tp->free[1] = 1; /* Go to wait for Next_Step */
    }
}

static void ms_gameover_tick(struct _TASK* tp) {
    switch (tp->free[0]) {
    case 0:
        switch (tp->free[1]) {
        case 1:
            if (Next_Step) {
                tp->free[1] = 2;
                G_Timer = 420; // 7 minutes?
            }
            break;

        case 2:
            if (Scene_Cut) {
                G_Timer = 1;
            }
            if (--G_Timer <= 0) {
                tp->free[0] = 1; /* advance to phase 2 */
                tp->free[1] = 0;
            }
            break;

        case 0xFF:
            /* We hit the Break_Com[WINNER][0] early exit in on_enter */
            if (Scene_Cut) {
                G_Timer = 1;
            }
            if (--G_Timer <= 0) {
                tp->free[0] = 1; /* advance */
                tp->free[1] = 0;
            }
            break;
        }
        break;

    case 1:
        switch (tp->free[1]) {
        case 0:
            /* fallthrough to 1 */
        case 1:
            tp->free[1] = 2;
            Forbid_Break = 0;
            FadeInit();
            break;

        case 2:
            if (FadeOut(1, 8, 8) != 0) {
                tp->free[1] = 3;
                Cover_Timer = 5;
                Suicide[3] = 1;
                Suicide[2] = 0;

                if (Break_Com[WINNER][0]) {
                    Setup_BG(0, 0x200, 0);
                    bg_etc_write(PL_Color_Data[My_char[Winner_id]]);
                }

                if (use_rmlui && rmlui_screen_gameover) {
                    rmlui_gameover_show_results();
                } else {
                    Setup_Result_OBJ();
                    spawn_effect_76(0x41, 3, 1);
                }
            }
            break;

        case 3:
            FadeOut(1, 8, 8);

            if (--Cover_Timer <= 0) {
                tp->free[1] = 4;
                Forbid_Break = -1;
                FadeInit();
            }
            break;

        case 4:
            if (FadeIn(1, 8, 8) != 0) {
                Forbid_Break = 0;
                BGM_Request(54);
                Ignore_Entry[LOSER] = 0;

                if ((E_Number[0][0] != 2) && (E_Number[1][0] != 2)) {
                    tp->free[1] = 6;
                    G_Timer = 60;
                } else {
                    tp->free[1] = 5;
                }
            }
            break;

        case 5:
            if ((E_Number[0][0] != 2) && (E_Number[1][0] != 2)) {
                tp->free[1] = 6;
                G_Timer = 60;
            }
            break;

        case 6:
            if (--G_Timer <= 0) {
                tp->free[1] = 7;
                G_Timer = Result_Timer[Player_id];
            }
            break;

        case 7:
            if (Scene_Cut) {
                G_Timer = 1;
            }
            if (--G_Timer <= 0) {
                tp->free[0] = 2;
                SsBgmFadeOut(0x222);
                MenuScreen_RequestFadeOut(); /* FADE_OUT -> EXIT */
            }
            break;
        }
        break;

    case 2:
        /* Waiting for fade out to complete via MenuScreen framework */
        break;
    }
}

static void ms_gameover_exit(struct _TASK* tp) {
    GAME_OVER_X = 1;
}

/** @brief Spawn all visual effects/objects for the result screen (labels, character cards). */
static void Setup_Result_OBJ(void) {
    spawn_effect_76(0x32, 3, 1);
    spawn_effect_76(0x33, 3, 1);

    effect_L1_init(7);
    effect_L1_init(8);
    effect_L1_init(9);
    effect_L1_init(0xA);
    effect_L1_init(0xB);
    effect_L1_init(0xC);
    effect_L1_init(0xD);
    effect_L1_init(0xE);
}

__attribute__((constructor)) static void register_ms_gameover() {
    extern MenuScreen g_screens[];

    g_screens[MENU_SCREEN_GAMEOVER] = (MenuScreen){
        .name = "gameover",
        .id = MENU_SCREEN_GAMEOVER,
        .parent = MENU_SCREEN_NONE,
        .on_enter = ms_gameover_enter,
        .on_tick = ms_gameover_tick,
        .on_exit = ms_gameover_exit,
        .cursor_max = 0,
        .cancel_item = -1,
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = (MenuHeader)-1,
        .effect_slot = 0
    };
}