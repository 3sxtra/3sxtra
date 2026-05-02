/**
 * @file next_cpu.c
 * Next CPU character selection
 */

#include "sf33rd/Source/Game/screen/next_cpu.h"
#include "game_state.h"
#include "common.h"
#include "arcade/arcade_char_data.h"
#include "port/sdl/rmlui/rmlui_char_select.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/com/com_data.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/eff38.h"
#include "sf33rd/Source/Game/effect/eff39.h"
#include "sf33rd/Source/Game/effect/eff42.h"
#include "sf33rd/Source/Game/effect/eff43.h"
#include "sf33rd/Source/Game/effect/eff58.h"
#include "sf33rd/Source/Game/effect/eff75.h"
#include "sf33rd/Source/Game/effect/eff76.h"
#include "sf33rd/Source/Game/effect/eff98.h"
#include "sf33rd/Source/Game/effect/effa9.h"
#include "sf33rd/Source/Game/effect/effe0.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effk6.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/rendering/mmtmcnt.h"
#include "sf33rd/Source/Game/screen/sel_data.h"
#include "sf33rd/Source/Game/select_timer.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sys_sub2.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

static void Next_CPU_1st();
static void Next_CPU_2nd();
static void Next_CPU_3rd();
static void Next_CPU_4th();
static void Next_CPU_4th_0_Sub();
static void Next_CPU_4th_1_Sub();
static void Next_CPU_4th_2_Sub();
static void Next_CPU_5th();
static u8 Check_EM_Speech();
static void Next_CPU_6th();
static void Wait_Load_Complete();
static void Wait_Load_Complete2();
static void Wait_Load_Complete3();
static void After_Bonus_1st();
static void After_Bonus_2nd();
static void Select_CPU_1st();
static void Select_CPU_2nd();
static void NC_Cut_Sub();
static void Select_CPU_3rd();
static void Select_CPU_4th();
static void Next_Bonus_1st();
static void Next_Bonus_2nd();
static void Next_Bonus_3rd();
static void Next_Bonus_End();
static void Next_Q_1st();
static void Next_Q_2nd();
static void Next_Q_3rd();
static void Sel_CPU_Sub(s16 PL_id, u16 sw, u16 /* unused */);
static void Setup_EM_List();
static void Setup_Next_Fighter();
static s8 Setup_Com_Arts();
static void Setup_Com_Color();
static void Setup_Regular_OBJ(s16 PL_id);
static void Regular_OBJ_Sub(s16 PL_id, s16 Dir);
static void Setup_History_OBJ();
static void Setup_VS_OBJ(s16 Option);
static s8 Check_Bonus_Type();
static void Setup_Next_Stage(s16 dir_step);
static void Check_Auto_Cut();

// sbss
u8 SEL_CPU_X;
s16 Start_X;

/** @brief Main next-CPU dispatcher — step through opponent select phases and return exit flag. */
s16 Next_CPU() {
    if (g_state.Break_Into) {
        return 0;
    }

    SEL_CPU_X = 0;
    g_state.Scene_Cut = Cut_Cut_Cut();

    switch (g_state.SC_No[0]) {
    case 0:
        Next_CPU_1st();
        break;
    case 1:
        Next_CPU_2nd();
        break;
    case 2:
        Next_CPU_3rd();
        break;
    case 3:
        Next_CPU_4th();
        break;
    case 4:
        Next_CPU_5th();
        break;
    case 5:
        Next_CPU_6th();
        break;
    case 6:
        Next_Bonus_1st();
        break;
    case 7:
        Next_Bonus_2nd();
        break;
    case 8:
        Next_Bonus_3rd();
        break;
    case 9:
        Next_Bonus_End();
        break;
    case 10:
        Wait_Load_Complete();
        break;
    case 11:
        Wait_Load_Complete2();
        break;
    default:
        break;
    }

    g_state.Time_Over = false;

    if (Check_Exit_Check() == 0 && Debug_w[DEBUG_TIME_STOP] == -1) {
        SEL_CPU_X = 0;
    }

    return SEL_CPU_X;
}

/** @brief Phase 1 — init BG scroll, build EM list, spawn history/regular objects, start BGM. */
static void Next_CPU_1st() {
    u16 Rnd;

    g_state.SC_No[0]++;
    g_state.Target_BG_X[3] = g_state.bg_w.bgw[3].wxy[0].disp.pos + 458;
    g_state.Offset_BG_X[3] = 0;
    Start_X = g_state.bg_w.bgw[3].wxy[0].disp.pos;
    g_state.bg_mvxy.a[0].sp = 0x40000;
    g_state.bg_mvxy.d[0].sp = 0;
    g_state.Sel_EM_Complete[g_state.Player_id] = 0;
    g_state.Temporary_EM[g_state.Player_id] = g_state.Last_Selected_EM[g_state.Player_id];
    g_state.Select_Timer = 0x20;
    Setup_EM_List();

    if (g_state.VS_Index[g_state.Player_id] == 0) {
        effect_A9_init(32, 0, 0, 0);
    } else {
        Setup_History_OBJ();

        if (g_state.VS_Index[g_state.Player_id] < 9) {
            Setup_Next_Stage(58);
        } else {
            Setup_Next_Stage(59);
        }
    }

    Setup_Regular_OBJ(g_state.Player_id);

    /* Keep RmlUI overlay active so native timer/label effects stay gated */
    if (use_rmlui && rmlui_screen_select)
        rmlui_char_select_show();

    g_state.Moving_Plate[g_state.Player_id] = 0;

    if (g_state.G_No[1] == 5) {
        BGM_Request(57);
        g_state.Order[56] = 3;
        g_state.Order_Timer[56] = 1;
    }

    g_state.Time_Stop = 1;
    g_state.Unit_Of_Timer = UNIT_OF_TIMER_MAX;
    SelectTimer_Init();
    Rnd = random_16() & 3;
    effect_58_init(6, 10, EM_Select_Voice_Data[Rnd]);
    g_state.Next_Step = 0;
    g_state.Suicide[2] = 1;
    g_state.Cut_Scroll = 2;
    effect_58_init(13, 1, 3);
    effect_58_init(16, 5, 2);
}

/** @brief Phase 2 — auto-cut check sub-routine dispatch. */
static void Next_CPU_2nd() {
    NC_Cut_Sub();
}

