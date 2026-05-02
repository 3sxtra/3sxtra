/**
 * @file Game.c
 * @brief Master game state machine.
 *
 * Controls the entire game flow through 13 numbered states (Game00–Game12):
 * title screen, character select, fight, win/loss, continue, ending,
 * ranking, demo loop, bonus stages, and more.
 *
 * The top-level entry point is Game_Task(), called once per frame by the
 * task scheduler. It dispatches to Wait_Auto_Load, Loop_Demo, or Game()
 * based on g_state.G_No[0].
 *
 * Part of the game core module.
 * Originally from the PS2 game module.
 */

#include "sf33rd/Source/Game/game.h"
#include "game_state.h"
#include "sf33rd/Source/Game/system/country_region.h"
#include "port/menu_task.h"
#include "port/init_task.h"
#include "port/task_api.h"
#include "common.h"

/* Phase 3 RmlUi bypass */
#include "main.h"
#include "port/menu_screen.h"
#include "port/sdl/rmlui/rmlui_attract_overlay.h"
#include "port/sdl/rmlui/rmlui_char_select.h"
#include "port/sdl/rmlui/rmlui_continue.h"
#include "port/sdl/rmlui/rmlui_copyright.h"
#include "port/sdl/rmlui/rmlui_gameover.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui/rmlui_title_screen.h"
#include "port/sdl/rmlui/rmlui_win_screen.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/demo/demo00.h"
#include "sf33rd/Source/Game/demo/demo01.h"
#include "sf33rd/Source/Game/demo/demo02.h"
#include "sf33rd/Source/Game/effect/eff35.h"
#include "sf33rd/Source/Game/effect/eff58.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effj2.h"
#include "sf33rd/Source/Game/ending/end_main.h"
#include "sf33rd/Source/Game/engine/bbbscom.h"
#include "sf33rd/Source/Game/engine/cmb_win.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/manage.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/plcnt2.h"
#include "sf33rd/Source/Game/engine/plcnt3.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/spgauge.h"
#include "sf33rd/Source/Game/engine/stun.h"
#include "sf33rd/Source/Game/engine/vital.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/menu/menu.h"
#include "sf33rd/Source/Game/opening/op_sub.h"
#include "sf33rd/Source/Game/opening/opening.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/training/trials.h"
#include "port/I_System.h"
#include "sf33rd/Source/Game/fsm.h"
#include <stdbool.h>
#include <stdio.h>

#include "netplay/netplay.h"

#include "port/tracy_zones.h"

/* === Named Constants === */
/* MAIN_STATE_COUNT and GAME_STATE_COUNT now provided by fsm.h enums */
#define DEMO_TIMEOUT_FRAMES 1800 /**< Attract-mode demo timeout: 30 sec × 60 fps */
#define CONTROL_TIME_DEFAULT 481 /**< Default control time for demo/match start */
#define MAX_VITALITY_DEFAULT 160 /**< Default maximum vitality value */

#include "sf33rd/Source/Game/rendering/mmtmcnt.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/screen/continue.h"
#include "sf33rd/Source/Game/screen/entry.h"
#include "sf33rd/Source/Game/screen/gameover.h"
#include "sf33rd/Source/Game/screen/next_cpu.h"
#include "sf33rd/Source/Game/screen/ranking.h"
#include "sf33rd/Source/Game/screen/sel_pl.h"
#include "sf33rd/Source/Game/screen/win.h"
#include "sf33rd/Source/Game/select_timer.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/stage/ta_sub.h"
#include "sf33rd/Source/Game/stage/tate00.h"
#include "sf33rd/Source/Game/system/reset.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sys_sub2.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/count.h"
#include "sf33rd/Source/Game/ui/flash_lp.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "structs.h"

void Wait_Auto_Load(struct _TASK* unused1);
void Loop_Demo(struct _TASK* unused1);
void Game(struct _TASK* task_ptr);
void Game00();
void Game01();
void Game02();
void Game03();
void Game04();
void Game05();
void Game06();
void Game07();
void Game08();
void Game09();
void Game10();
void Game11();
void Game12();
void Check_Back_Demo();
void Game0_0();
void Game0_1();
void Game0_2();
void Next_Demo_Loop();
void Game12_0();
void Game12_1();
void Game12_2();
void Game2_0();
void Game2_1();
void Game2_2();
void Game2_3();
void Game2_4();
void Game2_5();
void Game2_6();
void Game2_7();
void Time_Control();
static s32 Check_Disp_Ranking();
s32 Disp_Ranking();
void Request_Break_Sub(s16 PL_id);
s16 Disp_Rank_Sub(s16 PL_id);
static s16 Bonus_Sub();
s16 Ck_Coin();
void Loop_Demo_Sub();
void Before_Select_Sub();
static void Set_Appear_Type_For_Mode() {
    g_state.appear_type = Is_Training_Mode(g_state.Mode_Type) ? APPEAR_TYPE_NON_ANIMATED : APPEAR_TYPE_ANIMATED;
}

/**
 * @brief Performs a single simulation tick.
 *
 * Extracted from Game_Task to reduce function length and improve readability.
 * Handles game state updates, rendering prep, and subsystem ticks.
 *
 * @param task_ptr Task system pointer passed down to handlers.
 * @param is_last_frame 1 if this is the final tick of the frame (controls rendering trans flag).
 */
static void Game_UpdateFrame(struct _TASK* task_ptr, s32 is_last_frame) {
    if (is_last_frame) {
        No_Trans = 0;
    } else {
        No_Trans = 1;
    }

    g_state.Play_Game = 0;

    if (g_state.Game_pause != 0x81) {
        g_state.system_timer += 1;
    }

    init_texcash_before_process();

    seqsBeforeProcess();

    if (nowSoftReset() == 0) {
        switch (g_state.G_No[0]) {
        case MAIN_STATE_WAIT_AUTO_LOAD: Wait_Auto_Load(task_ptr); break;
        case MAIN_STATE_LOOP_DEMO:     Loop_Demo(task_ptr);      break;
        case MAIN_STATE_GAME:          Game(task_ptr);            break;
        default: break;
        }
    }

    seqsAfterProcess();

    texture_cash_update();

    move_pulpul_work();

    Check_LDREQ_Queue();
}

/**
 * @brief Main game task — top-level per-frame entry point.
 *
 * Dispatches to one of three modes based on g_state.G_No[0]:
 *   0 = Wait_Auto_Load (idle while loading)
 *   1 = Loop_Demo (attract-mode demo loop)
 *   2 = Game (active gameplay)
 *
 * Also handles fast-forward (sysFF) by running multiple iterations per frame.
 */
void Game_Task(struct _TASK* task_ptr) {
    s16 ix;
    s16 ff;

    HUD_Shift_Init();
    init_color_trans_req();
    ff = sysFF;

    for (ix = 0; ix < ff; ix++) {
        Game_UpdateFrame(task_ptr, (ix == ff - 1));
    }

    Check_Check_Screen();
    Check_Pos_BG();
    Disp_Sound_Code();
}

/**
 * @brief Game state dispatcher — routes to Game00–Game12 based on g_state.G_No[1].
 *
 * Also sets g_state.Play_Game flag (1=fight, 2=ending) for subsystems that need
 * to know whether active gameplay is in progress.
 */
