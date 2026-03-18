/**
 * @file ms_win.c
 * @brief MenuScreen registry integration for the Win and Lose screens.
 *
 * Implements the on_enter, on_tick, and on_exit callbacks for
 * MENU_SCREEN_WIN and MENU_SCREEN_LOSER.
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/effect/eff58.h"
#include "sf33rd/Source/Game/effect/eff76.h"
#include "sf33rd/Source/Game/effect/effb8.h"
#include "sf33rd/Source/Game/effect/effl1.h"
#include "sf33rd/Source/Game/engine/workuser.h"
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
#include "sf33rd/Source/Game/io/gd3rd.h"

#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui/rmlui_win_screen.h"

extern u8 WIN_X;
extern void Setup_Wins_OBJ(void);

static void ms_win_enter(struct _TASK* tp) {
    s16 ix;

    Switch_Screen(0);
    Play_Mode = 1;
    Replay_Status[0] = 0;
    Replay_Status[1] = 0;

    tp->free[0] = 1; /* Match M_No[0] phases 1 to 5 */
    tp->free[1] = 0; /* Matches M_No[1] */

    Game_pause = 0;
    BGM_Request(55);
    Cover_Timer = 23;
    All_Clear_Suicide();
    base_y_pos = 40;

    for (ix = 0; ix < 4; ix++) {
        Unsubstantial_BG[ix] = 0;
    }

    System_all_clear_Level_B();
    Purge_mmtm_area(4);
    Make_texcash_of_list(4);
    load_any_texture_patnum(0x7F30, 0xC, 0);
    Setup_BG(0, 0x200, 0);
    bg_etc_write(PL_Color_Data[My_char[Winner_id]]);
    Setup_BG(2, 0x300, 0);
    Setup_BG(1, 0x200, 0);
    Setup_BG(3, 0x2C0, 0);

    if (Play_Type == 0) {
        Last_Selected_EM[Winner_id] = 1;
    }

    pulpul_stop();
}

static void ms_win_tick(struct _TASK* tp) {
    switch (tp->free[0]) {
    case 1:
        Switch_Screen(0);
        tp->free[0] += 1;

        /* Score/win globals — always compute, even in RmlUi mode */
        WGJ_Score = Continue_Coin[Winner_id] + Score[Winner_id][Play_Type];
        WGJ_Win = Win_Record[Winner_id];

        if (use_rmlui && rmlui_screen_winner) {
            rmlui_win_screen_show();
        } else {
            spawn_effect_76(0x37, 1, 1);
            spawn_effect_76(0x35, 3, 1);
            spawn_effect_76(0x34, 3, 1);
            spawn_effect_76(0x2B, 3, 1);
            spawn_effect_76(0x3A, 3, 1);
            spawn_effect_76(0x2C, 3, 1);

            Order_Dir[0x2D] = 4;
            spawn_effect_76(0x2D, 1, 0x1E);

            spawn_effect_76(0x38, 6, 1);
        }

        /* Sparkle + character victory anim — always run */
        effect_L1_init(1);
        effect_L1_init(2);
        effect_L1_init(3);
        effect_L1_init(4);
        effect_L1_init(5);
        effect_L1_init(6);

        if (!use_rmlui || !rmlui_screen_winner) {
            Setup_Wins_OBJ();
        }
        effect_B8_init(WINNER, 0x3C);
        break;

    case 2:
        switch (tp->free[1]) {
        case 0:
            Switch_Screen(0);
            tp->free[1] += 1;
            Clear_Flash_No();
            Switch_Screen_Init(1);
            break;

        case 1:
            if (Switch_Screen_Revival(1) != 0) {
                tp->free[0] += 1;
                tp->timer = 90;
                Forbid_Break = -1;
                Ignore_Entry[LOSER] = 0;
                Target_BG_X[2] = bg_w.bgw[2].wxy[0].disp.pos - 384;
                Offset_BG_X[2] = 0;
                Next_Step = 0;
                bg_mvxy.a[0].sp = -0x100000;
                bg_mvxy.d[0].sp = 0x800;

                effect_58_init(0xE, 0x14, 2);

                if (Mode_Type == MODE_ARCADE) {
                    Push_LDREQ_Queue_Player(Winner_id, My_char[Winner_id]);
                }
            }
            break;
        }
        break;

    case 3:
        if (--tp->timer <= 0) {
            tp->free[0] += 1;
            tp->free[1] = 0;
            tp->timer = 0xAA;
            Forbid_Break = 0;
        }
        break;

    case 4:
        switch (tp->free[1]) {
        case 0:
            if (Scene_Cut) {
                tp->timer = 9;
            }

            if (tp->timer < 10) {
                tp->timer = 9;
                tp->free[1] += 1;

                if (Mode_Type == MODE_ARCADE) {
                    SsBgmFadeOut(0x1000);
                }
            }
            break;
        }

        if (--tp->timer <= 0) {
            MenuScreen_RequestFadeOut(); /* FADE_OUT -> EXIT */
        }
        break;
    }
}