/** @brief Phase 3 — player picks CPU opponent from EM list, queue load, handle boss speech path. */
static void Next_CPU_3rd() {
    switch (g_state.SC_No[1]) {
    case 0:
        if (g_state.Player_id) {
            Sel_CPU_Sub(1, ~p2sw_1 & p2sw_0, p2sw_0);
        } else {
            Sel_CPU_Sub(0, ~p1sw_1 & p1sw_0, p1sw_0);
        }

        if (!g_state.Sel_EM_Complete[g_state.Player_id]) {
            break;
        }

        g_state.SC_No[1]++;
        g_state.SC_No[2] = 0;

        if (Debug_w[DEBUG_MY_CHAR_PL1]) {
            g_state.My_char[0] = Debug_w[DEBUG_MY_CHAR_PL1] - 1;
        }

        if (Debug_w[DEBUG_MY_CHAR_PL2]) {
            g_state.My_char[1] = Debug_w[DEBUG_MY_CHAR_PL2] - 1;
        }

        Push_LDREQ_Queue_Player(g_state.COM_id, g_state.My_char[g_state.COM_id]);
        Setup_Next_Fighter();

        if (Debug_w[DEBUG_MY_CHAR_PL1]) {
            g_state.My_char[0] = Debug_w[DEBUG_MY_CHAR_PL1] - 1;
        }

        if (Debug_w[DEBUG_MY_CHAR_PL2]) {
            g_state.My_char[1] = Debug_w[DEBUG_MY_CHAR_PL2] - 1;
        }

        if (g_state.VS_Index[g_state.Player_id] < 8) {
            g_state.S_Timer = 50;
            break;
        }

        g_state.SC_No[1] = 2;
        g_state.S_Timer = 100;
        break;

    case 1:
        switch (g_state.SC_No[2]) {
        case 0:
            if (g_state.S_Timer < 10) {
                g_state.S_Timer = 9;
                g_state.SC_No[2]++;
                SsBgmFadeOut(0x1000);
            }

            break;
        }

        if ((g_state.S_Timer -= 1) == 0) {
            g_state.SC_No[0]++;
            g_state.SC_No[1] = 0;
            g_state.SC_No[2] = 0;
        }

        break;

    case 2:
        if ((g_state.S_Timer -= 1) < 71) {
            if (Check_EM_Speech() == 0) {
                g_state.SC_No[1]++;
            } else {
                g_state.SC_No[0] = 4;
                g_state.SC_No[1] = 0;
            }

            g_state.SC_No[2] = 0;
            break;
        }

        break;

    case 3:
        switch (g_state.SC_No[2]) {
        case 0:
            if (g_state.Scene_Cut) {
                g_state.S_Timer = 9;
            }

            if (g_state.S_Timer < 10) {
                g_state.S_Timer = 9;
                g_state.SC_No[2]++;
                SsBgmFadeOut(0x1000U);
            }

            break;
        }

        if ((g_state.S_Timer -= 1) == 0) {
            g_state.SC_No[0]++;
            g_state.SC_No[1] = 0;
            g_state.SC_No[2] = 0;
        }

        break;
    }
}

/** @brief Phase 4 — fade-in VS screen, then route to load-wait or bonus. */
static void Next_CPU_4th() {
    switch (g_state.SC_No[1]) {
    case 0:
        FadeInit();
        /* NOTE: Do NOT call rmlui_char_select_hide() here.
         * Keeping rmlui_char_select_visible == true ensures native effects
         * (eff79 SA plates, eff42 timer, etc.) stay gated through the VS
         * screen.  The auto-hide in rmlui_char_select_update() will clean
         * up when g_state.Play_Game != 0. */
        Next_CPU_4th_0_Sub();
        break;

    case 1:
        Next_CPU_4th_1_Sub();
        break;

    case 2:
        Next_CPU_4th_2_Sub();
        break;

    default:
        if (g_state.Scene_Cut) {
            g_state.S_Timer = 1;
        }

        if ((g_state.S_Timer -= 1) != 0) {
            break;
        }

        if (g_state.G_No[1] == 5 || g_state.G_No[1] == 10) {
            g_state.SC_No[0] = 10;
            break;
        }

        g_state.SC_No[0] = 6;
        break;
    }
}

extern bool mods_menu_fast_pre_game;

/** @brief Phase 4.0 — init fade, set up VS BG/objects, start BGM 51. */
static void Next_CPU_4th_0_Sub() {
    FadeIn(0, 4, 8);
    g_state.SC_No[1]++;
    g_state.Forbid_Break = 0;
    g_state.bgPalCodeOffset[0] = 144;
    BGM_Request(51);
    g_state.S_Timer = mods_menu_fast_pre_game ? 1 : 178;
    g_state.Exit_Timer = 2;
    g_state.bg_w.bgw[0].wxy[0].disp.pos += 512;
    g_state.bg_w.bgw[1].wxy[1].disp.pos = 512;
    g_state.bg_w.bgw[3].wxy[1].disp.pos += 512;
    Setup_BG(0, g_state.bg_w.bgw[0].wxy[0].disp.pos, g_state.bg_w.bgw[0].wxy[1].disp.pos);
    Setup_BG(1, g_state.bg_w.bgw[1].wxy[0].disp.pos + 512, g_state.bg_w.bgw[1].wxy[1].disp.pos);
    Setup_BG(3, g_state.bg_w.bgw[3].wxy[0].disp.pos, g_state.bg_w.bgw[3].wxy[1].disp.pos);
    Setup_VS_OBJ(0);
    g_state.Suicide[0] = 1;
    FadeInit();
}

/** @brief Phase 4.1 — continue fade-in and wait for exit timer. */
static void Next_CPU_4th_1_Sub() {
    FadeIn(0, 4, 8);

    if ((g_state.Exit_Timer -= 1) == 0) {
        g_state.SC_No[1]++;
    }
}

/** @brief Phase 4.2 — count down while still fading in. */
static void Next_CPU_4th_2_Sub() {
    g_state.S_Timer--;

    if (!FadeIn(0, 4, 8)) {
        return;
    }

    g_state.SC_No[1]++;

    if (g_state.S_Timer < 0) {
        g_state.S_Timer = 1;
    }
}