void Game(struct _TASK* task_ptr) {
    (void)task_ptr;

    // Safety bounds check
    if (g_state.G_No[1] >= GAME_STATE_COUNT) {
        I_Error("Game(): gs_G_No[1]=%d is out of bounds [0-%d)!", g_state.G_No[1], GAME_STATE_COUNT);
        return;
    }

    if (g_state.G_No[1] == MODE_FIGHT || g_state.G_No[1] == MODE_STAFF_ROLL) {
        g_state.Play_Game = 1;
    } else if (g_state.G_No[1] == MODE_ENDING) {
        g_state.Play_Game = 2;
    }

    switch (g_state.G_No[1]) {
    case MODE_TITLE:      Game00(); break;
    case MODE_ATTRACT:    Game01(); break;
    case MODE_FIGHT:      Game02(); break;
    case MODE_VS_SCREEN:  Game03(); break;
    case MODE_WIN_QUOTE:  Game04(); break;
    case MODE_CONTINUE:   Game05(); break;
    case MODE_GAME_OVER:  Game06(); break;
    case MODE_RANKING:    Game07(); break;
    case MODE_ENDING:     Game08(); break;
    case MODE_STAFF_ROLL: Game09(); break;
    case MODE_TRAINING:   Game10(); break;
    case MODE_OPTIONS:    Game11(); break;
    case MODE_CHALLENGE:  Game12(); break;
    default: break;
    }
}

/** @brief State 0: Title screen — logo display, copyright, transition to menu. */
void Game00() {
    if (g_state.G_No[2] >= 3) {
        return;
    }

    switch (g_state.G_No[2]) {
    case 0: Game0_0(); break;
    case 1: Game0_1(); break;
    case 2: Game0_2(); break;
    default: break;
    }
    // njSetBackColor(0, 0, 0);
    BG_Draw_System();
    Basic_Sub();
    Check_Back_Demo();
}

void Game0_0() {
    if (Title_At_a_Dash() != 0) {
        FSM_AdvanceSubState();
    }
}

void Game0_1() {
    Disp_Copyright();
    TITLE_Move(1);

    if (g_state.Request_G_No) {
        FSM_AdvanceSubState();
    }
}

void Game0_2() {
    switch (g_state.G_No[3]) {
    case 0:
        Disp_Copyright();
        TITLE_Move(1);
        FSM_AdvanceSubSubState();
        Switch_Screen_Init(1);
        break;

    case 1:
        if (Switch_Screen(1) != 0) {
            FSM_AdvanceSubSubState();
            g_state.Cover_Timer = 23;
            return;
        }

        TITLE_Move(1);
        Disp_Copyright();
        break;

    case 2:
        FadeOut(1, 0xFF, 8);
        FSM_AdvanceSubSubState();
        break;

    case 3:
        FadeOut(1, 0xFF, 8);
        FSM_AdvanceSubSubState();
        TexRelease(601);
        title_tex_flag = 0;
        break;

    case 4:
        FadeOut(1, 0xFF, 8);
        FSM_AdvanceSubSubState();
        Purge_mmtm_area(2);
        Make_texcash_of_list(2);
        break;

    case 5:
        FadeOut(1, 0xFF, 8);
        BGM_Request(65);
        FSM_SetMode(MODE_CHALLENGE);
        cpReadyTask(TASK_MENU, Menu_Task);
        break;
    }
}

/** @brief Check if the attract-mode demo timer has expired (1800 frames = 30 seconds). */
void Check_Back_Demo() {
    if (++g_state.G_Timer < DEMO_TIMEOUT_FRAMES) {
        return;
    }

    if (g_state.G_No[1] == MODE_CHALLENGE || (g_state.G_No[2] == 2 && g_state.G_No[3] >= 2)) {
        return;
    }

    TexRelease(601);
    title_tex_flag = 0;

    /* Hide RmlUi title screen before switching to demo loop */
    if (use_rmlui && rmlui_screen_title) {
        rmlui_title_screen_hide();
    }
    if (use_rmlui && rmlui_screen_attract_overlay) {
        rmlui_attract_overlay_hide();
    }

    Next_Demo_Loop();
    effect_work_init();
}

/** @brief State 12: Screen transition to character select (from menu). */
void Game12() {
    if (g_state.G_No[2] >= 3) {
        return;
    }

    switch (g_state.G_No[2]) {
    case 0: Game12_0(); break;
    case 1: Game12_1(); break;
    case 2: Game12_2(); break;
    default: break;
    }
    BG_Draw_System();
    Basic_Sub();
    bg_pos_hosei_sub2(0);
    bg_pos_hosei_sub2(1);
    bg_pos_hosei_sub2(2);
    Bg_Family_Set_appoint(0);
    Bg_Family_Set_appoint(1);
    Bg_Family_Set_appoint(2);
    BG_move_Ex(0);
}

void Game12_0() {
    // Do nothing
}

void Game12_1() {
    FSM_AdvanceSubState();
    Switch_Screen_Init(1);
    SsBgmFadeOut(0x1000);
}

void Game12_2() {
    if (!Switch_Screen(1)) {
        // Transition is still running, can't proceed
        return;
    }

    // Proceed to character select
    FSM_SetMode(MODE_ATTRACT);
    g_state.Control_Time = CONTROL_TIME_DEFAULT;
    g_state.Cover_Timer = 23;
    effect_work_init();
    cpExitTask(TASK_MENU);
}

/** @brief State 1: Character select — player picks character, super art, and color. */
void Game01() {
    BG_Draw_System();
    Basic_Sub();
    Setup_Play_Type();
    SelectTimer_Run();

    switch (g_state.G_No[2]) {
    case 0:
        /* Hide any leftover Phase 3 docs (win, continue, gameover, etc.)
         * The attract_overlay is excluded from hide-all and managed separately. */
        if (use_rmlui)
            rmlui_wrapper_hide_all_game_documents();
        // The menu task resets g_state.Mode_Type to MODE_ARCADE between matches.
        // Restore it so all MODE_NETWORK-guarded paths (RNG seeding, Game2_0
        // initialization) work correctly for rematches.
        if (Netplay_GetSessionState() == NETPLAY_SESSION_RUNNING) {
            g_state.Mode_Type = MODE_NETWORK;
        }
        Switch_Screen(1);
        FSM_AdvanceSubState();
        g_state.S_No[0] = 0;
        g_state.S_No[1] = 0;
        g_state.S_No[2] = 0;
        g_state.S_No[3] = 0;
        SsBgmHalfVolume(0);

        if (g_state.Mode_Type == MODE_ARCADE) {
            BGM_Request(53);
        } else {
            BGM_Request(66);
        }

        g_state.Break_Into = 0;
        g_state.Stop_Combo = 0;

        if (g_state.Mode_Type != MODE_NETWORK) {
            g_state.Random_ix32 = Interrupt_Timer;
            g_state.Random_ix32_ex = Interrupt_Timer;
        } else {
            Setup_Net_Random_ix();
            All_Clear_Timer();
        }

        init_slow_flag();
        System_all_clear_Level_B();
        pulpul_stop();
        init_pulpul_work();
        break;

    case 1:
        Switch_Screen(1);
        FSM_AdvanceSubState();
        break;

    case 2:
        if (Select_Player()) {
            FSM_AdvanceSubState();
            g_state.Bonus_Game_Flag = 0;
            Switch_Screen_Init(0);
        }

        break;

    default:
        Select_Player();

        if (Switch_Screen(0) != 0) {
            Game01_Sub();
            g_state.Cover_Timer = 5;
            Set_Appear_Type_For_Mode();
            set_hitmark_color();

            if (Debug_w[DEBUG_MY_CHAR_PL1]) {
                g_state.My_char[0] = Debug_w[DEBUG_MY_CHAR_PL1] - 1;
            }

            if (Debug_w[DEBUG_MY_CHAR_PL2]) {
                g_state.My_char[1] = Debug_w[DEBUG_MY_CHAR_PL2] - 1;
            }

            Purge_texcash_of_list(3);
            Make_texcash_of_list(3);

            if (g_state.Demo_Flag) {
                FSM_SetMode(MODE_FIGHT);
                g_state.E_No[0] = 4;
                g_state.E_No[1] = 0;
                g_state.E_No[2] = 0;
                g_state.E_No[3] = 0;
            } else {
                g_state.Demo_Time_Stop = 1;
                g_state.plw[0].wu.pl_operator = 0;
                g_state.Operator_Status[0] = 0;
                g_state.plw[1].wu.pl_operator = 0;
                g_state.Operator_Status[1] = 0;
            }

            if (g_state.plw[0].wu.pl_operator != 0) {
                g_state.Sel_Arts_Complete[0] = -1;
            }

            if (g_state.plw[1].wu.pl_operator != 0) {
                g_state.Sel_Arts_Complete[1] = -1;
            }

            if ((g_state.plw[0].wu.pl_operator != 0) && (g_state.plw[1].wu.pl_operator != 0)) {
                g_state.Play_Type = 1;
            } else {
                g_state.Play_Type = 0;
            }
        }

        break;
    }

    BG_move();
}

