/**
 * @file manage.c
 * Engine management
 */

#include "sf33rd/Source/Game/engine/manage.h"
#include "game_state.h"
#include "port/menu_task.h"
#include "common.h"
#include "main.h"
#include "sf33rd/Source/Game/animation/appear.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/eff08.h"
#include "sf33rd/Source/Game/effect/eff14.h"
#include "sf33rd/Source/Game/effect/eff35.h"
#include "sf33rd/Source/Game/effect/eff56.h"
#include "sf33rd/Source/Game/effect/eff58.h"
#include "sf33rd/Source/Game/effect/eff76.h"
#include "sf33rd/Source/Game/effect/eff81.h"
#include "sf33rd/Source/Game/effect/eff84.h"
#include "sf33rd/Source/Game/effect/eff92.h"
#include "sf33rd/Source/Game/effect/effb2.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effg0.h"
#include "sf33rd/Source/Game/effect/effj4.h"
#include "sf33rd/Source/Game/ending/end_main.h"
#include "sf33rd/Source/Game/engine/cmb_win.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/menu/menu.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/stage/ta_sub.h"
#include "sf33rd/Source/Game/system/pause.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sys_sub2.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/count.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

static void Game_Manage_1st();
static void Clear_1Stage_Work();
static void Game_Manage_2nd();
static void Game_Manage_2_0();
static void Game_Manage_2_1();
static void Game_Manage_2_2();
static void Game_Manage_2_3();
static void Game_Manage_2_4();
static void Game_Manage_3rd();
static void setFinishType();
static void Game_Manage_4th();
static void Game_Manage_5th();
static void Game_Manage_5_0();
static void Game_Manage_5_1();
static void Game_Manage_5_2();
static void Game_Manage_5_3();
static void Game_Manage_5_4();
static void Game_Manage_5_5();
static void Game_Manage_5_6();
static void Game_Manage_5_7();
static void Game_Manage_6th();
static void Game_Manage_7th();
static void Game_Manage_7_0();
static void Game_Manage_7_1();
static void Game_Manage_7_2();
static s32 Check_Disp_Combo();
static void Game_Manage_7_3();
static void Game_Manage_7_4();
static void Game_Manage_7_5();
static void Game_Manage_7_6();
static void Game_Manage_7_7();
static void Game_Manage_7_8();
static void Game_Manage_7_9();
static void Game_Manage_8th();
static void Game_Manage_8_0();
static void Game_Manage_8_1();
static void Game_Manage_81_0();
static void Game_Manage_81_1();
static void Game_Manage_81_2();
static void Game_Manage_81_3();
static void Game_Manage_8_2();
static void Game_Manage_8_3();
static void Game_Manage_9th();
static void Game_Manage_10th();
static void Check_Naming(s16 PL_id);
static s32 Check_Ending();
static s32 Check_Ending_Sub();
static void Additional_Bonus(s16 PL_id);
static u32 Setup_Comp_Bonus();
void request_center_message(s16 Kind_of_Message);
static void Setup_Win_Mark();
static void Check_Perfect(s16 PL_id);
static void Update_VS_Data();
static void BGM_Fade_Sub();
static void BGM_Control();
static void Setup_BGM_Fade_In(s16 Time);
static void Check_Stage_BGM();
void Control_Music_Fade(s16 Time);
static void Check_Conclusion_Type();
static void chkComWins();
static void Update_BI_Term();
static void Ck_Win_Record();
static void Update_Level_Control();
static s32 Judge_Next_Disposal();
static void Quick_Entry();
static s32 Check_Entry_Again();
static void Loser_Sub();
static void Be_Continue();
static void Disp_Winner();
static void Pool_Score(s16 PL_id);
static s32 Check_Break_Into_CPU(s16 PL_id);
static void Judge_Winner();
static s32 Check_Disp_Winner();
static void Check_Fade_Out_BGM(s16 Time);
static s32 Check_BI_Grade(s16 PL_id);
static void Game_Manage_11th();
static void Game_Manage_12th();
static void Game_Manage_12_0();
static void Game_Manage_12_1();
static void Game_Manage_12_2();
static void Game_Manage_12_3();
static void Game_Manage_12_4();
static void Game_Manage_12_5();
static void Game_Manage_12_7();
static void Game_Manage_12_8();
static u8 Check_Bonus_Perfect();
static void Disp_Bonus_Perfect();
static void Flash_Bonus_Perfect();
static u32 Setup_Final_Score(s16 Type);
static s32 Bonus_Cut_Sub();
static s16 Check_Time_Over();
void complete_victory_pause();
static void Game_Manage_13th();

u8 Disp_Bonus_Contents;
s8 MANAGE_X;

const u32 Comp_Bonus_Data[11] = { 30000, 40000, 50000, 60000, 70000, 80000, 90000, 100000, 110000, 120000, 130000 };

const u8 BIC_SA_Data[2][4] = { { 3, 5, 7, 9 }, { 1, 1, 1, 1 } };

const u32 Ball_Perfect_PTS[2][5] = { { 20000, 30000, 50000, 80000, 120000 }, { 10000, 20000, 40000, 80000, 160000 } };

/** @brief Main match management dispatcher — routes to the current management phase via g_state.C_No[0]. */
s32 Game_Management() {
    if (g_state.Break_Into) {
        return 0;
    }

    MANAGE_X = 0;

    switch (g_state.C_No[0]) {
    case 0:
        Game_Manage_1st();
        break;
    case 1:
        Game_Manage_2nd();
        break;
    case 2:
        Game_Manage_3rd();
        break;
    case 3:
        Game_Manage_4th();
        break;
    case 4:
        Game_Manage_5th();
        break;
    case 5:
        Game_Manage_6th();
        break;
    case 6:
        Game_Manage_7th();
        break;
    case 7:
        Game_Manage_8th();
        break;
    case 8:
        Game_Manage_9th();
        break;
    case 9:
        Game_Manage_10th();
        break;
    case 10:
        Game_Manage_11th();
        break;
    case 11:
        Game_Manage_12th();
        break;
    case 12:
        Game_Manage_13th();
        break;
    default:
        break;
    }

    BGM_Fade_Sub();
    BGM_Control();
    return MANAGE_X;
}

/** @brief Phase 1: Match initialization — clears work, starts appear sequence, sets up operators. */
static void Game_Manage_1st() {
    Switch_Screen(0);
    g_state.EXE_obroll = 0;

    if (g_state.bg_w.stage == 21 || g_state.bg_w.stage == 20) {
        g_state.C_No[0] = 11;
    } else {
        g_state.C_No[0] = 1;
    }

    appear_work_clear();
    g_state.win_sp_flag = 0;
    g_state.BGM_No[1] = 0;
    g_state.BGM_No[0] = 0;
    g_state.Appear_Q = 0;
    Clear_1Stage_Work();
    All_Clear_Suicide();
    g_state.Round_Operator[0] = 0;
    g_state.Round_Operator[1] = 0;

    if (g_state.plw[0].wu.pl_operator) {
        g_state.Round_Operator[0] = 1;
        g_state.Final_Play_Type[0] = g_state.Play_Type;
    }

    if (g_state.plw[1].wu.pl_operator) {
        g_state.Round_Operator[1] = 1;
        g_state.Final_Play_Type[1] = g_state.Play_Type;
    }

    g_state.Battle_Q[0] = 0;
    g_state.Battle_Q[1] = 0;

    if (g_state.Play_Type == 0) {
        g_state.Control_Time = g_state.SC_Personal_Time[g_state.Player_id];
        g_state.paring_ctr_ori[g_state.Player_id] = g_state.paring_ctr_vs[0][g_state.Player_id] = 0;
        g_state.Stage_Stock_Score[g_state.Player_id] = g_state.Score[g_state.Player_id][0];
        g_state.Request_Disp_Rank[g_state.COM_id][0] = -1;
        g_state.Request_Disp_Rank[g_state.COM_id][1] = -1;
        g_state.Request_Disp_Rank[g_state.COM_id][2] = -1;
        g_state.Request_Disp_Rank[g_state.COM_id][3] = -1;

        if (g_state.EM_id == 17) {
            g_state.Break_Into_CPU = 2;
        } else {
            g_state.Break_Into_CPU = 0;
        }
    }

    eff_hit_flag_clear();
    Check_Stage_BGM();
    Pause_Family_On();
    g_state.Fade_Flag = 0;
    Clear_Flash_No();
    g_state.seraph_flag = 0;
    grade_check_work_stage_init(0);
    grade_check_work_stage_init(1);

    if (g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_PARRY_TRAINING || g_state.Mode_Type == MODE_TRIALS) {
        cpReadyTask(TASK_MENU, Menu_Task);
        MenuTask_SetPhase(MTP_IN_GAME);
        g_state.plw[g_state.New_Challenger].wu.pl_operator = 0;
        g_state.Operator_Status[g_state.New_Challenger] = 0;
        g_state.Lever_LR[0] = 0;
        g_state.Lever_LR[1] = 0;
        return;
    }

    if (g_state.Mode_Type != MODE_NETWORK) {
        cpReadyTask(TASK_PAUSE, Pause_Task);
    }
}

/** @brief Clears per-stage work variables (bonuses, counters, finish flags). */
static void Clear_1Stage_Work() {
    s16 xx;

    for (xx = 0; xx < 2; xx++) {
        g_state.Vital_Bonus[xx] = 0;
        g_state.Time_Bonus[xx] = 0;
        g_state.Perfect_Bonus[xx] = 0;
        g_state.Perfect_Counter[xx] = 0;
        g_state.Stage_SA_Finish[xx] = 0;
        g_state.Stage_Lost_Round[xx] = 0;
        g_state.Stage_Perfect_Finish[xx] = 0;
        g_state.Stage_Cheap_Finish[xx] = 0;
        g_state.Stage_Time_Finish[xx] = 0;
    }

    g_state.Disp_Cockpit = 0;
}

/** @brief Phase 2 dispatcher: pre-round setup (screen switch, round init, appear wait). */
static void Game_Manage_2nd() {
    switch (g_state.C_No[1]) {
    case 0:
        Game_Manage_2_0();
        break;
    case 1:
        Game_Manage_2_1();
        break;
    case 2:
        Game_Manage_2_2();
        break;
    case 3:
        Game_Manage_2_3();
        break;
    case 4:
        Game_Manage_2_4();
        break;
    default:
        break;
    }
}