/** @brief Phase 5 — screen-switch sequence for boss intros, then proceed to load-wait. */
static void Next_CPU_5th() {
    switch (g_state.SC_No[1]) {
    case 0:
        g_state.SC_No[1]++;
        Switch_Screen_Init(1);
        break;

    case 1:
        if (Switch_Screen(1) != 0) {
            g_state.SC_No[1]++;
            g_state.Cover_Timer = 9;
        }

        break;

    case 2:
        Switch_Screen(1);
        g_state.SC_No[1]++;
        g_state.bgPalCodeOffset[0] = 144;
        g_state.bg_w.bgw[0].wxy[0].disp.pos += 512;
        g_state.bg_w.bgw[1].wxy[1].disp.pos = 512;
        g_state.bg_w.bgw[3].wxy[1].disp.pos += 512;
        Setup_BG(0, g_state.bg_w.bgw[0].wxy[0].disp.pos, g_state.bg_w.bgw[0].wxy[1].disp.pos);
        Setup_BG(1, g_state.bg_w.bgw[1].wxy[0].disp.pos, g_state.bg_w.bgw[1].wxy[1].disp.pos);
        Setup_BG(3, g_state.bg_w.bgw[3].wxy[0].disp.pos, g_state.bg_w.bgw[3].wxy[1].disp.pos);
        Setup_VS_OBJ(1);
        g_state.Suicide[0] = 1;
        g_state.Next_Step = 0;
        g_state.Order[67] = 1;
        g_state.Order_Timer[67] = 10;
        g_state.Order_Dir[67] = 8;
        effect_76_init(67);
        g_state.Order[68] = 1;
        g_state.Order_Timer[68] = 10;
        g_state.Order_Dir[68] = 4;
        effect_76_init(68);
        break;

    case 3:
        Switch_Screen(1);

        if ((g_state.Cover_Timer -= 1) == 0) {
            g_state.SC_No[1]++;
            Switch_Screen_Init(1);
        }

        break;

    case 4:
        if (Switch_Screen_Revival(1) != 0) {
            g_state.SC_No[1]++;
            g_state.Forbid_Break = 0;
        }

        break;

    case 5:
        if ((g_state.Next_Step & 0x80) != 0) {
            g_state.SC_No[1]++;
            g_state.S_Timer = 8;
            SsBgmFadeOut(0x1000);
        }

        break;

    case 6:
        if (!(g_state.S_Timer -= 1)) {
            FadeInit();
            FadeIn(0, 4, 8);
            g_state.SC_No[1]++;
            g_state.Forbid_Break = 0;
            g_state.Suicide[3] = 1;
            effect_43_init(1, 0);
            BGM_Request(0x33);
            g_state.S_Timer = mods_menu_fast_pre_game ? 1 : 0xb2;
        }

        break;

    case 7:
        g_state.S_Timer--;

        if (FadeIn(0, 4, 8)) {
            g_state.SC_No[1]++;

            if (g_state.S_Timer < 0) {
                g_state.S_Timer = 1;
            }

            g_state.Introduce_Boss[g_state.Player_id][g_state.VS_Index[g_state.Player_id] - 8] |= 1;
        }

        break;

    default:
        if (g_state.Scene_Cut) {
            g_state.S_Timer = 1;
        }

        if ((g_state.S_Timer -= 1) == 0) {
            g_state.SC_No[0] = 10;
        }

        break;
    }
}

/** @brief Return non-zero if the boss has an unplayed intro speech for the current matchup. */
u8 Check_EM_Speech() {
    if (g_state.Introduce_Boss[g_state.Player_id][g_state.VS_Index[g_state.Player_id] - 8] & 1) {
        return 0;
    }

    return Boss_Speech_Data[g_state.My_char[g_state.Player_id]][g_state.VS_Index[g_state.Player_id] - 8];
}

/** @brief Phase 6 — signal completion of next-CPU sequence. */
static void Next_CPU_6th() {
    SEL_CPU_X = 1;
}

/** @brief Return true when player sprites, BG, and audio are all loaded. */
static bool is_load_complete() {
    if (!Check_PL_Load()) {
        return false;
    }

    if (!Check_LDREQ_Queue_BG(g_state.bg_w.stage)) {
        return false;
    }

    if (!(adx_now_playend() || g_state.Scene_Cut)) {
        return false;
    }

    return true;
}

/** @brief Wait for player/BG/sound loads to finish, then init omop and signal exit. */
static void Wait_Load_Complete() {
    if (!is_load_complete()) {
        return;
    }

    SEL_CPU_X = 1;
    init_omop();
    g_state.SC_No[0] = 5;
}

/** @brief Wait for loads then signal exit with code 2 (post-VS path). */
static void Wait_Load_Complete2() {
    if (!is_load_complete()) {
        return;
    }

    SEL_CPU_X = 2;
    init_omop();
    g_state.SC_No[0] = 10;
}

/** @brief Wait for loads then signal exit with code 2 (bonus-end path). */
static void Wait_Load_Complete3() {
    if (!is_load_complete()) {
        return;
    }

    SEL_CPU_X = 2;
    init_omop();
    g_state.SC_No[0] = 7;
}

/** @brief After-bonus dispatcher — rebuild BG, run next-CPU phases, return exit flag. */
s32 After_Bonus() {
    if (g_state.Break_Into) {
        return 0;
    }

    SEL_CPU_X = 0;
    g_state.Scene_Cut = Cut_Cut_Cut();

    switch (g_state.SC_No[0]) {
    case 0:
        After_Bonus_1st();
        break;
    case 1:
        After_Bonus_2nd();
        break;
    case 2:
        Next_CPU_1st();
        break;
    case 3:
        Next_CPU_2nd();
        break;
    case 4:
        Next_CPU_3rd();
        break;
    case 5:
        Next_CPU_4th();
        break;
    case 6:
        Wait_Load_Complete2();
        break;
    case 7: /* fallthrough */
    case 8: /* fallthrough */
    case 9:
        Next_Bonus_End();
        break;
    case 10:
        Wait_Load_Complete3();
        break;
    default:
        break;
    }

    g_state.Time_Over = false;
    return SEL_CPU_X;
}

/** @brief After-bonus phase 1 — clear screen, set up virtual BG and scroll layers. */
static void After_Bonus_1st() {
    Switch_Screen(0);
    g_state.SC_No[0]++;
    g_state.Cover_Timer = 23;
    All_Clear_Suicide();
    System_all_clear_Level_B();
    g_state.base_y_pos = 40;
    bg_etc_write(2);
    Setup_Virtual_BG(0, 0x100, 0);
    Setup_BG(2, 0x300, 0);
    Setup_BG(1, 0x200, 0);
    Setup_BG(3, 0x2C0, 0);
    g_state.Unsubstantial_BG[0] = 1;
}

/** @brief After-bonus phase 2 — purge texcache, screen switch, start BGM, and re-enable break. */
static void After_Bonus_2nd() {
    switch (g_state.SC_No[1]) {
    case 0:
        Switch_Screen(0);
        Purge_mmtm_area(7);
        Purge_com_player_from_mm();
        Make_texcash_of_list(7);
        g_state.SC_No[1]++;
        effect_76_init(55);
        g_state.Order[55] = 3;
        g_state.Order_Timer[55] = 1;
        effect_76_init(56);
        g_state.Order[56] = 3;
        g_state.Order_Timer[56] = 1;
        /* fallthrough */

    case 1:
        Switch_Screen(1);

        if ((g_state.Cover_Timer -= 1) == 0) {
            g_state.SC_No[1]++;
            Clear_Flash_No();
            Switch_Screen_Init(1);
        }

        break;

    case 2:
        if (Switch_Screen_Revival(1) != 0) {
            g_state.SC_No[0]++;
            g_state.SC_No[1] = 0;
            g_state.S_Timer = 30;
            BGM_Request(57);
            g_state.Forbid_Break = 0;
            g_state.Ignore_Entry[g_state.LOSER] = 0;
        }

        break;
    }
}