/** @brief State 2: Main fight — round setup, in-match gameplay, round transitions. */
void Game02() {
    g_state.Scene_Cut = Cut_Cut_Cut();

    if (g_state.G_No[2] >= 8) {
        return;
    }

    switch (g_state.G_No[2]) {
    case 0: Game2_0(); break;
    case 1: Game2_1(); break;
    case 2: Game2_2(); break;
    case 3: Game2_3(); break;
    case 4: Game2_4(); break;
    case 5: Game2_5(); break;
    case 6: Game2_6(); break;
    case 7: Game2_7(); break;
    default: break;
    }
    BG_move_Ex(3);
}

void Game2_0() {
    s16 ix;

    BG_Draw_System();
    Switch_Screen(0);

    if (Check_LDREQ_Clear() == 0) {
        return;
    }

    System_all_clear_Level_B();

    switch (g_state.Mode_Type) {
    case MODE_ARCADE:
        g_state.Play_Mode = 0;
        g_state.Replay_Status[0] = 0;
        g_state.Replay_Status[1] = 0;
        break;

    case MODE_VERSUS:
        for (ix = 0; ix < 2; ix++) {
            if (save_w[SAVEW_ARCADE].Partner_Type[ix]) {
                g_state.plw[ix].wu.pl_operator = 0;
                g_state.Operator_Status[ix] = 0;
            }
        }

        cpExitTask(TASK_ENTRY);
        /* fallthrough */

    case MODE_NETWORK:
        g_state.Play_Mode = 1;
        All_Clear_Random_ix();
        All_Clear_Timer();
        All_Clear_ETC();
        break;

    case MODE_REPLAY:
        g_state.Play_Mode = 3;
        All_Clear_Timer();
        break;

    default:
        // Do nothing
        break;
    }

    Check_Replay();

    if (g_state.Demo_Flag == 0) {
        g_state.Play_Mode = 0;
        g_state.Replay_Status[0] = 0;
        g_state.Replay_Status[1] = 0;
    }

    g_state.Game_difficulty = 15;
    g_state.Game_pause = 0;
    g_state.Demo_Time_Stop = 0;
    g_state.C_No[0] = 0;
    g_state.C_No[1] = 0;
    g_state.C_No[2] = 0;
    g_state.C_No[3] = 0;
    FSM_SetSubState(6);
    g_state.G_Timer = 10;
    g_state.Round_num = 0;
    g_state.Keep_Grade[0] = 0;
    g_state.Keep_Grade[1] = 0;

    if (g_state.Win_Record[0]) {
        g_state.Keep_Grade[0] = grade_get_my_grade(0) + 1;
    }

    if (g_state.Win_Record[1]) {
        g_state.Keep_Grade[1] = grade_get_my_grade(1) + 1;
    }

    g_state.Allow_a_battle_f = 0;
    g_state.Time_in_Time = 60;
    init_slow_flag();
    clear_hit_queue();
    g_state.pcon_rno[0] = g_state.pcon_rno[1] = g_state.pcon_rno[2] = g_state.pcon_rno[3] = 0;
    ca_check_flag = 1;
    bg_work_clear();
    win_lose_work_clear();
    player_face_init();
}

/** @brief In-fight per-frame update: player control, rendering, HUD, hit detection. */
void Game2_1() {
    mpp_w.inGame = true;

    if (g_state.Game_pause != 0x81) {
        g_state.Game_timer += 1;
    }

    set_EXE_flag();
    ppgPurgeFromVRAM(5);

    if (g_state.Disp_Cockpit) {
        Time_Control();
    }

    Player_control();

    if (g_state.Disp_Cockpit) {
        vital_cont_main();
        combo_cont_main();
    }

    TATE00();
    Game_Management();
    BG_Draw_System();
    ppgPurgeFromVRAM(4);
    reqPlayerDraw();
    Basic_Sub_Ex();

    if (g_state.Disp_Cockpit) {
        if (!use_rmlui || !rmlui_hud_faces)
            player_face();
        if (!use_rmlui || !rmlui_hud_names)
            player_name();
        stngauge_cont_main();
        spgauge_cont_main();
        if (!use_rmlui || !rmlui_hud_super)
            Sa_frame_Write();
        if (!use_rmlui || !rmlui_hud_score)
            Score_Sub();
        if (!use_rmlui || !rmlui_hud_wins)
            Flash_Lamp();
        if (!use_rmlui || !rmlui_hud_wins)
            Disp_Win_Record();
    }

    ppgPurgeFromVRAM(0);
    hit_check_main_process();
}

void Game2_2() {
    s16 i;

    BG_Draw_System();
    Switch_Screen(0);

    if (Check_LDREQ_Clear() == 0) {
        return;
    }

    SsBgmHalfVolume(0);
    All_Clear_Timer();
    Check_Replay();
    g_state.Game_difficulty = 15;
    g_state.Game_timer = 0;
    g_state.Game_pause = 0;
    g_state.Demo_Time_Stop = 0;
    g_state.C_No[0] = 0;
    g_state.C_No[1] = 0;
    g_state.C_No[2] = 0;
    g_state.C_No[3] = 0;
    g_state.G_Timer = 10;
    g_state.Round_num = 0;
    g_state.Keep_Grade[0] = 0;
    g_state.Keep_Grade[1] = 0;

    if (g_state.Win_Record[0]) {
        g_state.Keep_Grade[0] = grade_get_my_grade(0) + 1;
    }

    if (g_state.Win_Record[1]) {
        g_state.Keep_Grade[1] = grade_get_my_grade(1) + 1;
    }

    g_state.Allow_a_battle_f = 0;
    g_state.Time_in_Time = 60;
    init_slow_flag();
    effect_work_quick_init();
    clear_hit_queue();
    g_state.pcon_rno[0] = g_state.pcon_rno[1] = g_state.pcon_rno[2] = g_state.pcon_rno[3] = 0;
    ca_check_flag = 1;
    bg_work_clear();
    win_lose_work_clear();
    player_face_init();
    Game01_Sub();
    Set_Appear_Type_For_Mode();
    TATE00();

    for (i = 0; i < 3; i++) {
        if (stage_bgw_number[g_state.bg_w.stage][i] > 0) {
            Bg_On_R(1 << i);
        }
    }

    if (g_state.bg_w.stage == 7) {
        Bg_On_R(4);
    }

    FSM_SetSubState(7);
}

void Game2_3() {
    Game2_1();

    if (--g_state.G_Timer == 0) {
        FSM_SetSubState(1);
        Clear_Flash_No();
    }
}

void Game2_4() {
    BG_Draw_System();
}