/** @brief Phase 2.0: Wait for cover timer and seek time before round start. */
static void Game_Manage_2_0() {
    Switch_Screen(0);
    g_state.request_message = 0;
    g_state.SA_shadow_on = 0;

    if (g_state.Demo_Flag == 0) {
        g_state.C_No[1] = 2;
        return;
    }

    if (--g_state.Cover_Timer > 0) {
        return;
    }

    if (Wait_Seek_Time() == 0) {
        g_state.Cover_Timer = 1;
        return;
    }

    g_state.C_No[1]++;
    Switch_Screen_Init(0);
}

/** @brief Waits for network/demo seek synchronization before proceeding. */
s32 Wait_Seek_Time() {
    s16 ix;
    s16 ix2;

    switch (g_state.Play_Mode) {
    case 1:
        if (g_state.Mode_Type != MODE_NETWORK) {
            for (ix = 0; ix < 2; ix++) {
                for (ix2 = 0; ix2 < 3; ix2++) {
                    g_state.Separate_Area[ix][ix2] = 0;
                    g_state.Shell_Separate_Area[ix][ix2] = 0;
                }
            }

            return 1;
        }

        Lag_Ptr[0] = g_state.Lag_Timer;
        Lag_Ptr++;
        g_state.Lag_Timer = 1;
        return 1;

    case 3:
        if (g_state.Mode_Type == MODE_NORMAL_TRAINING) {
            return 1;
        }

        if (g_state.Mode_Type == MODE_PARRY_TRAINING) {
            return 1;
        }

        for (ix = 0; ix < 2; ix++) {
            for (ix2 = 0; ix2 < 3; ix2++) {
                g_state.Separate_Area[ix][ix2] = 0;
                g_state.Shell_Separate_Area[ix][ix2] = 0;
            }
        }

        if (--g_state.Lag_Timer == 0) {
            g_state.Lag_Timer = Lag_Ptr[0];
            Lag_Ptr++;
            return 1;
        }

        return 0;

    default:
        return 1;
    }
}

/** @brief Phase 2.1: Wait for screen revival and training menu readiness. */
static void Game_Manage_2_1() {
    switch (g_state.C_No[2]) {
    case 0:
        if (!Switch_Screen_Revival(0)) {
            break;
        }

        if (g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_PARRY_TRAINING || g_state.Mode_Type == MODE_TRIALS) {
            g_state.C_No[2]++;
            break;
        }

        g_state.C_No[1]++;

        break;

    case 1:
        if (MenuTask_GetPhase() == MTP_TRAINING) {
            g_state.C_No[1]++;
            g_state.C_No[2] = 0;
        }

        break;
    }
}

/** @brief Phase 2.2: Initialize round state — clear suicides, reset flags, init grade work. */
static void Game_Manage_2_2() {
    s16 ix;

    g_state.Suicide[0] = 0;
    g_state.Suicide[6] = 0;

    for (ix = 0; ix < 4; ix++) {
        g_state.Message_Suicide[ix] = 0;
    }

    if (effect_84_init()) {
        return;
    }

    g_state.C_No[1]++;
    g_state.Forbid_Break = 0;
    g_state.Extra_Break = 0;
    g_state.Complete_Victory = 0;
    g_state.Conclusion_Flag = 0;
    g_state.Perfect_Flag = 0;
    g_state.Round_Result = 0;
    g_state.Reserve_Cut = 0;
    g_state.Next_Step = 0;
    g_state.judge_flag = 0;
    g_state.Stop_Combo = 0;

    if (g_state.Demo_Flag) {
        g_state.Stop_SG = 0;
    }

    g_state.Complete_Judgement = 0;
    g_state.Music_Fade = 0;
    g_state.Pause_Hit_Marks = 0;
    g_state.count_end = 0;
    g_state.sag_inc_timer[0] = g_state.sag_inc_timer[1] = 0;
    g_state.CP_No[0][0] = 0;
    g_state.CP_No[1][0] = 0;
    g_state.Stock_Score[0] = g_state.Score[0][0];
    g_state.Stock_Score[1] = g_state.Score[1][0];
    grade_check_work_round_init(0);
    grade_check_work_round_init(1);
}

/** @brief Phase 2.3: Wait for character appear animations to complete. */
static void Game_Manage_2_3() {
    if (g_state.Appear_end < 2) {
        return;
    }

    if (g_state.bg_app) {
        return;
    }

    appear_work_clear();
    g_state.win_sp_flag = 0;

    if (g_state.pcon_rno[0] != 0) {
        return;
    }

    if (g_state.pcon_rno[1] != 1) {
        return;
    }

    g_state.C_No[1]++;

    if (Is_Training_Mode(g_state.Mode_Type)) {
        g_state.Next_Step = 1;
    } else {
        effect_B2_init();
    }
}

/** @brief Phase 2.4: Cockpit fade-in, then transition to fighting phase. */
static void Game_Manage_2_4() {
    switch (g_state.C_No[2]) {
    case 0:
        if (g_state.Round_num) {
            g_state.C_No[2] = 3;
            return;
        }

        g_state.C_No[2]++;
        g_state.C_Timer = 3;
        g_state.Forbid_Break = 1;
        FadeInit();
        FadeOut(0, 0xFF, 8);
        g_state.Disp_Cockpit = 1;

        if (Is_Training_Mode(g_state.Mode_Type)) {
            g_state.Score[0][2] = 0;
            g_state.Score[1][2] = 0;
            g_state.Game_pause = 0;
            g_state.pcon_rno[0] = 0;
            g_state.pcon_rno[1] = 0;
            g_state.pcon_rno[2] = 0;
            g_state.pcon_rno[3] = 0;
            g_state.appear_type = APPEAR_TYPE_NON_ANIMATED;
            erase_extra_plef_work();
            compel_bg_init_position();
            win_lose_work_clear();
        }

        break;

    case 1:
        FadeOut(0, 0xFF, 8);

        if (--g_state.C_Timer == 0) {
            g_state.C_No[2]++;
            Clear_Flash_No();
        }

        break;

    case 2:
        g_state.C_No[2]++;
        g_state.Forbid_Break = 0;
        break;

    case 3:
        if (g_state.Next_Step == 0) {
            break;
        }

        g_state.C_No[0]++;
        g_state.C_No[1] = 0;
        g_state.C_No[2] = 0;
        g_state.Allow_a_battle_f = 1;
        g_state.vital_inc_timer = 50;
        g_state.vital_dec_timer = 40;
        g_state.sag_inc_timer[0] = g_state.sag_inc_timer[1] = 0;

        if (g_state.Play_Type == 0 && (g_state.EM_id == 0 || (g_state.My_char[g_state.Player_id] == 0 && g_state.EM_id == 1)) &&
            !(g_state.Introduce_Boss[g_state.Player_id][1] & 0x80)) {
            g_state.Introduce_Boss[g_state.Player_id][1] |= 128;
            Check_Stage_BGM();
        }

        if (g_state.Demo_Flag == 0 && !Is_Training_Mode(g_state.Mode_Type)) {
            effect_58_init(10, 60, 0);
        }

        break;
    }
}

/** @brief Phase 3: Check for round conclusion during demo playback. */
static void Game_Manage_3rd() {
    if (g_state.Demo_Flag == 0) {
        return;
    }

    if (g_state.Conclusion_Flag == 0) {
        return;
    }

    g_state.C_No[0]++;
    g_state.Forbid_Break = -1;
    g_state.Allow_a_battle_f = 0;
    g_state.count_end = 1;
    Check_Conclusion_Type();
}

/** @brief Determines finish type and updates break-in term data. */
static void setFinishType() {
    if (g_state.Play_Type == 0 && g_state.Mode_Type == MODE_ARCADE && g_state.PL_Wins[g_state.Winner_id] >= CurrentSave()->Battle_Number[g_state.Play_Type] &&
        g_state.VS_Index[g_state.Winner_id] > 8 && g_state.plw[g_state.Winner_id].wu.pl_operator != 0 && g_state.E_Number[g_state.Loser_id][0] != 2) {
        g_state.E_Number[g_state.Loser_id][0] = 99;
    }

    Update_BI_Term();
}

/** @brief Phase 4: Process round conclusion — KO, draw, or time-over routing. */
static void Game_Manage_4th() {
    switch (g_state.Conclusion_Type) {
    case 0:
        g_state.C_No[0] = 6;
        Setup_Win_Mark();
        Check_Perfect(g_state.Winner_id);
        setFinishType();
        g_state.PL_Wins[g_state.Winner_id]++;
        Update_Level_Control();
        Update_VS_Data();
        Ck_Win_Record();
        break;

    case 1:
        SsRequest(121);
        SsRequest(139);

        if (Judge_Next_Disposal()) {
            g_state.C_No[0] = 4;
            break;
        }

        g_state.C_No[0] = 5;
        g_state.Round_Result |= 1024;
        setFinishType();
        g_state.win_type[0][g_state.PL_Wins[0]] = 5;
        g_state.win_type[1][g_state.PL_Wins[1]] = 5;
        g_state.PL_Wins[0]++;
        g_state.PL_Wins[1]++;

        if (g_state.PL_Wins[0] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            g_state.Winner_id = 0;
            g_state.Loser_id = 1;
            Update_VS_Data();
            Ck_Win_Record();
            break;
        }

        if (g_state.PL_Wins[1] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            g_state.Winner_id = 1;
            g_state.Loser_id = 0;
            Update_VS_Data();
            Ck_Win_Record();
        }

        break;

    default:
        SsRequest(143);

        if (g_state.plw[0].wu.vital_new != g_state.plw[1].wu.vital_new || g_state.Mode_Type == MODE_NORMAL_TRAINING ||
            g_state.Mode_Type == MODE_PARRY_TRAINING) {
            g_state.C_No[0] = 6;
            g_state.Round_Result |= 1;
            setFinishType();
            g_state.win_type[g_state.Winner_id][g_state.PL_Wins[g_state.Winner_id]] = 1;
            Check_Perfect(g_state.Winner_id);
            g_state.PL_Wins[g_state.Winner_id]++;
            Update_Level_Control();
            Update_VS_Data();
            Ck_Win_Record();
            break;
        }

        g_state.C_No[0] = 4;
        break;
    }
}