/** @brief First CPU-select dispatcher — used when game starts or after demo. */
s16 Select_CPU_First() {
    if (g_state.Break_Into) {
        return 0;
    }

    SEL_CPU_X = 0;

    switch (g_state.SC_No[0]) {
    case 0:
        Select_CPU_1st();
        break;
    case 1:
        Select_CPU_2nd();
        break;
    case 2:
        Select_CPU_3rd();
        break;
    case 3:
        Select_CPU_4th();
        break;
    default:
        break;
    }

    g_state.Time_Over = false;
    return SEL_CPU_X;
}

/** @brief Select_CPU phase 1 — build EM list, set up BG, spawn objects. */
static void Select_CPU_1st() {
    g_state.SC_No[0]++;
    g_state.Sel_EM_Complete[g_state.Player_id] = 0;
    g_state.Temporary_EM[g_state.Player_id] = g_state.Last_Selected_EM[g_state.Player_id];
    g_state.Select_Timer = 0x20;
    Setup_EM_List();
    g_state.Target_BG_X[3] = g_state.bg_w.bgw[3].wxy[0].disp.pos + 458;
    g_state.Offset_BG_X[3] = 0;

    if (g_state.VS_Index[g_state.Player_id] == 0) {
        g_state.bg_mvxy.a[0].sp = 0xA0000;
        g_state.bg_mvxy.d[0].sp = 0x18000;
        effect_A9_init(32, 0, 0, 1);
    } else {
        Setup_History_OBJ();
        g_state.bg_mvxy.a[0].sp = 0x40000;
        g_state.bg_mvxy.d[0].sp = 0;

        if (g_state.VS_Index[g_state.Player_id] < 9) {
            Setup_Next_Stage(58);
        } else {
            Setup_Next_Stage(59);
        }

        effect_76_init(66);
        g_state.Order[66] = 3;
        g_state.Order_Timer[66] = 1;
    }

    Setup_Regular_OBJ(g_state.Player_id);
    g_state.Moving_Plate[g_state.Player_id] = 0;

    if (g_state.VS_Index[g_state.Player_id] >= 8) {
        Push_LDREQ_Queue_Direct(9, 2);
    }
}

/** @brief Select_CPU phase 2 — display EM list, play voice, auto-cut, and dispatch. */
static void Select_CPU_2nd() {
    u16 Rnd;

    switch (g_state.SC_No[1]) {
    case 0:
        g_state.SC_No[1]++;
        g_state.Order[g_state.Aborigine + 13] = 5;
        g_state.Order_Timer[g_state.Aborigine + 13] = 1;
        g_state.Order[g_state.Aborigine + 31] = 5;
        g_state.Order_Timer[g_state.Aborigine + 31] = 1;
        g_state.Order[g_state.Aborigine + 25] = 5;
        g_state.Order_Timer[g_state.Aborigine + 25] = 1;
        g_state.Order[37] = 4;
        g_state.Order_Timer[37] = 1;
        Rnd = random_16() & 3;
        effect_58_init(6, 10, EM_Select_Voice_Data[Rnd]);
        g_state.Cut_Scroll = 2;
        g_state.Next_Step = 0;
        effect_58_init(12, 1, 3);
        /* fallthrough */

    case 1:
        NC_Cut_Sub();
        break;
    }
}

/** @brief Check auto-cut and advance phase if a scene cut has been triggered. */
static void NC_Cut_Sub() {
    Check_Auto_Cut();

    if (g_state.Next_Step) {
        g_state.SC_No[0]++;
        g_state.SC_No[1] = 0;
        g_state.Time_Stop = 0;
    }
}

/** @brief Select_CPU phase 3 — process player/demo input, commit opponent, load assets. */
static void Select_CPU_3rd() {
    switch (g_state.SC_No[1]) {
    case 0:
        if (g_state.Demo_Flag == 0) {
            if (g_state.Player_id) {
                Sel_CPU_Sub(1, Check_Demo_Data(1), 0);
            } else {
                Sel_CPU_Sub(0, Check_Demo_Data(0), 0);
            }
        } else if (g_state.Player_id) {
            Sel_CPU_Sub(1, ~p2sw_1 & p2sw_0, p2sw_0);
        } else {
            Sel_CPU_Sub(0, ~p1sw_1 & p1sw_0, p1sw_0);
        }

        if (!g_state.Sel_EM_Complete[g_state.Player_id]) {
            break;
        }

        g_state.SC_No[1]++;

        if (Debug_w[DEBUG_MY_CHAR_PL1]) {
            g_state.My_char[0] = Debug_w[DEBUG_MY_CHAR_PL1] - 1;
        }

        if (Debug_w[DEBUG_MY_CHAR_PL2]) {
            g_state.My_char[1] = Debug_w[DEBUG_MY_CHAR_PL2] - 1;
        }

        Push_LDREQ_Queue_Player(g_state.COM_id, g_state.My_char[g_state.COM_id]);
        Setup_Next_Fighter();

        if (Debug_w[DEBUG_MY_CHAR_PL1]) {
            g_state.My_char[0] = Debug_w[DEBUG_MY_CHAR_PL1] - 1;
        }

        if (Debug_w[DEBUG_MY_CHAR_PL2]) {
            g_state.My_char[1] = Debug_w[DEBUG_MY_CHAR_PL2] - 1;
        }

        if (g_state.VS_Index[g_state.Player_id] < 8) {
            g_state.S_Timer = 50;
        } else {
            g_state.SC_No[1] = 2;
            g_state.S_Timer = 100;
        }

        break;

    case 1:
        if ((g_state.S_Timer -= 1) == 0) {
            g_state.SC_No[1] = 4;
        }

        break;

    case 2:
        if ((g_state.S_Timer -= 1) < 51) {
            if (Check_LDREQ_Queue_Direct(9)) {
                g_state.SC_No[1]++;
            } else {
                g_state.S_Timer = 1;
            }
        }

        break;

    case 3:
        if (g_state.Scene_Cut) {
            g_state.S_Timer = 1;
        }

        if ((g_state.S_Timer -= 1) == 0) {
            g_state.SC_No[1]++;
        }

        break;

    case 4:
        g_state.SC_No[1] = 6;
        g_state.Order[g_state.Player_id + 11] = 4;
        g_state.Order_Timer[g_state.Player_id + 11] = 5;
        effect_38_init(g_state.COM_id, g_state.COM_id + 11, g_state.My_char[g_state.COM_id], 1, 2);
        g_state.Order[g_state.COM_id + 11] = 1;
        g_state.Order_Timer[g_state.COM_id + 11] = 1;

        if (check_use_all_SA() == 0 && check_without_SA() == 0 && g_state.EM_id != 0) {
            effect_98_init(g_state.COM_id, g_state.COM_id + 0x28, g_state.Super_Arts[g_state.COM_id], 2);
            g_state.Order[g_state.COM_id + 40] = 1;
            g_state.Order_Timer[g_state.COM_id + 40] = 1;
        }

        effect_75_init(42, 3, 2);
        g_state.Order[42] = 3;
        g_state.Order_Timer[42] = 1;
        g_state.Order_Dir[42] = 3;
        g_state.Target_BG_X[3] = g_state.bg_w.bgw[3].wxy[0].disp.pos + 480;
        g_state.Offset_BG_X[3] = 0;

        if (8 <= g_state.VS_Index[g_state.Player_id] && Check_EM_Speech()) {
            g_state.SC_No[1] = 5;
            g_state.Order[67] = 1;
            g_state.Order_Timer[67] = 10;
            g_state.Order_Dir[67] = 8;
            effect_76_init(67);
            g_state.Order[68] = 1;
            g_state.Order_Timer[68] = 10;
            g_state.Order_Dir[68] = 4;
            effect_76_init(68);
        }

        g_state.Next_Step = 0;
        g_state.Cut_Scroll = 2;
        g_state.bg_mvxy.a[0].sp = 0x200000;
        g_state.bg_mvxy.d[0].sp = 0x18000;
        effect_58_init(12, 1, 3);
        break;

    case 5:
        if (g_state.Next_Step & 0x80) {
            g_state.SC_No[1] = 7;
            g_state.S_Timer = 20;
            g_state.Introduce_Boss[g_state.Player_id][g_state.VS_Index[g_state.Player_id] - 8] = 1;
        }

        break;

    case 6:
        if (g_state.Next_Step & 1) {
            g_state.SC_No[1]++;
            g_state.S_Timer = 20;
        }

        break;

    case 7:
        switch (g_state.SC_No[2]) {
        case 0:
            if (g_state.Scene_Cut) {
                g_state.S_Timer = 9;
            }

            if (g_state.S_Timer < 10) {
                g_state.S_Timer = 9;
                g_state.SC_No[2]++;
                SsBgmFadeOut(0x1000);
            }

            break;
        }

        if ((g_state.S_Timer -= 1) == 0) {
            g_state.SC_No[0]++;
            g_state.SC_No[1] = 0;
            g_state.SC_No[2] = 0;
        }

        break;
    }
}