void Game2_5() {
    BG_Draw_System();

    switch (g_state.G_No[3]) {
    case 0:
        Switch_Screen(0);
        FSM_AdvanceSubSubState();
        g_state.Stop_Update_Score = 0;
        vital_cont_init();
        count_cont_init(0);
        stngauge_cont_init();
        stngauge_work_clear();
        combo_cont_init();
        count_cont_init(1);
        g_state.Score[0][2] = 0;
        g_state.Score[1][2] = 0;
        g_state.Suicide[0] = 1;
        g_state.Game_pause = 0;
        g_state.pcon_rno[0] = 0;
        g_state.pcon_rno[1] = 0;
        g_state.pcon_rno[2] = 0;
        g_state.pcon_rno[3] = 0;
        g_state.appear_type = APPEAR_TYPE_NON_ANIMATED;
        erase_extra_plef_work();
        compel_bg_init_position();
        win_lose_work_clear();
        TATE00();
        break;

    default:
        Game2_1();

        if (--g_state.G_Timer == 0) {
            FSM_SetSubState(1);
            Clear_Flash_No();
        }

        break;
    }
}

void Game2_6() {
    BG_Draw_System();
    Switch_Screen(0);

    if (Wait_Seek_Time() != 0) {
        FSM_SetSubState(3);
        TATE00();
    }
}

void Game2_7() {
    BG_Draw_System();
    Switch_Screen(0);

    if (Wait_Seek_Time() != 0) {
        FSM_SetSubState(3);
    }
}

/**
 * @brief Shared initialization for a new round/match.
 *
 * Resets HUD, vitals, scores, combo state, super gauge, stun gauge,
 * and win counters for both players.
 */
void Game01_Sub() {
    g_state.Disp_Cockpit = 0;
    g_state.Stop_Update_Score = 0;
    vital_cont_init();
    // Initialize time limit from local settings once (synchronized for netplay)
    Time_Limit = CurrentSave()->Time_Limit;
    count_cont_init(0);
    g_state.Score[0][1] = 0;
    g_state.Score[0][2] = 0;
    g_state.Score[1][1] = 0;
    g_state.Score[1][2] = 0;
    g_state.PL_Wins[0] = 0;
    g_state.PL_Wins[1] = 0;
    combo_cont_init();
    Clear_Win_Type();
    g_state.Lamp_No = 0;
    set_kizetsu_status(0);
    set_kizetsu_status(1);
    set_super_arts_status(0);
    set_super_arts_status(1);

    if (g_state.Demo_Flag && (sag_ikinari_max() != 0)) {
        spgauge_cont_init();
    } else {
        spgauge_cont_demo_init();
    }

    stngauge_cont_init();
    trials_init();
}

/** @brief State 3: Win/loss result — winner scene, rankings, mode-specific branching. */
void Game03() {
    BG_Draw_System();
    move_effect_work(4);
    move_effect_work(5);
    g_state.Play_Mode = 0;
    g_state.Replay_Status[0] = 0;
    g_state.Replay_Status[1] = 0;

    switch (g_state.G_No[2]) {
    case 0:
        if (Winner_Scene() != 0) {
            /* Hide win screen before transitioning to next state */
            if (use_rmlui && rmlui_screen_winner)
                rmlui_win_screen_hide();
            switch (g_state.Mode_Type) {
            case MODE_VERSUS:
            case MODE_NETWORK:
                FSM_AdvanceSubState();
                Rep_Game_Infor[10].play_type = 1;
                Rep_Game_Infor[10].winner = g_state.Winner_id;
                Switch_Screen_Init(0);

                if (g_state.Country == COUNTRY_ASIA) {
                    Rep_Game_Infor[10].play_type = 4;
                }

                break;

            case MODE_REPLAY:
                FSM_SetSubState(5);
                cpReadyTask(TASK_MENU, Menu_Task);
                MenuTask_SetPhase(MTP_SCREEN_DISPATCH);
                break;

            default:
                FSM_SetMode(MODE_CONTINUE);
                g_state.E_No[0] = 9;
                g_state.E_No[1] = 0;
                g_state.E_No[2] = 0;
                g_state.E_No[3] = 0;

                if (g_state.Battle_Q[g_state.WINNER]) {
                    FSM_SetMode(MODE_OPTIONS);
                    FSM_SetSubState(3);
                }

                g_state.Cover_Timer = 24;

                if (g_state.Round_Operator[g_state.LOSER]) {
                    g_state.E_Number[g_state.LOSER][0] = 1;
                    g_state.E_Number[g_state.LOSER][1] = 0;
                    g_state.E_Number[g_state.LOSER][2] = 0;
                    g_state.E_Number[g_state.LOSER][3] = 0;
                }

                break;
            }
        }

        break;

    case 1:
        if (Switch_Screen(1) != 0) {
            FSM_AdvanceSubState();
            g_state.E_No[0] = 1;
            g_state.E_No[1] = 2;
            g_state.E_No[2] = 2;
            g_state.E_No[3] = 0;
            g_state.Request_E_No = 0;
            cpReadyTask(TASK_MENU, Menu_Task);
            MenuTask_SetSubPhase(MTSP_SA_CUT);
            g_state.Cursor_Y_Pos[0][0] = 0;
            g_state.Cursor_Y_Pos[1][0] = 0;
            g_state.G_Timer = 4;
        }

        break;

    case 2:
        Switch_Screen(1);

        if (--g_state.G_Timer == 0) {
            g_state.Cover_Timer = 10;
            FSM_SetMode(MODE_CHALLENGE);
        }

        break;

    case 3:
        if (Switch_Screen(1) != 0) {
            FSM_AdvanceSubState();
            Saver2_Task_SetPhase(1);
            g_state.G_Timer = 4;
        }

        break;

    case 4:
        Switch_Screen(1);

        if (--g_state.G_Timer == 0) {
            // Do nothing
        }

        break;

    case 5:
        // Do nothing
        break;
    }

    BG_move();
}

/** @brief State 4: Loser scene — triggers continue or game-over flow. */
void Game04() {
    s16 i;

    BG_Draw_System();
    move_effect_work(4);
    move_effect_work(5);

    switch (g_state.G_No[2]) {
    case 0:
        if (Loser_Scene() != 0) {
            /* Hide win/loser screen before transitioning */
            if (use_rmlui && rmlui_screen_winner)
                rmlui_win_screen_hide();
            if (g_state.Mode_Type == 5) {
                FSM_SetSubState(5);
                cpReadyTask(TASK_MENU, Menu_Task);
                MenuTask_SetPhase(MTP_SCREEN_DISPATCH);
            } else {
                FSM_SetMode(MODE_RANKING);
                g_state.E_No[0] = 7;
                g_state.Cont_No[0] = 0;
                g_state.E_Number[g_state.LOSER][0] = 1;

                for (i = 1; i < 4; i++) {
                    g_state.E_No[i] = 0;
                    g_state.Cont_No[i] = 0;
                    g_state.E_Number[g_state.LOSER][i] = 0;
                }
            }
        }

        break;

    default:
        // Do nothing
        break;
    }

    BG_move();
}

/** @brief State 5: Next CPU opponent selection in arcade mode. */
void Game05() {
    BG_Draw_System();
    Basic_Sub();
    Setup_Play_Type();

    switch (g_state.G_No[2]) {
    case 0:
        FSM_AdvanceSubState();
        g_state.SC_No[0] = 0;
        g_state.SC_No[1] = 0;
        g_state.SC_No[2] = 0;
        g_state.SC_No[3] = 0;

        if (Check_Bonus_Stage()) {
            g_state.SC_No[0] = 6;
        }

        g_state.Stop_Combo = 0;
        init_slow_flag();
        pulpul_stop();
        break;

    case 1:
        if (Next_CPU()) {
            FSM_AdvanceSubState();
            Switch_Screen_Init(0);
        }

        break;

    default:
        Next_CPU();

        if (Switch_Screen(0) != 0) {
            g_state.Cover_Timer = 24;
            Purge_texcash_of_list(3);
            Make_texcash_of_list(3);

            if (g_state.Bonus_Type == 0) {
                Game01_Sub();
            }

            BGM_Stop();

            if (g_state.Bonus_Type == 0) {
                FSM_SetMode(MODE_FIGHT);
                // g_state.G_No[2] zeroed by FSM_SetMode
                g_state.E_No[0] = 4;
                g_state.E_No[1] = 0;
                g_state.E_No[2] = 0;
                g_state.E_No[3] = 0;
                g_state.Bonus_Game_Flag = 0;
            } else {
                FSM_SetMode(MODE_STAFF_ROLL);
                g_state.E_No[0] = 4;
                g_state.E_No[1] = 0;
                g_state.E_No[2] = 0;
                g_state.E_No[3] = 0;
            }
        }

        break;
    }

    BG_move();
}