static void ms_win_exit(struct _TASK* tp) {
    WIN_X = 1;
}

static void ms_loser_enter(struct _TASK* tp) {
    s16 ix;

    Switch_Screen(0);
    Play_Mode = 1;
    Replay_Status[0] = 0;
    Replay_Status[1] = 0;

    tp->free[0] = 1; /* Match M_No[0] */
    tp->free[1] = 0; /* Match M_No[1] */

    Game_pause = 0;
    BGM_Request(55);
    Cover_Timer = 23;
    All_Clear_Suicide();
    base_y_pos = 40;

    for (ix = 0; ix < 4; ix++) {
        Unsubstantial_BG[ix] = 0;
    }

    System_all_clear_Level_B();
    Purge_mmtm_area(4);
    Make_texcash_of_list(4);
    load_any_texture_patnum(0x7F30, 0xC, 0);
    Setup_BG(0, 0x200, 0);
    bg_etc_write(PL_Color_Data[My_char[Winner_id]]);
    Setup_BG(2, 0x300, 0);
    Setup_BG(1, 0x200, 0);
    Setup_BG(3, 0x2C0, 0);

    if (Play_Type == 0) {
        Last_Selected_EM[Winner_id] = 1;
    }

    pulpul_stop();
}

static void ms_loser_tick(struct _TASK* tp) {
    switch (tp->free[0]) {
    case 1:
        Switch_Screen(0);
        tp->free[0] += 1;

        if (use_rmlui && rmlui_screen_winner) {
            rmlui_win_screen_show();
        } else {
            spawn_effect_76(0x37, 1, 1);
            spawn_effect_76(0x40, 3, 1);
            spawn_effect_76(0x36, 3, 1);
            spawn_effect_76(0x39, 3, 1);

            Order_Dir[0x2D] = 4;
            spawn_effect_76(0x2D, 1, 30);
        }

        effect_B8_init(WINNER, 0x3C);
        break;

    case 2:
        switch (tp->free[1]) {
        case 0:
            Switch_Screen(0);
            tp->free[1] += 1;
            Clear_Flash_No();
            Switch_Screen_Init(1);
            break;

        case 1:
            if (Switch_Screen_Revival(1) != 0) {
                tp->free[0] += 1;
                tp->timer = 90;
                Forbid_Break = -1;
                Ignore_Entry[LOSER] = 0;
            }
            break;
        }
        break;

    case 3:
        if (--tp->timer <= 0) {
            tp->free[0] += 1;
            tp->free[1] = 0;
            tp->timer = 0xAA;
            Forbid_Break = 0;
        }
        break;

    case 4:
        switch (tp->free[1]) {
        case 0:
            if (Scene_Cut) {
                tp->timer = 9;
            }

            if (tp->timer < 10) {
                tp->timer = 9;
                tp->free[1] += 1;

                if (Mode_Type == MODE_ARCADE) {
                    SsBgmFadeOut(0x1000);
                }
            }
            break;
        }

        if (--tp->timer <= 0) {
            MenuScreen_RequestFadeOut(); /* FADE_OUT -> EXIT */
        }
        break;
    }
}

static void ms_loser_exit(struct _TASK* tp) {
    WIN_X = 1;
}

__attribute__((constructor)) static void register_ms_win() {
    extern MenuScreen g_screens[];

    g_screens[MENU_SCREEN_WIN] = (MenuScreen){
        .name = "win",
        .id = MENU_SCREEN_WIN,
        .parent = MENU_SCREEN_NONE,
        .on_enter = ms_win_enter,
        .on_tick = ms_win_tick,
        .on_exit = ms_win_exit,
        .cursor_max = 0,
        .cancel_item = -1,
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = (MenuHeader)-1,
        .effect_slot = 0
    };

    g_screens[MENU_SCREEN_LOSER] = (MenuScreen){
        .name = "loser",
        .id = MENU_SCREEN_LOSER,
        .parent = MENU_SCREEN_NONE,
        .on_enter = ms_loser_enter,
        .on_tick = ms_loser_tick,
        .on_exit = ms_loser_exit,
        .cursor_max = 0,
        .cancel_item = -1,
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = (MenuHeader)-1,
        .effect_slot = 0
    };
}