/** @brief Select_CPU phase 4 — signal completion and init omop. */
static void Select_CPU_4th() {
    SEL_CPU_X = 1;
    g_state.Next_Step = 1;
    init_omop();
}

/** @brief Bonus phase 1 — init BG scroll, spawn history objects, start BGM. */
static void Next_Bonus_1st() {
    u16 Rnd;

    g_state.SC_No[0]++;
    g_state.Target_BG_X[3] = g_state.bg_w.bgw[3].wxy[0].disp.pos + 458;
    g_state.Offset_BG_X[3] = 0;
    Start_X = g_state.bg_w.bgw[3].wxy[0].disp.pos;
    g_state.bg_mvxy.a[0].sp = 0x40000;
    g_state.bg_mvxy.d[0].sp = 0;
    Setup_History_OBJ();
    Setup_Next_Stage(60);
    BGM_Request(57);
    g_state.Order[56] = 3;
    g_state.Order_Timer[56] = 1;
    Rnd = random_16() & 3;
    effect_58_init(6, 10, EM_Select_Voice_Data[Rnd]);
    g_state.Suicide[2] = 1;
    g_state.Next_Step = 0;
    g_state.Cut_Scroll = 2;
    effect_58_init(13, 1, 3);
    effect_58_init(16, 5, 2);
}

/** @brief Bonus phase 2 — auto-cut and timer countdown before transition. */
static void Next_Bonus_2nd() {
    switch (g_state.SC_No[1]) {
    case 0:
        Check_Auto_Cut();

        if (g_state.Next_Step) {
            g_state.SC_No[1]++;
            g_state.SC_No[2] = 0;
            g_state.S_Timer = 90;
            effect_58_init(6, 5, 160);
        }

        break;

    case 1:
        switch (g_state.SC_No[2]) {
        case 0:
            if (g_state.Scene_Cut) {
                g_state.S_Timer = 9;
            }

            if (g_state.S_Timer < 10) {
                g_state.S_Timer = 9;
                g_state.SC_No[2]++;
                SsBgmFadeOut(0x1000);
            }

            break;
        }

        if ((g_state.S_Timer -= 1) == 0) {
            g_state.SC_No[0]++;
            g_state.SC_No[1] = 0;
            g_state.SC_No[2] = 0;
        }

        break;
    }
}

/** @brief Bonus phase 3 — fade-in VS screen for the bonus stage. */
static void Next_Bonus_3rd() {
    switch (g_state.SC_No[1]) {
    case 0:
        g_state.My_char[g_state.COM_id] = g_state.Bonus_Type;
        Next_CPU_4th_0_Sub();
        break;

    case 1:
        Next_CPU_4th_1_Sub();
        break;

    case 2:
        Next_CPU_4th_2_Sub();
        break;

    default:
        if (g_state.Scene_Cut) {
            g_state.S_Timer = 1;
        }

        if ((g_state.S_Timer -= 1) == 0) {
            if (Check_PL_Load() == 0) {
                g_state.S_Timer = 1;
                break;
            }

            g_state.SC_No[0] = 11;
        }

        break;
    }
}

/** @brief Bonus end — signal exit with code 2. */
static void Next_Bonus_End() {
    SEL_CPU_X = 2;
}

/** @brief Next-Q dispatcher — set up the Q-character fight sequence and return exit flag. */
s16 Next_Q() {
    if (g_state.Break_Into) {
        return 0;
    }

    SEL_CPU_X = 0;
    g_state.Scene_Cut = Cut_Cut_Cut();

    switch (g_state.SC_No[0]) {
    case 0:
        Next_Q_1st();
        break;
    case 1:
        Next_Q_2nd();
        break;
    case 2:
        Next_Q_3rd();
        break;
    case 3: /* fallthrough */
    case 4:
        Wait_Load_Complete();
        break;
    case 5:
        Next_CPU_6th();
        break;
    default:
        break;
    }

    if (Check_Exit_Check() == 0 && Debug_w[DEBUG_TIME_STOP] == -1) {
        SEL_CPU_X = 0;
    }

    g_state.Time_Over = false;
    return SEL_CPU_X;
}

/** @brief Next_Q phase 1 — set up Q opponent, purge mm, queue player load. */
static void Next_Q_1st() {
    After_Bonus_1st();
    Setup_ID();
    g_state.EM_id = 17;
    Setup_Next_Fighter();
    Purge_mmtm_area(8);
    Purge_com_player_from_mm();
    Make_texcash_of_list(7);
    Push_LDREQ_Queue_Player(g_state.COM_id, 17);
}