/** @brief State 6: Game Over — ranking check, auto-save, return to attract. */
void Game06() {
    s16 xx;

    BG_Draw_System();
    Basic_Sub_Ex();

    if (!g_state.Break_Into) {
        switch (g_state.G_No[2]) {
        case 0:
            FSM_AdvanceSubState();
            g_state.Game_pause = 0;
            g_state.Stock_Com_Color[g_state.Player_id] = -1;
            g_state.Stock_Com_Arts[g_state.Player_id] = -1;
            g_state.Last_Player_id = -1;
            g_state.Control_Time = CONTROL_TIME_DEFAULT;
            g_state.E_No[0] = 8;
            g_state.E_No[1] = 0;
            g_state.E_No[2] = 0;
            g_state.E_No[3] = 0;

            for (xx = 0; xx < 4; xx++) {
                g_state.GO_No[xx] = 0;
            }

            make_texcash_work(13);
            break;

        case 1:
            if (Game_Over()) {
                /* Hide gameover screen once the game-over flow completes */
                if (use_rmlui && rmlui_screen_gameover)
                    rmlui_gameover_hide();
                g_state.G_Timer = 60;

                if (Check_Disp_Ranking() != 0) {
                    FSM_AdvanceSubState();
                } else {
                    FSM_SetSubState(3);
                }
            }

            break;

        case 2:
            if (Disp_Ranking() != 0) {
                FSM_AdvanceSubState();
                g_state.G_Timer = 1;
            }

            break;

        case 3:
            if (--g_state.G_Timer == 0) {
                FSM_AdvanceSubState();
                Clear_Disp_Ranking(0);
                Clear_Disp_Ranking(1);
                Switch_Screen_Init(1);
            }

            break;

        case 4:
            if (Switch_Screen(1) != 0) {
                g_state.Cover_Timer = 24;
                g_state.Forbid_Break = 0;
                Clear_Flash_No();
                Clear_Personal_Data(g_state.LOSER);
                grade_check_work_1st_init(g_state.LOSER, 0);
                grade_check_work_1st_init(g_state.LOSER, 1);

                if (g_state.Request_Break[0] != 0 || g_state.Request_Break[1] != 0) {
                    Request_Break_Sub(0);
                    Request_Break_Sub(1);
                    FSM_SetMode(MODE_ATTRACT);
                    g_state.E_No[0] = 2;
                    g_state.E_No[1] = 0;
                    g_state.E_No[2] = 0;
                    g_state.E_No[3] = 0;
                    break;
                }

                for (xx = 0; xx < 20; xx++) {
                    CurrentSave()->Ranking[xx] = Ranking_Data[xx];
                }

                if (CurrentSave()->Auto_Save) {
                    FSM_SetSubState(5);
                    g_state.G_Timer = 4;
                    g_state.Pause_ID = g_state.Player_id;
                    cpReadyTask(TASK_MENU, Menu_Task);
                    System_all_clear_Level_B();
                    Menu_Init(MenuTask_GetTaskPtr());
                    MenuTask_GotoPhase(MTP_GOTO_GAME);
                    g_state.Forbid_Reset = 1;
                    make_texcash_work(12);
                    g_state.Unsubstantial_BG[0] = 1;
                    Copy_Check_w();
                    cpExitTask(TASK_SAVER);
                } else {
                    FSM_SetSubState(6);
                }
            }

            break;

        case 5:
            if (g_state.G_No[3] == 0) {
                FadeOut(1, 0xFF, 8);

                if (--g_state.G_Timer == 0) {
                    g_state.G_No[3] = 1;
                }
            }

            break;

        case 6:
            Switch_Screen(1);
            FSM_SetMainState(MAIN_STATE_LOOP_DEMO);
            /* 0x63 is an arcade sentinel — deliberately out-of-bounds so Game()
               dispatch is a no-op while Loop_Demo takes over. Bypass FSM_SetMode
               to avoid the bounds check. */
            g_state.G_No[1] = 0x63;
            g_state.G_No[2] = 0;
            g_state.G_No[3] = 0;
            g_state.E_No[0] = 0;
            g_state.E_No[1] = 0x63;
            g_state.E_No[2] = 0;
            g_state.E_No[3] = 0;
            g_state.D_No[0] = 0;
            g_state.D_No[1] = 0;
            g_state.D_No[2] = 0;
            g_state.D_No[3] = 0;
            g_state.Get_Demo_Index = 0;
            g_state.Combo_Demo_Flag = 0;
            cpReadyTask(TASK_ENTRY, Entry_Task);
            Purge_mmtm_area(5);
            Make_texcash_of_list(5);
            System_all_clear_Level_B();
            break;
        }

        BG_move();
    }
}

void Request_Break_Sub(s16 PL_id) {
    if ((g_state.Request_Break[PL_id] != 0) && (Ck_Break_Into(0, 0, PL_id) != 0)) {
        g_state.plw[PL_id].wu.pl_operator = 1;
        g_state.Operator_Status[PL_id] = 1;
    }
}

s32 Check_Disp_Ranking() {
    s16 rank_type = Disp_Rank_Sub(0);

    if (rank_type != -1) {
        g_state.Rank_Type = rank_type;
        g_state.Present_Rank[0] = *Get_Ranking_Slot(0, rank_type);
        g_state.Present_Rank[1] = *Get_Ranking_Slot(1, rank_type);
        return 1;
    }

    rank_type = Disp_Rank_Sub(1);

    if (rank_type != -1) {
        g_state.Rank_Type = rank_type;
        g_state.Present_Rank[1] = *Get_Ranking_Slot(1, rank_type);
        return 1;
    }

    return 0;
}

s16 Disp_Rank_Sub(s16 PL_id) {
    if (g_state.Request_Disp_Rank[PL_id][3] >= 0) {
        return 15;
    }

    if (g_state.Request_Disp_Rank[PL_id][2] >= 0) {
        return 10;
    }

    if (g_state.Request_Disp_Rank[PL_id][1] >= 0) {
        return 5;
    }

    if (g_state.Request_Disp_Rank[PL_id][0] >= 0) {
        return 0;
    }

    return -1;
}

s32 Disp_Ranking() {
    switch (g_state.G_No[3]) {
    case 0:
        FSM_AdvanceSubSubState();
        Switch_Screen_Init(1);
        BGM_Request(57);
        break;

    case 1:
        if (Switch_Screen(1) != 0) {
            g_state.Cover_Timer = 24;
            FSM_AdvanceSubSubState();
            g_state.D_No[0] = 1;
            g_state.D_No[1] = 0;
            g_state.D_No[2] = 0;
            g_state.D_No[3] = 0;
            Clear_Personal_Data(0);
            grade_check_work_1st_init(0, 0);
            grade_check_work_1st_init(0, 1);
            Clear_Personal_Data(1);
            grade_check_work_1st_init(1, 0);
            grade_check_work_1st_init(1, 1);
        }

        break;

    case 2:
        Switch_Screen(1);
        Ranking();

        if (--g_state.Cover_Timer == 0) {
            FSM_AdvanceSubSubState();
            Switch_Screen_Init(1);
        }

        break;

    case 3:
        Ranking();

        if (Switch_Screen_Revival(1) != 0) {
            FSM_AdvanceSubSubState();
            g_state.Forbid_Break = 0;
        }

        break;

    default:
        if (Ranking() != 0) {
            BGM_Stop();
            return 1;
        }

        break;
    }

    return 0;
}