/** @brief Phase 5 dispatcher: complete victory (judgement gals) sequence. */
static void Game_Manage_5th() {
    switch (g_state.C_No[1]) {
    case 0:
        Game_Manage_5_0();
        break;
    case 1:
        Game_Manage_5_1();
        break;
    case 2:
        Game_Manage_5_2();
        break;
    case 3:
        Game_Manage_5_3();
        break;
    case 4:
        Game_Manage_5_4();
        break;
    case 5:
        Game_Manage_5_5();
        break;
    case 6:
        Game_Manage_5_6();
        break;
    case 7:
        Game_Manage_5_7();
        break;
    default:
        break;
    }
}

static void Game_Manage_5_0() {
    if (g_state.Complete_Victory) {
        g_state.C_No[1]++;
        g_state.C_Timer = 30;
        g_state.Event_Judge_Gals = 0;
    }
}

static void Game_Manage_5_1() {
    if (Button_Cut_EX(&g_state.C_Timer, 10)) {
        g_state.C_No[1]++;
        request_center_message(3);
        SsRequest(154);
    }
}

static void Game_Manage_5_2() {
    if (!g_state.request_message) {
        g_state.C_No[1]++;
        g_state.C_Timer = 30;
    }
}

static void Game_Manage_5_3() {
    if (Button_Cut_EX(&g_state.C_Timer, 10)) {
        g_state.C_No[1]++;
        Judge_Winner();
        chkComWins();
        g_state.Stop_Combo = 1;
        Switch_Screen_Init(0);
        SsBgmFadeOut(2048);
    }
}

static void Game_Manage_5_4() {
    if (Switch_Screen(0)) {
        g_state.C_No[1]++;
        g_state.Cover_Timer = 5;
        g_state.Suicide[6] = 1;
        g_state.judge_flag = 1;
        effect_J4_init(0xFF);
        compel_bg_init_position();
        g_state.pcon_rno[0] = 0;
        g_state.pcon_rno[1] = 0;
        g_state.pcon_rno[2] = 0;
        g_state.pcon_rno[3] = 0;
        g_state.appear_type = APPEAR_TYPE_VICTORY;
    }
}

static void Game_Manage_5_5() {
    Switch_Screen(0);

    if (--g_state.Cover_Timer == 0) {
        g_state.C_No[1]++;
        g_state.pcon_rno[1] = 3;
        g_state.pcon_rno[2] = 1;
        Clear_Flash_No();
        Switch_Screen_Init(0);
    }
}

static void Game_Manage_5_6() {
    if (Switch_Screen_Revival(0)) {
        g_state.C_No[1]++;
        g_state.C_Timer = 60;
        g_state.Stop_SG = 0;
        g_state.BGM_No[0] = 3;
        g_state.BGM_Timer[0] = 1;
    }
}

static void Game_Manage_5_7() {
    if (--g_state.C_Timer != 0) {
        return;
    }

    if (Wait_Seek_Time() == 0) {
        g_state.C_Timer = 1;
        return;
    }

    g_state.C_No[0] = 6;
    g_state.C_No[1] = 7;
    g_state.C_Timer = 30;
    g_state.Fade_Half_Flag = 1;
    g_state.Complete_Judgement = 1;
    g_state.Round_Result |= 0x8000;
    g_state.win_type[g_state.Winner_id][g_state.PL_Wins[g_state.Winner_id]] = 6;
    setFinishType();
    Check_Perfect(g_state.Winner_id);
    g_state.PL_Wins[g_state.Winner_id]++;
    Update_Level_Control();
    Update_VS_Data();
}

/** @brief Phase 6: Post-round cleanup — grade calculation, advance to next round or training end. */
static void Game_Manage_6th() {
    switch (g_state.C_No[1]) {
    case 0:
        if (!g_state.Complete_Victory) {
            break;
        }

        g_state.C_No[1]++;
        g_state.C_Timer = 60;
        g_state.pcon_rno[1] = 3;
        g_state.pcon_rno[2] = 0;
        grade_makeup_round_para_dko();

        if (g_state.Mode_Type != MODE_NORMAL_TRAINING && g_state.Mode_Type != MODE_PARRY_TRAINING && g_state.Mode_Type != MODE_TRIALS &&
            omop_cockpit) {
            effect_58_init(6, 1, g_state.Winner_id + 100);
            effect_92_init(0, g_state.PL_Wins[0] - 1);
            effect_92_init(1, g_state.PL_Wins[1] - 1);
            break;
        }

        break;

    case 1:
        if (--g_state.C_Timer != 0) {
            break;
        }

        if (g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_PARRY_TRAINING || g_state.Mode_Type == MODE_TRIALS) {
            g_state.C_No[0] = 12;
            g_state.End_Training = 1;
            break;
        }

        g_state.C_No[0] = 7;
        g_state.C_No[1] = 0;
        g_state.Round_num++;
        Quick_Entry();
        break;
    }
}

/** @brief Phase 7 dispatcher: win presentation, winner display, perfect announcement. */
static void Game_Manage_7th() {
    switch (g_state.C_No[1]) {
    case 0:
        Game_Manage_7_0();
        break;
    case 1:
        Game_Manage_7_1();
        break;
    case 2:
        Game_Manage_7_2();
        break;
    case 3:
        Game_Manage_7_3();
        break;
    case 4:
        Game_Manage_7_4();
        break;
    case 5:
        Game_Manage_7_5();
        break;
    case 6:
        Game_Manage_7_6();
        break;
    case 7:
        Game_Manage_7_7();
        break;
    case 8:
        Game_Manage_7_8();
        break;
    case 9:
        Game_Manage_7_9();
        break;
    default:
        break;
    }
}

static void Game_Manage_7_0() {
    if (!g_state.Complete_Victory) {
        return;
    }

    g_state.C_No[1]++;
    g_state.C_Timer = 1;
    grade_makeup_round_parameter(g_state.Winner_id);

    if (g_state.Mode_Type != MODE_NORMAL_TRAINING && g_state.Mode_Type != MODE_PARRY_TRAINING && g_state.Mode_Type != MODE_TRIALS &&
        omop_cockpit) {
        effect_58_init(6, 1, g_state.Winner_id + 100);
        effect_92_init(g_state.Winner_id, g_state.PL_Wins[g_state.Winner_id] - 1);
    }
}

static void Game_Manage_7_1() {
    if (--g_state.C_Timer == 0) {
        g_state.C_No[1]++;
        g_state.C_Timer = 10;
    }
}

static void Game_Manage_7_2() {
    if (!Button_Cut_EX(&g_state.C_Timer, 0x7FFF)) {
        return;
    }

    if (Check_Disp_Combo()) {
        g_state.C_Timer = 1;
        return;
    }

    g_state.C_No[1]++;

    if (Check_Disp_Winner() == 0) {
        g_state.C_Timer = 50;
    } else {
        Disp_Winner();
        g_state.C_Timer = 90;
    }

    if (g_state.Round_Operator[g_state.Winner_id] == 0 && g_state.Perfect_Flag == 0) {
        Check_Fade_Out_BGM(182);
    }
}

static s32 Check_Disp_Combo() {
    if (g_state.cmb_all_stock[0] != 0 || g_state.cmb_calc_now[0] != 0 || g_state.cmb_calc_now[1] != 0) {
        return 1;
    }

    if (g_state.PL_Wins[g_state.Winner_id] < CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
        return 0;
    }

    return 0;
}

static void Game_Manage_7_3() {
    if (g_state.Play_Type == 0 && g_state.Perfect_Flag == 0) {
        if (--g_state.C_Timer) {
            return;
        }
    } else {
        if (--g_state.C_Timer) {
            return;
        }
    }

    g_state.Message_Suicide[1] = 1;

    if (g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_PARRY_TRAINING || g_state.Mode_Type == MODE_TRIALS) {
        g_state.C_No[0] = 12;
        g_state.End_Training = 1;
        return;
    }

    if (g_state.Perfect_Flag) {
        g_state.C_No[1]++;
        g_state.C_Timer = 10;
        return;
    }

    g_state.C_No[0]++;
    g_state.C_No[1] = 0;
    g_state.Event_Judge_Gals = -1;
}

static void Game_Manage_7_4() {
    if (--g_state.C_Timer == 0) {
        g_state.C_No[1]++;
        request_center_message(4);
        effect_58_init(6, 1, 155);
        effect_58_init(6, 60, 156);
    }
}

static void Game_Manage_7_5() {
    if (!g_state.request_message) {
        g_state.C_No[1]++;
        g_state.C_Timer = 6;
        g_state.Event_Judge_Gals = -1;
    }
}

static void Game_Manage_7_6() {
    if (g_state.Scene_Cut) {
        g_state.C_Timer = 1;
    }

    if (--g_state.C_Timer == 0) {
        g_state.C_No[0]++;
        g_state.C_No[1] = 0;
    }
}

static void Game_Manage_7_7() {
    if (--g_state.C_Timer == 0) {
        g_state.C_No[1]++;
        g_state.Event_Judge_Gals = 3;
    }
}

static void Game_Manage_7_8() {
    if (g_state.Event_Judge_Gals == 0) {
        g_state.C_No[1]++;
        g_state.C_Timer = 30;
        Ck_Win_Record();
    }
}

static void Game_Manage_7_9() {
    if (--g_state.C_Timer == 0) {
        g_state.C_No[1] = 0;
    }
}

/** @brief Phase 8 dispatcher: post-match score tallying and bonus display. */
static void Game_Manage_8th() {
    switch (g_state.C_No[1]) {
    case 0:
        Game_Manage_8_0();
        break;
    case 1:
        Game_Manage_8_1();
        break;
    case 2:
        Game_Manage_8_2();
        break;
    case 3:
        Game_Manage_8_3();
        break;
    default:
        break;
    }
}