/** @brief Next_Q phase 2 — screen switch, set up VS objects, and wait for screen revival. */
static void Next_Q_2nd() {
    switch (g_state.SC_No[1]) {
    case 0:
        g_state.SC_No[1]++;
        /* fallthrough */

    case 1:
        Switch_Screen(0);

        if ((g_state.Cover_Timer -= 1) == 5) {
            g_state.SC_No[1]++;
            effect_work_quick_init();
            g_state.bg_w.bgw[0].wxy[0].disp.pos += 512;
            Setup_BG(0, g_state.bg_w.bgw[0].wxy[0].disp.pos, g_state.bg_w.bgw[0].wxy[1].disp.pos);
            Setup_VS_OBJ(1);
        }

        break;

    case 2:
        Switch_Screen(0);

        if ((g_state.Cover_Timer -= 1) == 0) {
            g_state.SC_No[1]++;
            Clear_Flash_No();
            Switch_Screen_Init(1);
        }

        break;

    case 3:
        if (Switch_Screen_Revival(1U) != 0) {
            g_state.SC_No[0]++;
            g_state.SC_No[1] = 0;
            g_state.S_Timer = 10;
            g_state.Forbid_Break = 0;
            g_state.Ignore_Entry[g_state.LOSER] = 0;
        }

        break;
    }
}

/** @brief Next_Q phase 3 — fade-in with BGM, then count down before exit. */
static void Next_Q_3rd() {
    switch (g_state.SC_No[1]) {
    case 0:
        if ((g_state.S_Timer -= 1) == 0) {
            g_state.SC_No[1]++;
        }

        break;

    case 1:
        FadeInit();
        FadeIn(0, 4, 8);
        g_state.SC_No[1]++;
        g_state.Forbid_Break = 0;
        effect_43_init(1, 0);
        g_state.bgPalCodeOffset[0] = 144;
        BGM_Request(51);
        g_state.S_Timer = mods_menu_fast_pre_game ? 1 : 180;
        effect_58_init(15, 5, 0);
        return;

    case 2:
        Next_CPU_4th_2_Sub();
        return;

    default:
        if (g_state.Scene_Cut) {
            g_state.S_Timer = 1;
        }

        if ((g_state.S_Timer -= 1) == 0) {
            g_state.SC_No[0]++;
        }

        break;
    }
}

/** @brief Process lever/button input for CPU opponent selection (up/down to pick, attack to confirm). */
static void Sel_CPU_Sub(s16 PL_id, u16 sw, u16 /* unused */) {
    u16 lever_sw;

    if (g_state.Sel_EM_Complete[PL_id]) {
        return;
    }

    if (g_state.Moving_Plate[PL_id]) {
        return;
    }

    if (g_state.Time_Over) {
        sw = SWK_WEST;
    }

    if (g_state.VS_Index[PL_id] >= 8) {
        sw = SWK_WEST;
    }

    lever_sw = sw & (SWK_UP | SWK_DOWN);

    if (lever_sw & SWK_DOWN) {
        if (g_state.Temporary_EM[g_state.Player_id] == 2) {
            return;
        }

        Sound_SE(PL_id + 96);
        g_state.Moving_Plate[PL_id] = 2;
        g_state.Moving_Plate_Counter[PL_id] = 2;
        g_state.Temporary_EM[g_state.Player_id] = 2;
    }

    if (lever_sw & SWK_UP) {
        if (g_state.Temporary_EM[g_state.Player_id] == 1) {
            return;
        }

        Sound_SE(PL_id + 96);
        g_state.Moving_Plate[PL_id] = 1;
        g_state.Moving_Plate_Counter[PL_id] = 2;
        g_state.Temporary_EM[g_state.Player_id] = 1;
    }

    if (sw & SWK_ATTACKS) {
        g_state.Sel_EM_Complete[PL_id] = 1;
        g_state.EM_id = g_state.EM_List[g_state.Player_id][g_state.Temporary_EM[g_state.Player_id] - 1];
        g_state.My_char[g_state.COM_id] = g_state.EM_id;
        g_state.Time_Stop = 2;

        if (g_state.VS_Index[PL_id] < 8) {
            Sound_SE(g_state.ID + 98);
            Sound_SE(Voice_EM_Random_Data[random_16()]);
        }

        g_state.Last_Selected_EM[PL_id] = g_state.Temporary_EM[PL_id];
    }
}

/** @brief Populate the 2-entry g_state.EM_List from the candidate table for the current VS index. */
static void Setup_EM_List() {
    if (g_state.My_char[g_state.Player_id] == 0) {
        g_state.EM_Candidate[g_state.Player_id][0][9] = 1;
        g_state.EM_Candidate[g_state.Player_id][1][9] = 1;
    } else {
        g_state.EM_Candidate[g_state.Player_id][0][9] = 0;
        g_state.EM_Candidate[g_state.Player_id][1][9] = 0;
    }

    g_state.EM_List[g_state.Player_id][0] = g_state.EM_Candidate[g_state.Player_id][0][g_state.VS_Index[g_state.Player_id]];
    g_state.EM_List[g_state.Player_id][1] = g_state.EM_Candidate[g_state.Player_id][1][g_state.VS_Index[g_state.Player_id]];
}

/** @brief Set COM character, stage, super-arts, and colour; queue BG load. */
static void Setup_Next_Fighter() {
    g_state.paring_counter[g_state.COM_id] = 0;
    g_state.paring_bonus_r[g_state.COM_id] = 0;
    g_state.My_char[g_state.COM_id] = g_state.EM_id;

    if (g_state.EM_id == 17) {
        g_state.Battle_Country = g_state.Q_Country;
        g_state.bg_w.stage = g_state.Q_Country;
    } else {
        g_state.Battle_Country = g_state.EM_id;

        if (g_state.My_char[g_state.Player_id] == 0 && g_state.EM_id == 1) {
            g_state.Battle_Country = 0;
        }

        g_state.bg_w.stage = g_state.Battle_Country;
    }

    if (Debug_w[DEBUG_STAGE_SELECT]) {
        g_state.Battle_Country = g_state.bg_w.stage = Debug_w[DEBUG_STAGE_SELECT] - 1;
    }

    Push_LDREQ_Queue_BG(g_state.bg_w.stage + 0);
    g_state.bg_w.area = 0;
    g_state.Super_Arts[g_state.COM_id] = g_state.Stock_Com_Arts[g_state.Player_id] = Setup_Com_Arts();

    if (Debug_w[DEBUG_CPU_SA]) {
        g_state.Super_Arts[g_state.COM_id] = g_state.bg_w.stage = Debug_w[DEBUG_CPU_SA] - 1;
    }

    Setup_Com_Color();
    Setup_PL_Color(g_state.COM_id, g_state.Com_Color_Shot);
}

const u8 Arts_Rnd_Data[8] = { 0, 0, 0, 1, 1, 1, 2, 2 };