/** @brief State 7: Continue scene — countdown for inserting credits. */
void Game07() {
    BG_Draw_System();
    Basic_Sub();

    switch (g_state.G_No[2]) {
    case 0:
        if (Continue_Scene() != 0) {
            /* Hide continue screen before entering game-over */
            if (use_rmlui && rmlui_screen_continue)
                rmlui_continue_hide();
            FSM_SetMode(MODE_GAME_OVER);
            // g_state.G_No[2] zeroed by FSM_SetMode
        }

        break;
    }

    BG_move();
}

/** @brief State 8: Ending sequence — plays the winning character's ending. */
void Game08() {
    BG_Draw_System();

    switch (g_state.G_No[2]) {
    case 0:
        Switch_Screen(0);
        FSM_SetSubState(1);
        g_state.Game_pause = 0;
        g_state.Final_Result_id = g_state.WINNER;
        g_state.WGJ_Target = g_state.WINNER;
        g_state.WGJ_Win = g_state.Win_Record[g_state.WINNER];
        grade_final_grade_bonus();
        g_state.WGJ_Score = g_state.Continue_Coin[g_state.WINNER] + g_state.Score[g_state.WINNER][0];
        Purge_mmtm_area(6);
        cpExitTask(TASK_MENU);
        cpExitTask(TASK_PAUSE);
        break;

    case 1:
        if (Ending_main(g_state.End_PL) && (Request_Fade(9) != 0)) {
            FSM_AdvanceSubState();
        }

        break;

    case 2:
        if (Check_Fade_Complete_SP() != 0) {
            FSM_AdvanceSubState();
            g_state.G_Timer = 10;
            g_state.Suicide[4] = 1;
        }

        break;

    case 3:
        if (--g_state.G_Timer == 0) {
            FSM_SetMode(MODE_GAME_OVER);
            // g_state.G_No[2] zeroed by FSM_SetMode
            g_state.E_No[0] = 8;
            g_state.E_No[1] = 0;
            g_state.E_No[2] = 0;
            g_state.E_No[3] = 0;
            Clear_Personal_Data(0);
            Clear_Personal_Data(1);
            g_state.plw[0].wu.pl_operator = 0;
            g_state.plw[1].wu.pl_operator = 0;
            g_state.Operator_Status[0] = 0;
            g_state.Operator_Status[1] = 0;
            g_state.Last_Player_id = g_state.Player_Number = -1;
            Purge_mmtm_area(6);
            System_all_clear_Level_B();
        }

        break;
    }

    move_effect_work(4);
}

/** @brief State 9: Bonus stage — car/barrel smashing mini-game. */
void Game09() {
    switch (g_state.G_No[2]) {
    case 0:
        BG_Draw_System();
        Switch_Screen(0);
        System_all_clear_Level_B();
        g_state.Bonus_Game_Flag = g_state.Bonus_Type;
        g_state.Game_difficulty = 15;
        g_state.Game_timer = 0;
        g_state.Game_pause = 0;
        g_state.Demo_Time_Stop = 0;
        g_state.C_No[0] = 0;
        g_state.C_No[1] = 0;
        g_state.C_No[2] = 0;
        g_state.C_No[3] = 0;
        FSM_AdvanceSubState();
        g_state.G_Timer = 19;
        g_state.Round_num = 0;
        g_state.Allow_a_battle_f = 0;
        g_state.Time_in_Time = 60;
        init_slow_flag();
        clear_hit_queue();
        g_state.pcon_rno[0] = g_state.pcon_rno[1] = g_state.pcon_rno[2] = g_state.pcon_rno[3] = 0;
        bbbs_com_initialize();
        ca_check_flag = 1;
        g_state.Bonus_Game_Work = 20;
        g_state.Bonus_Game_result = 0;
        g_state.Bonus_Game_ex_result = 0;
        bg_work_clear();
        win_lose_work_clear();

        if (g_state.Bonus_Game_Flag == 0x15) {
            g_state.My_char[g_state.COM_id] = 12;
        } else {
            g_state.My_char[g_state.COM_id] = g_state.My_char[g_state.Player_id];
        }

        break;

    case 1:
        BG_Draw_System();
        Switch_Screen(1);

        if (--g_state.G_Timer == 0) {
            if (Check_LDREQ_Queue_BG((u16)g_state.bg_w.stage) == 0) {
                g_state.G_Timer = 1;
            } else {
                FSM_AdvanceSubState();
                Clear_Flash_No();

                if (g_state.Bonus_Type == 0x15) {
                    makeup_bonus_game_level(g_state.COM_id);
                    effect_35_init(0x3C, 5);
                    effect_J2_init(0x78);
                    effect_35_init(0xB4, 7);
                    effect_58_init(6, 0xB4, 0xA1);
                } else {
                    effect_35_init(0x3C, 6);
                    effect_35_init(0x78, 7);
                    effect_58_init(6, 0x78, 0xA1);
                }

                TATE00();
                Switch_Screen_Init(0);
                Bonus_Sub();
            }
        }

        break;

    case 2:
        Bonus_Sub();

        if (Switch_Screen_Revival(1) != 0) {
            FSM_AdvanceSubState();
            g_state.Forbid_Break = 0;
        }

        break;

    case 3:
        if (Bonus_Sub()) {
            FSM_AdvanceSubState();
            g_state.Cover_Timer = 24;
            g_state.Stop_Combo = 1;
            Switch_Screen_Init(0);
        }

        break;

    case 4:
        Bonus_Sub();

        if (Switch_Screen(0) != 0) {
            FSM_AdvanceSubState();
            g_state.G_Timer = 3;
            SE_All_Off();
            Clear_Flash_No();
            effect_work_kill_mod_plcol();
        }

        break;

    default:
        Switch_Screen(0);
        Bonus_Sub();

        if (--g_state.G_Timer == 0) {
            g_state.Cover_Timer = 24;
            g_state.Suicide[0] = 1;
            System_all_clear_Level_B();
            FSM_SetMode(MODE_TRAINING);
            g_state.E_No[0] = 9;
            g_state.E_No[1] = 0;
            g_state.E_No[2] = 0;
            g_state.E_No[3] = 0;
        }

        break;
    }

    BG_move();
}

/** @brief Bonus stage per-frame sub-routine: player control, hit check, management. */
s16 Bonus_Sub() {
    s16 x;

    mpp_w.inGame = true;
    g_state.Scene_Cut = Cut_Cut_Cut();
    g_state.Bonus_Game_Complete = 0;

    if (g_state.Game_pause != 0x81) {
        g_state.Game_timer += 1;
    }

    set_EXE_flag();
    Time_Control();

    if (g_state.Bonus_Type == 0x15) {
        g_state.Bonus_Game_Complete = Player_control_bonus();
    } else {
        g_state.Bonus_Game_Complete = Player_control_bonus2();
    }

    TATE00();
    x = 0;
    x = Game_Management();
    BG_Draw_System();
    reqPlayerDraw();
    Basic_Sub_Ex();
    hit_check_main_process();
    return x;
}

/** @brief State 10: Post-bonus transition — shows bonus results, returns to arcade. */
void Game10() {
    BG_Draw_System();
    Basic_Sub();
    Setup_Play_Type();

    switch (g_state.G_No[2]) {
    case 0:
        Switch_Screen(0);
        FSM_AdvanceSubState();
        g_state.SC_No[0] = 0;
        g_state.SC_No[1] = 0;
        g_state.SC_No[2] = 0;
        g_state.SC_No[3] = 0;
        g_state.Stop_Combo = 0;
        init_slow_flag();
        break;

    case 1:
        if (After_Bonus() != 0) {
            FSM_AdvanceSubState();
            Switch_Screen_Init(0);
        }

        break;

    default:
        After_Bonus();

        if (Switch_Screen(0) != 0) {
            g_state.Cover_Timer = 24;
            Game01_Sub();
            BGM_Stop();
            FSM_SetMode(MODE_FIGHT);
            // g_state.G_No[2] zeroed by FSM_SetMode
            g_state.E_No[0] = 4;
            g_state.E_No[1] = 0;
            g_state.E_No[2] = 0;
            g_state.E_No[3] = 0;
            g_state.Bonus_Game_Flag = 0;
            Purge_texcash_of_list(3);
            Make_texcash_of_list(3);
        }

        break;
    }

    BG_move();
}