static void Game_Manage_8_0() {
    g_state.Round_num++;
    Quick_Entry();
    g_state.Stop_Update_Score = 1;

    if (g_state.Round_Operator[g_state.Winner_id] != 0 || g_state.Mode_Type == MODE_VERSUS || g_state.Mode_Type == 5) {
        Pool_Score(g_state.Winner_id);

        if (g_state.PL_Wins[g_state.Winner_id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            g_state.C_No[1]++;
            Additional_Bonus(g_state.WINNER);
            grade_makeup_stage_parameter(g_state.WINNER);
            grade_makeup_stage_parameter(g_state.LOSER);
            Check_Break_Into_CPU(g_state.WINNER);
            return;
        }

        g_state.C_No[1] = 3;
        g_state.C_Timer = 1;
        return;
    }

    g_state.C_No[1] = 3;
    g_state.C_Timer = 30;

    if (g_state.PL_Wins[g_state.Winner_id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
        grade_makeup_stage_parameter(g_state.WINNER);
        grade_makeup_stage_parameter(g_state.LOSER);
    }
}

static void Game_Manage_8_1() {
    switch (g_state.C_No[2]) {
    case 0:
        Game_Manage_81_0();
        break;
    case 1:
        Game_Manage_81_1();
        break;
    case 2:
        Game_Manage_81_2();
        break;
    case 3:
        Game_Manage_81_3();
        break;
    default:
        break;
    }
}

static void Game_Manage_81_0() {
    s16 time;
    s16 pos_id;
    s16 pos_id2;

    Check_Fade_Out_BGM(546);
    g_state.C_No[2]++;
    g_state.C_Timer = 20;
    g_state.Forbid_Break = -1;
    pos_id = 0;
    pos_id2 = 0;
    time = 1;
    g_state.Order[74] = 1;
    g_state.Order_Timer[74] = time;
    g_state.Order_Dir[74] = pos_id++;
    effect_76_init(74);
    time += 5;

    if (g_state.Perfect_Flag) {
        g_state.Order[76] = 1;
        g_state.Order_Timer[76] = time;
        g_state.Order_Dir[76] = pos_id++;
        effect_76_init(76);
        g_state.Order[81] = 0;
        effect_G0_init(81, time, g_state.Perfect_Bonus[g_state.Winner_id], pos_id2++);
        time += 5;
    }

    g_state.Order[78] = 1;
    g_state.Order_Timer[78] = time;
    g_state.Order_Dir[78] = pos_id++;
    effect_76_init(78);
    g_state.Order[83] = 0;
    effect_G0_init(83, time, g_state.Vital_Bonus[g_state.Winner_id], pos_id2++);
    time += 5;
    g_state.Order[79] = 1;
    g_state.Order_Timer[79] = time;
    g_state.Order_Dir[79] = pos_id++;
    effect_76_init(79);
    g_state.Order[84] = 0;
    effect_G0_init(84, time, g_state.Time_Bonus[g_state.Winner_id], pos_id2++);
    time += 5;
    g_state.Order[75] = 1;
    g_state.Order_Timer[75] = time;
    g_state.Order_Dir[75] = pos_id++;
    effect_76_init(75);
    g_state.Order[80] = 0;
    g_state.Order_Dir[80] = 1;
    effect_G0_init(80, time, g_state.Complete_Bonus, pos_id2);
}

static void Game_Manage_81_1() {
    if (g_state.Order_Dir[80] == 0) {
        g_state.C_No[2]++;
        g_state.C_Timer = 20;
    }
}

static void Game_Manage_81_2() {
    if (g_state.Scene_Cut) {
        g_state.C_Timer = 1;
    }

    if (--g_state.C_Timer != 0) {
        return;
    }

    g_state.C_No[2]++;
    g_state.Stop_Update_Score = 0;
    g_state.Order_Dir[80] = 1;
    g_state.Order[81] = 1;
    g_state.Order[83] = 1;
    g_state.Order[84] = 1;
    g_state.Order[80] = 1;
    Sound_SE(100);
}

static void Game_Manage_81_3() {
    if (g_state.Order_Dir[80] == 0) {
        g_state.C_No[1]++;
        g_state.C_No[2] = 0;
        g_state.C_Timer = 50;
    }
}

static void Game_Manage_8_2() {
    if (g_state.Request_Break[g_state.Winner_id ^ 1]) {
        g_state.C_Timer = 1;
    }

    if (g_state.Scene_Cut) {
        g_state.C_Timer = 1;
    }

    if (--g_state.C_Timer != 0) {
        return;
    }

    if (Check_Entry_Again()) {
        g_state.Forbid_Break = 0;
    }

    g_state.Disp_Cockpit = 0;
    g_state.Suicide[2] = 1;
    g_state.gauge_stop_flag[0] = 1;
    g_state.gauge_stop_flag[1] = 1;
    g_state.C_No[0]++;
    g_state.C_No[1] = 0;
    g_state.C_Timer = 30;
}

static void Game_Manage_8_3() {
    if (g_state.Scene_Cut) {
        g_state.C_Timer = 1;
    }

    if (--g_state.C_Timer == 0) {
        g_state.C_No[0]++;
        g_state.C_No[1] = 0;
    }
}

/** @brief Phase 9: Between-round transition — screen switch, BGM restart, next round init. */
static void Game_Manage_9th() {
    switch (g_state.C_No[1]) {
    case 0:
        if (g_state.PL_Wins[g_state.Winner_id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            g_state.C_No[0]++;
            g_state.C_No[1] = 0;
            g_state.C_Timer = 75;
            cpExitTask(TASK_PAUSE);

            if (g_state.Play_Type != 1 && g_state.Round_Operator[g_state.WINNER] && g_state.Battle_Q[g_state.WINNER]) {
                g_state.C_No[0] = 10;
            }

            break;
        }

        g_state.C_No[1]++;
        g_state.C_Timer = 60;
        g_state.Stop_Combo = 1;
        g_state.BGM_Timer[1] = 1;
        break;

    case 1:
        if (g_state.Scene_Cut) {
            g_state.C_Timer = 1;
        }

        if (--g_state.C_Timer > 0) {
            break;
        }

        g_state.C_No[1]++;
        g_state.Game_pause = 1;
        Switch_Screen_Init(0);

        if (g_state.judge_flag) {
            SsBgmFadeOut(2048);
        }

        /* fallthrough */

    default:
        if (Switch_Screen(0)) {
            g_state.BGM_No[0] = 1;
            g_state.BGM_Timer[0] = 1;
            g_state.G_No[2] = 5;
            g_state.G_No[3] = 0;
            g_state.G_Timer = 4;
            g_state.Cover_Timer = 5;
            g_state.C_No[0] = 1;
            g_state.C_No[1] = g_state.C_No[2] = g_state.C_No[3] = 0;
            g_state.Suicide[0] = 1;
            g_state.Suicide[6] = 1;
            g_state.judge_flag = 0;
        }

        break;
    }
}

/** @brief Phase 10: Post-match conclusion — naming check, ending check, exit to results. */
static void Game_Manage_10th() {
    switch (g_state.C_No[1]) {
    case 0:
        if (Button_Cut_EX(&g_state.C_Timer, 0x7FFF)) {
            g_state.C_No[1]++;
            g_state.Cover_Timer = 25;
            pulpul_stop();
            g_state.Stop_Combo = 1;
            g_state.Game_pause = 1;
            Switch_Screen_Init(0);
        }

        break;

    case 1:
        if (Switch_Screen(0)) {
            effect_work_quick_init();
            g_state.judge_flag = 0;
            SE_All_Off();
            Check_Naming(0);
            Check_Naming(1);
            g_state.pcon_rno[0] = 0;
            g_state.pcon_rno[1] = 0;
            g_state.pcon_rno[2] = 0;
            g_state.pcon_rno[3] = 0;
            g_state.appear_type = APPEAR_TYPE_ANIMATED;
            g_state.Continue_Coin2[g_state.WINNER] = 0;

            if (g_state.Mode_Type == MODE_VERSUS || g_state.Mode_Type == 5 || g_state.Round_Operator[g_state.WINNER]) {
                g_state.G_No[1] = 3;
                g_state.G_No[2] = 0;
                g_state.G_No[3] = 0;
                g_state.M_No[0] = 0;
                g_state.M_No[1] = 0;
                g_state.M_No[2] = 0;
                g_state.M_No[3] = 0;
                g_state.E_No[0] = 5;
                g_state.E_No[1] = 0;
                g_state.E_No[2] = 0;
                g_state.E_No[3] = 0;
                Check_Ending();
                g_state.Continue_Coin2[g_state.WINNER] = 0;
                Clear_Flash_No();
                break;
            }

            g_state.G_No[1] = 4;
            g_state.G_No[2] = 0;
            g_state.G_No[3] = 0;
            g_state.M_No[0] = 0;
            g_state.M_No[1] = 0;
            g_state.M_No[2] = 0;
            g_state.M_No[3] = 0;
            g_state.E_No[0] = 6;
            g_state.E_No[1] = 0;
            g_state.E_No[2] = 0;
            g_state.E_No[3] = 0;
            g_state.E_07_Flag[0] = 0;
            g_state.E_07_Flag[1] = 0;
            Clear_Flash_No();
        }

        break;
    }
}

/** @brief Clears ranking slots for a player unless they reached an ending path. */
static void Check_Naming(s16 PL_id) {
    if (g_state.Mode_Type != MODE_ARCADE) {
        return;
    }

    if (g_state.E_Number[PL_id][0] == 2) {
        return;
    }

    if (g_state.E_Number[PL_id][0] == 3) {
        return;
    }

    g_state.Rank_In[PL_id][0] = -1;
    g_state.Rank_In[PL_id][1] = -1;
    g_state.Rank_In[PL_id][2] = -1;
    g_state.Rank_In[PL_id][3] = -1;
}

/** @brief Checks if the winner qualifies for a character ending and initializes it. */
static s32 Check_Ending() {
    s16 xx;

    if (g_state.Play_Type == 1) {
        return 0;
    }

    if (Check_Ending_Sub()) {
        g_state.G_No[1] = 8;
        g_state.G_No[2] = 0;
        g_state.E_No[0] = 10;
        g_state.Break_Com[g_state.WINNER][0] = 1;
        g_state.Extra_Break = 0;
        g_state.Pause_ID = g_state.WINNER;
        g_state.End_PL = g_state.My_char[g_state.WINNER];
        g_state.plw[g_state.WINNER].wu.pl_operator = 0;
        g_state.Operator_Status[g_state.WINNER] = 0;
        SsBgmControl(0, 0);
        g_state.Control_Time = 481;
        Ending_init();
        g_state.Stock_My_char[g_state.WINNER] = g_state.My_char[g_state.WINNER];
        g_state.Stock_Player_Color[g_state.WINNER] = g_state.Player_Color[g_state.WINNER];

        if (g_state.Direction_Working[g_state.Present_Mode]) {
            return 1;
        }

        if (Check_Extra_Setting() == 0) {
            for (xx = 1; xx < 5; xx++) {
                save_w[xx].PL_Color[0][g_state.My_char[g_state.WINNER]] = 1;
                save_w[xx].PL_Color[1][g_state.My_char[g_state.WINNER]] = 1;
            }

            if (g_state.My_char[g_state.WINNER] == 0) {
                CurrentSave()->Extra_Option = 1;
            }
        }

        return 1;
    }

    return 0;
}

/** @brief Sub-check: returns true if winner has beaten enough opponents or debug ending is on. */
static s32 Check_Ending_Sub() {
    if (g_state.VS_Index[g_state.WINNER] > 9) {
        return 1;
    }

    if (Debug_w[DEBUG_ENDING_CHECK]) {
        return 1;
    }

    return 0;
}

/** @brief Adds perfect, vitality, time, and completion bonuses to the player's score. */
static void Additional_Bonus(s16 PL_id) {
    g_state.Complete_Bonus = Setup_Comp_Bonus();
    g_state.Score[PL_id][g_state.Play_Type] += g_state.Perfect_Bonus[g_state.Winner_id];
    g_state.Score[PL_id][g_state.Play_Type] += g_state.Vital_Bonus[g_state.Winner_id];
    g_state.Score[PL_id][g_state.Play_Type] += g_state.Time_Bonus[g_state.Winner_id];
    g_state.Score[PL_id][g_state.Play_Type] += g_state.Complete_Bonus;

    if (g_state.Score[PL_id][g_state.Play_Type] >= 99999900) {
        g_state.Score[PL_id][g_state.Play_Type] = 99999900;
    }
}

/** @brief Calculates the completion bonus based on the winner's straight win count. */
static u32 Setup_Comp_Bonus() {
    u32 xx;
    u16 zz;

    if (g_state.Play_Type == 1) {
        if (g_state.PL_Wins[g_state.Loser_id]) {
            return 0;
        }

        return 30000;
    }

    if (g_state.PL_Wins[g_state.Loser_id]) {
        return g_state.Straight_Counter[g_state.Winner_id] = 0;
    }

    g_state.Straight_Counter[g_state.Winner_id]++;

    if (g_state.Straight_Counter[g_state.Winner_id] >= 12) {
        g_state.Straight_Counter[g_state.Winner_id] = 11;
    }

    zz = g_state.Straight_Counter[g_state.Winner_id];
    xx = Comp_Bonus_Data[zz - 1];
    return xx;
}

/** @brief Requests a center-screen message display (e.g., "PERFECT", "DRAW GAME"). */
void request_center_message(s16 Kind_of_Message) {
    g_state.request_message = 1;
    g_state.message_index = Kind_of_Message;
}

/** @brief Sets the win mark type (SA finish, special, normal) and triggers sound effects. */
static void Setup_Win_Mark() {
    if (g_state.Round_Result & 0x200) {
        g_state.win_type[g_state.Winner_id][g_state.PL_Wins[g_state.Winner_id]] = 7;
        SsRequest(121);
        SsRequest(140);
        Finish_SE();
        return;
    }

    if (g_state.Round_Result & 0x180) {
        Control_Music_Fade(150);
        g_state.win_type[g_state.Winner_id][g_state.PL_Wins[g_state.Winner_id]] = 4;
        SsRequest(140);
        Finish_SE();
        return;
    }

    if (g_state.Round_Result & 0x800) {
        if (g_state.Shin_Gouki_BGM == 0) {
            Control_Music_Fade(150);
        } else {
            g_state.Shin_Gouki_BGM = 0;
        }

        g_state.win_type[g_state.Winner_id][g_state.PL_Wins[g_state.Winner_id]] = 4;
        SsRequest(0x8CU);
        Finish_SE();
        return;
    }

    g_state.win_type[g_state.Winner_id][g_state.PL_Wins[g_state.Winner_id]] = 1;
    SsRequest(121);
    SsRequest(140);
    Finish_SE();
}

/** @brief Checks if the player won with full health and sets the Perfect flag. */
static void Check_Perfect(s16 PL_id) {
    if (g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_PARRY_TRAINING || g_state.Mode_Type == MODE_TRIALS) {
        return;
    }

    if (g_state.plw[PL_id].wu.vitality != g_state.plw[PL_id].wu.vital_new) {
        return;
    }

    g_state.Perfect_Flag = 1;
    g_state.Perfect_Counter[g_state.Winner_id]++;
    g_state.Round_Result |= 2;
    g_state.win_type[PL_id][g_state.PL_Wins[PL_id]] = 3;
}

/** @brief Updates match history, VS index, win records, and scoring data after a set ends. */
static void Update_VS_Data() {
    if (g_state.PL_Wins[g_state.Winner_id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
        g_state.WINNER = g_state.Winner_id;
        g_state.LOSER = g_state.Loser_id;
        g_state.Stock_My_char[g_state.LOSER] = g_state.My_char[g_state.LOSER];
        g_state.Stock_Player_Color[g_state.LOSER] = g_state.Player_Color[g_state.LOSER];

        if (g_state.Play_Type != 0) {
            return;
        }

        if (g_state.Round_Operator[g_state.WINNER]) {
            g_state.SC_Personal_Time[g_state.WINNER] = g_state.Control_Time;
            g_state.Stage_Continue[g_state.WINNER] = 0;
            g_state.Request_Disp_Rank[g_state.LOSER][0] = -1;
            g_state.Request_Disp_Rank[g_state.LOSER][1] = -1;
            g_state.Request_Disp_Rank[g_state.LOSER][2] = -1;
            g_state.Request_Disp_Rank[g_state.LOSER][3] = -1;
            g_state.Stock_Com_Color[g_state.WINNER] = -1;
            g_state.Stock_Com_Arts[g_state.WINNER] = -1;
            g_state.EM_History[g_state.WINNER][g_state.VS_Index[g_state.WINNER]] = g_state.EM_id;
            g_state.Result_Timer[g_state.WINNER] += 30;

            if (g_state.EM_id == 17) {
                g_state.Break_Com[g_state.WINNER][g_state.EM_id] = (s8)(g_state.VS_Index[g_state.WINNER]);
            } else {
                g_state.VS_Index[g_state.WINNER]++;
                g_state.Break_Com[g_state.WINNER][g_state.EM_id] = 1;
            }

            if (g_state.PL_Wins[g_state.LOSER]) {
                g_state.Straight_Counter[g_state.WINNER] = 0;
                g_state.Straight_Flag[g_state.WINNER] = 1;
            }

            if (++g_state.Round_Level <= 7) {
                return;
            }

            g_state.Round_Level = 7;
            return;
        }

        g_state.Score[g_state.LOSER][0] = g_state.Stage_Stock_Score[g_state.LOSER];
        g_state.SC_Personal_Time[g_state.LOSER] = g_state.Control_Time;
        g_state.Win_Record[g_state.LOSER] = 0;
        g_state.Straight_Counter[g_state.LOSER] = 0;
        g_state.Straight_Flag[g_state.LOSER] = 1;
        return;
    }

    g_state.Score[g_state.Loser_id][0] = g_state.Stock_Score[g_state.Loser_id];
}

/** @brief Handles gradual BGM fade-in after a round-ending silence. */
static void BGM_Fade_Sub() {
    switch (g_state.BGM_No[1]) {
    case 1:
        if (--g_state.BGM_Timer[1] == 0) {
            g_state.BGM_No[1]++;
            g_state.BGM_Timer[1] = 1;
            g_state.BGM_Vol = -128;
        }

        break;

    case 0:
        break;

    default:
        if (--g_state.BGM_Timer[1] == 0) {
            g_state.BGM_Timer[1] = 2;

            if (++g_state.BGM_Vol == 0) {
                g_state.BGM_No[1] = 0;
            }
        }

        if (!g_state.Music_Fade) {
            SsBgmControl(0, g_state.BGM_Vol);
        }

        break;
    }
}

/** @brief Controls BGM playback state — delayed start, stage BGM, and victory music. */
static void BGM_Control() {
    switch (g_state.BGM_No[0]) {
    case 0:
        return;

    case 1:
        if (--g_state.BGM_Timer[0] == 0) {
            g_state.BGM_No[0]++;
        }

        /* fallthrough */

    case 2:
        g_state.BGM_No[0] = 0;

        if (g_state.Play_Type == 0 && g_state.EM_id == 17) {
            Stage_BGM(17, g_state.Round_num);
            break;
        }

        Stage_BGM(g_state.bg_w.stage, g_state.Round_num);
        break;

    case 3:
        if (--g_state.BGM_Timer[0] == 0) {
            g_state.BGM_No[0]++;
        }

        /* fallthrough */

    case 4:
        g_state.BGM_No[0] = 0;
        BGM_Request(56);
        break;
    }
}

/** @brief Starts a BGM fade-in from silence over the given time if music is allowed. */
static void Setup_BGM_Fade_In(s16 Time) {
    if (!g_state.PB_Music_Off) {
        g_state.BGM_No[1] = 1;
        g_state.BGM_Timer[1] = Time;
    }
}

/** @brief Selects and starts the correct stage BGM based on opponent and boss status. */
static void Check_Stage_BGM() {
    if (g_state.Play_Type == 1) {
        Stage_BGM(g_state.bg_w.stage, g_state.Round_num);
        return;
    }

    switch (g_state.EM_id) {
    case 1:
        if (g_state.My_char[g_state.Player_id] != 0) {
            Stage_BGM(g_state.bg_w.stage, g_state.Round_num);
        }

        /* fallthrough */

    case 0:
        if (g_state.Introduce_Boss[g_state.Player_id][1] & 0x80) {
            Stage_BGM(g_state.bg_w.stage, g_state.Round_num);
        }

        break;

    case 17:
        Stage_BGM(17, g_state.Round_num);
        break;

    default:
        Stage_BGM(g_state.bg_w.stage, g_state.Round_num);
        break;
    }
}

/** @brief Fades music in and mutes the current BGM. */
void Control_Music_Fade(s16 Time) {
    Setup_BGM_Fade_In(Time);
    SsBgmControl(0, -128);
}

/** @brief Determines conclusion type and updates lost-round/time-finish stats. */
static void Check_Conclusion_Type() {
    if (g_state.Play_Type == 1) {
        return;
    }

    switch (g_state.Conclusion_Type) {
    case 0:
        chkComWins();
        break;

    case 1:
        g_state.Lost_Round[g_state.Player_id]++;
        g_state.Stage_Lost_Round[g_state.Player_id]++;
        break;

    case 2:
        if (g_state.plw[0].wu.vital_new != g_state.plw[1].wu.vital_new) {
            g_state.Stage_Time_Finish[g_state.Winner_id]++;
            chkComWins();
        }

        break;
    }
}

/** @brief Increments lost rounds for the COM-controlled loser. */
static void chkComWins() {
    if (g_state.Round_Operator[g_state.Winner_id] == 0) {
        g_state.Lost_Round[g_state.Loser_id]++;
        g_state.Stage_Lost_Round[g_state.Loser_id]++;
    }
}

/** @brief Updates the break-in term data — SA finish, perfect, cheap finish counters. */
static void Update_BI_Term() {
    if (g_state.Play_Type == 1) {
        return;
    }

    if (g_state.plw[g_state.Winner_id].sa_healing) {
        g_state.Super_Arts_Finish[g_state.Winner_id]++;
        g_state.Stage_SA_Finish[g_state.Winner_id]++;
        return;
    }

    if (g_state.plw[g_state.Winner_id].wu.vitality == g_state.plw[g_state.Winner_id].wu.vital_new) {
        g_state.Perfect_Finish[g_state.Winner_id]++;
        g_state.Stage_Perfect_Finish[g_state.Winner_id]++;

        if (g_state.Round_Result & 0x980) {
            g_state.Super_Arts_Finish[g_state.Winner_id]++;
            g_state.Stage_SA_Finish[g_state.Winner_id]++;
        }

        return;
    }

    if (g_state.Round_Result & 0x200) {
        g_state.Cheap_Finish[g_state.Winner_id]++;
        g_state.Stage_Cheap_Finish[g_state.Winner_id]++;
        return;
    }

    if (g_state.Round_Result & 0x980) {
        g_state.Super_Arts_Finish[g_state.Winner_id]++;
        g_state.Stage_SA_Finish[g_state.Winner_id]++;
    }
}

/** @brief Updates the raw win record (arcade or versus mode). */
static void Ck_Win_Record() {
    if (g_state.PL_Wins[g_state.Winner_id] < CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
        return;
    }

    switch (g_state.Mode_Type) {
    case MODE_ARCADE:
        if (g_state.Play_Type == 1) {
            g_state.Win_Record[g_state.Loser_id] = 0;

            if (++g_state.Win_Record[g_state.Winner_id] > 999) {
                g_state.Win_Record[g_state.Winner_id] = 999;
            }

            g_state.Stock_Win_Record[g_state.Winner_id] = g_state.Win_Record[g_state.Winner_id];
        }

        break;

    case MODE_VERSUS:
        if (++g_state.VS_Win_Record[g_state.Winner_id] > 999) {
            g_state.VS_Win_Record[g_state.Winner_id] = 999;
        }

        break;

    default:
        // Do nothing
        break;
    }
}

/** @brief Adjusts difficulty control time — increases for human wins, decreases for CPU wins. */
static void Update_Level_Control() {
    if (g_state.Round_Operator[g_state.Winner_id]) {
        if ((g_state.Round_Operator[g_state.Loser_id]) != 0) {
            return;
        }

        g_state.Control_Time += 40;

        if (g_state.Control_Time > g_state.Limit_Time) {
            g_state.Control_Time = g_state.Limit_Time;
        }

        return;
    }

    if ((g_state.Control_Time -= 40) < 0) {
        g_state.Control_Time = 0;
    }
}

/** @brief Returns 1 if the next round should be a draw (both players at same win count at max). */
static s32 Judge_Next_Disposal() {
    if (g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_PARRY_TRAINING || g_state.Mode_Type == MODE_TRIALS) {
        return 0;
    }

    if (g_state.PL_Wins[0] != g_state.PL_Wins[1]) {
        return 0;
    }

    if (g_state.PL_Wins[0] >= CurrentSave()->Battle_Number[g_state.Play_Type]) {
        return 1;
    }

    return 0;
}

/** @brief Handles quick-entry logic at match end: loser removal, best grade, continue. */
static void Quick_Entry() {
    s8 grade;

    if (Check_Entry_Again()) {
        g_state.Forbid_Break = 0;
        g_state.Extra_Break = 0;
    }

    if (g_state.PL_Wins[g_state.Winner_id] < CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
        return;
    }

    if (g_state.plw[g_state.LOSER].wu.pl_operator) {
        Loser_Sub();

        if (g_state.Mode_Type != MODE_ARCADE) {
            g_state.plw[g_state.LOSER].wu.pl_operator = 1;
        }

        Be_Continue();
    }

    if (g_state.Play_Type == 1) {
        grade = g_state.judge_item[g_state.Winner_id][1].grade;

        if (grade > g_state.Best_Grade[g_state.Winner_id]) {
            g_state.Best_Grade[g_state.Winner_id] = grade;
        }
    }
}

/** @brief Returns 1 if a new challenger can enter, based on battle queue and VS index. */
static s32 Check_Entry_Again() {
    if (g_state.Battle_Q[g_state.Winner_id]) {
        return 0;
    }

    if (g_state.Play_Type == 1) {
        return 1;
    }

    if (g_state.VS_Index[g_state.WINNER] >= 6) {
        return 0;
    }

    if (g_state.VS_Index[g_state.WINNER] < 10) {
        return 1;
    }

    return 0;
}

/** @brief Clears operator status for the loser and decrements difficulty. */
static void Loser_Sub() {
    g_state.plw[g_state.LOSER].wu.pl_operator = 0;
    g_state.Operator_Status[g_state.LOSER] = 0;
    g_state.Sel_PL_Complete[g_state.LOSER] = 0;
    g_state.Sel_Arts_Complete[g_state.LOSER] = 0;

    if (g_state.Play_Type == 0) {
        if (--g_state.Round_Level < 0) {
            g_state.Round_Level = 0;
        }

        g_state.Stage_Continue[g_state.LOSER]++;
    }
}

/** @brief Sets up continue-screen data for the losing player in arcade mode. */
static void Be_Continue() {
    if (g_state.Mode_Type != MODE_ARCADE) {
        return;
    }

    g_state.Continue_Count_Down[g_state.LOSER] = 0;
    g_state.Continue_Count[g_state.LOSER] = 9;
    g_state.E_Number[g_state.LOSER][0] = 5;
    g_state.E_Number[g_state.LOSER][0] = 5;
    g_state.E_Number[g_state.LOSER][1] = 0;
    g_state.E_Number[g_state.LOSER][2] = 0;
    g_state.E_Number[g_state.LOSER][3] = 0;
}

/** @brief Shows the winner/loser announcement effect and sound. */
static void Disp_Winner() {
    if (g_state.Play_Type == 1) {
        effect_56_init(g_state.My_char[g_state.Winner_id] + 7, 1);
        SsRequest(141);
    } else if (g_state.Round_Operator[g_state.Winner_id]) {
        effect_56_init(5, 1);
        SsRequest(141);
    } else {
        effect_56_init(6, 1);
        SsRequest(142);
    }
}

/** @brief Accumulates perfect, vitality, and time bonus scores for the winner. */
static void Pool_Score(s16 PL_id) {
    u32 Score_Buff;

    if (g_state.Perfect_Flag) {
        g_state.Perfect_Bonus[g_state.Winner_id] += 50000;
    }

    Score_Buff = g_state.plw[PL_id].wu.vital_new * 100 / g_state.Max_vitality;
    Score_Buff *= 500;
    g_state.Vital_Bonus[g_state.Winner_id] += Score_Buff;

    if (CurrentSave()->Time_Limit == -1) {
        g_state.Time_Bonus[g_state.Winner_id] = 0;
    } else {
        g_state.Time_Bonus[g_state.Winner_id] += g_state.round_timer * 300;
    }
}

/** @brief Checks if conditions are met for a hidden boss (Shin Gouki) break-in. */
static s32 Check_Break_Into_CPU(s16 PL_id) {
    if (Debug_w[DEBUG_YOSHIZUMI_EXP] == 9) {
        g_state.Break_Into_CPU = 2;
        return g_state.Battle_Q[PL_id] = 1;
    }

    g_state.Break_Into_CPU = 0;
    g_state.Battle_Q[PL_id] = 0;

    if (g_state.Round_Result & 0x8000) {
        return 0;
    }

    if (g_state.Break_Com[PL_id][17]) {
        return 0;
    }

    if (g_state.Continue_Coin[PL_id]) {
        return 0;
    }

    if (g_state.VS_Index[PL_id] < 7 || g_state.VS_Index[PL_id] >= 9) {
        return 0;
    }

    if (g_state.Straight_Flag[PL_id]) {
        return 0;
    }

    if (g_state.judge_final[g_state.Player_id][0].sp_point < 2) {
        return 0;
    }

    if (g_state.Super_Arts_Finish[PL_id] < BIC_SA_Data[0][CurrentSave()->Battle_Number[g_state.Play_Type]]) {
        return 0;
    }

    if (Check_BI_Grade(PL_id)) {
        g_state.Break_Into_CPU = 2;
        return g_state.Battle_Q[PL_id] = 1;
    }

    return 0;
}

/** @brief Determines the overall match winner via judgement gals grade comparison. */
static void Judge_Winner() {
    grade_makeup_judgement_gals();

    if (g_state.judge_gals[0].grade == g_state.judge_gals[1].grade) {
        if (g_state.Play_Type == 0) {
            g_state.Winner_id = g_state.Player_id;
            g_state.Loser_id = g_state.COM_id;
            return;
        }

        g_state.Winner_id = g_state.Champion;
        g_state.Loser_id = g_state.Champion ^ 1;
        return;
    }

    if (g_state.judge_gals[0].grade > g_state.judge_gals[1].grade) {
        g_state.Winner_id = 0;
        g_state.Loser_id = 1;
        return;
    }

    g_state.Winner_id = 1;
    g_state.Loser_id = 0;
}

/** @brief Returns 1 if the winner's name should be displayed on screen after the match. */
static s32 Check_Disp_Winner() {
    if (g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_PARRY_TRAINING || g_state.Mode_Type == MODE_TRIALS) {
        return g_state.Disp_Win_Name = 0;
    }

    if (g_state.PL_Wins[g_state.Winner_id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
        return g_state.Disp_Win_Name = 1;
    }

    if (g_state.Conclusion_Type == 0) {
        return g_state.Disp_Win_Name = 0;
    }

    return g_state.Disp_Win_Name = 1;
}

/** @brief Initiates BGM fade-out if the match is over and music hasn't been faded yet. */
static void Check_Fade_Out_BGM(s16 Time) {
    if (g_state.Music_Fade) {
        return;
    }

    if (g_state.PL_Wins[g_state.Winner_id] < CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
        return;
    }

    g_state.Music_Fade = 1;
    SsBgmFadeOut(Time);
}

/** @brief Returns 1 if all the player's VS grades are high enough for a boss break-in. */
static s32 Check_BI_Grade(s16 PL_id) {
    s16 ix;

    for (ix = 0; ix < g_state.VS_Index[PL_id]; ix++) {
        if (g_state.judge_final[PL_id][0].vs_cpu_grade[ix] < 9) {
            return 0;
        }
    }

    return 1;
}

/** @brief Phase 11: Intro sequence for rival/sub-boss encounter. */
static void Game_Manage_11th() {
    switch (g_state.C_No[1]) {
    case 0:
        g_state.Forbid_Break = -1;
        g_state.C_No[1]++;
        g_state.EM_Rank = 2;
        g_state.Q_Country = g_state.Battle_Country;
        g_state.C_Timer = 90;
        effect_81_init(30);
        break;

    case 1:
        if (--g_state.C_Timer == 0) {
            g_state.C_No[1]++;
            g_state.C_Timer = 150;
            g_state.C_Timer = 60;
        }

        break;

    case 2:
        if (--g_state.C_Timer == 0) {
            g_state.C_No[1]++;
        }

        break;

    case 3:
        g_state.C_No[1]++;
        Switch_Screen_Init(0);
        break;

    case 4:
        if (Switch_Screen(0)) {
            g_state.G_No[1] = 11;
            g_state.G_No[2] = 0;
            g_state.G_No[3] = 0;
            g_state.E_No[0] = 9;
            g_state.E_No[1] = 0;
            g_state.E_No[2] = 0;
            g_state.E_No[3] = 0;
            effect_work_kill_mod_plcol();
            g_state.Cover_Timer = 21;
        }

        break;
    }
}

/** @brief Phase 12 dispatcher: bonus stage (car destruction / parry ball) management. */
static void Game_Manage_12th() {
    switch (g_state.C_No[1]) {
    case 0:
        Game_Manage_12_0();
        break;
    case 1:
        Game_Manage_12_1();
        break;
    case 2:
        Game_Manage_12_2();
        break;
    case 3:
        Game_Manage_12_3();
        break;
    case 4:
        Game_Manage_12_4();
        break;
    case 5:
        Game_Manage_12_5();
        break;
    case 6:
        Game_Manage_12_1();
        break; /* fallthrough from parry ball path */
    case 7:
        Game_Manage_12_7();
        break;
    case 8:
        Game_Manage_12_8();
        break;
    case 9:
        Game_Manage_12_5();
        break; /* fallthrough from parry ball path */
    default:
        break;
    }

    if (g_state.Bonus_Type == 20) {
        bcounter_write();
    }
}

static void Game_Manage_12_0() {
    s16 ix;

    g_state.Suicide[0] = 0;
    g_state.Suicide[6] = 0;
    g_state.Suicide[5] = 0;

    if (effect_84_init()) {
        return;
    }

    g_state.C_No[1]++;
    g_state.Extra_Break = 0;
    g_state.request_message = 0;
    g_state.Complete_Victory = 0;
    g_state.Conclusion_Flag = 0;
    g_state.Perfect_Flag = 0;
    g_state.Round_Result = 0;
    g_state.Reserve_Cut = 0;
    g_state.Next_Step = 0;
    g_state.judge_flag = 0;
    g_state.Stop_Combo = 0;

    if (g_state.Demo_Flag) {
        g_state.Stop_SG = 0;
    }

    g_state.Complete_Judgement = 0;
    g_state.Music_Fade = 0;
    g_state.Round_Operator[0] = g_state.plw[0].wu.pl_operator;
    g_state.Round_Operator[1] = g_state.plw[1].wu.pl_operator;
    g_state.CP_No[0][0] = 0;
    g_state.CP_No[1][0] = 0;

    for (ix = 0; ix < 4; ix++) {
        g_state.Message_Suicide[ix] = 0;
    }

    g_state.Stock_Score[g_state.Player_id] = g_state.Score[g_state.Player_id][0];

    if (g_state.Bonus_Type == 20) {
        g_state.C_No[1] = 6;
        g_state.Time_Stop = 1;
        g_state.Time_Over = false;
        g_state.Exit_No = 0;
        g_state.Unit_Of_Timer = 0;
        setup_bonus_car_parts();
        bcount_cont_init();
    }
}

static void Game_Manage_12_1() {
    bcount_cont_main();

    if (g_state.Next_Step != 0) {
        g_state.C_No[1]++;
        g_state.C_No[2] = 0;
        g_state.C_No[3] = 0;
        g_state.Allow_a_battle_f = 1;
        Disp_Bonus_Contents = 0;
    }
}

static void Game_Manage_12_2() {
    bcount_cont_main();

    if (!g_state.Bonus_Game_Complete) {
        return;
    }

    g_state.C_No[1]++;
    g_state.C_No[2] = 0;
    g_state.C_Timer = 30;
    g_state.Allow_a_battle_f = 0;
    g_state.Forbid_Break = -1;
    g_state.Completion_Bonus[g_state.Player_id][1] = -128;
    g_state.Stock_Bonus_Game_Result = g_state.Bonus_Game_result;
    g_state.Bonus_Score = 0;
    g_state.Final_Bonus_Score = Setup_Final_Score(21);
    effect_58_init(6, 10, 169);
    grade_makeup_bonus_parameter(g_state.Player_id);

    if (Check_Bonus_Perfect()) {
        g_state.C_Timer = 20;
    } else {
        g_state.C_No[1] = 4;
    }

    cpExitTask(TASK_PAUSE);
}

static void Game_Manage_12_3() {
    switch (g_state.C_No[2]) {
    case 0:
        if (Cut_Cut_C_Timer() == 0) {
            g_state.C_No[2]++;
            g_state.C_Timer = 10;
            request_center_message(4);
            effect_58_init(6, 1, 155);
            effect_58_init(6, 60, 156);
        }

        break;

    case 1:
        if (g_state.request_message == 0) {
            g_state.C_No[2]++;
            g_state.C_Timer = 6;
        }

        break;

    case 2:
        if (--g_state.C_Timer == 0) {
            g_state.C_No[2]++;
            g_state.C_Timer = 20;
        }

        break;

    case 3:
        if (Cut_Cut_C_Timer() == 0) {
            g_state.C_No[1]++;
            g_state.C_No[2] = 0;
            g_state.C_No[3] = 0;
            g_state.C_Timer = 30;
        }

        break;
    }
}

static void Game_Manage_12_4() {
    switch (g_state.C_No[2]) {
    case 0:
        if (Bonus_Cut_Sub() == 0 && --g_state.C_Timer == 0) {
            g_state.C_No[2]++;
            g_state.C_Timer = 20;
            effect_08_init(7, 0, 1, 15, 0);
            g_state.Disp_Score_Buff[0] = g_state.Bonus_Score;
            effect_14_init(0, 35, 11, 15);
        }

        break;

    case 1:
        if (Bonus_Cut_Sub() == 0 && --g_state.C_Timer == 0) {
            g_state.C_No[2]++;
            g_state.C_Timer = 1;
            g_state.Bonus_Score = 0;
        }

        break;

    case 2:
        if (Bonus_Cut_Sub() == 0 && --g_state.C_Timer == 0) {
            if (g_state.Bonus_Game_result == 0 && !(g_state.PB_Status & 2)) {
                g_state.C_No[2] = 4;
                g_state.C_Timer = 30;
                break;
            }

            if (g_state.Bonus_Game_result == 0) {
                g_state.Bonus_Game_result = 1;
            } else {
                g_state.Bonus_Score += 1000;
                g_state.Score[g_state.Player_id][0] += 1000;
                g_state.Disp_Score_Buff[0] = g_state.Bonus_Score;
                Sound_SE(100);
            }

            if (--g_state.Bonus_Game_result == 0) {
                g_state.C_No[2]++;

                if (g_state.PB_Status) {
                    g_state.C_No[3] = 1;
                    g_state.C_Timer = 10;
                    break;
                }

                g_state.C_No[3] = 0;
                g_state.C_Timer = 20;
                break;
            }

            g_state.C_Timer = 3;
        }

        break;

    case 3:
        switch (g_state.C_No[3]) {
        case 0:
            if (Bonus_Cut_Sub() == 0 && --g_state.C_Timer == 0) {
                g_state.C_No[2]++;
                g_state.C_Timer = 30;
                g_state.Bonus_Game_result = g_state.Stock_Bonus_Game_Result;
            }

            break;

        case 1:
            if (Bonus_Cut_Sub() == 0 && --g_state.C_Timer == 0) {
                g_state.C_No[3]++;
                g_state.C_Timer = 10;
                Disp_Bonus_Perfect();
            }

            break;

        case 2:
            if (Bonus_Cut_Sub() == 0 && --g_state.C_Timer == 0) {
                g_state.C_No[3]++;
                g_state.C_Timer = 40;

                if (g_state.PB_Status & 1) {
                    g_state.Score[g_state.Player_id][0] += Ball_Perfect_PTS[0][g_state.Bonus_Stage_Level];
                }

                if (g_state.PB_Status & 2) {
                    g_state.Score[g_state.Player_id][0] += Ball_Perfect_PTS[1][g_state.Bonus_Stage_Level];
                }

                if (g_state.Score[g_state.Player_id][0] >= 99999900) {
                    g_state.Score[g_state.Player_id][0] = 99999900;
                }

                Flash_Bonus_Perfect();
                break;
            }

            break;

        default:
            if (--g_state.C_Timer == 0) {
                g_state.C_No[2]++;
                g_state.C_Timer = 30;
            }

            break;
        }

        break;

    default:
        if (Cut_Cut_C_Timer() == 0) {
            g_state.C_No[1]++;
            g_state.C_No[2] = 0;
            g_state.C_No[3] = 0;
            g_state.C_Timer = 10;
            g_state.Forbid_Break = 0;
            g_state.Suicide[5] = 1;
            Check_Fade_Out_BGM(546);
        }

        break;
    }
}

static void Game_Manage_12_5() {
    switch (g_state.C_No[2]) {
    case 0:
        if (Debug_w[DEBUG_YOSHIZUMI_EXP] != 2 && --g_state.C_Timer == 0) {
            g_state.C_No[2]++;
            g_state.C_Timer = 20;
        }

        break;

    case 1:
        if (g_state.Scene_Cut) {
            g_state.C_Timer = 1;
        }

        if (--g_state.C_Timer == 0) {
            g_state.C_No[2]++;
        }

        break;

    default:
        MANAGE_X = 1;
        break;
    }
}

static void Game_Manage_12_7() {
    bcount_cont_main();

    if (Check_Time_Over()) {
        return;
    }

    if (!g_state.Bonus_Game_Complete) {
        return;
    }

    g_state.C_No[1]++;
    g_state.C_No[2] = 0;
    g_state.C_No[3] = 0;
    g_state.C_Timer = 30;
    g_state.Allow_a_battle_f = 0;
    g_state.Forbid_Break = -1;
    g_state.Completion_Bonus[g_state.Player_id][0] = -128;
    g_state.Final_Bonus_Score = Setup_Final_Score(20);
    grade_makeup_bonus_parameter(g_state.Player_id);
    effect_58_init(6, 10, 169);
}

static void Game_Manage_12_8() {
    switch (g_state.C_No[2]) {
    case 0:
        switch (g_state.C_No[3]) {
        case 0:
            g_state.Next_Step = 0;

            if (effect_35_init(60, 10) == 0) {
                g_state.C_No[3]++;
            }

            break;

        case 1:
            if (g_state.Next_Step) {
                g_state.C_No[3]++;
                g_state.C_Timer = 20;
            }

            break;

        case 2:
            if (g_state.C_Timer < 11 && g_state.Scene_Cut) {
                g_state.C_Timer = 1;
            }

            if (--g_state.C_Timer == 0) {
                g_state.C_No[2]++;
                g_state.C_No[3] = 0;
                g_state.C_Timer = 30;
            }

            break;
        }

        break;

    case 1:
        if (Bonus_Cut_Sub() == 0 && --g_state.C_Timer == 0) {
            g_state.C_No[2]++;
            g_state.C_Timer = 20;
            g_state.Score[g_state.Player_id][0] += g_state.Bonus_Score;
            effect_08_init(7, 0, 1, 15, 0);
            g_state.Disp_Score_Buff[0] = g_state.Bonus_Score;
            effect_14_init(0, 35, 11, 15);

            if (g_state.Bonus_Game_result == 0) {
                g_state.C_No[2] = 99;
                g_state.C_Timer = 120;
            }
        }

        break;

    case 2:
        if (Bonus_Cut_Sub() == 0 && --g_state.C_Timer == 0) {
            g_state.C_No[2]++;
            g_state.C_Timer = 1;
        }

        break;

    case 3:
        if (Bonus_Cut_Sub() == 0 && --g_state.C_Timer == 0) {
            if (bcounter_down(0) == 0) {
                g_state.C_No[2]++;
                g_state.C_Timer = 30;
                g_state.C_Timer = 3;
                g_state.Bonus_Score += 1000;
                g_state.Score[g_state.Player_id][0] += 1000;
                g_state.Disp_Score_Buff[0] = g_state.Bonus_Score;
                Sound_SE(100);
                break;
            }

            g_state.C_Timer = 3;
            g_state.Bonus_Score += 1000;
            g_state.Score[g_state.Player_id][0] += 1000;
            g_state.Disp_Score_Buff[0] = g_state.Bonus_Score;
            Sound_SE(100);
        }

        break;

    case 4:
        if (--g_state.C_Timer == 0) {
            g_state.C_No[2]++;
            g_state.C_Timer = 30;
        }

        break;

    default:
        if (Debug_w[DEBUG_YOSHIZUMI_EXP] != 2 && Cut_Cut_C_Timer() == 0) {
            g_state.C_No[1]++;
            g_state.C_No[2] = 0;
            g_state.C_No[3] = 0;
            g_state.C_Timer = 10;
            g_state.Forbid_Break = 0;
            g_state.Suicide[5] = -128;
            Check_Fade_Out_BGM(546);
        }

        break;
    }
}

/** @brief Checks if both bonus stage targets were fully destroyed (perfect bonus). */
static u8 Check_Bonus_Perfect() {
    g_state.PB_Status = 0;

    if (g_state.Stock_Bonus_Game_Result >= 20) {
        g_state.PB_Status |= 1;
    }

    if (g_state.Bonus_Game_ex_result >= 20) {
        g_state.PB_Status |= 2;
    }

    return g_state.PB_Status;
}

/** @brief Displays the bonus perfect splash effect and score. */
static void Disp_Bonus_Perfect() {
    switch (g_state.PB_Status) {
    case 1:
        effect_08_init(6, 0, 5, 15, 0);
        g_state.Disp_Score_Buff[0] = Ball_Perfect_PTS[0][g_state.Bonus_Stage_Level];
        effect_14_init(0, 35, 15, 15);
        break;

    case 2:
        effect_08_init(6, 0, 5, 26, 1);
        g_state.Disp_Score_Buff[1] = Ball_Perfect_PTS[1][g_state.Bonus_Stage_Level];
        effect_14_init(1, 35, 15, 26);
        break;

    case 3:
        effect_08_init(6, 0, 5, 15, 0);
        g_state.Disp_Score_Buff[0] = Ball_Perfect_PTS[0][g_state.Bonus_Stage_Level];
        effect_14_init(0, 35, 15, 15);
        effect_08_init(6, 0, 9, 26, 1);
        g_state.Disp_Score_Buff[1] = Ball_Perfect_PTS[1][g_state.Bonus_Stage_Level];
        effect_14_init(1, 35, 19, 26);
        break;
    }

    SsRequest(g_state.Winner_id + 102);
}

/** @brief Triggers the flash effect for bonus perfect achievement. */
static void Flash_Bonus_Perfect() {
    switch (g_state.PB_Status) {
    case 1:
        g_state.Suicide[5] = 1;
        break;

    case 2:
        g_state.Suicide[5] = 1;
        break;

    case 3:
        g_state.Suicide[5] = 1;
        break;
    }
}

static u32 Setup_Final_Score(s16 Type) {
    u32 xx;

    if (Type == 21) {
        xx = g_state.Bonus_Game_result * 1000;

        if (g_state.Stock_Bonus_Game_Result >= 20) {
            xx += Ball_Perfect_PTS[0][g_state.Bonus_Stage_Level];
        }

        if (g_state.Bonus_Game_ex_result >= 20) {
            xx += Ball_Perfect_PTS[1][g_state.Bonus_Stage_Level];
        }

        xx += g_state.Score[g_state.Player_id][0];

        if (xx >= 99999900) {
            xx = 99999900;
        }

        return xx;
    }

    switch (g_state.Bonus_Game_result) {
    case 2:
        xx = 30000;
        break;

    case 3:
        xx = 50000;
        break;

    default:
        xx = 0;
        break;
    }

    g_state.Bonus_Score = xx;
    xx += g_state.Counter_hi * 1000;
    g_state.Bonus_Score_Plus = xx;
    xx += g_state.Score[g_state.Player_id][0];

    if (xx >= 99999900) {
        xx = 99999900;
    }

    return xx;
}

static s32 Bonus_Cut_Sub() {
    if (g_state.Scene_Cut) {
        Sound_SE(100);
        g_state.Bonus_Game_result = 0;
        g_state.Score[g_state.Player_id][0] = g_state.Final_Bonus_Score;

        if (g_state.Score[g_state.Player_id][0] >= 99999900) {
            g_state.Score[g_state.Player_id][0] = 99999900;
        }

        if (Disp_Bonus_Contents == 0) {
            effect_08_init(7, 0, 1, 15, 0);
        }

        if (g_state.Bonus_Type == 21) {
            if (Disp_Bonus_Contents == 0) {
                g_state.Disp_Score_Buff[0] = g_state.Stock_Bonus_Game_Result * 1000;
                effect_14_init(0, 35, 11, 15);
            }

            Disp_Bonus_Perfect();
            Flash_Bonus_Perfect();
            g_state.C_No[2] = 3;
            g_state.C_No[3] = 99;
            return g_state.C_Timer = 90;
        }

        bcounter_down(1);

        if (Disp_Bonus_Contents == 0) {
            g_state.Disp_Score_Buff[0] = g_state.Bonus_Score_Plus;
            effect_14_init(0, 35, 11, 15);
        }

        g_state.C_No[2] = 4;
        g_state.C_No[3] = 99;
        return g_state.C_Timer = 90;
    }

    return 0;
}

static s16 Check_Time_Over() {
    s16 return_x = 0;

    switch (g_state.C_No[2]) {
    case 0:
        if (g_state.Time_Over) {
            g_state.C_No[2]++;
            g_state.C_Timer = 60;
            request_center_message(2);
            SsRequest(143);
            return_x = 1;
        }

        break;

    case 1:
        if (--g_state.C_Timer == 0) {
            g_state.C_No[2]++;
            g_state.Game_pause = 0;
            g_state.Suicide[5] = 1;
        }

        break;
    }

    return return_x;
}

/** @brief Pauses the game briefly for a complete victory announcement. */
void complete_victory_pause() {
    g_state.Complete_Victory = 1;
}

static void Game_Manage_13th() {};