/** @brief Pick a super-art for the CPU (random if none stocked, otherwise use the stocked one). */
static s8 Setup_Com_Arts() {
    if (g_state.EM_id == 0) {
        return 1;
    }

    if (g_state.Stock_Com_Arts[g_state.Player_id] == -1) {
        return Arts_Rnd_Data[random_16() & 7];
    }

    return g_state.Stock_Com_Arts[g_state.Player_id];
}

/** @brief Select the CPU’s costume colour (special colour if g_state.Break_Com flagged). */
static void Setup_Com_Color() {
    g_state.Com_Color_Shot = g_state.Stock_Com_Color[g_state.Player_id];

    if (g_state.Break_Com[g_state.Player_id][g_state.EM_id]) {
        g_state.Com_Color_Shot = 1024;
        return;
    }

    g_state.Com_Color_Shot = 16;
}

/** @brief Determine the player's costume colour based on button held and opponent colour. */
void Setup_PL_Color(s16 PL_id, u16 sw) {
    s8 id_0;
    s8 id_1;
    u16 sw_new;

    sw_new = 0;

    if (g_state.plw[PL_id ^ 1].wu.pl_operator == 0) {
        id_0 = -1;
        id_1 = 1;
    } else {
        id_0 = g_state.My_char[PL_id];
        id_1 = g_state.My_char[PL_id ^ 1];
    }

    if (g_state.Sel_PL_Complete[PL_id ^ 1] == 0) {
        id_0 = 127;
    }

    if (g_state.plw[PL_id].wu.pl_operator != 0 && g_state.My_char[PL_id] == CHAR_GILL) {
        sw_new = 0;
    } else {
        if (Debug_w[DEBUG_NEW_COLOR]) {
            if (PL_id == 0) {
                sw_new = p1sw_0;
            } else {
                sw_new = p2sw_0;
            }
        }

        if (CurrentSave()->PL_Color[PL_id][g_state.My_char[PL_id]]) {
            if (PL_id == 0) {
                sw_new = p1sw_0;
            } else {
                sw_new = p2sw_0;
            }
        }
    }

    if (g_state.My_char[PL_id] == CHAR_GILL) {
        switch (sw) {
        case SWK_WEST:
        case SWK_NORTH:
        case SWK_RIGHT_SHOULDER:
            if (g_state.Player_Color[PL_id ^ 1] == 0 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 1;
            } else {
                g_state.Player_Color[PL_id] = 0;
            }

            break;

        default:
            if (g_state.Player_Color[PL_id ^ 1] == 1 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 0;
            } else {
                g_state.Player_Color[PL_id] = 1;
            }

            break;
        }
    } else if (sw_new & SWK_START) {
        switch (sw) {
        case SWK_WEST:
            if (g_state.Player_Color[PL_id ^ 1] == 7 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 10;
            } else {
                g_state.Player_Color[PL_id] = 7;
            }

            break;

        case SWK_NORTH:
            if (g_state.Player_Color[PL_id ^ 1] == 8 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 11;
            } else {
                g_state.Player_Color[PL_id] = 8;
            }

            break;

        case SWK_RIGHT_SHOULDER:
            if (g_state.Player_Color[PL_id ^ 1] == 9 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 12;
            } else {
                g_state.Player_Color[PL_id] = 9;
            }

            break;

        case SWK_SOUTH:
            if (g_state.Player_Color[PL_id ^ 1] == 10 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 7;
            } else {
                g_state.Player_Color[PL_id] = 10;
            }

            break;

        case SWK_EAST:
            if (g_state.Player_Color[PL_id ^ 1] == 11 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 8;
            } else {
                g_state.Player_Color[PL_id] = 11;
            }

            break;

        default:
            if (g_state.Player_Color[PL_id ^ 1] == 12 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 9;
            } else {
                g_state.Player_Color[PL_id] = 12;
            }

            break;
        }
    } else {
        switch (sw) {
        case SWK_WEST | SWK_RIGHT_SHOULDER | SWK_EAST:
            if (g_state.Player_Color[PL_id ^ 1] == 6 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 0;
            } else {
                g_state.Player_Color[PL_id] = 6;
            }

            break;

        case SWK_WEST:
            if (g_state.Player_Color[PL_id ^ 1] == 0 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 3;
            } else {
                g_state.Player_Color[PL_id] = 0;
            }

            break;

        case SWK_NORTH:
            if (g_state.Player_Color[PL_id ^ 1] == 1 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 4;
            } else {
                g_state.Player_Color[PL_id] = 1;
            }

            break;

        case SWK_RIGHT_SHOULDER:
            if (g_state.Player_Color[PL_id ^ 1] == 2 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 5;
            } else {
                g_state.Player_Color[PL_id] = 2;
            }

            break;

        case SWK_SOUTH:
            if (g_state.Player_Color[PL_id ^ 1] == 3 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 0;
            } else {
                g_state.Player_Color[PL_id] = 3;
            }

            break;

        case SWK_EAST:
            if (g_state.Player_Color[PL_id ^ 1] == 4 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 1;
            } else {
                g_state.Player_Color[PL_id] = 4;
            }

            break;

        default:
            if (g_state.Player_Color[PL_id ^ 1] == 5 && id_0 == id_1) {
                g_state.Player_Color[PL_id] = 2;
            } else {
                g_state.Player_Color[PL_id] = 5;
            }

            break;
        }
    }
}

/** @brief Spawn the regular opponent selection UI objects (name plates, portraits, grade). */
static void Setup_Regular_OBJ(s16 PL_id) {
    s16 em_id;

    if (g_state.VS_Index[g_state.Player_id] < 8) {
        Regular_OBJ_Sub(PL_id, 2);
        Regular_OBJ_Sub(PL_id, 1);
        if (!rmlui_char_select_visible)
            effect_A9_init(16, 5, 10, 0);
        effect_42_init(9);
        effect_42_init(10);
        g_state.Order[9] = 0;
        g_state.Order[10] = 0;
        g_state.Order_Timer[9] = 1;
        g_state.Order_Timer[10] = 1;
        return;
    }

    effect_A9_init(33, g_state.EM_List[PL_id][1], 5, 0);
    effect_A9_init(12, g_state.EM_List[PL_id][1], 21, 0);
    effect_A9_init(57, 0, 22, 0);
    em_id = g_state.EM_List[PL_id][1];

    if (chkNameAkuma(em_id, 1)) {
        em_id = 23;
    }

    effect_A9_init(34, em_id, 20, 0);
}

/** @brief Spawn one set of EM plate objects (name, portrait, cursor arrows). */
static void Regular_OBJ_Sub(s16 PL_id, s16 Dir) {
    s16 ix = Dir - 1;
    s16 x;

    effect_A9_init(33, g_state.EM_List[PL_id][ix], ix + 4, 0);
    x = chkNameAkuma(g_state.EM_List[PL_id][ix], 9);
    effect_A9_init(34, x + g_state.EM_List[PL_id][ix], ix + 6, 0);
    effect_A9_init(12, g_state.EM_List[PL_id][ix], ix + 8, 0);
    effect_E0_init(Dir, 0, 0);
    effect_E0_init(Dir, 1, 0);
}