/** @brief State 11: Next Q (special opponent) selection in arcade mode. */
void Game11() {
    BG_Draw_System();
    Basic_Sub();
    Setup_Play_Type();

    switch (g_state.G_No[2]) {
    case 0:
        Switch_Screen(0);
        FSM_AdvanceSubState();
        g_state.SC_No[0] = 0;
        g_state.SC_No[1] = 0;
        g_state.SC_No[2] = 0;
        g_state.SC_No[3] = 0;
        g_state.Stop_Combo = 0;
        g_state.Bonus_Type = 0;
        init_slow_flag();
        break;

    case 1:
        if (Next_Q()) {
            FSM_AdvanceSubState();
            Switch_Screen_Init(0);
        }

        break;

    case 2:
        Next_Q();

        if (Switch_Screen(0) != 0) {
            g_state.Cover_Timer = 24;
            Game01_Sub();
            BGM_Stop();
            Purge_texcash_of_list(3);
            Make_texcash_of_list(3);

            if (g_state.Bonus_Type == 0) {
                FSM_SetMode(MODE_FIGHT);
                // g_state.G_No[2] zeroed by FSM_SetMode
                g_state.E_No[0] = 4;
                g_state.E_No[1] = 0;
                g_state.E_No[2] = 0;
                g_state.E_No[3] = 0;
                g_state.Bonus_Game_Flag = 0;
            } else {
                FSM_SetMode(MODE_STAFF_ROLL);
                g_state.E_No[0] = 4;
                g_state.E_No[1] = 0;
                g_state.E_No[2] = 0;
                g_state.E_No[3] = 0;
            }
        }

        break;

    case 3:
        FSM_AdvanceSubState();
        g_state.SC_No[0] = 0;
        g_state.SC_No[1] = 0;
        g_state.SC_No[2] = 0;
        g_state.SC_No[3] = 0;
        g_state.Stop_Combo = 0;
        g_state.Bonus_Type = 0;
        init_slow_flag();
        Switch_Screen_Init(0);
        break;

    case 4:
        if (Switch_Screen(0) != 0) {
            FSM_SetSubState(1);
            g_state.Cover_Timer = 24;
        }

        break;
    }

    BG_move();
}

/**
 * @brief Attract-mode demo loop.
 *
 * Cycles through: Capcom logo → title screen → demo fight → ranking →
 * another demo → ranking → back to title. Interrupted by coin/start.
 */
void Loop_Demo(struct _TASK* unused1) {
    if (Ck_Coin()) {
        Next_Title_Sub();
        return;
    }

    switch (g_state.G_No[1]) {
    case 0:
        FSM_AdvanceDemoPhase();
        g_state.D_No[0] = 0;
        g_state.D_No[1] = 0;
        g_state.D_No[2] = 0;
        g_state.D_No[3] = 0;
        g_state.E_No[1] = 99;
        g_state.Demo_PL_Index = 0;
        g_state.Demo_Stage_Index = 0;
        g_state.Select_Demo_Index = 0;
        check_screen_L = 0;
        check_screen_S = 0;
        g_state.Insert_Y = 23;
        g_state.Demo_Flag = 0;
        g_state.Play_Mode = 0;
        g_state.Replay_Status[0] = 0;
        g_state.Replay_Status[1] = 0;
        g_state.Present_Mode = 0;
        title_tex_flag = 0;
        g_state.Reset_Bootrom = 0;
        break;

    case 1:
        Basic_Sub();

        if (CAPCOM_Logo() != 0) {
            printf("[BOOT] Loop_Demo: CAPCOM_Logo done -> Title (g_state.G_No[1]=2)\n");
            Loop_Demo_Sub();
            g_state.Insert_Y = 23;
            g_state.E_No[1] = 2;
            g_state.E_Timer = 1;
            return;
        }

        break;

    case 2:
        Basic_Sub();

        if (Title()) {
            /* Hide RmlUi title elements before transitioning to attract demo */
            if (use_rmlui && rmlui_screen_title) {
                rmlui_title_screen_hide();
            }
            if (use_rmlui && rmlui_screen_copyright) {
                rmlui_copyright_hide();
            }
            /* Show attract overlay (small logo + PRESS START) for demo fight */
            if (use_rmlui && rmlui_screen_attract_overlay) {
                rmlui_attract_overlay_show();
            }
            Loop_Demo_Sub();
            g_state.Insert_Y = 17;
            g_state.D_No[0] = 1;
            return;
        }

        break;

    case 3:
        if (Play_Demo() != 0) {
            /* Hide just the logo when demo fight ends — text persists */
            if (use_rmlui && rmlui_screen_attract_overlay) {
                rmlui_attract_overlay_hide_logo();
            }
            Switch_Screen(1);
            Loop_Demo_Sub();
            g_state.Rank_Type = 0;
            g_state.Demo_Type = 0;
            SsAllNoteOff();
            return;
        }

        break;

    case 4:
        Basic_Sub();

        if (Ranking() != 0) {
            Switch_Screen(1);
            Loop_Demo_Sub();
            return;
        }

        break;

    case 5:
        if (Play_Demo() != 0) {
            /* Hide just the logo when demo fight ends — text persists */
            if (use_rmlui && rmlui_screen_attract_overlay) {
                rmlui_attract_overlay_hide_logo();
            }
            Loop_Demo_Sub();
            g_state.Demo_Type = 1;
            g_state.Rank_Type = 5;
            g_state.Demo_Type = 1;
            SsAllNoteOff();
            return;
        }

        break;

    case 6:
        Basic_Sub();

        if (Ranking() != 0) {
            /* Hide attract overlay before cycling back to title screen */
            if (use_rmlui && rmlui_screen_attract_overlay) {
                rmlui_attract_overlay_hide();
            }
            Switch_Screen(1);
            Loop_Demo_Sub();
            System_all_clear_Level_B();
            Purge_mmtm_area(6);
            g_state.Game_pause = 0;
            FSM_SetDemoPhase(1);
            g_state.E_No[1] = 99;
            return;
        }

        break;

    default:
        Switch_Screen(1);

        if (--g_state.Cover_Timer <= 0) {
            Next_Demo_Loop();
        }

        break;
    }
}

void Next_Demo_Loop() {
    FSM_SetMainState(MAIN_STATE_LOOP_DEMO);
    FSM_SetMode(MODE_ATTRACT);
    // g_state.G_No[2] zeroed by FSM_SetMode
    g_state.D_No[0] = 0;
    g_state.D_No[1] = 0;
    g_state.D_No[2] = 0;
    g_state.D_No[3] = 0;
    g_state.E_No[0] = 0;
    g_state.E_No[1] = 99;
    g_state.E_No[2] = 0;
    g_state.E_No[3] = 0;
    g_state.Demo_PL_Index = 0;
    g_state.Demo_Stage_Index = 0;
    g_state.Select_Demo_Index = 0;
    g_state.Demo_Flag = 0;
    g_state.Present_Mode = 0;
    g_state.Game_pause = 0;
    g_state.Play_Mode = 0;
    g_state.Replay_Status[0] = 0;
    g_state.Replay_Status[1] = 0;
    System_all_clear_Level_B();
    Purge_mmtm_area(6);
}