/** @brief Build the VS history strip showing all previously fought opponents and their grades. */
static void Setup_History_OBJ() {
    s16 q_index = g_state.Break_Com[g_state.Player_id][17];
    s16 xx;
    s16 ix;
    s16 grade;

    effect_A9_init(79, 12, 11, 0);
    g_state.Offset_BG_X[3] = 88;
    effect_A9_init(79, 13, 12, 0);
    g_state.Offset_BG_X[3] += 80;

    for (xx = 0; xx < g_state.VS_Index[g_state.Player_id]; xx++) {
        effect_A9_init(79, 13, 12, 0);
        effect_A9_init(79, xx, 13, 0);
        effect_A9_init(79, 10, 14, 0);
        ix = chkNameAkuma(g_state.EM_History[g_state.Player_id][xx], 6);
        effect_A9_init(81, ix + g_state.EM_History[g_state.Player_id][xx], 15, 0);
        effect_A9_init(12, g_state.EM_History[g_state.Player_id][xx], 16, 0);
        grade = g_state.judge_final[g_state.Player_id][0].vs_cpu_grade[xx];

        if (grade == -1) {
            grade = 0;
        }

        effect_A9_init(80, grade, 17, 0);
        g_state.Offset_BG_X[3] += 88;

        if (q_index == 0 || (q_index - 1) != xx) {
            continue;
        }

        effect_A9_init(79, 13, 12, 0);
        effect_A9_init(81, 17, 15, 0);
        effect_A9_init(12, 17, 16, 0);
        grade = g_state.judge_final[g_state.Player_id]->vs_cpu_grade[15];

        if (grade == -1) {
            grade = 0;
        }

        effect_A9_init(80, grade, 17, 0);
        g_state.Offset_BG_X[3] += 88;
    }

    g_state.Offset_BG_X[3] -= 40;
}

/** @brief Spawn the versus-screen character portraits, name plates, and stage label. */
static void Setup_VS_OBJ(s16 Option) {
    effect_38_init(0, 11, g_state.My_char[0], 1, 0);
    g_state.Order[11] = 3;
    g_state.Order_Timer[11] = 1;
    effect_38_init(1, 12, g_state.My_char[1], 1, 0);
    g_state.Order[12] = 3;
    g_state.Order_Timer[12] = 1;
    effect_K6_init(0, 35, 35, 0);
    g_state.Order[35] = 3;
    g_state.Order_Timer[35] = 1;
    effect_K6_init(1, 36, 35, 0);
    g_state.Order[36] = 3;
    g_state.Order_Timer[36] = 1;
    effect_39_init(0, 17, g_state.My_char[0], 0, 0);
    g_state.Order[17] = 3;
    g_state.Order_Timer[17] = 1;
    effect_39_init(1, 18, g_state.My_char[1], 0, 0);
    g_state.Order[18] = 3;
    g_state.Order_Timer[18] = 1;
    effect_K6_init(0, 29, 29, 0);
    g_state.Order[29] = 3;
    g_state.Order_Timer[29] = 1;
    effect_K6_init(1, 30, 29, 0);
    g_state.Order[30] = 3;
    g_state.Order_Timer[30] = 1;

    if (g_state.My_char[0] != 20) {
        effect_75_init(42, 3, 0);
    }

    g_state.Order[42] = 3;
    g_state.Order_Timer[42] = 1;
    g_state.Order_Dir[42] = 5;

    if (Option == 0) {
        effect_43_init(1, 0);
    }
}

/** @brief Check whether a bonus stage should be played next; set up stage/player if so. */
s8 Check_Bonus_Stage() {
    Setup_ID();
    g_state.Bonus_Type = Check_Bonus_Type();

    if (g_state.Bonus_Type == 0) {
        return 0;
    }

    g_state.bg_w.stage = g_state.Bonus_Type;
    g_state.bg_w.area = 0;

    if (g_state.Bonus_Type == 21) {
        g_state.My_char[g_state.COM_id] = 0xC;
    } else {
        g_state.My_char[g_state.COM_id] = g_state.My_char[g_state.Player_id];
    }

    Setup_Com_Color();
    Setup_PL_Color(g_state.COM_id, g_state.Com_Color_Shot);
    Push_LDREQ_Queue_Player(g_state.COM_id, g_state.My_char[g_state.COM_id]);
    Push_LDREQ_Queue_BG(g_state.Bonus_Type + 0);
    return g_state.Completion_Bonus[g_state.Player_id][g_state.Bonus_Type - 20] = 1;
}

/** @brief Return the bonus stage g_state.ID (20 or 21) if one is available, else 0. */
static s8 Check_Bonus_Type() {
    if (Debug_w[DEBUG_BONUS_CHECK] != 0) {
        if (Debug_w[DEBUG_BONUS_CHECK] == 1) {
            g_state.Completion_Bonus[g_state.Player_id][0] = 0;
            return 20;
        }

        if (Debug_w[DEBUG_BONUS_CHECK] == 2) {
            g_state.Completion_Bonus[g_state.Player_id][1] = 0;
            return 21;
        }

        return 0;
    }

    if (CurrentSave()->extra_option.contents[0][5] == 0) {
        return 0;
    }

    if (g_state.VS_Index[g_state.Player_id] >= 6) {
        if (g_state.Completion_Bonus[g_state.Player_id][1] & 0x80) {
            return 0;
        }

        return 21;
    }

    if (g_state.VS_Index[g_state.Player_id] >= 3) {
        if (g_state.Completion_Bonus[g_state.Player_id][0] & 0x80) {
            return 0;
        }

        return 20;
    }

    return 0;
}

/** @brief Spawn 4 stage-direction indicator objects at the given direction step. */
static void Setup_Next_Stage(s16 dir_step) {
    s16 ix;

    for (ix = 0; ix < 4; ix++) {
        effect_A9_init(dir_step, ix, ix + 23, 0);
    }
}

/** @brief If a player presses any attack button, decrement the scroll-cut counter. */
static void Check_Auto_Cut() {
    if (!Auto_Cut_Sub()) {
        return;
    }

    if ((g_state.Cut_Scroll -= 1) < 0) {
        g_state.Cut_Scroll = 0;
    }
}

/** @brief Return 1 if any human operator pressed an attack button this frame. */
s32 Auto_Cut_Sub() {
    if (g_state.plw[0].wu.pl_operator && ~p1sw_1 & p1sw_0 & 0xFF0) {
        return 1;
    }

    if (g_state.plw[1].wu.pl_operator && ~p2sw_1 & p2sw_0 & 0xFF0) {
        return 1;
    }

    return 0;
}