void Loop_Demo_Sub() {
    FSM_AdvanceDemoPhase();
    g_state.D_No[0] = 0;
    g_state.D_No[1] = 0;
    g_state.D_No[2] = 0;
    g_state.D_No[3] = 0;
    g_state.E_No[1] = 1;
    g_state.Play_Game = 0;
    pulpul_stop();
    pp_operator_check_flag(1);
}

void Next_Title_Sub() {
    s16 ix;

    if (g_state.G_No[1] != 99) {
        SsAllNoteOff();
    }

    if (g_state.Demo_Flag == 0) {
        SsRequest(106);
    }
    TexRelease(600);
    TexRelease_OP();
    System_all_clear_Level_B();
    Purge_mmtm_area(6);
    g_state.G_Timer = 0;

    InitTask_ClearAllRNo();
    for (ix = 0; ix < 4; ix++) {
        g_state.vm_w.r_no[ix] = 0;
        g_state.G_No[ix] = 0;
        g_state.E_No[ix] = 0;
        g_state.D_No[ix] = 0;
    }

    FSM_SetMainState(MAIN_STATE_GAME);
    FSM_SetSubState(2);        /* Skip Game0_0/Game0_1 (redundant title screen),
                            go directly to Game0_2 (fade-to-menu transition) */
    g_state.G_No[3] = 2;        /* Skip Game0_2 cases 0-1 (they render texture 601
                            which was never loaded since we skipped Game0_0) */
    g_state.E_No[0] = 1;        /* Entry_01 will still run ... */
    g_state.E_No[1] = 1;        /* ... pre-init its sub-state (what case 0 would set) ... */
    g_state.E_No[2] = 3;        /* ... and skip to default → Exit_Title_Sub_Entry() immediately */
    g_state.Break_Into = 0;     /* What Entry_01 case 0 would have set */
    title_tex_flag = 0; /* Title texture was never loaded */
    InitTask_SetPhase(ITP_RUNNING);
    g_state.Demo_Flag = 1;
    g_state.Game_pause = 0;
    g_state.judge_flag = 0;
    g_state.Pause_Down = 0;
    g_state.Disp_Attack_Data = 0;
    g_state.seraph_flag = 0;
    g_state.End_Training = 0;
    g_state.Forbid_Reset = 0;
    g_state.Exec_Wipe = 0;
    g_state.Present_Mode = 1;
    g_state.Insert_Y = 23;
    /* Hide attract overlay on coin insert */
    if (use_rmlui && rmlui_screen_attract_overlay) {
        rmlui_attract_overlay_hide();
    }
    /* Hide char select overlay if it was visible during the attract demo */
    if (use_rmlui && rmlui_screen_select && rmlui_char_select_visible) {
        rmlui_char_select_hide();
    }
    /* Hide title screen + copyright — Entry_01_Sub normally does this
       but we're fast-forwarding past it */
    if (use_rmlui && rmlui_screen_title) {
        rmlui_title_screen_hide();
    }
    if (use_rmlui && rmlui_screen_copyright) {
        rmlui_copyright_hide();
    }
    Before_Select_Sub();
    /* Clear any stale MenuScreen state (e.g. RANKING/DEMO still active
     * during the attract loop when a coin is inserted). */
    MenuScreen_ExitToLegacy(MenuTask_GetTaskPtr());
    cpReadyTask(TASK_ENTRY, Entry_Task);
}

/**
 * @brief Time Control — manages the in-round countdown timer.
 *
 * Decrements g_state.Control_Time once per second (60 frames). Skipped during
 * pauses, bonus games, and when the battle hasn't started yet.
 */
void Time_Control() {
    count_cont_main();

    if ((g_state.Allow_a_battle_f == 0) || (g_state.Demo_Time_Stop != 0) || (g_state.Bonus_Game_Flag != 0)) {
        return;
    }

    if (g_state.Game_pause == 0x81) {
        return;
    }

    if (g_state.Control_Time >= g_state.Limit_Time) {
        g_state.Control_Time = g_state.Limit_Time;
    } else if (--g_state.Time_in_Time == 0) {
        g_state.Time_in_Time = 60;
        g_state.Control_Time += 1;
    }
}

/**
 * @brief Check if either player pressed Start to insert a coin.
 *
 * Used during the attract-mode demo loop to detect coin-in / start press.
 *
 * @return Non-zero if a coin was inserted and the game should proceed.
 */
s16 Ck_Coin() {
    s16 PL_id;

    switch (g_state.G_No[3]) {
    case 0:
        PL_id = -1;

        if (~p1sw_1 & p1sw_0 & (SWK_START | SWK_ATTACKS)) {
            PL_id = 0;
        } else if (~p2sw_1 & p2sw_0 & (SWK_START | SWK_ATTACKS)) {
            PL_id = 1;
        }

        if (PL_id == -1) {
            return 0;
        }

        ToneDown(0xFF, 0);
        Request_LDREQ_Break();
        g_state.G_No[3] = 1;
        g_state.plw[PL_id].wu.pl_operator = 1;
        g_state.Operator_Status[PL_id] = 1;
        g_state.Champion = PL_id;
        g_state.plw[PL_id ^ 1].wu.pl_operator = 0;
        g_state.Operator_Status[PL_id ^ 1] = 0;
        return 0;

    default:
    case 1:
        ToneDown(0xFF, 0);
        PL_id = Check_LDREQ_Break();
        return PL_id ^ 1;
    }
}

/**
 * @brief Pre-select initialization — resets state before the character select screen.
 *
 * Clears control time, round level, rankings, fade flags, combo demos,
 * and randomizes the RNG seeds from g_state.system_timer.
 */
void Before_Select_Sub() {
    s16 xx;

    g_state.Request_G_No = 0;
    g_state.Request_E_No = 0;
    g_state.Allow_a_battle_f = 0;
    g_state.Bonus_Type = 0;

    if (g_state.Demo_Flag == 0) {
        g_state.Control_Time = 2048;
        g_state.Round_Level = 7;
    } else {
        g_state.Control_Time = CONTROL_TIME_DEFAULT;
    }

    g_state.Super_Arts[0] = 0;
    g_state.Super_Arts[1] = 0;
    g_state.Exec_Wipe = 0;
    g_state.Fade_Flag = 0;
    g_state.Stock_Com_Color[0] = -1;
    g_state.Stock_Com_Arts[0] = -1;
    g_state.Stock_Com_Color[1] = -1;
    g_state.Stock_Com_Arts[1] = -1;
    g_state.Bonus_Game_Flag = 0;
    g_state.Combo_Demo_Flag = 0;
    g_state.paring_counter[0] = 0;
    g_state.paring_bonus_r[0] = 0;
    g_state.paring_counter[1] = 0;
    g_state.paring_bonus_r[1] = 0;
    Clear_Disp_Ranking(0);
    Clear_Disp_Ranking(1);
    Clear_Personal_Data(0);
    grade_check_work_1st_init(0, 0);
    grade_check_work_1st_init(0, 1);
    Clear_Personal_Data(1);
    grade_check_work_1st_init(1, 0);
    grade_check_work_1st_init(1, 1);
    g_state.Last_Player_id = g_state.Player_Number = -1;
    g_state.Round_Level = 3;
    g_state.Time_in_Time = 60;

    if (g_state.Mode_Type != MODE_NETWORK) {
        xx = g_state.system_timer;
        g_state.Random_ix16 = xx & 0x3F;
        g_state.Random_ix32 = xx & 0x7F;
    }
}

/** @brief Idle task while waiting for auto-load to complete — renders background. */
void Wait_Auto_Load(struct _TASK* unused1) {
    Basic_Sub();
    BG_Draw_System();
    bg_pos_hosei_sub2(0);
    Bg_Family_Set_appoint(0);
    BG_move_Ex(0);
}
