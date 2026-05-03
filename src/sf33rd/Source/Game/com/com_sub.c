/**
 * @file com_sub.c
 * @brief CPU-controlled character AI subroutines.
 *
 * Provides the building blocks used by per-character AI patterns (active/passive/shell).
 * Includes movement primitives (walk, jump, approach, keep-away), attack commands
 * (normal, lever, command/special, rapid), guard logic, projectile handling,
 * distance/area calculations, difficulty scaling, and reaction timing.
 *
 * Part of the COM (computer player) AI module.
 */

#include "sf33rd/Source/Game/com/com_sub.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/com/active/ac0000.h"
#include "sf33rd/Source/Game/com/active/ac0001.h"
#include "sf33rd/Source/Game/com/active/ac0002.h"
#include "sf33rd/Source/Game/com/active/ac0003.h"
#include "sf33rd/Source/Game/com/active/ac0004.h"
#include "sf33rd/Source/Game/com/ck_pass.h"
#include "sf33rd/Source/Game/com/com_data.h"
#include "sf33rd/Source/Game/com/com_datu.h"
#include "sf33rd/Source/Game/com/com_pl.h"
#include "sf33rd/Source/Game/com/follow/fl_com00.h"
#include "sf33rd/Source/Game/com/follow/fl_com02.h"
#include "sf33rd/Source/Game/com/passive/pass0000.h"
#include "sf33rd/Source/Game/com/passive/pass0001.h"
#include "sf33rd/Source/Game/com/passive/pass0002.h"
#include "sf33rd/Source/Game/com/passive/pass0003.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/cmd_data.h"
#include "sf33rd/Source/Game/engine/cmd_main.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/pls03.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/screen/vs_shell.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "structs.h"

// sbss
s8 Lv;
s8 Rnd;

// forward decls
void End_Pattern(PLW* wk);
void Next_Be_Passive(PLW* wk, s32);
void Turn_Over_On(PLW* wk);
void Only_Shot(PLW* wk, s16 Lever_Data);
void Lever_On(PLW* wk, u16 LR_Lever, u16 UD_Lever);
void Lever_Off(PLW* wk);
void Enable_Overhead_Attack_Flag(PLW* wk);
void Setup_DENJIN_LEVEL(PLW* wk);
void Hold_Attack_Button(PLW* wk, s16 Power_Level);
s32 Check_Exit_DENJIN(PLW* wk);
void Keep_Away(PLW* wk, s16 Target_Pos, s16 Option);
void Setup_KA_Jump(PLW* wk);
void Setup_Keep_Away_Walk(PLW* wk);
void Check_Safe_Retreat_Space(PLW* wk, s16 Move_Value, s16 Next_Action, s16 Next_Menu);
void Approach_Walk(PLW* wk, s16 Target_Pos, s16 Option);
s32 Check_Arrival(PLW* wk, s16 Target_Pos, s16 Option);
void Walk(PLW* wk, u16 Lever, s16 Time, s16 unused);
void Forced_Guard(PLW* wk, s16 arg_Guard_Type);
void Provoke(PLW* wk, s16 Lever);
void Normal_Attack(PLW* wk, s16 Reaction, u16 Lever_Data);
s32 Small_Jump_Measure(PLW* wk);
void Normal_Attack_SP(PLW* wk, s16 Reaction, u16 Lever_Data, s16 Time);
void Adjust_Attack(PLW* wk, s16 Reaction, u16 Lever_Data);
s32 Check_Squat(PLW* wk);
s32 Check_Start_Normal_Attack(PLW* wk, s16 Reaction, u16 Lever_Data); // extra arg unused
void Lever_Attack(PLW* wk, s16 Reaction, u16 Lever, u16 Lever_Data);
void Lever_Attack_SP(PLW* wk, s16 Reaction, u16 Lever, u16 Lever_Data, s16 Time);
s32 Setup_Guard_Lever(PLW* wk, u16 Lever);
s32 Check_Start_Lever_Attack(PLW* wk, u16 Lever, u16 Lever_Data); // extra args
void Check_Super_Art_Conditions(PLW* wk, u16 SA0, u16 SA1, u16 SA2, u16 arg_Term_No);
s32 DENJIN_Check(PLW* wk, u16 SA2, u16* xx, u16 arg_Term_No);
s32 YAGYOU_Check(PLW* wk, s16* xx, u16 arg_Term_No);
s32 SA_Range_Check(PLW* wk, s16 SA_No, u16 Range);
void Check_SA(PLW* wk, s16 Next_Action, s16 Next_Menu);
void Check_EX(PLW* wk, s16 Next_Action, s16 Next_Menu);
void Check_SA_Full(PLW* wk, s16 Next_Action, s16 Next_Menu);
void Branch_By_Distance(PLW* wk, s16 Next_Action, s16 Menu_00, s16 Menu_01, s16 Menu_02, s16 Menu_03);
void AI_Random_Action_Select(PLW* wk, s16 Next_Action, s16 Menu_00, s16 Menu_01, s16 Menu_02, s16 Menu_03, s16 Rnd_Type);
void Branch_Wait_Area(PLW* wk, s16 Time_00, s16 Time_01, s16 Time_02, s16 Time_03);
void Wait(PLW* wk, s16 Time); // unused arg
void Look(PLW* wk, s16 Time);
void Keep_Status(PLW* wk, u16 Lever_Data, s16 Option_Data);
void VS_Jump_Guard(PLW* wk);
void Wait_Lie(PLW* wk, u16 Lever_Data);
void Wait_Get_Up(PLW* wk, u16 Lever_Data, s16 Option);
s32 Check_Wait_Term(PLW* wk, s16 Option);
void Wait_Attack_Complete(PLW* wk, u16 Lever_Data, s16 Option);
s32 Check_Exit_Guard(PLW* wk, s16 Option);
void Short_Range_Attack(PLW* wk, s16 Reaction, u16 Lever_Data, s16 Next_Action, s16 Next_Menu);
void Check_Enemy_Distance(PLW* wk, s16 Range_X, s16 Range_Y, s16 Exit_Number, s16 Next_Action, s16 Next_Menu);
void Check_Projectile_Impact_Time(PLW* wk, s16 Next_Command, s16 Exit_Number, s16 Next_Action, s16 Next_Menu, s16 unused); // unused arg
s32 Check_Term_Sub_Air(PLW* wk, s16 Distance, s16 Range);
s32 Check_Term_Sub(PLW* wk, s16 Distance, s16 Range);
s32 Correct_Unit_PL(PLW* wk);
s32 Check_Term_Sub_Y(PLW* wk, s16 Distance, s16 Range);
void Jump(PLW* wk, s16 Jump_Dir);
void Hi_Jump(PLW* wk, s16 Pl_Number, s16 Jump_Dir);
s32 Check_Start_Hi_Jump(PLW* wk);
s32 Check_Air_Guard(PLW* wk);
void Jump_Attack(PLW* wk, s16 Reaction, s16 Time_Data, u16 Lever_Data, s16 Jump_Dir);
void Check_Jump_Attack_Conditions(PLW* wk, s16 Range_X, s16 Range_Y, s16 Reaction, u16 Lever_Data, s16 Jump_Dir, s16 Range_JX,
                      s16 Range_JY, s16 J_Lever_Data);
s32 Check_SP_Jump_Attack(PLW* wk, s16 Lever_Data);
s32 Check_VS_Air_Attack(PLW* wk, s16 Range_JX, s16 Range_JY, s16 J_Lever_Data);
void Hi_Jump_Attack(PLW* wk, s16 Reaction, s16 Time_Data, u16 Lever_Data, s16 Jump_Dir);
void Hi_Jump_Attack_Term(PLW* wk, s16 Range_X, s16 Range_Y, s16 Reaction, u16 Lever_Data, s16 Jump_Dir, s16 Range_JX,
                         s16 Range_JY, u16 J_Lever_Data);
s32 Check_Term_ABS_Distance(PLW* wk);
s32 Check_Com_Add_Y(PLW* wk, s16 Pos_Y, s16 Range);
void Oro_Check_Jump_Attack(PLW* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX, s16 RY, u16 Lever_Data, s16 RJX,
                 s16 RJY, u16 JLD);
void Oro_Check_High_Jump_Attack(PLW* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX, s16 RY, u16 Lever_Data, s16 RJX,
                  s16 RJY, u16 JLD);
void Command_Attack(PLW* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot);
s32 Hadou_Check(PLW* wk, u16 Tech_Number);
s32 Check_Resume_Lever(PLW* wk);
void Jump_Command_Attack(PLW* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot);
void Rapid_Command_Attack(PLW* wk, s16 Reaction, u16 Tech_Number, s16 Shot, u16 Time);
void Check_Rapid(PLW* wk, u16 Tech_Number);
void Setup_Rapid_End_Term(PLW* wk, s16 Tech_Number);
s32 Setup_Rapid_Time(PLW* wk, u16 Tech_Number); // unused all args
void Rapid_Sub(PLW* wk);
s32 Check_Rapid_End(PLW* wk);
s32 Check_Start_Command_Attack(PLW* wk, s16 Reaction, u16 Tech_Number);
void Oro_Check_Jump_Command_Attack(PLW* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX, s16 RY, u16 Tech_Number,
                  s16 Power_Level, s16 Ex_Shot, s16 RJX, s16 RJY, u16 JLD);
void Oro_Check_High_Jump_Command_Attack(PLW* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX, s16 RY, u16 Tech_Number,
                   s16 Power_Level, s16 Ex_Shot, s16 RJX, s16 RJY, u16 JLD);
void Jump_Command_Attack_Term(PLW* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot, s16 RX, s16 RY,
                              s16 Jump_Dir, s16 JRX, s16 JRY, u16 JLD);
void Hi_Jump_Command_Attack_Term(PLW* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot, s16 RX, s16 RY,
                                 s16 Jump_Dir, s16 JRX, s16 JRY, u16 JLD);
s32 Check_Landed(PLW* wk, s16 Reaction);
s32 Check_Dash_Hit(PLW* wk, u16 Tech_Number);
s32 Setup_Front_or_Back(PLW* wk, s16 xx);
s32 Check_Hit_Shell(PLW* wk, WORK_Other* tmw, u16 Tech_Number);
void Jump_Init(PLW* wk, s16 Jump_Dir);
s32 Command_Type_00(PLW* wk, s16 Power_Level, u16 Tech_Number, s16 Ex_Shot);
s32 Command_Type_06(PLW* wk, s16 Power_Level, u16 Tech_Number, s16 Ex_Shot); // unused last arg
s32 Command_Type_01(PLW* wk, s16 Power_Level, s16 Ex_Shot);                  // unused args
void Setup_Command_01(PLW* wk);
void Check_Store_Lever(PLW* wk, u16 Tech_Number, s16 Next_Action, s16 Next_Menu);
s32 Check_Store_Direction(PLW* wk, u16 lever, s16 time);
s32 Select_Combo_Speed(PLW* wk);
s32 Select_Reflection_Time(PLW* wk);
s32 Setup_Lv04(s16 xx);
s32 Setup_Lv08(s16 xx);
s32 Setup_Lv10(s16 xx);
s32 Setup_Lv18(s16 xx);
s32 Setup_VS_Catch_Data(PLW* wk);
s32 Setup_LP_Data(PLW* wk);
s32 Setup_WT_Data(PLW* wk);
void Ck_Distance(PLW* wk);
s32 Ck_Distance_Height(PLW* wk);
s32 Ck_Area(PLW* wk);
s32 Ck_Area_Shell(PLW* wk);
void Ck_Distance_Lv(PLW* wk);
void Check_Jump_Distance_Level(PLW* wk);
void Next_End(PLW* wk);
void Next_Another_Menu(PLW* wk, s16 Next_Action, u16 Next_Menu);
void Reaction_Sub(PLW* wk, s16 Reaction, s16 Power_Level);
s32 Check_Target_Combo_Attack(PLW* wk, s16 Reaction, s16 Power_Level); // unused last 2 args
s32 Get_Target_Combo_Data(PLW* wk);
void Reaction_Exit_Sub(PLW* wk);
void Check_First_Menu(PLW* wk);
void Select_Active(PLW* wk);
s32 Check_SA_Active(PLW* wk, s16* pl_id);
void Setup_Follow(PLW* wk, s16 Follow_Type);
void Decide_Follow_Menu(PLW* wk);
s32 Select_Passive(PLW* wk);
void Devide_Level(s16 xx);
void Setup_Random(PLW* wk);
s32 Check_Dramatic(PLW* wk, s16 PL_id);
s32 Check_Passive(PLW* wk);
s32 Check_Guard(PLW* wk);
s32 Check_Makoto(PLW* wk);
s32 Check_Flip_Term(PLW* wk, WORK* tmw);
s32 Setup_EM_Rank_Index(PLW* wk);
s32 Flip_Term_Correct(PLW* wk);
void Next_Be_Guard(PLW* wk, WORK* em, s16 Type_Of_Guard);
s32 Check_Flip_Tech(WORK* em);
void Next_Be_Flip(PLW* wk, s16 xx);
s32 Check_Diagonal_Shell(PLW* wk);
s32 Check_Ignore_Shell2(WORK_Other* tmw);
s32 Check_Shell(PLW* wk);
s32 Check_Shell_Another_in_Flip(PLW* wk);
s32 Check_Ignore_Shell(WORK_Other* tmw);
s32 Compute_Hit_Time(PLW* wk, WORK_Other* tmw);
s32 Decide_Shell_Guard(PLW* wk, WORK_Other* tmw); // unused second arg
void Guard_or_Jump_VS_Shell(PLW* wk, WORK_Other* tmw, s16 xx);
void Setup_Shell_Disposal(PLW* wk, WORK_Other* tmw);
void Next_Be_Shell_Guard(PLW* wk, WORK* tmw);
s32 Decide_Shell_Reaction(PLW* wk, WORK_Other* tmw, u16 dir_step); // unused second arg
s32 Ck_Distance_XX(s16 x1, s16 x2);
s32 Check_Behind(PLW* wk, WORK_Other* tmw);
void Setup_Lever_LR(PLW* wk, s16 PL_id, s16 Lever);
s32 Check_Exit_Term(PLW* wk, WORK* em, s16 arg_Exit_No);
s32 VS_Jump_Term(PLW* wk, WORK* em, s16* xx);
s32 Exit_Term_0000(PLW* wk, WORK* em);
s32 Exit_Term_0001(PLW* wk, WORK* em);
s32 Exit_Term_0002(PLW* wk, WORK* em);
s32 Exit_Term_0003(PLW* wk, WORK* em);
s32 Exit_Term_0004(PLW* wk, WORK* em);
s32 Exit_Term_0005(PLW* wk, WORK* em);
s32 Exit_Term_0006(PLW* wk, WORK* em);
s32 Exit_Term_0007(PLW* wk, WORK* em);
s32 Exit_Term_0008(PLW* wk, WORK* em);
s32 Check_Drop_Term(WORK* em, s16 Y);
s32 Check_SHINRYU(PLW* wk);
void Check_BOSS(PLW* wk, u32 Next_Action, u16 Next_Menu);
void Check_BOSS_EX(PLW* wk, u32 Next_Action, u16 Next_Menu);
void Check_Miscellaneous_Conditions(PLW* wk, s16 arg_Exit_No, u32 Next_Action, u16 Next_Menu);
s32 Check_Misc_Cond_0000(PLW* wk, WORK* em);
s32 Check_Misc_Cond_0001(PLW* wk, WORK* em);
s32 Check_Misc_Cond_0002(PLW* wk, WORK* em);
s32 Check_Misc_Cond_0003(PLW* wk, WORK* em);
s32 Check_Misc_Cond_0004(PLW* wk, WORK* em);
s32 Check_Misc_Cond_0005(PLW* wk, WORK* em);
s32 Check_Misc_Cond_0006(PLW* wk, WORK* em);
s32 Check_Misc_Cond_0007(PLW* wk, WORK* em);
s32 Check_Misc_Cond_0008(PLW* wk, WORK* em);
s32 Check_Misc_Cond_0009(PLW* wk, WORK* em);
s32 emLevelRemake(s32 now, s32 max, s32 exd);
s32 emGetMaxBlocking();

/** @brief Finish the current AI pattern step and advance to the next. */
void End_Pattern(PLW* wk) {
    Next_Be_Free(wk);
}

/** @brief Transition to passive (before-passive) state. */
void Next_Be_Passive(PLW* wk, s32 unused) {
    Next_Be_Free(wk);
}

/** @brief Enable the turn-over flag to face the opponent. */
void Turn_Over_On(PLW* wk) {
    g_state.Disposal_Again[wk->wu.id] = 1;
    g_state.Turn_Over[wk->wu.id] = 1;
    g_state.CP_Index[wk->wu.id][0]++;
}

/** @brief Press a button only (no directional lever). */
void Only_Shot(PLW* wk, s16 Lever_Data) {
    g_state.Lever_Buff[wk->wu.id] = Lever_Data;
    g_state.CP_Index[wk->wu.id][0]++;
}

/** @brief Set directional lever input (left/right and up/down). */
void Lever_On(PLW* wk, u16 LR_Lever, u16 UD_Lever) {
    g_state.CP_Index[wk->wu.id][0]++;
    g_state.Disposal_Again[wk->wu.id] = 1;
    if ((LR_Lever == 0) || (LR_Lever == 1)) {
        g_state.Lever_LR[wk->wu.id] = Setup_Guard_Lever(wk, LR_Lever);
    } else {
        g_state.Lever_LR[wk->wu.id] = 0;
    }
    g_state.Lever_LR[wk->wu.id] |= UD_Lever;
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
}

/** @brief Clear the lever input buffer (neutral position). */
void Lever_Off(PLW* wk) {
    g_state.CP_Index[wk->wu.id][0]++;
    g_state.Disposal_Again[wk->wu.id] = 1;
    g_state.Lever_LR[wk->wu.id] = 0;
}

/** @brief Enable the pierce/overhead attack flag (unblockable move setup). */
void Enable_Overhead_Attack_Flag(PLW* wk) {
    g_state.Disposal_Again[wk->wu.id] = 1;
    g_state.CP_Index[wk->wu.id][0]++;
    g_state.Pierce_Menu[wk->wu.id] = 1;
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
}

/** @brief Set Denjin Hadouken charge level based on difficulty. */
void Setup_DENJIN_LEVEL(PLW* wk) {
    u16 xx;

    g_state.Disposal_Again[wk->wu.id] = 1;
    if ((xx = g_state.DENJIN_No[wk->wu.id])) {
        Next_Another_Menu(wk, 2, xx);
    } else {
        Next_Another_Menu(wk, 2, Denjin_Data[g_state.Area_Number[wk->wu.id]][random_16_com()]);
    }
}

/** @brief Press attack button(s) at the specified power level. */
void Hold_Attack_Button(PLW* wk, s16 Power_Level) {
    s16 xx;

    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        if ((wk->wu.cg_type == 0x40) || (wk->wu.routine_no[1] == 0)) {
            Reaction_Exit_Sub(wk);
        } else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
            if ((wk->wu.now_koc == 8) && (wk->wu.char_index == 0xD)) {

                xx = wk->wu.graphic_index / wk->wu.cgd_type;
                if (xx >= Power_Level) {
                    g_state.CP_Index[wk->wu.id][1] = 0x63;
                }
            }
            if (Check_Exit_DENJIN(wk) != 0) {
                g_state.CP_Index[wk->wu.id][1] = 0x63;
            }
        }
        break;
    case 1:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (Check_Exit_DENJIN(wk) != 0) {
            g_state.CP_Index[wk->wu.id][1] = 0x63;
        }
        /* fallthrough */
    default:
        if ((wk->wu.cg_type == 0x40) || (wk->wu.routine_no[1] == 0)) {
            Reaction_Exit_Sub(wk);
        } else {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
            Reaction_Sub(wk, 8, Power_Level);
        }
        break;
    }
}

/** @brief Check whether Denjin Hadouken should be released based on charge state. */
s32 Check_Exit_DENJIN(PLW* wk) {
    s16 xx;
    WORK* em;

    if (!(g_state.DENJIN_Term[wk->wu.id] & 1)) {
        if (g_state.CP_Index[wk->wu.id][1] == 0) {
            return 0;
        }
    }

    if ((g_state.DENJIN_Term[wk->wu.id] & 8)) {
        if (g_state.Attack_Flag[wk->wu.id]) {
            return 1;
        }
    }
    em = (WORK*)wk->wu.target_adrs;

    xx = 0;
    if ((em->xyz[0].disp.pos) != (em->old_pos[0])) {
        if (Check_Attack_Direction(wk, em) != 0) {
            xx = -1;
        } else {
            xx = 1;
        }
    }

    if ((g_state.DENJIN_Term[wk->wu.id] & 1) && (em->xyz[0].disp.pos != 0)) {
        if (g_state.CP_Index[wk->wu.id][2] == 0) {
            g_state.CP_Index[wk->wu.id][2]++;
            g_state.CP_Index[wk->wu.id][3] = g_state.Area_Number[wk->wu.id];
        }
        switch (g_state.CP_Index[wk->wu.id][3]) {
        case 0:
        case 1:
        case 2:
            if (em->mvxy.a[1].real.h > 0) {
                return 1;
            }
            if (em->mvxy.a[1].real.h < 0) {
                if (em->xyz[1].disp.pos < 0x29) {
                    return 1;
                }
            }
            break;
        default:
            if ((em->mvxy.a[1].real.h > 0) && (xx == -1)) {
                return 1;
            }
            if (em->mvxy.a[1].real.h < 0) {
                if (em->xyz[1].disp.pos < 0x29) {
                    return 1;
                }
            }
            break;
        }
    }
    if (xx == 0) {
        return 0;
    }

    if ((g_state.DENJIN_Term[wk->wu.id] & 2) && (xx == 1)) {
        return 1;
    }

    if ((g_state.DENJIN_Term[wk->wu.id] & 4) && (xx == -1)) {
        return 1;
    }

    if ((g_state.DENJIN_Term[wk->wu.id] & 0x20) && (g_state.Lie_Flag[wk->wu.id] == 0)) {
        return 1;
    }
    return 0;
}

/** @brief Move backward to create distance from the opponent. */
void Keep_Away(PLW* wk, s16 Target_Pos, s16 Option) {
    switch (g_state.CP_Index[wk->wu.id][3]) {

    case 0:
        if (Option == 0) {
            if (random_16_com() < 4) {
                Setup_KA_Jump(wk);
            } else {
                Setup_Keep_Away_Walk(wk);
            }
        } else {
            if (Option == 1) {
                Setup_KA_Jump(wk);
            } else {
                g_state.CP_Index[wk->wu.id][3] = Option + 1;
            }
        }
        /* fallthrough */

    case 1:
    case 2:
        Jump(wk, g_state.CP_Index[wk->wu.id][3] - 1);
        break;

    case 3:
    case 4:
        Approach_Walk(wk, Target_Pos, g_state.CP_Index[wk->wu.id][3] - 1);
        break;
    }
}

/** @brief Set up a backward jump for the Keep_Away movement. */
void Setup_KA_Jump(PLW* wk) {
    s16 xx;

    g_state.CP_Index[wk->wu.id][3] = 2;
    xx = Back_Jump_Data[wk->player_number];

    if (wk->wu.active_move) {
        xx = wk->wu.xyz[0].disp.pos - Back_Jump_Data[wk->player_number];
        if ((g_state.bg_w.bgw[1].l_limit2 - g_state.bg_w.pos_offset) > xx) {
            g_state.CP_Index[wk->wu.id][3] = 1;
        }
    } else {
        xx = wk->wu.xyz[0].disp.pos + Back_Jump_Data[wk->player_number];
        if ((g_state.bg_w.bgw[1].r_limit2 + g_state.bg_w.pos_offset) < xx) {
            g_state.CP_Index[wk->wu.id][3] = 1;
        }
    }
}

/** @brief Set up backward walk for Keep_Away movement. */
void Setup_Keep_Away_Walk(PLW* wk) {
    g_state.CP_Index[wk->wu.id][3] = 4;
}

/** @brief Search for a safe position behind the CPU then walk/jump there. */
void Check_Safe_Retreat_Space(PLW* wk, s16 Move_Value, s16 Next_Action, s16 Next_Menu) {
    if (wk->wu.active_move) {
        Move_Value = wk->wu.xyz[0].disp.pos - Move_Value;
        if ((g_state.bg_w.bgw[1].l_limit2 - g_state.bg_w.pos_offset) > Move_Value) {
            Next_Another_Menu(wk, Next_Action, Next_Menu);
        } else {
            g_state.CP_Index[wk->wu.id][0]++;
        }
    } else {
        Move_Value = wk->wu.xyz[0].disp.pos + Move_Value;
        if (((g_state.bg_w.bgw[1].r_limit2) + (g_state.bg_w.pos_offset)) < (Move_Value)) {
            Next_Another_Menu(wk, Next_Action, Next_Menu);
        } else {
            g_state.CP_Index[wk->wu.id][0]++;
        }
    }
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
}

/** @brief Walk toward the opponent to close distance. */
void Approach_Walk(PLW* wk, s16 Target_Pos, s16 Option) {
    s16 xx;

    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.CP_Index[wk->wu.id][1]++;
        dash_flag_clear(wk->wu.id);
        g_state.Timer_00[wk->wu.id] = 0x78;
        /* fallthrough */

    case 1:
        xx = g_state.Standing_Timer[wk->wu.id];

        if (g_state.Lie_Flag[wk->wu.id] == 0) {
            if (Check_Passive(wk) != 0) {
                break;
            }
        }
        g_state.Standing_Timer[wk->wu.id] = xx;

        if (--g_state.Timer_00[wk->wu.id] == 0) {
            Next_Be_Free(wk);
        }

        else if (Check_Arrival(wk, Target_Pos, Option) != 0) {
            g_state.Disposal_Again[wk->wu.id] = 1;
            g_state.CP_Index[wk->wu.id][0]++;
            g_state.CP_Index[wk->wu.id][1] = 0;
            g_state.CP_Index[wk->wu.id][2] = 0;
            g_state.CP_Index[wk->wu.id][3] = 0;

            g_state.Flip_Flag[wk->wu.id] = 0;
            g_state.Limited_Flag[wk->wu.id] = 0;

            if (g_state.CP_No[wk->wu.id][0] != 6) {
                g_state.Passive_Flag[wk->wu.id] = 0;
            }
        } else {
            Ck_Distance_Lv(wk);
            if (Option == 3) {
                g_state.Lever_Buff[wk->wu.id] ^= 0xC;
            }
        }
    }
}

/** @brief Check if the CPU has arrived at the target distance. */
s32 Check_Arrival(PLW* wk, s16 Target_Pos, s16 Option) {
    if (Option == 3) {
        if (Target_Pos <= g_state.PL_Distance[wk->wu.id]) {
            return 1;
        }
        return wk->close_proximity_flag;
    }

    if (wk->hos_em_flag) {
        return 1;
    }
    if (Target_Pos >= g_state.PL_Distance[wk->wu.id]) {
        return 1;
    }

    return 0;
}

/** @brief Walk in a specified direction for a given time. */
void Walk(PLW* wk, u16 Lever, s16 Time, s16 unused) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.CP_Index[wk->wu.id][1]++;
        dash_flag_clear(wk->wu.id);
        g_state.Timer_00[wk->wu.id] = Time;
        g_state.Timer_01[wk->wu.id] = wk->wu.rl_flag;
        g_state.Free_Lever[wk->wu.id] = Setup_Guard_Lever(wk, Lever);
        /* fallthrough */

    case 1:
        if (g_state.Lie_Flag[wk->wu.id] == 0) {
            if (Check_Passive(wk) != 0) {
                break;
            }
        }

        if (--g_state.Timer_00[wk->wu.id] == 0) {
            g_state.CP_Index[wk->wu.id][0]++;
            g_state.CP_Index[wk->wu.id][1] = 0;
            g_state.CP_Index[wk->wu.id][2] = 0;
            g_state.CP_Index[wk->wu.id][3] = 0;

            g_state.Flip_Flag[wk->wu.id] = 0;
            g_state.Limited_Flag[wk->wu.id] = 0;

            if (*g_state.CP_No[wk->wu.id] != 6) {
                g_state.Passive_Flag[wk->wu.id] = 0;
            }
        } else {
            if ((g_state.Timer_01[wk->wu.id] != (s16)wk->wu.rl_flag) || (wk->close_proximity_flag != 0) ||
                (wk->hos_em_flag != 0)) {
                Next_Be_Free(wk);
            }
            g_state.Lever_Buff[wk->wu.id] = g_state.Free_Lever[wk->wu.id];
        }
        break;
    }
}

/** @brief Force the CPU into a specific guard stance. */
void Forced_Guard(PLW* wk, s16 arg_Guard_Type) {
    WORK* em;
    s16 xx;

    em = (WORK*)wk->wu.target_adrs;

    if (g_state.Attack_Flag[wk->wu.id] == 0) {
        Next_Be_Free(wk);
    }
    xx = Hit_Range_Data[em->hit_range];
    xx += g_state.Com_Width_Data[wk->wu.id];

    if (g_state.PL_Distance[wk->wu.id] > xx) {
        Next_Be_Free(wk);
    }

    Next_Be_Guard(wk, em, arg_Guard_Type);
    g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_Squat[wk->wu.id];
}

/** @brief Perform a taunt/provoke action. */
void Provoke(PLW* wk, s16 Lever) {
    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (wk->spmv_ng_flag & DIP_TAUNT_DISABLED) {
            Next_Be_Free(wk);
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        if (Lever != -1) {
            g_state.Lever_LR[wk->wu.id] = Setup_Guard_Lever(wk, Lever & 1);
            g_state.Lever_LR[wk->wu.id] |= Lever & 2;
        }
        /* fallthrough */
    case 1:

        if (wk->permited_koa & 0x80) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Lever_Buff[wk->wu.id] = 0x440;
        }
        break;

    default:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if ((wk->wu.routine_no[1] != 4) || (wk->wu.routine_no[2] != 0x1E)) {
            Reaction_Exit_Sub(wk);
        }
        break;
    }
}

/** @brief Execute a normal (non-special) attack with reaction checking. */
void Normal_Attack(PLW* wk, s16 Reaction, u16 Lever_Data) {
    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (Lever_Data & 2) {
            g_state.Lever_LR[wk->wu.id] = Setup_Guard_Lever(wk, 1);
        } else {
            g_state.Lever_LR[wk->wu.id] = 0;
        }

        g_state.Lever_LR[wk->wu.id] |= Lever_Data & 2;
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

        if (Check_Start_Normal_Attack(wk, Reaction, Lever_Data) != 0) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        Check_First_Menu(wk);
        /* fallthrough */

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id] == 0) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Lever_Buff[wk->wu.id] = Lever_Data;
            g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_LR[wk->wu.id];
        } else {
            g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_LR[wk->wu.id];
        }
        break;

    default:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Reaction_Sub(wk, Reaction, 0);
        break;
    }
}

/** @brief Check if opponent is attempting a small jump (to counter it). */
s32 Small_Jump_Measure(PLW* wk) {
    if (g_state.Lever_Squat[wk->wu.id] & 2) {
        return Setup_Guard_Lever(wk, 1);
    }
    return 0;
}

/** @brief Execute a normal attack with a timed delay before pressing the button. */
void Normal_Attack_SP(PLW* wk, s16 Reaction, u16 Lever_Data, s16 Time) {
    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (Check_Start_Normal_Attack(wk, Reaction, Lever_Data) != 0) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Timer_00[wk->wu.id] = Time;
        Check_First_Menu(wk);
        /* fallthrough */

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id] == 0) {
            g_state.Lever_Buff[wk->wu.id] = Lever_Data;
            g_state.Lever_Squat[wk->wu.id] = Lever_Data & 2;
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Timer_00[wk->wu.id]--;
        } else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Squat[wk->wu.id];
        }
        break;

    case 2:
        if (--g_state.Timer_00[wk->wu.id]) {
            g_state.Lever_Buff[wk->wu.id] = Lever_Data;
            g_state.Lever_Squat[wk->wu.id] = Lever_Data & 2;
        } else {
            g_state.CP_Index[wk->wu.id][1]++;
        }
        break;

    default:
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Reaction_Sub(wk, Reaction, 0);
        break;
    }
}

/** @brief Execute an attack that adjusts to the opponent's posture (stand/crouch). */
void Adjust_Attack(PLW* wk, s16 Reaction, u16 Lever_Data) {
    u16 xx;

    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (Check_Start_Normal_Attack(wk, Reaction, Lever_Data) != 0) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        Check_First_Menu(wk);
        /* fallthrough */

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id] == 0) {
            xx = Check_Squat(wk);
            g_state.Lever_Buff[wk->wu.id] = Lever_Data | xx;
            g_state.Lever_LR[wk->wu.id] = xx;
            g_state.Lever_Buff[wk->wu.id] |= Small_Jump_Measure(wk);
            g_state.CP_Index[wk->wu.id][1]++;
        } else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
            g_state.Lever_Buff[wk->wu.id] |= Small_Jump_Measure(wk);
        }
        break;

    default:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Reaction_Sub(wk, Reaction, 0);
        break;
    }
}

/** @brief Check if the opponent is crouching. */
s32 Check_Squat(PLW* wk) {
    if (((WORK*)wk->wu.target_adrs)->pat_status == 0x20) {
        return 0;
    }

    return 2;
}

/** @brief Check if conditions are met to start a normal attack (reaction timing). */
s32 Check_Start_Normal_Attack(PLW* wk, s16 Reaction, u16 Lever_Data) {
    if (((wk->wu.routine_no[1]) != 4) || ((wk->wu.cg_type) == 0x40)) {
        return 0;
    }

    if (wk->wu.cg_cancel & 4) {
        return 0;
    }

    if (wk->permited_koa & 0x10) {
        return 0;
    }

    if ((wk->wu.cg_cancel & 8) && (Reaction == 0xE)) {
        return 0;
    }

    return 1;
}

/** @brief Execute an attack with a directional lever component (e.g. forward+punch). */
void Lever_Attack(PLW* wk, s16 Reaction, u16 Lever, u16 Lever_Data) {
    s16 xx;

    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (Check_Start_Lever_Attack(wk, Lever, Lever_Data) != 0) {
            break;
        }
        dash_flag_clear(wk->wu.id);

        g_state.CP_Index[wk->wu.id][1]++;
        Check_First_Menu(wk);
        /* falltrhough */

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id]) {
            break;
        }

        xx = Setup_Guard_Lever(wk, Lever);
        g_state.Lever_Buff[wk->wu.id] = (Lever_Data | xx);
        g_state.CP_Index[wk->wu.id][1]++;
        break;

    default:
        if (wk->wu.routine_no[1] == 2) {
            Be_Catch(wk);
        } else {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
            Reaction_Sub(wk, Reaction, 0);
        }
        break;
    }
}

/** @brief Execute a lever attack with a timed delay before pressing the button. */
void Lever_Attack_SP(PLW* wk, s16 Reaction, u16 Lever, u16 Lever_Data, s16 Time) {
    s16 xx;

    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (Check_Start_Lever_Attack(wk, Lever, Lever_Data) != 0) {
            break;
        }
        dash_flag_clear(wk->wu.id);

        g_state.Timer_00[wk->wu.id] = Time;
        g_state.CP_Index[wk->wu.id][1]++;
        Check_First_Menu(wk);
        /* fallthrough */

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id]) {
            break;
        }

        xx = Setup_Guard_Lever(wk, Lever);
        g_state.Lever_Buff[wk->wu.id] = (Lever_Data | xx);

        g_state.Timer_00[wk->wu.id]--;
        g_state.CP_Index[wk->wu.id][1]++;
        break;

    case 2:
        if (--g_state.Timer_00[wk->wu.id]) {
            g_state.Lever_Buff[wk->wu.id] = Lever_Data;
            g_state.Lever_Squat[wk->wu.id] = Lever_Data & 2;
        } else {
            g_state.CP_Index[wk->wu.id][1]++;
        }
        break;

    default:
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Reaction_Sub(wk, Reaction, 0);
        break;
    }
}

/** @brief Calculate the correct guard lever direction based on facing and type. */
s32 Setup_Guard_Lever(PLW* wk, u16 Lever) {
    s32 rnum = 0;

    switch (Lever) {
    case 0:
        if (wk->wu.active_move == 0) {
            rnum = 4;
        } else {
            rnum = 8;
        }
        break;
    case 1:
        if (wk->wu.active_move == 1) {
            rnum = 4;
        } else {
            rnum = 8;
        }
        break;
    }
    return rnum;
}

/** @brief Check if conditions are met to start a lever attack. */
s32 Check_Start_Lever_Attack(PLW* wk, u16 Lever, u16 Lever_Data) {
    if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
        return 0;
    }

    if (wk->wu.cg_cancel & 4) {
        return 0;
    }

    if (wk->wu.cg_cancel & 8) {
        return 0;
    }

    return 1;
}

/** @brief Set up a super art (SA) attack with SA meter and range checking. */
void Check_Super_Art_Conditions(PLW* wk, u16 SA0, u16 SA1, u16 SA2, u16 arg_Term_No) {
    s16 xx[3];

    if (((g_state.Passive_Flag[wk->wu.id]) == 0) && (Check_Passive(wk) != 0)) {
        return;
    }

    xx[0] = SA0;
    xx[1] = SA1;
    xx[2] = SA2;
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

    if ((xx[g_state.plw[wk->wu.id].sa->kind_of_arts] == -1) || g_state.plw[wk->wu.id].metamorphose) {
        g_state.CP_Index[wk->wu.id][0]++;
    } else if ((g_state.plw[wk->wu.id].sa->ok) || (g_state.plw[wk->wu.id].sa->mp)) {
        g_state.Disposal_Again[wk->wu.id] = 1;

        if ((arg_Term_No != 0xFFFF) || (arg_Term_No != 0)) {
            switch (wk->player_number) {
            case 2:
                if (SA_Range_Check(wk, 1, arg_Term_No) != 0) {
                    return;
                }
                DENJIN_Check(wk, SA2, (u16*)&xx[2], arg_Term_No);
                Next_Another_Menu(wk, 2, xx[g_state.plw[wk->wu.id].sa->kind_of_arts]);
                return;

            case 11:
                if (SA_Range_Check(wk, 1, arg_Term_No) != 0) {
                    return;
                }
                Next_Another_Menu(wk, 2, xx[g_state.plw[wk->wu.id].sa->kind_of_arts]);
                return;

            case 1:
                if (SA_Range_Check(wk, 1, arg_Term_No) != 0) {
                    return;
                }
                Next_Another_Menu(wk, 2, xx[g_state.plw[wk->wu.id].sa->kind_of_arts]);
                return;

            case 5:
                if (((WORK*)wk->wu.target_adrs)->xyz[1].disp.pos >= 0x10) {
                    g_state.CP_Index[wk->wu.id][0]++;
                    return;
                }
                if (SA_Range_Check(wk, 1, arg_Term_No) != 0) {
                    return;
                }
                Next_Another_Menu(wk, 2, xx[g_state.plw[wk->wu.id].sa->kind_of_arts]);
                return;

            case 6:
                if (SA_Range_Check(wk, 0, arg_Term_No) != 0) {
                    return;
                }
                Next_Another_Menu(wk, 2, xx[g_state.plw[wk->wu.id].sa->kind_of_arts]);
                return;

            case 8:
                if ((g_state.plw[wk->wu.id].sa->kind_of_arts == 2) &&
                    (g_state.plw[wk->wu.id].wu.vital_new <= (g_state.Max_vitality / 2))) {
                    break;
                }
                g_state.CP_Index[wk->wu.id][0]++;
                return;

            case 9:
                YAGYOU_Check(wk, &xx[1], arg_Term_No);
                Next_Another_Menu(wk, 2, xx[g_state.plw[wk->wu.id].sa->kind_of_arts]);
                return;

            case 14:
                if (SA_Range_Check(wk, 1, arg_Term_No) != 0) {
                    return;
                }
                if (SA_Range_Check(wk, 2, arg_Term_No) != 0) {
                    return;
                }
                Next_Another_Menu(wk, 2, xx[g_state.plw[wk->wu.id].sa->kind_of_arts]);
                return;

            default:
                Next_Another_Menu(wk, 2, xx[g_state.plw[wk->wu.id].sa->kind_of_arts]);
                return;
            }
        }

        Next_Another_Menu(wk, 2, xx[g_state.plw[wk->wu.id].sa->kind_of_arts]);
    } else {
        g_state.CP_Index[wk->wu.id][0]++;
    }
}

/** @brief Check if Denjin Hadouken super should be used (Ryu SA3 specific). */
s32 DENJIN_Check(PLW* wk, u16 SA2, u16* xx, u16 arg_Term_No) {
    if (g_state.plw[wk->wu.id].sa->kind_of_arts != 2) {
        return 0;
    }

    g_state.DENJIN_No[wk->wu.id] = arg_Term_No;
    g_state.DENJIN_Term[wk->wu.id] = SA2;
    xx[0] = 0x37;
    return 1;
}

const u8 YAGYOU_Data[0x10] = { 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3 };

/** @brief Check if Yagyou Dama super should be used (Oro SA3 specific). */
s32 YAGYOU_Check(PLW* wk, s16* xx, u16 arg_Term_No) {
    if (g_state.plw[wk->wu.id].sa->kind_of_arts != 1) {
        return 0;
    }

    if (arg_Term_No == 0) {
        arg_Term_No = YAGYOU_Data[random_16_com()];
        arg_Term_No += 0x64;
    }
    xx[0] = arg_Term_No;
    return 1;
}

/** @brief Check if the opponent is within range for a super art. */
s32 SA_Range_Check(PLW* wk, s16 SA_No, u16 Range) {
    if (SA_No != g_state.plw[wk->wu.id].sa->kind_of_arts) {
        return 0;
    }

    if (Range & 0x8000) {
        if ((g_state.PL_Distance[wk->wu.id]) < (Range & 0x7FFF)) {
            g_state.CP_Index[wk->wu.id][0]++;
            return 1;
        }

    }

    else if (g_state.PL_Distance[wk->wu.id] > Range) {
        g_state.CP_Index[wk->wu.id][0]++;
        return 1;
    }

    return 0;
}

/** @brief Check for SA gauge and set up a super art if available. */
void Check_SA(PLW* wk, s16 Next_Action, s16 Next_Menu) {
    if (g_state.plw[wk->wu.id].sa->ok) {
        g_state.CP_Index[wk->wu.id][0]++;
    } else {
        g_state.CP_No[wk->wu.id][0] = Next_Action;
        g_state.Disposal_Again[wk->wu.id] = 1;
        Next_Another_Menu(wk, Next_Action, Next_Menu);
    }
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
}

/** @brief Check for EX gauge and set up an EX move if available. */
void Check_EX(PLW* wk, s16 Next_Action, s16 Next_Menu) {
    if (g_state.plw[wk->wu.id].sa->ex) {
        g_state.CP_Index[wk->wu.id][0]++;
    } else {
        g_state.CP_No[wk->wu.id][0] = Next_Action;
        g_state.Disposal_Again[wk->wu.id] = 1;
        Next_Another_Menu(wk, Next_Action, Next_Menu);
    }
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
}

/** @brief Check if the SA gauge is completely full and set up if so. */
void Check_SA_Full(PLW* wk, s16 Next_Action, s16 Next_Menu) {
    g_state.Disposal_Again[wk->wu.id] = 1;

    if (wk->permited_koa & 0x40) {
        g_state.CP_Index[wk->wu.id][0]++;
    } else {
        g_state.CP_No[wk->wu.id][0] = Next_Action;
        Next_Another_Menu(wk, Next_Action, Next_Menu);
    }
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
}

/** @brief Branch to different menus based on the current distance area (close/mid/far). */
void Branch_By_Distance(PLW* wk, s16 Next_Action, s16 Menu_00, s16 Menu_01, s16 Menu_02, s16 Menu_03) {
    s16 xx[4];

    g_state.CP_No[wk->wu.id][0] = Next_Action;
    xx[0] = Menu_00;
    xx[1] = Menu_01;
    xx[2] = Menu_02;
    xx[3] = Menu_03;

    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
    g_state.Disposal_Again[wk->wu.id] = 1;
    Next_Another_Menu(wk, Next_Action, xx[g_state.Area_Number[wk->wu.id]]);
}

/** @brief Randomly select one of four menus based on difficulty-weighted RNG. */
void AI_Random_Action_Select(PLW* wk, s16 Next_Action, s16 Menu_00, s16 Menu_01, s16 Menu_02, s16 Menu_03, s16 Rnd_Type) {
    s16 xx[4];
    s16 zz;

    zz = Com_Rnd_Select_Data[Rnd_Type][random_16_com()];

    xx[0] = Menu_00;
    xx[1] = Menu_01;
    xx[2] = Menu_02;
    xx[3] = Menu_03;
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

    if (xx[zz] == 0xFF) {
        Next_End(wk);
    } else {
        g_state.Disposal_Again[wk->wu.id] = 1;
        Next_Another_Menu(wk, Next_Action, xx[zz]);
    }
}

/** @brief Set a wait timer based on the current distance area. */
void Branch_Wait_Area(PLW* wk, s16 Time_00, s16 Time_01, s16 Time_02, s16 Time_03) {
    s16 xx[4];

    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:

        g_state.CP_Index[wk->wu.id][1]++;

        xx[0] = Time_00;
        xx[1] = Time_01;
        xx[2] = Time_02;
        xx[3] = Time_03;
        g_state.Timer_00[wk->wu.id] = xx[g_state.Area_Number[wk->wu.id]];
        break;

    default:
        if (--g_state.Timer_00[wk->wu.id]) {
            break;
        }
        g_state.CP_Index[wk->wu.id][0]++;
        g_state.CP_Index[wk->wu.id][1] = 0;
        g_state.CP_Index[wk->wu.id][2] = 0;
        g_state.CP_Index[wk->wu.id][3] = 0;

        g_state.Flip_Flag[wk->wu.id] = 0;
        g_state.Limited_Flag[wk->wu.id] = 0;

        if (g_state.CP_No[wk->wu.id][0] != 6) {
            g_state.Passive_Flag[wk->wu.id] = 0;
        }
        break;
    }
}

/** @brief Wait idle for a specified number of frames. */
void Wait(PLW* wk, s16 Time) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.CP_Index[wk->wu.id][1]++;

        if (Time == 0) {
            g_state.Timer_00[wk->wu.id] = Setup_WT_Data(wk);
        } else {
            g_state.Timer_00[wk->wu.id] = Time;
        }

        break;

    default:
        if (--g_state.Timer_00[wk->wu.id]) {
            break;
        }
        g_state.CP_Index[wk->wu.id][0]++;
        g_state.CP_Index[wk->wu.id][1] = 0;
        g_state.CP_Index[wk->wu.id][2] = 0;
        g_state.CP_Index[wk->wu.id][3] = 0;

        g_state.Flip_Flag[wk->wu.id] = 0;
        g_state.Limited_Flag[wk->wu.id] = 0;

        if (g_state.CP_No[wk->wu.id][0] != 6) {
            g_state.Passive_Flag[wk->wu.id] = 0;
        }
        break;
    }
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
}

/** @brief Watch the opponent (stand facing them) for a specified duration. */
void Look(PLW* wk, s16 Time) {
    g_state.Passive_Flag[wk->wu.id] = 0;
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        if (Check_Passive(wk) != 0) {
            break;
        }
        g_state.CP_Index[wk->wu.id][1]++;

        if (Time == 0) {
            g_state.Timer_00[wk->wu.id] = Setup_LP_Data(wk);
        } else {
            g_state.Timer_00[wk->wu.id] = Time;
        }

        if (g_state.Lever_LR[wk->wu.id] & 2) {
            g_state.Timer_00[wk->wu.id] += 0x32;
        }

        break;

    default:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Timer_00[wk->wu.id] != 0) {
            break;
        }

        g_state.CP_Index[wk->wu.id][0]++;
        g_state.CP_Index[wk->wu.id][1] = 0;
        g_state.CP_Index[wk->wu.id][2] = 0;
        g_state.CP_Index[wk->wu.id][3] = 0;

        g_state.Flip_Flag[wk->wu.id] = 0;
        g_state.Limited_Flag[wk->wu.id] = 0;
        g_state.Before_Look[wk->wu.id] = 1;
        break;
    }
}

/** @brief Hold a directional position and optionally input buttons for a duration. */
void Keep_Status(PLW* wk, u16 Lever_Data, s16 Option_Data) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.CP_Index[wk->wu.id][1]++;
        dash_flag_clear(wk->wu.id);
        g_state.Timer_00[wk->wu.id] = 0xA;

        g_state.Free_Lever[wk->wu.id] = Lever_Data;
        if (Option_Data != -1) {
            g_state.Free_Lever[wk->wu.id] |= Setup_Guard_Lever(wk, Option_Data);
        }
        g_state.Lever_Buff[wk->wu.id] = g_state.Free_Lever[wk->wu.id];

        break;

    default:
        g_state.Lever_Buff[wk->wu.id] = g_state.Free_Lever[wk->wu.id];
        if (--g_state.Timer_00[wk->wu.id]) {
            break;
        }
        g_state.Timer_00[wk->wu.id] = 1;

        if (g_state.Attack_Flag[wk->wu.id] == 0) {
            g_state.CP_Index[wk->wu.id][0]++;
            g_state.CP_Index[wk->wu.id][1] = 0;
            g_state.CP_Index[wk->wu.id][2] = 0;
            g_state.CP_Index[wk->wu.id][3] = 0;

            g_state.Flip_Flag[wk->wu.id] = 0;
            g_state.Limited_Flag[wk->wu.id] = 0;

            if (g_state.CP_No[wk->wu.id][0] != 6) {
                g_state.Passive_Flag[wk->wu.id] = 0;
            }
        }
        break;
    }
}

/** @brief Guard against an incoming jump attack. */
void VS_Jump_Guard(PLW* wk) {
    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        if (Check_Guard(wk) == 0) {
            dash_flag_clear(wk->wu.id);
            g_state.CP_Index[wk->wu.id][1]++;
        }

        break;

    default:
        if (Check_Guard(wk) != 0) {
            break;
        }

        if (((WORK*)wk->wu.target_adrs)->xyz[1].disp.pos < 0x19) {
            g_state.CP_Index[wk->wu.id][0]++;
            g_state.CP_Index[wk->wu.id][1] = 0;
            g_state.CP_Index[wk->wu.id][2] = 0;
            g_state.CP_Index[wk->wu.id][3] = 0;

            g_state.Passive_Flag[wk->wu.id] = 0;
            g_state.Flip_Flag[wk->wu.id] = 0;
            g_state.Limited_Flag[wk->wu.id] = 0;
        }

        break;
    }
}

/** @brief Wait while the opponent is lying down, optionally inputting buttons. */
void Wait_Lie(PLW* wk, u16 Lever_Data) {
    WORK* em;

    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.CP_Index[wk->wu.id][1]++;
        dash_flag_clear(wk->wu.id);
        g_state.Rolling_Flag[wk->wu.id] = 0;

        if (Lever_Data != 0) {
            g_state.Free_Lever[wk->wu.id] = Setup_Guard_Lever(wk, 1);
            g_state.Free_Lever[wk->wu.id] |= Lever_Data & 2;
        } else {
            g_state.Free_Lever[wk->wu.id] = 0;
        }
        /* fallthrough */

    default:
        g_state.Lever_Buff[wk->wu.id] = g_state.Free_Lever[wk->wu.id];

        em = (WORK*)wk->wu.target_adrs;
        if ((Check_Blow_Off(wk, em, 0) == 0) || (g_state.Lie_Flag[wk->wu.id] != 0)) {
            g_state.CP_Index[wk->wu.id][0]++;
            g_state.CP_Index[wk->wu.id][1] = 0;
            g_state.CP_Index[wk->wu.id][2] = 0;
            g_state.CP_Index[wk->wu.id][3] = 0;

            g_state.Flip_Flag[wk->wu.id] = 0;

            g_state.Limited_Flag[wk->wu.id] = 0;
        }
        break;
    }
}

/** @brief Wait for the opponent to get up, optionally pressing buttons. */
void Wait_Get_Up(PLW* wk, u16 Lever_Data, s16 Option) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.CP_Index[wk->wu.id][1]++;
        dash_flag_clear(wk->wu.id);
        g_state.Rolling_Flag[wk->wu.id] = 0;

        if (Lever_Data != 0) {
            g_state.Lever_LR[wk->wu.id] = Setup_Guard_Lever(wk, 1);
            g_state.Lever_LR[wk->wu.id] |= Lever_Data & 2;
        } else {
            g_state.Lever_LR[wk->wu.id] = 0;
        }

        /* fallthrough */

    default:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

        if (Check_Wait_Term(wk, Option) != 0) {
            g_state.CP_Index[wk->wu.id][0]++;
            g_state.CP_Index[wk->wu.id][1] = 0;
            g_state.CP_Index[wk->wu.id][2] = 0;
            g_state.CP_Index[wk->wu.id][3] = 0;

            g_state.Disposal_Again[wk->wu.id] = 1;
            g_state.Passive_Flag[wk->wu.id] = 1;
            g_state.Flip_Flag[wk->wu.id] = 0;
            g_state.Limited_Flag[wk->wu.id] = 0;
        }
        break;
    }
}

/** @brief Check conditions for exiting a wait state (opponent standing, guard, etc.). */
s32 Check_Wait_Term(PLW* wk, s16 Option) {
    WORK* em;

    em = (WORK*)wk->wu.target_adrs;

    if ((em->routine_no[1] == 1) && (em->pat_status == 0x18)) {
        return 0;
    }
    if (g_state.Lie_Flag[wk->wu.id] == 0) {
        return 1;
    }
    if (Option != 0) {
        return 0;
    }
    if (em->cg_type == 0xB) {
        return 1;
    }
    return 0;
}

/** @brief Wait for the current attack animation to complete. */
void Wait_Attack_Complete(PLW* wk, u16 Lever_Data, s16 Option) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.CP_Index[wk->wu.id][1]++;
        dash_flag_clear(wk->wu.id);

        if (Lever_Data != 0) {
            g_state.Lever_LR[wk->wu.id] = Setup_Guard_Lever(wk, 1);
            g_state.Lever_LR[wk->wu.id] |= Lever_Data & 2;
            g_state.Guard_Flag[wk->wu.id] = 1;
        } else {
            g_state.Lever_LR[wk->wu.id] = 0;
        }

        /* fallthrough */

    default:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

        if (Check_Exit_Guard(wk, Option) == 0) {
            g_state.CP_Index[wk->wu.id][0]++;
            g_state.CP_Index[wk->wu.id][1] = 0;
            g_state.CP_Index[wk->wu.id][2] = 0;
            g_state.CP_Index[wk->wu.id][3] = 0;
            g_state.Guard_Flag[wk->wu.id] = 0;

            g_state.Flip_Flag[wk->wu.id] = 0;
            g_state.Limited_Flag[wk->wu.id] = 0;
            if (Option == 0) {
                g_state.Passive_Flag[wk->wu.id] = 0;
            }
        }
        break;
    }
}

/** @brief Check if the guard period has expired (time-limited guard). */
s32 Check_Exit_Guard(PLW* wk, s16 Option) {
    WORK* em;

    if (wk->wu.routine_no[1] == 1) {
        return 1;
    }

    if (Option == 0) {
        em = (WORK*)wk->wu.target_adrs;
        if (em->routine_no[1] != 4) {
            return 0;
        }
        return 1;
    }
    return g_state.Attack_Flag[wk->wu.id];
}

/** @brief Attack at close range with follow-up on hit. */
void Short_Range_Attack(PLW* wk, s16 Reaction, u16 Lever_Data, s16 Next_Action, s16 Next_Menu) {
    u16 xx;

    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (Check_Start_Normal_Attack(wk, Reaction, Lever_Data) == 0) {
            g_state.CP_Index[wk->wu.id][1]++;
            Check_First_Menu(wk);

            Check_Jump_Distance_Level(wk);
            xx = get_nearing_range(wk->player_number, xx = Lever_Data & 0xFF0);
            if (g_state.PL_Distance[wk->wu.id] > xx) {
                Next_Another_Menu(wk, Next_Action, Next_Menu);
            }
        }

        break;

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id]) {
            break;
        }
        g_state.Lever_Buff[wk->wu.id] = Lever_Data;
        g_state.CP_Index[wk->wu.id][1]++;
        break;

    default:
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Reaction_Sub(wk, Reaction, 0);
        break;
    }
}

/** @brief Check enemy distance and decide: attack, follow-up menu, or exit. */
void Check_Enemy_Distance(PLW* wk, s16 Range_X, s16 Range_Y, s16 Exit_Number, s16 Next_Action, s16 Next_Menu) {
    WORK* em;

    em = (WORK*)wk->wu.target_adrs;
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Term_No[wk->wu.id] = 0;
        /* fallthrough */

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }

        switch (Check_Exit_Term(wk, em, Exit_Number)) {
        case 0:
            if (Check_Term_Sub(wk, g_state.PL_Distance[wk->wu.id], Range_X) == 0) {
                break;
            }

            if (Exit_Number != 8) {
                if (Check_Term_Sub_Y(wk, em->xyz[1].disp.pos, Range_Y) == 0) {
                    break;
                }
            } else {
                if (Check_Term_Sub(wk, wk->wu.xyz[1].disp.pos, Range_Y) == 0) {
                    break;
                }
            }

            g_state.Disposal_Again[wk->wu.id] = 1;
            g_state.CP_Index[wk->wu.id][0]++;
            g_state.CP_Index[wk->wu.id][1] = 0;
            g_state.CP_Index[wk->wu.id][2] = 0;
            g_state.CP_Index[wk->wu.id][3] = 0;

            g_state.Flip_Flag[wk->wu.id] = 0;
            g_state.Limited_Flag[wk->wu.id] = 0;
            break;

        case 1:
            g_state.Disposal_Again[wk->wu.id] = 1;
            Next_Another_Menu(wk, Next_Action, Next_Menu);
            break;

        case 2:
            break;

        case 3:
            Select_Passive(wk);
            break;

        default:
            g_state.Counter_Attack[wk->wu.id] = 1;
            Select_Passive(wk);
            break;
        }

        break;
    }
}

/** @brief Check projectile distance and decide: dodge, guard, or counter. */
void Check_Projectile_Impact_Time(PLW* wk, s16 Next_Command, s16 Exit_Number, s16 Next_Action, s16 Next_Menu, s16 unused) {
    WORK* em;
    WORK_Other* tmw;
    s16 xx;

    em = (WORK*)Shell_Address[wk->wu.id];
    tmw = (WORK_Other*)Shell_Address[wk->wu.id];

    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Term_No[wk->wu.id] = 0;
        /* fallthrough */

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (Check_Exit_Term(wk, em, Exit_Number) == 1) {
            Next_Another_Menu(wk, Next_Action, Next_Menu);
        } else {
            xx = Compute_Hit_Time(wk, tmw);
            if (xx < Shell_Dodge_Data[Next_Command][wk->player_number]) {
                g_state.Disposal_Again[wk->wu.id] = 1;
                g_state.CP_Index[wk->wu.id][0]++;
                g_state.CP_Index[wk->wu.id][1] = 0;
                g_state.CP_Index[wk->wu.id][2] = 0;
                g_state.CP_Index[wk->wu.id][3] = 0;

                g_state.Flip_Flag[wk->wu.id] = 0;
                g_state.Limited_Flag[wk->wu.id] = 0;
            }
        }
        break;
    }
}

const s16 Correct_VS_Air_Data[0x14] = { 0, 0x20, 0, 0, 0, 0x20, 0x20, 0, 0x20, 0, 0, 0, 0, 0x20, 0, 0, 0, 0, 0, 0 };

/** @brief  */
s32 Correct_Unit_PL(PLW* wk) {
    return Correct_VS_Air_Data[g_state.My_char[g_state.Player_id]];
}

/** @brief Shared distance-vs-range check with optional correction offset.
 *  When correction == 0 this is the standard ground check (Check_Term_Sub).
 *  When correction != 0 (e.g. Correct_Unit_PL) this is the air check (Check_Term_Sub_Air). */
static s32 check_term_sub_impl(PLW* wk, s16 Distance, s16 Range, s32 correction) {
    if (Range == -1) {
        return 1;
    }
    if (!(Range & 0x8000)) {
        if (Distance >= Range) {
            return 1;
        }
        return 0;
    } else {
        Range += correction;
        if (Distance <= (Range & 0x7FFF)) {
            return 1;
        }
        return 0;
    }
}

/** @brief  */
s32 Check_Term_Sub_Air(PLW* wk, s16 Distance, s16 Range) {
    return check_term_sub_impl(wk, Distance, Range, Correct_Unit_PL(wk));
}

/** @brief  */
s32 Check_Term_Sub(PLW* wk, s16 Distance, s16 Range) {
    return check_term_sub_impl(wk, Distance, Range, 0);
}

/** @brief  */
s32 Check_Term_Sub_Y(PLW* wk, s16 Distance, s16 Range) {
    WORK* em;

    if (Range == -1) {
        return 1;
    }

    em = (WORK*)wk->wu.target_adrs;
    if (!(Range & 0x8000)) {
        if (Distance >= Range) {
            return 1;
        }
        return 0;
    } else {
        if (em->mvxy.a[1].real.h > 0) {
            return 0;
        }
        if (Distance <= (Range & 0x7FFF)) {
            return 1;
        }
        return 0;
    }
}

/** @brief  */
void Jump(PLW* wk, s16 Jump_Dir) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }

        if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
            g_state.CP_Index[wk->wu.id][1]++;
            hi_jump_flag_clear(wk->wu.id);
            Check_First_Menu(wk);
        }

        break;

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (--g_state.Combo_Speed[wk->wu.id] != 0) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        Jump_Init(wk, Jump_Dir);
        if (Check_Diagonal_Shell(wk) != 0) {
            Next_Be_Free(wk);
        }

        break;

    case 2:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];

        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
            Check_Air_Guard(wk);
        }
        break;

    default:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (wk->wu.xyz[1].disp.pos) {
            break;
        }

        g_state.CP_Index[wk->wu.id][0]++;
        g_state.CP_Index[wk->wu.id][1] = 0;
        g_state.CP_Index[wk->wu.id][2] = 0;
        g_state.CP_Index[wk->wu.id][3] = 0;
        break;
    }
}

/** @brief  */
void Hi_Jump(PLW* wk, s16 Pl_Number, s16 Jump_Dir) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }

        if (Check_Start_Hi_Jump(wk) == 0) {
            g_state.CP_Index[wk->wu.id][1]++;
            if (g_state.cmd_sel[wk->wu.id]) {
                Tech_Address[wk->wu.id] = player_CMD[Pl_Number][2];
            } else {
                Tech_Address[wk->wu.id] = player_cmd[Pl_Number][2];
            }
            Check_First_Menu(wk);
        }

        break;

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id] != 0) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Tech_Index[wk->wu.id] = 0xC;

        Jump_Init(wk, Jump_Dir);
        if (Check_Diagonal_Shell(wk) != 0) {
            Next_Be_Free(wk);
        }

        g_state.Lever_Buff[wk->wu.id] = 0;

        break;

    case 2:
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (Command_Type_00(wk, 8, 0xFFFF, -1) == -1) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_Pool[wk->wu.id];
            break;
        }

        if (!(g_state.Lever_Buff[wk->wu.id] & 2)) {
            g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_Pool[wk->wu.id];
        }
        break;

    case 3:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
            Check_Air_Guard(wk);
        }
        break;

    default:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        Check_Air_Guard(wk);

        if (wk->wu.xyz[1].disp.pos) {
            break;
        }
        g_state.CP_Index[wk->wu.id][0]++;
        g_state.CP_Index[wk->wu.id][1] = 0;
        g_state.CP_Index[wk->wu.id][2] = 0;
        g_state.CP_Index[wk->wu.id][3] = 0;
        break;
    }
}

/** @brief  */
s32 Check_Start_Hi_Jump(PLW* wk) {
    if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
        return 0;
    }

    if (wk->wu.cg_cancel & 4) {
        return 0;
    }

    if (wk->wu.cg_cancel & 1) {
        return 0;
    }

    return 1;
}

/** @brief  */
s32 Check_Air_Guard(PLW* wk) {
    WORK* em;
    s16 xx;
    s16 zz;

    em = (WORK*)wk->wu.target_adrs;

    if (g_state.Lever_LR[wk->wu.id]) {
        return g_state.Lever_LR[wk->wu.id];
    }
    if (g_state.Guard_Counter[wk->wu.id] == g_state.Attack_Counter[wk->wu.id]) {
        return g_state.Lever_LR[wk->wu.id];
    }
    if (g_state.Attack_Flag[wk->wu.id] == 0) {
        return g_state.Lever_LR[wk->wu.id];
    }

    xx = Hit_Range_Data[em->hit_range] + 0x20;
    xx += g_state.Com_Width_Data[wk->wu.id];
    if (g_state.PL_Distance[wk->wu.id] > xx) {
        return 0;
    }

    g_state.Guard_Counter[wk->wu.id] = g_state.Attack_Counter[wk->wu.id];
    Lv = Setup_Lv10(0);
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 2;
    }

    Rnd = random_16_com();
    Lv += g_state.CC_Value[0];

    if (Lv >= 7) {
        Lv = 0xA;
    }

    Lv = emLevelRemake(Lv, 0xB, 1);

    zz = Setup_EM_Rank_Index(wk);

    if (Guard_Data[zz][Lv][Rnd] == 3) {
        return g_state.Lever_LR[wk->wu.id] = 0;
    }
    g_state.Guard_Type[wk->wu.id] = Guard_Data[zz][Lv][random_16_ex_com()];

    g_state.Lever_LR[wk->wu.id] = Setup_Guard_Lever(wk, 1);
    g_state.Lever_LR[wk->wu.id] |= 2;
    return g_state.Guard_Type[wk->wu.id] |= 0x8000;
}

/** @brief  */
void Jump_Attack(PLW* wk, s16 Reaction, s16 Time_Data, u16 Lever_Data, s16 Jump_Dir) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        Setup_Lever_LR(wk, wk->wu.id, Reaction & 0xF000);
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }

        if ((wk->wu.routine_no[1] == 4) && (wk->wu.cg_type != 0x40)) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Timer_00[wk->wu.id] = Time_Data;
        g_state.Continue_Menu[wk->wu.id] = 0;
        wk->wu.hf.hit.player = 0;
        hi_jump_flag_clear(wk->wu.id);
        Check_First_Menu(wk);
        /* fallthrough */

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id] != 0) {
            break;
        }

        g_state.Timer_00[wk->wu.id] = Time_Data;
        g_state.CP_Index[wk->wu.id][1]++;
        dash_flag_clear(wk->wu.id);
        Jump_Init(wk, Jump_Dir);
        if (Check_Diagonal_Shell(wk) != 0) {
            Next_Be_Free(wk);
        }

        break;

    case 2:
        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        }

        else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
        }

        break;

    case 3:
        Check_Air_Guard(wk);

        if (--g_state.Timer_00[wk->wu.id] != 0) {
            break;
        }

        Lever_Data = Check_SP_Jump_Attack(wk, Lever_Data);
        g_state.Lever_Buff[wk->wu.id] = Lever_Data;
        g_state.CP_Index[wk->wu.id][1]++;

        break;

    default:
        Check_Air_Guard(wk);
        if (wk->wu.hf.hit.player) {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        }
        Check_Landed(wk, Reaction & 0xFFF);
        break;
    }
    if (g_state.CP_Index[wk->wu.id][1] >= 3) {
        g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_LR[wk->wu.id];
    }
}

/** @brief  */
void Check_Jump_Attack_Conditions(PLW* wk, s16 Range_X, s16 Range_Y, s16 Reaction, u16 Lever_Data, s16 Jump_Dir, s16 Range_JX,
                      s16 Range_JY, s16 J_Lever_Data) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        Setup_Lever_LR(wk, wk->wu.id, Reaction & 0xF000);

        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }
        if ((wk->wu.routine_no[1] == 4) && (wk->wu.cg_type != 0x40)) {
            break;
        }

        hi_jump_flag_clear(wk->wu.id);
        g_state.Continue_Menu[wk->wu.id] = 0;

        wk->wu.hf.hit.player = 0;
        g_state.CP_Index[wk->wu.id][1]++;
        Check_First_Menu(wk);
        /* Fallthrough */

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (--g_state.Combo_Speed[wk->wu.id] != 0) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        dash_flag_clear(wk->wu.id);

        Jump_Init(wk, Jump_Dir);
        if (Check_Diagonal_Shell(wk) != 0) {
            Next_Be_Free(wk);
        }

        break;

    case 2:
        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        } else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
        }
        break;

    case 3:
        Check_Air_Guard(wk);
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        if (Check_VS_Air_Attack(wk, Range_JX, Range_JY, J_Lever_Data) != 0) {
            break;
        }
        Check_Term_ABS_Distance(wk);

        if (Check_Term_Sub(wk, g_state.PL_Distance[wk->wu.id], Range_X) == 0) {
            break;
        }
        if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, Range_Y) == 0) {
            break;
        }
        if (Check_Term_Sub(wk, wk->wu.xyz[1].disp.pos, Range_Y) == 0) {
            break;
        }

        Lever_Data = Check_SP_Jump_Attack(wk, Lever_Data);
        g_state.Lever_Buff[wk->wu.id] = Lever_Data;

        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Stock_Hit_Flag[wk->wu.id] = 0;
        break;

    case 4:
        Check_Air_Guard(wk);
        if (wk->wu.hf.hit.player) {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        }
        Check_Landed(wk, Reaction & 0x7F);
        break;

    case 5:
        if (Check_Landed(wk, Reaction & 0x7F) == 0) {
            switch (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]]) {
            default:
            case 1:
            case 10:
                if (Command_Type_00(wk, 8, 0xFFFF, -1) == -1) {
                    g_state.CP_Index[wk->wu.id][1] = 0x63;
                }
            }
        }
        break;
    default:
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Check_Landed(wk, Reaction & 0xFFF);
        break;
    }
    if (g_state.CP_Index[wk->wu.id][1] >= 3) {
        g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_LR[wk->wu.id];
    }
}

/** @brief  */
s32 Check_SP_Jump_Attack(PLW* wk, s16 Lever_Data) {
    u16 xx;

    if (!(Lever_Data & 0x8000)) {
        return Lever_Data;
    }

    xx = Setup_Guard_Lever(wk, 0);
    xx |= Lever_Data & 0x7FFF;
    return xx | 2;
}

/** @brief  */
s32 Check_VS_Air_Attack(PLW* wk, s16 Range_JX, s16 Range_JY, s16 J_Lever_Data) {
    WORK* em;

    if ((Range_JX == -1) && (Range_JY == -1)) {
        return 0;
    }
    if (J_Lever_Data == -1) {
        return 0;
    }

    em = (WORK*)wk->wu.target_adrs;
    if ((em->pat_status != 0xE) && (em->pat_status != 0x14) && (em->pat_status != 0x1A) && (em->xyz[1].disp.pos <= 0)) {
        return 0;
    }

    if (Check_Term_Sub_Air(wk, g_state.PL_Distance[wk->wu.id], Range_JX) == 0) {
        return 0;
    }
    if (Check_Term_Sub(wk, Ck_Distance_Height(wk), Range_JY) != 0) {
        switch (g_state.CP_Index[wk->wu.id][2]) {
        case 0:
            g_state.CP_Index[wk->wu.id][2]++;
            g_state.Timer_01[wk->wu.id] = Select_Reflection_Time(wk);
            g_state.Timer_01[wk->wu.id]++;
            break;
        default:
            if (--g_state.Timer_01[wk->wu.id] != 0) {
                break;
            }

            if (J_Lever_Data & 0x4000) {
                g_state.CP_Index[wk->wu.id][1] += 2;
                if (g_state.cmd_sel[wk->wu.id]) {
                    Tech_Address[wk->wu.id] = player_CMD[wk->player_number][J_Lever_Data & 0x3FFF];
                } else {
                    Tech_Address[wk->wu.id] = player_cmd[wk->player_number][J_Lever_Data & 0x3FFF];
                }
                g_state.Continue_Menu[wk->wu.id] = 1;
                return -1;
            }

            g_state.Lever_Buff[wk->wu.id] = J_Lever_Data;
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Continue_Menu[wk->wu.id] = 1;
            return 1;
        }
    }

    return 0;
}

/** @brief  */
void Hi_Jump_Attack(PLW* wk, s16 Reaction, s16 Time_Data, u16 Lever_Data, s16 Jump_Dir) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        Setup_Lever_LR(wk, wk->wu.id, Reaction & 0xF000);
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }
        if (Check_Start_Hi_Jump(wk) == 0) {
            g_state.Continue_Menu[wk->wu.id] = 0;
            wk->wu.hf.hit.player = 0;
            g_state.CP_Index[wk->wu.id][1]++;
            if (g_state.cmd_sel[wk->wu.id]) {
                Tech_Address[wk->wu.id] = player_CMD[wk->player_number][2];
            } else {
                Tech_Address[wk->wu.id] = player_cmd[wk->player_number][2];
            }
            g_state.Timer_00[wk->wu.id] = Time_Data;
            Check_First_Menu(wk);
        }

        break;

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (--g_state.Combo_Speed[wk->wu.id] != 0) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Tech_Index[wk->wu.id] = 0xC;

        dash_flag_clear(wk->wu.id);
        Jump_Init(wk, Jump_Dir);
        g_state.Lever_Pool[wk->wu.id] &= 0xC;
        g_state.Lever_Buff[wk->wu.id] = 0;
        Check_Air_Guard(wk);
        if (Check_Diagonal_Shell(wk) != 0) {
            Next_Be_Free(wk);
        }

        break;

    case 2:
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (Command_Type_00(wk, 8, 0xFFFF, -1) == -1) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_Pool[wk->wu.id];
            break;
        }

        if (g_state.Lever_Buff[wk->wu.id] & 2) {
            return;
        }
        g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_Pool[wk->wu.id];

        break;

    case 3:
        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        }

        else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id] | 1;
        }

        break;

    case 4:
        Check_Air_Guard(wk);

        if (--g_state.Timer_00[wk->wu.id] != 0) {
            break;
        }

        Lever_Data = Check_SP_Jump_Attack(wk, Lever_Data);
        g_state.Lever_Buff[wk->wu.id] = Lever_Data;
        g_state.CP_Index[wk->wu.id][1] += 2;
        if (Reaction & 0x80) {
            g_state.CP_Index[wk->wu.id][1]++;
        }

        break;

    case 6:
        Check_Air_Guard(wk);
        if (g_state.Attack_Flag[wk->wu.id]) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        if (wk->wu.hf.hit.player == 0) {
            break;
        }

        if (!(wk->wu.cg_cancel & 8)) {
            break;
        }

        g_state.Lever_Buff[wk->wu.id] = Get_Target_Combo_Data(wk);

        break;

    default:
        if (wk->wu.hf.hit.player) {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        }
        Check_Landed(wk, Reaction & 0xFFF);
        break;
    }
    if (g_state.CP_Index[wk->wu.id][1] >= 4) {
        g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_LR[wk->wu.id];
    }
}

/** @brief  */
void Hi_Jump_Attack_Term(PLW* wk, s16 Range_X, s16 Range_Y, s16 Reaction, u16 Lever_Data, s16 Jump_Dir, s16 Range_JX,
                         s16 Range_JY, u16 J_Lever_Data) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        Setup_Lever_LR(wk, wk->wu.id, Reaction & 0xF000);
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }
        if (Check_Start_Hi_Jump(wk) != 0) {
            break;
        }

        g_state.Continue_Menu[wk->wu.id] = 0;
        wk->wu.hf.hit.player = 0;
        g_state.CP_Index[wk->wu.id][1]++;
        if (g_state.cmd_sel[wk->wu.id]) {
            Tech_Address[wk->wu.id] = player_CMD[wk->player_number][2];
        } else {
            Tech_Address[wk->wu.id] = player_cmd[wk->player_number][2];
        }
        Check_First_Menu(wk);

        break;

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id] != 0) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Tech_Index[wk->wu.id] = 0xC;

        dash_flag_clear(wk->wu.id);
        Jump_Init(wk, Jump_Dir);
        g_state.Lever_Pool[wk->wu.id] &= 0xC;
        g_state.Lever_Buff[wk->wu.id] = 0;
        if (Check_Diagonal_Shell(wk) != 0) {
            Next_Be_Free(wk);
        }

        break;

    case 2:
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (Command_Type_00(wk, 8, 0xFFFF, -1) == -1) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_Pool[wk->wu.id];
        } else {
            if (g_state.Lever_Buff[wk->wu.id] & 2) {
                return;
            }
            g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_Pool[wk->wu.id];
        }
        break;

    case 3:
        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        }

        else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id] | 1;
        }

        break;

    case 4:
        Check_Air_Guard(wk);
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        if (Check_VS_Air_Attack(wk, Range_JX, Range_JY, J_Lever_Data) != 0) {
            break;
        }

        if (Check_Term_Sub(wk, g_state.PL_Distance[wk->wu.id], Range_X) == 0) {
            break;
        }
        if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, Range_Y) == 0) {
            break;
        }
        if (Check_Term_Sub(wk, wk->wu.xyz[1].disp.pos, Range_Y) == 0) {
            break;
        }

        Lever_Data = Check_SP_Jump_Attack(wk, Lever_Data);
        g_state.Lever_Buff[wk->wu.id] = Lever_Data;
        g_state.CP_Index[wk->wu.id][1]++;
        if (Reaction & 0x80) {
            g_state.CP_Index[wk->wu.id][1] = 8;
        }
        break;

    case 5:
        if (wk->wu.hf.hit.player) {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        }
        Check_Landed(wk, Reaction & 0xFFF);
        break;

    case 6:
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        switch (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]]) {
        default:
        case 1:
        case 10:

            if (Command_Type_00(wk, 8, 0xFFFF, -1) == -1) {
                g_state.CP_Index[wk->wu.id][1] = 0x63;
            }
        }

        break;

    case 7:
        if (--g_state.Combo_Speed[wk->wu.id]) {
            break;
        }
        g_state.Lever_Buff[wk->wu.id] = Tech_Address[wk->wu.id][8];
        g_state.CP_Index[wk->wu.id][1]++;
        break;

    case 8:
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Check_Landed(wk, Reaction & 0xFFF);
        break;

    default:
        if (g_state.Attack_Flag[wk->wu.id]) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1] = 8;
        if (wk->wu.hf.hit.player == 0) {
            break;
        }

        if (wk->wu.cg_cancel & 8) {
            g_state.Lever_Buff[wk->wu.id] = Get_Target_Combo_Data(wk);
        }
        break;
    }
    if (g_state.CP_Index[wk->wu.id][1] >= 4) {
        g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_LR[wk->wu.id];
    }
}

/** @brief  */
s32 Check_Term_ABS_Distance(PLW* wk) {
    if (g_state.Turn_Over[wk->wu.id]) {
        return 1;
    }

    if (g_state.My_char[wk->wu.id] == 5) {
        return 0;
    }

    if (g_state.PL_Distance[wk->wu.id] < 0x31) {
        return 1;
    }

    if (wk->wu.mvxy.a[1].real.h >= 0) {
        return 0;
    }

    if (wk->wu.xyz[1].disp.pos < 0x31) {
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Check_Com_Add_Y(PLW* wk, s16 Pos_Y, s16 Range) {
    if (Range == -1) {
        return 1;
    }
    if (!(Range & 0x8000)) {
        if (Pos_Y >= Range) {
            return 1;
        }
        return 0;
    } else {
        if (wk->wu.mvxy.a[1].real.h >= 0) {
            return 0;
        }
        if (Pos_Y <= (Range & 0x7FFF)) {
            return 1;
        }
        return 0;
    }
}

/** @brief  */
void Oro_Check_Jump_Attack(PLW* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX, s16 RY, u16 Lever_Data, s16 RJX,
                 s16 RJY, u16 JLD) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }

        Setup_Lever_LR(wk, wk->wu.id, Reaction & 0xF000);

        if ((wk->wu.routine_no[1] == 4) && (wk->wu.cg_type != 0x40)) {
            break;
        }

        hi_jump_flag_clear(wk->wu.id);
        g_state.Continue_Menu[wk->wu.id] = 0;

        wk->wu.hf.hit.player = 0;
        g_state.CP_Index[wk->wu.id][1]++;
        Check_First_Menu(wk);
        /* fallthrough */

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id] != 0) {
            break;
        }

        g_state.CP_Index[wk->wu.id][1]++;
        dash_flag_clear(wk->wu.id);

        Jump_Init(wk, Jump_Dir);
        Check_Air_Guard(wk);
        if (Check_Diagonal_Shell(wk) != 0) {
            Next_Be_Free(wk);
        }

        break;

    case 2:
        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        } else {

            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
            g_state.Timer_00[wk->wu.id] = 2;
        }
        break;

    case 3:
        Check_Air_Guard(wk);

        if (Lever_Data != 0xFFFF) {
            if (Check_Landed(wk, Reaction) != 0) {
                break;
            }
            if (Check_VS_Air_Attack(wk, RJX, RJY, JLD) != 0) {
                break;
            }
            if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, JY) == 0) {
                break;
            }
        }
        if ((wk->air_jump_ok_time == 0) && (wk->wu.position_y >= 0x30)) {
            Jump_Init(wk, Jump_Dir2);

            if ((Lever_Data) == 0xFFFF) {
                g_state.CP_Index[wk->wu.id][1] += 2;
            } else {
                g_state.CP_Index[wk->wu.id][1]++;
            }
        }

        break;

    case 4:
        Check_Air_Guard(wk);
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        if (Check_VS_Air_Attack(wk, RJX, RJY, JLD) != 0) {
            break;
        }
        if (Check_Term_Sub(wk, g_state.PL_Distance[wk->wu.id], RX) == 0) {
            break;
        }
        if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }
        if (Check_Term_Sub(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }

        g_state.Lever_Buff[wk->wu.id] = Lever_Data;

        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Stock_Hit_Flag[wk->wu.id] = 0;
        break;

    case 5:
        if (wk->wu.hf.hit.player) {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        }
        Check_Landed(wk, Reaction & 0x7F);
        break;

    case 6:
        if (Check_Landed(wk, Reaction & 0x7F) != 0) {
            break;
        }

        switch (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]]) {
        default:
        case 1:
        case 10:
            if (Command_Type_00(wk, 8, 0xFFFF, -1) == -1) {
                g_state.CP_Index[wk->wu.id][1] = 0x63;
            }
        }

        break;
    default:
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Check_Landed(wk, Reaction & 0xFFF);
        break;
    }
    if (g_state.CP_Index[wk->wu.id][1] >= 3) {
        g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_LR[wk->wu.id];
    }
}

/** @brief  */
void Oro_Check_High_Jump_Attack(PLW* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX, s16 RY, u16 Lever_Data, s16 RJX,
                  s16 RJY, u16 JLD) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }
        if (Check_Start_Hi_Jump(wk) == 0) {
            g_state.Continue_Menu[wk->wu.id] = 0;
            g_state.CP_Index[wk->wu.id][1]++;
            if (g_state.cmd_sel[wk->wu.id]) {
                Tech_Address[wk->wu.id] = player_CMD[wk->player_number][2];
            } else {
                Tech_Address[wk->wu.id] = player_cmd[wk->player_number][2];
            }
            Check_First_Menu(wk);
        }
        break;

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id == 0]) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Tech_Index[wk->wu.id] = 0xC;

            Jump_Init(wk, Jump_Dir);
            g_state.Lever_Pool[wk->wu.id] &= 0xC;
            g_state.Lever_Buff[wk->wu.id] = 0;
            Check_Air_Guard(wk);
            if (Check_Diagonal_Shell(wk) != 0) {
                Next_Be_Free(wk);
            }
        }
        break;

    case 2:
        if (Check_Passive(wk) != 0) {
            break;
        }
        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Lever_Buff[wk->wu.id] = 2;
        g_state.Lever_Pool[wk->wu.id] |= 1;
        break;

    case 3:
        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        } else {

            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
            g_state.Timer_00[wk->wu.id] = 2;
        }
        break;

    case 4:
        Check_Air_Guard(wk);

        if (Lever_Data != 0xFFFF) {
            if (Check_Landed(wk, Reaction) != 0) {
                break;
            }
            if (Check_VS_Air_Attack(wk, RJX, RJY, JLD) != 0) {
                break;
            }
            if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, JY) == 0) {
                break;
            }
        }
        if ((wk->air_jump_ok_time == 0) && (wk->wu.position_y >= 0x30)) {
            Jump_Init(wk, Jump_Dir2);

            if ((Lever_Data) == 0xFFFF) {
                g_state.CP_Index[wk->wu.id][1] += 2;
            } else {
                g_state.CP_Index[wk->wu.id][1]++;
            }
        }

        break;

    case 5:
        Check_Air_Guard(wk);
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        if (Check_VS_Air_Attack(wk, RJX, RJY, JLD) != 0) {
            break;
        }
        if (Check_Term_Sub(wk, g_state.PL_Distance[wk->wu.id], RX) == 0) {
            break;
        }
        if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }
        if (Check_Term_Sub(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }

        g_state.Lever_Buff[wk->wu.id] = Lever_Data;

        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Stock_Hit_Flag[wk->wu.id] = 0;
        break;

    case 6:
        if (wk->wu.hf.hit.player) {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        }
        Check_Landed(wk, Reaction & 0x7F);
        break;

    case 7:
        if (Check_Landed(wk, Reaction & 0x7F) != 0) {
            break;
        }

        switch (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]]) {
        default:
        case 1:
        case 10:
            if (Command_Type_00(wk, 8, 0xFFFF, -1) == -1) {
                g_state.CP_Index[wk->wu.id][1] = 0x63;
            }
        }

        break;
    default:
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Check_Landed(wk, Reaction & 0xFFF);
        break;
    }
    if (g_state.CP_Index[wk->wu.id][1] >= 3) {
        g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_LR[wk->wu.id];
    }
}

/** @brief  */
void Command_Attack(PLW* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        dash_flag_clear(wk->wu.id);
        if (g_state.cmd_sel[wk->wu.id]) {
            Tech_Address[wk->wu.id] = player_CMD[wk->player_number][Tech_Number & 0xFF];
        } else {
            Tech_Address[wk->wu.id] = player_cmd[wk->player_number][Tech_Number & 0xFF];
        }
        g_state.Tech_Index[wk->wu.id] = 0xC;
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

        if (Check_Start_Command_Attack(wk, Reaction, Tech_Number & 0x80FF) != 0) {
            break;
        }
        if (Check_Dash_Hit(wk, Tech_Number & 0x80FF) != 0) {
            Next_Be_Free(wk);
        }

        g_state.CP_Index[wk->wu.id][1]++;
        Check_First_Menu(wk);

        if (Power_Level & 0x4000) {
            g_state.Free_Lever[wk->wu.id] = Power_Lv_Data[(Power_Level & 0xF) - 8];
        } else {
            g_state.Free_Lever[wk->wu.id] = 0;
        }
        /* Fallthrough */

    case 1:
        if (--g_state.Combo_Speed[wk->wu.id]) {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
            break;
        }

        if (Hadou_Check(wk, Tech_Number & 0x80FF) != 0) {
            if (Check_Passive(wk) == 0) {
                g_state.Combo_Speed[wk->wu.id] = 1;
            }
            break;
        } else {
            g_state.CP_Index[wk->wu.id][1]++;
            Check_Rapid(wk, Tech_Number);
        }
        /* Fallthrough */
    case 2:
        if (Check_Passive(wk) != 0) {
            break;
        }
        switch (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]]) {

        default:
        case 1:

            if (Command_Type_00(wk, Power_Level & 0xF, Tech_Number, Ex_Shot) == -1) {
                if ((Tech_Number & 0xF) == 0 || (Tech_Number & 0xF) == 1) {
                    if (Reaction == 0xC) {
                        g_state.CP_Index[wk->wu.id][1] = 0x63;
                        g_state.Timer_00[wk->wu.id] = Dash_Time_Data[wk->player_number][Tech_Number];
                    } else {
                        g_state.CP_Index[wk->wu.id][1] = 4;
                    }
                } else {
                    g_state.CP_Index[wk->wu.id][1] = 3;
                }
            }
            break;

        case 2:
            if (Command_Type_01(wk, Power_Level & 0xF, Ex_Shot) != 0) {
                g_state.CP_Index[wk->wu.id][1]++;
            }
            break;

        case 7:
            if (Command_Type_06(wk, Power_Level & 0xF, Tech_Number, Ex_Shot) != 0) {
                g_state.CP_Index[wk->wu.id][1]++;
            }
            break;
        }
        break;

    case 3:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (g_state.plw[wk->wu.id].is_throwing) {
            break;
        }
        if (((wk->wu.cg_type) == 0x40) || (wk->wu.routine_no[1] == 0)) {
            Reaction_Exit_Sub(wk);
        } else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Free_Lever[wk->wu.id];
            Rapid_Sub(wk);
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
            Reaction_Sub(wk, Reaction, Power_Level);
        }
        break;

    case 4:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (((wk->wu.cg_type) == 0x40) || (wk->running_f == 0)) {
            Reaction_Exit_Sub(wk);
        }
        break;

    case 5:
        if (g_state.PL_Distance[wk->wu.id] > 0x70) {
            g_state.Lever_Buff[wk->wu.id] = 0x40;
        }
        if (((wk->wu.cg_type) == 0x40) || (wk->wu.routine_no[1] == 0)) {
            Reaction_Exit_Sub(wk);
        }
        break;

    default:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (--g_state.Timer_00[wk->wu.id] == 0) {
            Reaction_Exit_Sub(wk);
        }
        break;
    }
}

const s32 Hadou_Check_Data[20][2] = {
    { 0, 0 },    { 0, 0 }, { 1, 0x1D }, { 0, 0 },    { 1, 0x1F }, { 0, 0 }, { 1, 0x20 },
    { 1, 0x21 }, { 0, 0 }, { 0, 0 },    { 1, 0x1F }, { 1, 0x1D }, { 0, 0 }, { 0, 0 },
    { 1, 0x1F }, { 0, 0 }, { 1, 0x1E }, { 0, 0 },    { 0, 0 },    { 0, 0 },
};

/** @brief  */
s32 Hadou_Check(PLW* wk, u16 Tech_Number) {
    if (Hadou_Check_Data[wk->player_number][0] == 0) {
        return 0;
    }

    if (Tech_Number != Hadou_Check_Data[wk->player_number][1]) {
        return 0;
    }

    return Check_Resume_Lever(wk);
}

/** @brief  */
s32 Check_Resume_Lever(PLW* wk) {
    u16 Target_Lever;
    s16 xx;

    if (wk->wu.active_move) {
        Target_Lever = 8;
    } else {
        Target_Lever = 4;
    }

    for (xx = 0; xx <= 8; xx++) {
        if (Target_Lever == g_state.Resume_Lever[wk->wu.id][xx]) {
            return 1;
        }
    }
    return 0;
}

/** @brief  */
void Jump_Command_Attack(PLW* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }

        dash_flag_clear(wk->wu.id);

        if (g_state.cmd_sel[wk->wu.id]) {
            Tech_Address[wk->wu.id] = player_CMD[wk->player_number][Tech_Number & 0xFF];
        } else {
            Tech_Address[wk->wu.id] = player_cmd[wk->player_number][Tech_Number & 0xFF];
        }

        g_state.Tech_Index[wk->wu.id] = 0xC;
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

        if (Check_Start_Command_Attack(wk, Reaction, Tech_Number & 0x80FF) != 0) {
            break;
        }
        if (Check_Dash_Hit(wk, Tech_Number & 0x80FF) != 0) {
            Next_Be_Free(wk);
        }

        g_state.Continue_Menu[wk->wu.id] = 0;
        g_state.CP_Index[wk->wu.id][1]++;
        Check_First_Menu(wk);
        /* Fallthough */

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (--g_state.Combo_Speed[wk->wu.id]) {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
            break;
        }
        if (Check_Diagonal_Shell(wk) != 0) {
            Next_Be_Free(wk);
            break;
        }
        g_state.CP_Index[wk->wu.id][1]++;
        /* Fallthough */
    case 2:
        if (Check_Passive(wk) != 0) {
            break;
        }
        switch (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]]) {

        case 2:
            if (Command_Type_01(wk, Power_Level & 0xF, Ex_Shot) != 0) {
                g_state.CP_Index[wk->wu.id][1]++;
            }
            break;

        default:
        case 1:
        case 10:
            if (Command_Type_00(wk, Power_Level & 0xF, Tech_Number, Ex_Shot) == -1) {
                g_state.CP_Index[wk->wu.id][1] = 0x63;
            }
            break;
        }
        break;

    case 3:
        Check_Rapid(wk, Tech_Number);
        g_state.CP_Index[wk->wu.id][1]++;
        return;

    default:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        if ((wk->wu.cg_type == 0x40) || (wk->wu.routine_no[1] == 0)) {
            Reaction_Exit_Sub(wk);
        } else {
            Rapid_Sub(wk);
            if (Reaction == 0xC) {
                Reaction_Sub(wk, Reaction, Power_Level);
                break;
            }
            Check_Landed(wk, Reaction & 0xFFF);
        }
        break;
    }
}

/** @brief  */
void Rapid_Command_Attack(PLW* wk, s16 Reaction, u16 Tech_Number, s16 Shot, u16 Time) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        dash_flag_clear(wk->wu.id);
        if (g_state.cmd_sel[wk->wu.id]) {
            Tech_Address[wk->wu.id] = player_CMD[wk->player_number][Tech_Number & 0xFF];
        } else {
            Tech_Address[wk->wu.id] = player_cmd[wk->player_number][Tech_Number & 0xFF];
        }
        g_state.Tech_Index[wk->wu.id] = 0xC;
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

        if (Check_Start_Command_Attack(wk, Reaction, Tech_Number & 0x80FF) != 0) {
            break;
        }
        if (Check_Dash_Hit(wk, Tech_Number & 0x80FF) != 0) {
            Next_Be_Free(wk);
        }

        g_state.CP_Index[wk->wu.id][1]++;
        Check_First_Menu(wk);
        g_state.Free_Lever[wk->wu.id] = 0;
        /* Fallthough */

    case 1:
        if (--g_state.Combo_Speed[wk->wu.id]) {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
            break;
        }
        g_state.CP_Index[wk->wu.id][1]++;

        g_state.Rapid_No[wk->wu.id][0] = 0;
        g_state.Rapid_No[wk->wu.id][1] = 0;
        g_state.Timer_00[wk->wu.id] = Time;

    case 2:
        switch (g_state.Rapid_No[wk->wu.id][0]) {

        case 0:
            g_state.Rapid_No[wk->wu.id][0] = 1;
            g_state.Lever_Buff[wk->wu.id] = Shot;
            break;
        case 1:
            g_state.Rapid_No[wk->wu.id][0] = 0;
            g_state.Lever_Buff[wk->wu.id] = 0;
            break;
        }
        if (wk->wu.sp_tech_id == Tech_Number) {
            g_state.CP_Index[wk->wu.id][1] = 3;
        }
        break;

    case 3:
        if (--g_state.Timer_00[wk->wu.id] == 0) {
            g_state.CP_Index[wk->wu.id][1] = 4;
        }

        else {
            switch (g_state.Rapid_No[wk->wu.id][0]) {

            case 0:
                g_state.Rapid_No[wk->wu.id][0] = 1;
                g_state.Lever_Buff[wk->wu.id] = Shot;
                break;
            case 1:
                g_state.Rapid_No[wk->wu.id][0] = 0;
                g_state.Lever_Buff[wk->wu.id] = 0;
                break;
            }
            if (wk->wu.sp_tech_id != Tech_Number) {
                g_state.CP_Index[wk->wu.id][1] = 4;
            }
        }
        break;

    case 4:
        if (wk->wu.sp_tech_id == Tech_Number) {
            break;
        }
        if ((wk->wu.cg_type == 0x40) || (wk->wu.routine_no[1] == 0)) {
            Reaction_Exit_Sub(wk);
        }
        break;
    }
}

/** @brief  */
void Check_Rapid(PLW* wk, u16 Tech_Number) {
    if (!(Tech_Number & 0xF00)) {
        g_state.Rapid_No[wk->wu.id][0] = 0;
    } else {
        Lv = Setup_Lv08(0);
        if (g_state.Break_Into_CPU == 2) {
            Lv = 7;
        }
        g_state.Rapid_No[wk->wu.id][0] = Rapid_SA_Data[emLevelRemake(Lv, 8, 0)][random_32_com() & 7];
        g_state.Rapid_No[wk->wu.id][1] = 0;
        Setup_Rapid_End_Term(wk, Tech_Number);
        g_state.Rapid_Index[wk->wu.id] = 0xFF0;
    }
}

/** @brief  */
void Setup_Rapid_End_Term(PLW* wk, s16 Tech_Number) {
    g_state.Rapid_No[wk->wu.id][2] = (Tech_Number & 0xF00) >> 8;
    if ((Tech_Number & 0xF00) == 0x400) {
        g_state.Rapid_No[wk->wu.id][3] = Setup_Rapid_Time(wk, Tech_Number);
    }
}

/** @brief  */
s32 Setup_Rapid_Time(PLW* wk, u16 Tech_Number) {
    return 60;
}

/** @brief  */
void Rapid_Sub(PLW* wk) {
    if (Check_Rapid_End(wk) != 0) {
        return;
    }

    switch (g_state.Rapid_No[wk->wu.id][0]) {
    case 0:
        break;
    case 2:
        switch (g_state.Rapid_No[wk->wu.id][1]) {
        case 0:
            g_state.Rapid_No[wk->wu.id][1]++;
            g_state.Timer_00[wk->wu.id] = 1;
            g_state.Timer_01[wk->wu.id] = 3;
            return;
        case 1:
            if (--g_state.Timer_00[wk->wu.id] == 0) {
                g_state.Lever_Buff[wk->wu.id] = g_state.Rapid_Index[wk->wu.id];
                g_state.Timer_00[wk->wu.id] = 2;

                if (--g_state.Timer_01[wk->wu.id] == 0) {
                    g_state.Rapid_No[wk->wu.id][1]++;
                    g_state.Timer_01[wk->wu.id] = 0x18;
                }
            }
            break;
        case 2:
            if (--g_state.Timer_01[wk->wu.id] == 0) {
                g_state.Rapid_No[wk->wu.id][1]++;
                g_state.Timer_00[wk->wu.id] = 1;
                g_state.Timer_01[wk->wu.id] = 2;
            }
            break;
        default:
            if (--g_state.Timer_00[wk->wu.id] == 0) {
                g_state.Lever_Buff[wk->wu.id] = g_state.Rapid_Index[wk->wu.id];
                g_state.Timer_00[wk->wu.id] = g_state.Timer_01[wk->wu.id];
            }
            break;
        }
        break;
    default:
        switch (g_state.Rapid_No[wk->wu.id][1]) {
        case 0:
            g_state.Rapid_No[wk->wu.id][1]++;
            g_state.Timer_00[wk->wu.id] = 1;
            g_state.Timer_01[wk->wu.id] = 2;
            break;
        default:
            if (--g_state.Timer_00[wk->wu.id] == 0) {
                g_state.Lever_Buff[wk->wu.id] = g_state.Rapid_Index[wk->wu.id];
                g_state.Timer_00[wk->wu.id] = g_state.Timer_01[wk->wu.id];
            }
            break;
        }
        break;
    }
}

/** @brief  */
s32 Check_Rapid_End(PLW* wk) {
    switch (g_state.Rapid_No[wk->wu.id][2]) {
    case 1:
        if (wk->wu.mvxy.a[1].real.h < 0) {
            g_state.Rapid_No[wk->wu.id][0] = 0;
            return 1;
        }
        break;
    case 2:
        switch (g_state.Rapid_No[wk->wu.id][3]) {
        case 0:
            if (g_state.plw[wk->wu.id].caution_flag) {
                g_state.Rapid_No[wk->wu.id][3]++;
            }
            break;
        case 1:
            if (g_state.plw[wk->wu.id].caution_flag == 0) {
                g_state.Rapid_No[wk->wu.id][0] = 0;
                return 1;
            }
            break;
        }
        break;
    case 4:
        switch (g_state.Rapid_No[wk->wu.id][3]) {
        case 0:
            if (wk->wu.cg_ja.atix) {
                g_state.Rapid_No[wk->wu.id][3]++;
            }
            break;
        case 1:
            if (--g_state.Rapid_No[wk->wu.id][3] == 0) {
                g_state.Rapid_No[wk->wu.id][0] = 0;
                return 1;
            }
            break;
        }
        break;
    }
    return 0;
}

/** @brief  */
s32 Check_Start_Command_Attack(PLW* wk, s16 Reaction, u16 Tech_Number) {
    if (g_state.Before_Jump[wk->wu.id]) {
        return g_state.Before_Jump[wk->wu.id] = 0;
    }

    if (wk->wu.routine_no[1] == 2) {
        return 1;
    }
    if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
        return 0;
    }

    if ((Tech_Number == 0) || (Tech_Number == 1)) {
        if (wk->wu.cg_cancel & 2) {
            return 0;
        }
        return 1;
    }
    if ((Tech_Number & 0x8000) && (wk->wu.cg_cancel & 0x40)) {
        return 0;
    }
    if (wk->wu.cg_cancel & 0x20) {
        return 0;
    }
    if ((wk->wu.cg_cancel & 8) && (Reaction == 0xE)) {
        return 0;
    }
    return 1;
}

/** @brief  */
void Oro_Check_Jump_Command_Attack(PLW* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX, s16 RY, u16 Tech_Number,
                  s16 Power_Level, s16 Ex_Shot, s16 RJX, s16 RJY, u16 JLD) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        if (Check_Passive(wk) != 0) {
            break;
        }
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
            g_state.Continue_Menu[wk->wu.id] = 0;
            g_state.CP_Index[wk->wu.id][1]++;
            if (g_state.cmd_sel[wk->wu.id]) {
                Tech_Address[wk->wu.id] = player_CMD[wk->player_number][Tech_Number & 0xFF];
            } else {
                Tech_Address[wk->wu.id] = player_cmd[wk->player_number][Tech_Number & 0xFF];
            }
            Check_First_Menu(wk);
        }

        break;

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id] == 0) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Tech_Index[wk->wu.id] = 0xC;
            dash_flag_clear(wk->wu.id);

            Jump_Init(wk, Jump_Dir);
            Check_Air_Guard(wk);
            if (Check_Diagonal_Shell(wk) != 0) {
                Next_Be_Free(wk);
            }
        }
        break;

    case 2:
        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        }

        else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
            g_state.Timer_00[wk->wu.id] = 2;
        }
        break;

    case 3:
        Check_Air_Guard(wk);

        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }
        if (Check_VS_Air_Attack(wk, RJX, RJY, JLD) != 0) {
            break;
        }
        if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, JY) == 0) {
            break;
        }

        Jump_Init(wk, Jump_Dir2);
        if (--g_state.Timer_00[wk->wu.id] == 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        }
        break;

    case 4:
        Check_Air_Guard(wk);
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        if (Check_VS_Air_Attack(wk, RJX, RJY, JLD) != 0) {
            break;
        }
        if (Check_Term_Sub(wk, g_state.PL_Distance[wk->wu.id], RX) == 0) {
            break;
        }
        if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }
        if (Check_Term_Sub(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }
        g_state.CP_Index[wk->wu.id][1] += 2;
        break;

    case 5:
        if (wk->wu.hf.hit.player) {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        }
        Check_Landed(wk, Reaction & 0x7F);
        break;

    case 6:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        switch (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]]) {
        default:
        case 1:
        case 10:
            if (Command_Type_00(wk, Power_Level & 0xF, Tech_Number, Ex_Shot) == -1) {
                g_state.CP_Index[wk->wu.id][1] = 0x63;
            }
            break;
        }
        break;
    default:
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Rapid_Sub(wk);
        Check_Landed(wk, Reaction & 0xFFF);
        break;
    }
}

/** @brief  */
void Oro_Check_High_Jump_Command_Attack(PLW* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX, s16 RY, u16 Tech_Number,
                   s16 Power_Level, s16 Ex_Shot, s16 RJX, s16 RJY, u16 JLD) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }
        if (Check_Start_Hi_Jump(wk) != 0) {
            break;
        }

        g_state.Continue_Menu[wk->wu.id] = 0;
        g_state.CP_Index[wk->wu.id][1]++;
        if (g_state.cmd_sel[wk->wu.id]) {
            Tech_Address[wk->wu.id] = player_CMD[wk->player_number][Tech_Number & 0x7FFF];
        } else {
            Tech_Address[wk->wu.id] = player_cmd[wk->player_number][Tech_Number & 0x7FFF];
        }
        Check_First_Menu(wk);

        break;

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id == 0]) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Tech_Index[wk->wu.id] = 0xC;

            Jump_Init(wk, Jump_Dir);
            g_state.Lever_Pool[wk->wu.id] &= 0xC;
            g_state.Lever_Buff[wk->wu.id] = 0;
            Check_Air_Guard(wk);
            if (Check_Diagonal_Shell(wk) != 0) {
                Next_Be_Free(wk);
            }
        }
        break;

    case 2:
        if (Check_Passive(wk) != 0) {
            break;
        }
        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Lever_Buff[wk->wu.id] = 2;
        g_state.Lever_Pool[wk->wu.id] |= 1;
        break;

    case 3:
        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        }

        else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
            g_state.Timer_00[wk->wu.id] = 2;
        }
        break;

    case 4:
        Check_Air_Guard(wk);

        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }
        if (Check_VS_Air_Attack(wk, RJX, RJY, JLD) != 0) {
            break;
        }
        if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, JY) == 0) {
            break;
        }

        Jump_Init(wk, Jump_Dir2);
        if (--g_state.Timer_00[wk->wu.id] == 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        }
        break;

    case 5:
        Check_Air_Guard(wk);
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        if (Check_VS_Air_Attack(wk, RJX, RJY, JLD) != 0) {
            break;
        }
        if (Check_Term_Sub(wk, g_state.PL_Distance[wk->wu.id], RX) == 0) {
            break;
        }
        if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }
        if (Check_Term_Sub(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }
        g_state.CP_Index[wk->wu.id][1] += 2;
        break;

    case 6:
        if (wk->wu.hf.hit.player) {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        }
        Check_Landed(wk, Reaction & 0x7F);
        return;

    case 7:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        switch (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]]) {
        default:
        case 1:
        case 10:
            if (Command_Type_00(wk, Power_Level & 0xF, Tech_Number, Ex_Shot) == -1) {
                g_state.CP_Index[wk->wu.id][1] = 0x63;
            }
            break;
        }
        break;
    default:
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Rapid_Sub(wk);
        Check_Landed(wk, Reaction & 0xFFF);
        break;
    }
}

/** @brief  */
void Jump_Command_Attack_Term(PLW* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot, s16 RX, s16 RY,
                              s16 Jump_Dir, s16 JRX, s16 JRY, u16 JLD) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
            g_state.Continue_Menu[wk->wu.id] = 0;
            g_state.CP_Index[wk->wu.id][1]++;
            if (g_state.cmd_sel[wk->wu.id]) {
                Tech_Address[wk->wu.id] = player_CMD[wk->player_number][Tech_Number & 0xFF];
            } else {
                Tech_Address[wk->wu.id] = player_cmd[wk->player_number][Tech_Number & 0xFF];
            }
            Check_First_Menu(wk);
        }

        break;

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id] == 0) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Tech_Index[wk->wu.id] = 0xC;

            Jump_Init(wk, Jump_Dir);
            Check_Rapid(wk, Tech_Number);

            if (Check_Diagonal_Shell(wk) != 0) {
                Next_Be_Free(wk);
            }
        }
        break;

    case 2:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        }
        break;

    case 3:
        Check_Air_Guard(wk);
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        if (Check_VS_Air_Attack(wk, JRX, JRY, JLD) != 0) {
            break;
        }
        if (Check_Term_Sub(wk, g_state.PL_Distance[wk->wu.id], RX) == 0) {
            break;
        }
        if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }
        if (Check_Term_Sub(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }
        g_state.CP_Index[wk->wu.id][1] += 2;
        break;

    case 4:
        if (wk->wu.hf.hit.player) {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        }
        Check_Landed(wk, Reaction & 0xFFF);
        break;

    case 5:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        switch (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]]) {
        default:
        case 1:
        case 10:
            if (Command_Type_00(wk, Power_Level & 0xF, Tech_Number, Ex_Shot) == -1) {
                g_state.CP_Index[wk->wu.id][1] = 0x63;
            }
            break;
        }
        break;
    default:
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Rapid_Sub(wk);
        Check_Landed(wk, Reaction & 0xFFF);
        break;
    }
}

/** @brief  */
void Hi_Jump_Command_Attack_Term(PLW* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot, s16 RX, s16 RY,
                                 s16 Jump_Dir, s16 JRX, s16 JRY, u16 JLD) {
    switch (g_state.CP_Index[wk->wu.id][1]) {

    case 0:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (Check_Passive(wk) != 0) {
            break;
        }

        if (wk->spmv_ng_flag & 0x30000) {
            Next_Be_Free(wk);
            break;
        }
        if (Check_Start_Hi_Jump(wk) == 0) {
            g_state.Continue_Menu[wk->wu.id] = 0;
            g_state.CP_Index[wk->wu.id][1]++;
            if (g_state.cmd_sel[wk->wu.id]) {
                Tech_Address[wk->wu.id] = player_CMD[wk->player_number][Tech_Number & 0x7FFF];
            } else {
                Tech_Address[wk->wu.id] = player_cmd[wk->player_number][Tech_Number & 0x7FFF];
            }
            Check_First_Menu(wk);
        }

        break;

    case 1:
        if (Check_Passive(wk) != 0) {
            break;
        }
        if (--g_state.Combo_Speed[wk->wu.id == 0]) {
            g_state.CP_Index[wk->wu.id][1]++;
            g_state.Tech_Index[wk->wu.id] = 0xC;

            Jump_Init(wk, Jump_Dir);
            g_state.Lever_Pool[wk->wu.id] &= 0xC;
            g_state.Lever_Buff[wk->wu.id] = 0;
            Check_Air_Guard(wk);
            if (Check_Diagonal_Shell(wk) != 0) {
                Next_Be_Free(wk);
            }
        }
        break;

    case 2:
        if (Check_Passive(wk) != 0) {
            break;
        }
        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Lever_Buff[wk->wu.id] = 2;
        g_state.Lever_Pool[wk->wu.id] |= 1;
        break;

    case 3:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id] | 1;
        if (wk->wu.xyz[1].disp.pos > 0) {
            g_state.CP_Index[wk->wu.id][1]++;
        }
        break;

    case 4:
        Check_Air_Guard(wk);
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        if (Check_VS_Air_Attack(wk, JRX, JRY, JLD) != 0) {
            break;
        }
        if (Check_Term_Sub(wk, g_state.PL_Distance[wk->wu.id], RX) == 0) {
            break;
        }
        if (Check_Com_Add_Y(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }
        if (Check_Term_Sub(wk, wk->wu.xyz[1].disp.pos, RY) == 0) {
            break;
        }
        g_state.CP_Index[wk->wu.id][1] += 2;
        break;

    case 5:
        if (wk->wu.hf.hit.player) {
            g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        }
        Check_Landed(wk, Reaction & 0xFFF);
        break;

    case 6:
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
        if (Check_Landed(wk, Reaction) != 0) {
            break;
        }

        switch (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]]) {
        default:
        case 1:
        case 10:
            if (Command_Type_00(wk, Power_Level & 0xF, Tech_Number, Ex_Shot) == -1) {
                g_state.CP_Index[wk->wu.id][1] = 0x63;
            }
            break;
        }
        break;
    default:
        g_state.Stock_Hit_Flag[wk->wu.id] = wk->wu.hf.hit.player;
        Check_Landed(wk, Reaction & 0xFFF);
        break;
    }
}

/** @brief  */
s32 Check_Landed(PLW* wk, s16 Reaction) {
    if ((wk->wu.old_pos[1] != 0) && (wk->wu.xyz[1].disp.pos == 0)) {
        g_state.Lever_Buff[wk->wu.id] = 0;
        if (g_state.Continue_Menu[wk->wu.id]) {
            Next_End(wk);
            g_state.Before_Jump[wk->wu.id] = 1;
            return 1;
        } else {
            Reaction_Sub(wk, Reaction, 0);

            g_state.Lever_Buff[wk->wu.id] |= g_state.Lever_LR[wk->wu.id];
            Check_Guard(wk);

            g_state.Before_Jump[wk->wu.id] = 1;
            return 1;
        }
    }
    if ((wk->wu.old_pos[1] == 0) && (wk->wu.xyz[1].disp.pos == 0) && (wk->wu.routine_no[1] != 4)) {
        g_state.Lever_Buff[wk->wu.id] = 0;
        if (g_state.Continue_Menu[wk->wu.id]) {
            Next_End(wk);
            g_state.Before_Jump[wk->wu.id] = 1;
            return 1;
        } else {
            Reaction_Sub(wk, Reaction, 0);
            if (Check_Guard(wk) != 0) {
                return 1;
            }
            g_state.Before_Jump[wk->wu.id] = 1;
            return 1;
        }
    }
    return 0;
}

/** @brief  */
s32 Check_Dash_Hit(PLW* wk, u16 Tech_Number) {
    WORK_Other* tmw;
    WORK* em;
    s16 i;
    s16 xx;
    s16 zz;

    if ((Tech_Number != 0) && (Tech_Number != 1)) {
        return 0;
    }

    em = (WORK*)wk->wu.target_adrs;
    for (i = 0; i < 8; i++) {
        if ((get_vs_shell_adrs(em, em->id, i, &tmw) == 0) && (get_vs_shell_adrs((WORK*)wk, em->id, i, &tmw) == 0)) {
            return 0;
        }
        if (tmw->wu.routine_no[1] == 2) {
            continue;
        }

        xx = wk->wu.xyz[0].disp.pos - tmw->wu.xyz[0].disp.pos;
        zz = Setup_Front_or_Back(wk, xx);

        if (Tech_Number == 0) {
            if (zz != 1) {
                if (Check_Hit_Shell(wk, tmw, Tech_Number) != 0) {
                    return 1;
                }
            }
        } else {
            if (zz != 0) {
                if (Check_Hit_Shell(wk, tmw, Tech_Number) != 0) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

/** @brief  */
s32 Setup_Front_or_Back(PLW* wk, s16 xx) {
    if (wk->wu.active_move == 0) {
        if (xx >= 0) {
            return 0;
        }
        return 1;
    } else {
        if (xx >= 0) {
            return 1;
        }
        return 0;
    }
}

/** @brief  */
s32 Check_Hit_Shell(PLW* wk, WORK_Other* tmw, u16 Tech_Number) {
    s16 xx;

    if (wk->wu.active_move == 1) {
        Tech_Number ^= 1;
    }

    if (Tech_Number == 0) {
        xx = wk->wu.xyz[0].disp.pos - Dash_Distance_Data[wk->player_number][Tech_Number];
        if (xx <= tmw->wu.xyz[0].disp.pos) {
            return 1;
        }
        return 0;
    } else {
        xx = wk->wu.xyz[0].disp.pos + Dash_Distance_Data[wk->player_number][Tech_Number];
        if (xx >= tmw->wu.xyz[0].disp.pos) {
            return 1;
        }
        return 0;
    }
}

/** @brief  */
void Jump_Init(PLW* wk, s16 Jump_Dir) {
    switch (Jump_Dir) {
    case 0:
        Check_Jump_Distance_Level(wk);
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
        break;
    case 2:
        g_state.Lever_Pool[wk->wu.id] = 1;
        g_state.Lever_Buff[wk->wu.id] = 1;
        break;

    default:
        Check_Jump_Distance_Level(wk);
        g_state.Lever_Pool[wk->wu.id] ^= 0xC;
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
        break;
    }
}

/** @brief  */
s32 Command_Type_00(PLW* wk, s16 Power_Level, u16 Tech_Number, s16 Ex_Shot) {
    if (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id] + 4] != 0x1C) {
        g_state.Lever_Buff[wk->wu.id] = Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id] + 3] & 0x7FFF;
        g_state.Lever_Buff[wk->wu.id] = datacmd_conpanecmd(g_state.Lever_Buff[wk->wu.id]);

        if (wk->wu.active_move) {
            if (g_state.Lever_Buff[wk->wu.id] & 0xC) {
                g_state.Lever_Buff[wk->wu.id] ^= 0xC;
            }
        }
        g_state.Tech_Index[wk->wu.id] += 4;
        return 1;
    } else {
        g_state.Lever_Buff[wk->wu.id] = Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id] + 3] & 0x7FFF;
        if (wk->wu.active_move) {
            if (g_state.Lever_Buff[wk->wu.id] & 0xC) {
                g_state.Lever_Buff[wk->wu.id] ^= 0xC;
            }
        }
        if (Tech_Address[wk->wu.id][7] == 0x80) {
            return -1;
        }
        g_state.Tech_Index[wk->wu.id] = 7;

        if ((g_state.plw[wk->wu.id].sa->ex) && ((Ex_Shot == 0x70) || (Ex_Shot == 0x700))) {
            g_state.Lever_Buff[wk->wu.id] |= Ex_Shot;
        } else {
            g_state.Lever_Buff[wk->wu.id] |= renbanshot_conpaneshot(Tech_Address[wk->wu.id], Power_Level);
        }

        if ((g_state.My_char[wk->wu.id] == 2) && ((Tech_Number) == 0x8015) && (Power_Level != 8)) {
            g_state.CP_Index[wk->wu.id][0]++;
            g_state.Lever_LR[wk->wu.id] = g_state.Lever_Buff[wk->wu.id] & 0xFF0;

            if (Power_Level == 0xA) {
                g_state.CP_Index[wk->wu.id][1] = 1;
            } else {
                g_state.CP_Index[wk->wu.id][1] = 0;
            }
        } else {
            if (g_state.CP_No[wk->wu.id][0] == 0xA) {
                g_state.Rapid_Index[wk->wu.id] = g_state.Lever_Buff[wk->wu.id] & 0xFF0;
                g_state.Lever_Pool[wk->wu.id] = g_state.Lever_Buff[wk->wu.id] & 0xFF0;
            }
            if ((wk->player_number == 6) && ((Tech_Number) == 0x8016)) {
                g_state.CP_Index[wk->wu.id][1] = 5;
            } else {
                if ((wk->player_number == 7) && ((Tech_Number) == 0x1F)) {
                    Reaction_Exit_Sub(wk);
                } else {
                    g_state.CP_Index[wk->wu.id][1]++;
                }
            }
        }
        return 0;
    }
}

const u16 Rolling_Lv_Data[2][9] = {
    { 4, 1, 8, 2, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
    { 4, 2, 8, 1, 4, 2, 8, 1, 0xFFFF },
};

/** @brief  */
s32 Command_Type_06(PLW* wk, s16 Power_Level, u16 Tech_Number, s16 Ex_Shot) {
    s16 xx;

    xx = 0;

    if (Tech_Number & 0x8000) {
        if ((g_state.My_char[wk->wu.id] == 6) && (g_state.Super_Arts[wk->wu.id] == 0)) {
            xx = 1;
        }
    }
    g_state.Lever_Buff[wk->wu.id] = Rolling_Lv_Data[xx][g_state.CP_Index[wk->wu.id][2]];
    g_state.CP_Index[wk->wu.id][2]++;

    if ((Rolling_Lv_Data[xx][g_state.CP_Index[wk->wu.id][2]]) == 0xFFFF) {
        g_state.Lever_Buff[wk->wu.id] |= renbanshot_conpaneshot(Tech_Address[wk->wu.id], Power_Level);
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Command_Type_01(PLW* wk, s16 Power_Level, s16 Ex_Shot) {
    switch (g_state.CP_Index[wk->wu.id][2]) {
    case 0:
        g_state.CP_Index[wk->wu.id][2]++;
        g_state.Timer_01[wk->wu.id] = Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id] + 1] + 2;
        g_state.Lever_Pool[wk->wu.id] = Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id] + 3];
        Setup_Command_01(wk);

        if (wk->wu.active_move) {
            if (g_state.Lever_Pool[wk->wu.id] & 0xC) {
                g_state.Lever_Pool[wk->wu.id] ^= 0xC;
            }
        }
        /* fallthrough */

    case 1:
        if (wk->permited_koa & 2) {
            g_state.CP_Index[wk->wu.id][2]++;
        } else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
            g_state.Timer_00[wk->wu.id]++;
            break;
        }

    default:
        if (++g_state.Timer_00[wk->wu.id] < g_state.Timer_01[wk->wu.id]) {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
        } else {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
            g_state.Tech_Index[wk->wu.id] += 4;
            if (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]] == 0x1C) {
                return 1;
            }
        }
        break;
    }
    return 0;
}

/** @brief  */
void Setup_Command_01(PLW* wk) {
    switch (g_state.Lever_Pool[wk->wu.id]) {
    case 2:
        g_state.Timer_00[wk->wu.id] = g_state.Lever_Store[wk->wu.id][0];
        break;
    default:
        g_state.Timer_00[wk->wu.id] = g_state.Lever_Store[wk->wu.id][2];
        break;
    }
}

/** @brief  */
void Check_Store_Lever(PLW* wk, u16 Tech_Number, s16 Next_Action, s16 Next_Menu) {
    s16 time;
    u16 lever;

    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
    if (g_state.cmd_sel[wk->wu.id]) {
        Tech_Address[wk->wu.id] = player_CMD[wk->player_number][Tech_Number & 0xFF];
    } else {
        Tech_Address[wk->wu.id] = player_cmd[wk->player_number][Tech_Number & 0xFF];
    }
    time = Tech_Address[wk->wu.id][13] + 2;
    lever = Tech_Address[wk->wu.id][15];

    if (Check_Store_Direction(wk, lever, time) != 0) {
        g_state.CP_Index[wk->wu.id][0]++;
    } else {
        Next_Another_Menu(wk, Next_Action, Next_Menu);
    }
}

/** @brief  */
s32 Check_Store_Direction(PLW* wk, u16 lever, s16 time) {
    if (wk->wu.active_move) {
        if (lever & (SWK_LEFT | SWK_RIGHT)) {
            lever ^= (SWK_LEFT | SWK_RIGHT);
        }
    }

    switch (lever) {
    case SWK_DOWN:
        if (time <= g_state.Lever_Store[wk->wu.id][0]) {
            return 1;
        }

        break;

    case SWK_RIGHT:
        if (time <= g_state.Lever_Store[wk->wu.id][1]) {
            return 1;
        }

        break;

    case SWK_LEFT:
        if (time <= g_state.Lever_Store[wk->wu.id][2]) {
            return 1;
        }

        break;
    }

    return 0;
}

/** @brief  */
s32 Select_Combo_Speed(PLW* wk) {
    s8 xx;
    s8 zz;

    xx = (u8)random_32_com();

    Lv = Setup_Lv18(8);
    Lv += g_state.CC_Value[0];

    if (g_state.Break_Into_CPU == 2) {
        Lv = 0x13;
    }

    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 2;
    }

    Lv = emLevelRemake(Lv, 0x14, 2);

    if ((g_state.Break_Into_CPU == 1) || (g_state.Break_Into_CPU == 2)) {
        return zz = Combo_Speed_Unit_Data[17][Lv][xx];
    }
    return zz = Combo_Speed_Unit_Data[wk->player_number][Lv][xx];
}

/** @brief  */
s32 Select_Reflection_Time(PLW* wk) {
    s8 Lv;
    s8 xx;
    s8 zz;

    xx = (u8)random_32_com();
    Lv = Setup_Lv18(CurrentSave()->Difficulty + 0);
    Lv += g_state.CC_Value[0];
    if (g_state.Break_Into_CPU == 2) {
        Lv = 0x13;
    }
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 2;
    }

    Lv = emLevelRemake(Lv, 0x14, 2);

    time_check[time_check_ix] = xx;
    time_check_ix++;
    time_check_ix &= 3;

    if ((g_state.Break_Into_CPU == 1) || (g_state.Break_Into_CPU == 2)) {
        return zz = Reflection_Speed_Unit_Data[17][Lv][xx];
    }
    return zz = Reflection_Speed_Unit_Data[g_state.My_char[wk->wu.id]][Lv][xx];
}

/** @brief  */
s32 Setup_Lv04(s16 xx) {
    s16 i;
    s16* zz;

    zz = (s16*)&Level_04_Data[xx];

    for (i = 0; i < 3; i++) {
        if (g_state.Control_Time <= zz[i]) {
            return i;
        }
    }
    return 3;
}

/** @brief  */
s32 Setup_Lv08(s16 xx) {
    s16 i;
    s16* zz;

    zz = (s16*)&Level_08_Data[xx];

    for (i = 0; i < 7; i++) {
        if (g_state.Control_Time <= zz[i]) {
            break;
        }
    }
    return i;
}

/** @brief  */
s32 Setup_Lv10(s16 xx) {
    s16 i;
    s16* zz;

    zz = (s16*)&Level_10_Data[xx];

    for (i = 0; i < 9; i++) {
        if (g_state.Control_Time <= zz[i]) {
            break;
        }
    }
    return i;
}

/** @brief  */
s32 Setup_Lv18(s16 xx) {
    s16 i;
    s16* zz;

    zz = (s16*)&Level_18_Data[xx];

    for (i = 0; i < 17; i++) {
        if (g_state.Control_Time <= zz[i]) {
            break;
        }
    }
    return i;
}

/** @brief  */
s32 Setup_VS_Catch_Data(PLW* wk) {
    Lv = Setup_Lv08(0);
    if (g_state.Break_Into_CPU == 2) {
        Lv = 7;
    }
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 2;
    }
    return VS_Catch_Data[emLevelRemake(Lv, 8, 0)];
}

/** @brief  */
s32 Setup_LP_Data(PLW* wk) {
    Lv = Setup_Lv08(0);
    if (g_state.Break_Into_CPU == 2) {
        Lv = 7;
    }
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 2;
    }
    return LOOK_POSITION_Data[emLevelRemake(Lv, 8, 0)][random_32_com()];
}

/** @brief  */
s32 Setup_WT_Data(PLW* wk) {
    Lv = Setup_Lv04(0);
    if (g_state.Break_Into_CPU == 2) {
        Lv = 3;
    }
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 2;
    }
    return Wait_Time_Data[emLevelRemake(Lv, 4, 0)][random_16_com() & 7];
}

/** @brief  */
void Ck_Distance(PLW* wk) {
    g_state.PL_Distance[wk->wu.id] = ((WORK*)wk->wu.target_adrs)->xyz[0].disp.pos - wk->wu.xyz[0].disp.pos;
    if (g_state.PL_Distance[wk->wu.id] < 0) {
        g_state.PL_Distance[wk->wu.id] = g_state.PL_Distance[wk->wu.id] * -1;
    }
}

/** @brief  */
s32 Ck_Distance_Height(PLW* wk) {
    s16 xx;

    xx = ((WORK*)wk->wu.target_adrs)->xyz[1].disp.pos - wk->wu.xyz[1].disp.pos;
    if (xx < 0) {
        xx = xx * -1;
    }
    return xx;
}

/** @brief  */
s32 Ck_Area(PLW* wk) {
    s16 i;

    for (i = 0; i < 3; i++) {
        if (g_state.PL_Distance[wk->wu.id] <= g_state.Separate_Area[wk->wu.id][i]) {
            return i;
        }
    }
    return 3;
}

/** @brief  */
s32 Ck_Area_Shell(PLW* wk) {
    s16 i;

    for (i = 0; i < 3; i++) {
        if (g_state.PL_Distance[wk->wu.id] <= g_state.Shell_Separate_Area[wk->wu.id][i]) {
            return i;
        }
    }

    return 3;
}

/** @brief  */
void Ck_Distance_Lv(PLW* wk) {
    g_state.PL_Distance[wk->wu.id] = ((WORK*)wk->wu.target_adrs)->xyz[0].disp.pos - wk->wu.xyz[0].disp.pos;

    if (g_state.PL_Distance[wk->wu.id] > 0) {
        g_state.Lever_Buff[wk->wu.id] = 8;
    } else {
        g_state.Lever_Buff[wk->wu.id] = 4;
        g_state.PL_Distance[wk->wu.id] = g_state.PL_Distance[wk->wu.id] * -1;
    }
}

/** @brief  */
void Check_Jump_Distance_Level(PLW* wk) {
    g_state.PL_Distance[wk->wu.id] = ((WORK*)wk->wu.target_adrs)->xyz[0].disp.pos - wk->wu.xyz[0].disp.pos;

    if (g_state.PL_Distance[wk->wu.id] > 0) {
        g_state.Lever_Pool[wk->wu.id] = 9;
    } else {
        g_state.Lever_Pool[wk->wu.id] = 5;
        g_state.PL_Distance[wk->wu.id] = g_state.PL_Distance[wk->wu.id] * -1;
    }
}

/** @brief  */
void Next_End(PLW* wk) {
    if (Check_Guard(wk) != 0) {
        return;
    }

    g_state.CP_Index[wk->wu.id][0] = 0xFF;
    g_state.CP_Index[wk->wu.id][1] = 0;
    g_state.CP_Index[wk->wu.id][2] = 0;
    g_state.CP_Index[wk->wu.id][3] = 0;
}

/** @brief  */
void Next_Another_Menu(PLW* wk, s16 Next_Action, u16 Next_Menu) {
    if (Next_Action != 1) {
        g_state.CP_No[wk->wu.id][0] = Next_Action;
        g_state.Pattern_Index[wk->wu.id] = Next_Menu;
        g_state.CP_Index[wk->wu.id][0] = 0;
        g_state.CP_Index[wk->wu.id][1] = 0;
        g_state.CP_Index[wk->wu.id][2] = 0;
        g_state.CP_Index[wk->wu.id][3] = 0;
    } else {
        Next_Be_Free(wk);
    }
}

/** @brief  */
void Reaction_Sub(PLW* wk, s16 Reaction, s16 Power_Level) {
    switch (Reaction & 0x7F) {
    case 9:
        if (g_state.Stock_Hit_Flag[wk->wu.id]) {
            Reaction_Exit_Sub(wk);
        } else if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
            Next_End(wk);
        }
        break;

    case 10:
        if ((g_state.Stock_Hit_Flag[wk->wu.id] >> 2) != 0) {
            Next_End(wk);
            break;
        }
        if (g_state.Stock_Hit_Flag[wk->wu.id]) {
            Reaction_Exit_Sub(wk);
        } else {
            if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
                Reaction_Exit_Sub(wk);
            }
        }
        break;

    case 11:
        if (g_state.plw[wk->wu.id].caution_flag) {
            break;
        }
        if (g_state.plw[wk->wu.id].is_throwing) {
            break;
        }

        if ((g_state.Stock_Hit_Flag[wk->wu.id] >> 2) != 0) {
            Next_End(wk);
            break;
        }
        if (g_state.Stock_Hit_Flag[wk->wu.id]) {
            Reaction_Exit_Sub(wk);
        } else if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
            Next_End(wk);
        }
        break;

    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        if ((g_state.Stock_Hit_Flag[wk->wu.id] >> 2) != 0) {
            Setup_Follow(wk, Reaction & 0xFFF);
            break;
        }
        if (g_state.Stock_Hit_Flag[wk->wu.id]) {
            Reaction_Exit_Sub(wk);
        } else if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
            Setup_Follow(wk, Reaction & 0xFFF);
        }
        break;

    case 12:
        Reaction_Exit_Sub(wk);
        g_state.Counter_Attack[wk->wu.id] = 1;
        break;

    case 14:
        Reaction_Exit_Sub(wk);
        g_state.Counter_Attack[wk->wu.id] = 1;
        break;

    case 13:

        if ((g_state.Stock_Hit_Flag[wk->wu.id] >> 2) != 0) {
            Next_End(wk);
            break;
        }
        if (wk->permited_koa & 0x10) {
            if (Check_Target_Combo_Attack(wk, Reaction, Power_Level) != 0) {
                break;
            }
        }
        g_state.Last_Eftype[wk->wu.id] = -1;

        if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
            if (g_state.Stock_Hit_Flag[wk->wu.id]) {
                if ((g_state.CP_No[wk->wu.id][0] == 6) && (g_state.Pattern_Index[wk->wu.id] == 0)) {
                    g_state.CP_No[wk->wu.id][0] = g_state.Return_CP_No[wk->wu.id];
                    g_state.CP_Index[wk->wu.id][0] = g_state.Return_CP_Index[wk->wu.id];
                    g_state.CP_Index[wk->wu.id][1] = 0;
                    g_state.CP_Index[wk->wu.id][2] = 0;
                    g_state.CP_Index[wk->wu.id][3] = 0;
                    g_state.Pattern_Index[wk->wu.id] = g_state.Return_Pattern_Index[wk->wu.id];
                } else {
                    Reaction_Exit_Sub(wk);
                }
            } else {
                Next_End(wk);
            }
        }
        break;

    default:

        if (g_state.plw[wk->wu.id].caution_flag) {
            break;
        }
        if (g_state.plw[wk->wu.id].is_throwing) {
            break;
        }

        if (g_state.Stock_Hit_Flag[wk->wu.id]) {
            Reaction_Exit_Sub(wk);
        }

        else if ((wk->wu.routine_no[1] != 4) || (wk->wu.cg_type == 0x40)) {
            Reaction_Exit_Sub(wk);
        }

        break;
    }
}

/** @brief  */
s32 Check_Target_Combo_Attack(PLW* wk, s16 Reaction, s16 Power_Level) {
    if (wk->wu.cg_tc_state == g_state.Last_Eftype[wk->wu.id]) {
        return 0;
    }

    if (wk->permited_koa & 0x10) {
        g_state.Last_Eftype[wk->wu.id] = wk->wu.cg_tc_state;

        g_state.M_Lv[wk->wu.id] = Get_Target_Combo_Data(wk);

        if (g_state.Pattern_Index[wk->wu.id] != 0) {
            g_state.Return_CP_No[wk->wu.id] = g_state.CP_No[wk->wu.id][0];
            g_state.Return_CP_Index[wk->wu.id] = g_state.CP_Index[wk->wu.id][0] + 1;
            g_state.CP_Index[wk->wu.id][1] = 0;
            g_state.CP_Index[wk->wu.id][2] = 0;
            g_state.CP_Index[wk->wu.id][3] = 0;
            g_state.Return_Pattern_Index[wk->wu.id] = g_state.Pattern_Index[wk->wu.id];
        }
        g_state.CP_No[wk->wu.id][0] = 6;
        g_state.CP_Index[wk->wu.id][0] = 0;
        g_state.CP_Index[wk->wu.id][1] = 0;
        g_state.CP_Index[wk->wu.id][2] = 0;
        g_state.CP_Index[wk->wu.id][3] = 0;
        g_state.Pattern_Index[wk->wu.id] = 0;
        return 1;
    }
    return 0;
}

/** @brief  */
s32 Get_Target_Combo_Data(PLW* wk) {
    u16 lever;
    u16 shot;

    lever = get_tc_input_dir(wk->wu.cg_tc_state);
    shot = get_tc_input_button(wk->wu.cg_tc_state);

    if (wk->wu.rl_flag) {
        lever ^= 0xC;
    }

    return shot | lever;
}

/** @brief  */
void Reaction_Exit_Sub(PLW* wk) {
    g_state.CP_Index[wk->wu.id][0]++;
    g_state.CP_Index[wk->wu.id][1] = 0;
    g_state.CP_Index[wk->wu.id][2] = 0;
    g_state.CP_Index[wk->wu.id][3] = 0;

    g_state.Flip_Flag[wk->wu.id] = 0;
    g_state.Counter_Attack[wk->wu.id] = 0;
    g_state.Limited_Flag[wk->wu.id] = 0;
    g_state.Before_Jump[wk->wu.id] = 0;
    if (g_state.CP_No[wk->wu.id][0] != 6) {
        g_state.Passive_Flag[wk->wu.id] = 0;
    }
}

/** @brief  */
void Check_First_Menu(PLW* wk) {
    if (g_state.CP_Index[wk->wu.id][0] == 0) {
        g_state.Combo_Speed[wk->wu.id] = 1;
    } else {
        g_state.Combo_Speed[wk->wu.id] = Select_Combo_Speed(wk);
    }
}

/** @brief  */
void Select_Active(PLW* wk) {
    s16 pl_id;

    Lv = Setup_Lv08(0);
    if (g_state.Break_Into_CPU == 2) {
        Lv = 7;
    }
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 2;
    }

    Lv = emLevelRemake(Lv, 8, 0);

    Rnd = (u8)random_32_ex_com();

    if (Check_SA_Active(wk, &pl_id) != 0) {
        Lv = Setup_Lv04(0);
        if (g_state.Break_Into_CPU == 2) {
            Lv = 3;
        }
        if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
            Lv = 2;
        }

        Lv = emLevelRemake(Lv, 4, 0);

        switch (g_state.Area_Number[wk->wu.id]) {
        case 0:
            g_state.Pattern_Index[wk->wu.id] = SA_Active_A_Unit_Data[pl_id - 1][Lv][Rnd];
            break;
        case 1:
            g_state.Pattern_Index[wk->wu.id] = SA_Active_B_Unit_Data[pl_id - 1][Lv][Rnd];
            break;
        case 2:
            g_state.Pattern_Index[wk->wu.id] = SA_Active_C_Unit_Data[pl_id - 1][Lv][Rnd];
            break;
        default:
            g_state.Pattern_Index[wk->wu.id] = SA_Active_D_Unit_Data[pl_id - 1][Lv][Rnd];
            break;
        }
    } else {
        switch (g_state.Area_Number[wk->wu.id]) {
        case 0:
            g_state.Pattern_Index[wk->wu.id] = Active_A_Unit_Data[wk->player_number][Lv][Rnd];
            break;

        case 1:
            g_state.Pattern_Index[wk->wu.id] = Active_B_Unit_Data[wk->player_number][Lv][Rnd];
            break;

        case 2:
            g_state.Pattern_Index[wk->wu.id] = Active_C_Unit_Data[wk->player_number][Lv][Rnd];
            break;

        default:
            g_state.Pattern_Index[wk->wu.id] = Active_D_Unit_Data[wk->player_number][Lv][Rnd];
            break;
        }
    }

    if (Debug_w[DEBUG_ACTIVE_NO]) {
        g_state.Pattern_Index[wk->wu.id] = (u16)Debug_w[DEBUG_ACTIVE_NO] - 1;
    }
}

/** @brief  */
s32 Check_SA_Active(PLW* wk, s16* pl_id) {
    if (wk->sa->ok != -1) {
        return 0;
    }
    if (g_state.My_char[wk->wu.id] == 9) {
        if (g_state.plw[wk->wu.id].sa->kind_of_arts == 0) {
            return *pl_id = 3;
        }
        return *pl_id = 2;
    }
    if ((g_state.My_char[wk->wu.id] == 3) && (g_state.plw[wk->wu.id].sa->kind_of_arts == 2)) {
        return *pl_id = 1;
    }
    if ((g_state.My_char[wk->wu.id] == 0xA) && (g_state.plw[wk->wu.id].sa->kind_of_arts == 2)) {
        return *pl_id = 1;
    }
    if ((g_state.My_char[wk->wu.id] == 0x11) && (g_state.plw[wk->wu.id].sa->kind_of_arts == 2)) {
        return *pl_id = 4;
    }
    return 0;
}

/** @brief  */
void Setup_Follow(PLW* wk, s16 Follow_Type) {
    g_state.CP_No[wk->wu.id][0] = 3;
    g_state.CP_No[wk->wu.id][1] = Follow_Type;

    if (wk->wu.hf.hit.player == 0) {
        g_state.CP_No[wk->wu.id][2] = 0;
    } else {
        g_state.CP_No[wk->wu.id][2] = 1;
    }

    g_state.CP_No[wk->wu.id][3] = 0;
    g_state.Timer_00[wk->wu.id] = Select_Reflection_Time(wk);
    g_state.Timer_00[wk->wu.id]++;
}

// sdata
typedef const _anon6* const_anon6_p;

static const_anon6_p Follow_Menu_1st_Unit_Data[13] = {
    &COM00_Flollow_1st_Unit_Data, &COM00_Flollow_1st_Unit_Data, &COM02_Flollow_1st_Unit_Data,
    &COM00_Flollow_1st_Unit_Data, &COM00_Flollow_1st_Unit_Data, &COM00_Flollow_1st_Unit_Data,
    &COM00_Flollow_1st_Unit_Data, &COM00_Flollow_1st_Unit_Data, &COM00_Flollow_1st_Unit_Data,
    &COM00_Flollow_1st_Unit_Data, &COM00_Flollow_1st_Unit_Data, &COM00_Flollow_1st_Unit_Data,
    &COM00_Flollow_1st_Unit_Data,
};

typedef const _anon13* const_anon13_p;

static const_anon13_p Follow_Menu_2nd_Unit_Data[13] = {
    &Com00_Follow_Menu, &Com00_Follow_Menu, &Com02_Follow_Menu, &Com00_Follow_Menu, &Com00_Follow_Menu,
    &Com00_Follow_Menu, &Com00_Follow_Menu, &Com00_Follow_Menu, &Com00_Follow_Menu, &Com00_Follow_Menu,
    &Com00_Follow_Menu, &Com00_Follow_Menu, &Com00_Follow_Menu,
};

/** @brief  */
void Decide_Follow_Menu(PLW* wk) {
    s8 xx;
    const _anon6* Menu_Add_Ptr0;
    const _anon13* Menu_Add_Ptr1;

    Menu_Add_Ptr0 = Follow_Menu_1st_Unit_Data[wk->player_number];
    Rnd = (u8)random_32_com();
    xx = Menu_Add_Ptr0->xxxx[g_state.CP_No[wk->wu.id][1]][g_state.CP_No[wk->wu.id][2]][Rnd];

    Menu_Add_Ptr1 = Follow_Menu_2nd_Unit_Data[wk->player_number];
    g_state.Pattern_Index[wk->wu.id] = Menu_Add_Ptr1->zzzz[xx][g_state.Area_Number[wk->wu.id]];
}

/** @brief  */
s32 Select_Passive(PLW* wk) {
    u16 xx;

    if (g_state.VS_Tech[wk->wu.id] == 0xB) {
        g_state.Area_Number[wk->wu.id] = Ck_Area_Shell(wk);
    }
    if (g_state.VS_Tech[wk->wu.id] == 0x1E) {
        g_state.Area_Number[wk->wu.id] = Ck_Area_Shell(wk);
    }

    g_state.Last_Attack_Counter[wk->wu.id] = g_state.Attack_Counter[wk->wu.id];
    g_state.Standing_Timer[wk->wu.id] = 0;
    Devide_Level(g_state.VS_Tech[wk->wu.id] >> 0xC);
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 1;
    }
    g_state.VS_Tech[wk->wu.id] &= 0xFFF;

    Setup_Random(wk);

    switch (g_state.Area_Number[wk->wu.id]) {
    case 0:
        xx = Passive_A_Unit_Data_04[wk->player_number][g_state.VS_Tech[wk->wu.id]][Lv][Rnd];

        if (xx == 0xFF) {
            g_state.Counter_Attack[wk->wu.id] = 0;
            g_state.Passive_Flag[wk->wu.id] = 0;
            g_state.Jump_Pass_Timer[wk->wu.id][0] = 0x78;

            return -1;
        }
        g_state.Pattern_Index[wk->wu.id] = xx;
        g_state.Jump_Pass_Timer[wk->wu.id][0] = 0;
        break;

    case 1:
        xx = Passive_B_Unit_Data_04[wk->player_number][g_state.VS_Tech[wk->wu.id]][Lv][Rnd];

        if (xx == 0xFF) {
            g_state.Counter_Attack[wk->wu.id] = 0;
            g_state.Passive_Flag[wk->wu.id] = 0;
            g_state.Jump_Pass_Timer[wk->wu.id][1] = 0x78;

            return -1;
        }
        g_state.Pattern_Index[wk->wu.id] = xx;
        g_state.Jump_Pass_Timer[wk->wu.id][1] = 0;
        break;

    case 2:
        xx = Passive_C_Unit_Data_04[wk->player_number][g_state.VS_Tech[wk->wu.id]][Lv][Rnd];

        if (xx == 0xFF) {
            g_state.Counter_Attack[wk->wu.id] = 0;
            g_state.Passive_Flag[wk->wu.id] = 0;
            g_state.Jump_Pass_Timer[wk->wu.id][2] = 0x78;

            return -1;
        }
        g_state.Pattern_Index[wk->wu.id] = xx;
        g_state.Jump_Pass_Timer[wk->wu.id][2] = 0;
        break;

    default:
        xx = Passive_D_Unit_Data_04[wk->player_number][g_state.VS_Tech[wk->wu.id]][Lv][Rnd];

        if (xx == 0xFF) {
            g_state.Counter_Attack[wk->wu.id] = 0;
            g_state.Passive_Flag[wk->wu.id] = 0;
            g_state.Jump_Pass_Timer[wk->wu.id][3] = 0x78;

            return -1;
        }
        g_state.Pattern_Index[wk->wu.id] = xx;
        g_state.Jump_Pass_Timer[wk->wu.id][3] = 0;
        break;
    }

    g_state.Passive_Flag[wk->wu.id] = 1;
    g_state.CP_No[wk->wu.id][1] = 0;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;
    g_state.Timer_00[wk->wu.id] = Select_Reflection_Time(wk);

    if (Debug_w[DEBUG_PASSIVE_NO]) {
        g_state.Pattern_Index[wk->wu.id] = (u16)Debug_w[DEBUG_PASSIVE_NO] - 1;
    }

    if ((g_state.VS_Tech[wk->wu.id] == 0x19) || (g_state.VS_Tech[wk->wu.id] == 0x13) ||
        (g_state.Timer_00[wk->wu.id] == 0)) {
        g_state.CP_No[wk->wu.id][0] = 6;
        g_state.CP_Index[wk->wu.id][0] = 0;
        g_state.CP_Index[wk->wu.id][1] = 0;
        g_state.CP_Index[wk->wu.id][2] = 0;
        g_state.CP_Index[wk->wu.id][3] = 0;
    } else {
        g_state.CP_No[wk->wu.id][0] = 5;
    }
    return 1;
}

/** @brief  */
void Devide_Level(s16 xx) {
    switch (xx) {
    case 0:
        Lv = Setup_Lv04(0);
        if (g_state.Break_Into_CPU == 2) {
            Lv = 3;
        }
        break;
    default:
        Lv = Setup_Lv08(2);
        if (g_state.Break_Into_CPU == 2) {
            Lv = 7;
        }
        break;
    }
}

/** @brief  */
void Setup_Random(PLW* wk) {
    if (g_state.VS_Tech[wk->wu.id] == 0x20) {
        Rnd = (u8)random_16_com() & 7;
        g_state.VS_Tech[wk->wu.id] = 0x1C;
    } else {
        Rnd = (u8)random_16_com();
        Rnd = Check_Dramatic(wk, wk->wu.id);
    }
}

/** @brief  */
s32 Check_Dramatic(PLW* wk, s16 PL_id) {
    if (g_state.plw[wk->wu.id].sa->ok) {
        return Rnd | 8;
    }

    if ((g_state.plw[PL_id].wu.vital_new <= 0x30) || (g_state.plw[PL_id ^ 1].wu.vital_new <= 0x30)) {
        return Rnd | 8;
    }
    return Rnd;
}

const s8 PL_Status[0xA] = { 1, 0, 0, 0, 1, 1, 0, 0, 0, 0 };

/** @brief  */
s32 Check_Passive(PLW* wk) {
    WORK* em;

    if ((g_state.Counter_Attack[wk->wu.id] != 0) || (g_state.Pierce_Menu[wk->wu.id] != 0)) {
        return 0;
    }

    em = (WORK*)wk->wu.target_adrs;

    if (Check_Blow_Off(wk, em, 0) != 0) {
        *g_state.CP_No[wk->wu.id] = 0xE;
        g_state.CP_No[wk->wu.id][1] = 0;
        g_state.CP_No[wk->wu.id][2] = 0;
        g_state.CP_No[wk->wu.id][3] = 0;
        return -1;
    }
    if (Check_Thrown(wk, em) != 0) {
        if (Select_Passive(wk) != -1) {
            return 1;
        }
    }

    if (Check_Shell(wk) != 0) {
        return 1;
    }
    if ((g_state.Passive_Flag[wk->wu.id]) || (g_state.Flip_Flag[wk->wu.id])) {
        return Check_Guard(wk);
    }
    if (Check_Lie(wk) == 1) {
        return 1;
    }

    if (PL_Status[em->routine_no[1]] == 0) {
        return Check_Shell(wk);
    }

    g_state.Passive_Mode = 4;

    if (Ck_Passive_Term(wk) != 0) {
        if (Select_Passive(wk) != -1) {
            return 1;
        }
    }

    if (Check_Guard(wk) != 0) {
        return 1;
    }
    if ((g_state.Passive_Flag[wk->wu.id]) || (g_state.Flip_Flag[wk->wu.id])) {
        return 0;
    }

    g_state.Passive_Mode = 0;

    if (Ck_Passive_Term(wk) != 0) {
        return Select_Passive(wk);
    }

    return 0;
}

/** @brief  */
s32 Check_Guard(PLW* wk) {
    WORK* em;
    s16 xx;
    s16 zz;

    em = (WORK*)wk->wu.target_adrs;

    if (g_state.Attack_Flag[wk->wu.id] == 0) {
        return 0;
    }

    if (g_state.Guard_Counter[wk->wu.id] == g_state.Attack_Counter[wk->wu.id]) {
        return 0;
    }

    xx = Hit_Range_Data[em->hit_range];
    xx += g_state.Com_Width_Data[wk->wu.id];

    if (g_state.PL_Distance[wk->wu.id] > xx) {
        return 0;
    }

    Lv = Setup_Lv10(0);
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 2;
    }
    Lv += g_state.CC_Value[0];
    if (g_state.Break_Into_CPU == 2) {
        Lv = 0xA;
    }

    Rnd = random_16_com();

    zz = Setup_EM_Rank_Index(wk);

    Lv = emLevelRemake(Lv, 0xB, 1);

    if (Guard_Data[zz][Lv][Rnd] == 3) {
        g_state.Guard_Counter[wk->wu.id] = g_state.Attack_Counter[wk->wu.id];
        return 0;
    }

    if (Check_Flip_Term(wk, NULL) != 0) {
        Next_Be_Flip(wk, 0);
    } else {
        Next_Be_Guard(wk, em, Guard_Data[zz][Lv][random_16_ex_com()]);
    }

    return 1;
}

/** @brief  */
s32 Check_Makoto(PLW* wk) {
    if (wk->player_number != 0x10) {
        return 0;
    }
    if (wk->sa->ok != -1) {
        return 0;
    }
    if (g_state.plw[wk->wu.id].sa->kind_of_arts == 2) {
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Check_Flip_Term(PLW* wk, WORK* tmw) {
    WORK* em;
    s16 xx;

    if (tmw != NULL) {
        em = tmw;
    } else {
        em = (WORK*)wk->wu.target_adrs;
    }

    if (Check_Flip_Tech(em) == 0) {
        return 0;
    }

    Lv = Setup_Lv08(0);

    if (g_state.Break_Into_CPU == 2) {
        Lv = 7;
    }
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 2;
    }

    Rnd = random_32_com();
    Rnd -= Flip_Term_Correct(wk);

    xx = Setup_EM_Rank_Index(wk);

    if (Rnd >= (Flip_Data[xx][emLevelRemake(Lv, 8, 0)])) {
        return 0;
    }

    return 1;
}

/** @brief  */
s32 Setup_EM_Rank_Index(PLW* wk) {
    if (g_state.EM_Rank != 0) {
        return 0x11;
    }

    return wk->player_number;
}

/** @brief  */
s32 Flip_Term_Correct(PLW* wk) {
    s16 xx = 0;

    if (g_state.plw[wk->wu.id].wu.vital_new < 0x31) {
        xx += 1;
    }
    if ((g_state.PL_Wins[wk->wu.id]) < (g_state.PL_Wins[wk->wu.id ^ 1])) {
        xx += 2;
    }
    if (g_state.Counter_hi < 0xF) {
        xx += 1;
    }
    if (Check_Makoto(wk) != 0) {
        xx += 20;
    }

    return xx;
}

/** @brief  */
void Next_Be_Guard(PLW* wk, WORK* em, s16 Type_Of_Guard) {
    g_state.CP_No[wk->wu.id][0] = 7;
    g_state.CP_No[wk->wu.id][1] = 0;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;
    g_state.Timer_00[wk->wu.id] = 10;

    dash_flag_clear(wk->wu.id);
    g_state.Guard_Type[wk->wu.id] = Type_Of_Guard;
    Check_Guard_Type(wk, em);
}

/** @brief  */
s32 Check_Flip_Tech(WORK* em) {
    s32 rnum = 1;

    switch (CurrentSave()->Difficulty) {
    case 0:
        rnum = 0;
        break;
    default:
        if (em->attack_type & 0xF8) {
            rnum = 0;
        }
        /* fallthrough */
    case 6:
        if (em->attack_type == 0) {
            rnum = 0;
        }
        if (em->attack_type == 1) {
            rnum = 0;
        }
        break;
    case 7:
        break;
    }

    return rnum;
}

/** @brief  */
void Next_Be_Flip(PLW* wk, s16 xx) {
    WORK* em;

    em = (WORK*)wk->wu.target_adrs;

    g_state.CP_No[wk->wu.id][0] = 0xC;
    g_state.CP_No[wk->wu.id][1] = 0;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;
    g_state.Timer_00[wk->wu.id] = 9;

    g_state.Flip_Counter[wk->wu.id] = 0;

    if (xx) {
        if (xx == 8) {
            SetShellFlipLever(wk);
        } else {
            if ((em->pat_status == 0x21) || (em->pat_status == 0x20)) {
                g_state.Lever_Buff[wk->wu.id] = 2;
            } else {
                g_state.Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 0);
            }
        }
        g_state.CP_No[wk->wu.id][2] = 1;

        g_state.Timer_01[wk->wu.id] = xx;
    } else {
        Check_Flip_GO(wk, 0);
    }
}

/** @brief  */
s32 Check_Diagonal_Shell(PLW* wk) {
    WORK_Other* tmw;
    WORK* em;
    s16 i;

    Lv = Setup_Lv08(0);
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 2;
    }

    Rnd = random_16_com();
    Lv += *g_state.CC_Value;

    if (g_state.Break_Into_CPU == 2) {
        Lv = 7;
    }
    if (Rnd > VS_Diagonal_Shell_Data[emLevelRemake(Lv, 8, 0)]) {
        return 0;
    }

    em = (WORK*)wk->wu.target_adrs;

    for (i = 0; i < 8; i++) {
        if ((get_vs_shell_adrs(em, em->id, i, &tmw) == 0) && (get_vs_shell_adrs(&wk->wu, em->id, i, &tmw) == 0)) {
            return 0;
        }

        if (tmw->wu.routine_no[1] == 2) {
            continue;
        }
        if (wk->wu.active_move == tmw->wu.rl_flag) {
            continue;
        }

        if (Check_Behind(wk, tmw) != 0) {
            continue;
        }

        if (tmw->wu.charset_id == 2) {
            continue;
        }
        if (Check_Ignore_Shell2(tmw) != 0) {
            return 1;
        }
    }
    return 0;
}

/** @brief  */
s32 Check_Ignore_Shell2(WORK_Other* tmw) {
    if (tmw->wu.type == 0xDE) {
        return 1;
    }
    if ((tmw->wu.type >= 0x24) && (tmw->wu.type < 0x28)) {
        return 1;
    }
    if ((tmw->wu.type >= 0xD) && (tmw->wu.type < 0x10)) {
        return 1;
    }
    if ((tmw->wu.type == 0x54) || (tmw->wu.type == 0x55)) {
        return 1;
    }
    if ((tmw->wu.type >= 0x4D) && (tmw->wu.type < 0x51)) {
        return 1;
    }
    if ((tmw->wu.type >= 0x7A) && (tmw->wu.type < 0x7F)) {
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Check_Shell(PLW* wk) {
    WORK_Other* tmw;
    WORK* em;
    s16 i;
    s16 xx;

    if (g_state.Shell_Ignore_Timer[wk->wu.id]) {
        g_state.Shell_Ignore_Timer[wk->wu.id]--;
        return 0;
    }
    if (g_state.CP_No[wk->wu.id][0] == 8) {
        return 0;
    }

    em = (WORK*)wk->wu.target_adrs;

    for (i = 0; i < 8; i++) {
        if ((get_vs_shell_adrs(em, em->id, i, &tmw) == 0) && (get_vs_shell_adrs(&wk->wu, em->id, i, &tmw) == 0)) {
            return 0;
        }

        if (tmw->wu.routine_no[1] == 2) {
            continue;
        }
        if (wk->wu.active_move == tmw->wu.rl_flag) {
            continue;
        }
        if (tmw->wu.routine_no[0] != 1) {
            continue;
        }
        if (Check_Behind(wk, tmw) == 0) {
            if (tmw->wu.charset_id == 2) {
                continue;
            }
            if (Check_Ignore_Shell(tmw) == 0) {
                xx = Compute_Hit_Time(wk, tmw);

                if (Decide_Shell_Guard(wk, tmw) != 0) {
                    return 0;
                }

                g_state.CP_No[wk->wu.id][0] = 8;
                g_state.CP_No[wk->wu.id][1] = 0;
                g_state.CP_No[wk->wu.id][2] = 0;
                g_state.CP_No[wk->wu.id][3] = 0;

                g_state.CP_Index[wk->wu.id][0] = 0;
                g_state.CP_Index[wk->wu.id][1] = 0;
                g_state.CP_Index[wk->wu.id][2] = 0;
                g_state.CP_Index[wk->wu.id][3] = 0;

                Shell_Address[wk->wu.id] = tmw;

                Guard_or_Jump_VS_Shell(wk, tmw, xx);

                return 1;
            }
        }
    }

    return 0;
}

/** @brief  */
s32 Check_Shell_Another_in_Flip(PLW* wk) {
    WORK_Other* tmw;
    WORK* em;
    s32 i;
    s32 xx = 0;

    em = (WORK*)wk->wu.target_adrs;

    for (i = 0; i < 8; i++) {
        if ((get_vs_shell_adrs(em, em->id, i, &tmw) == 0) && (get_vs_shell_adrs(&wk->wu, em->id, i, &tmw) == 0)) {
            return 0;
        }

        if (tmw->wu.routine_no[1] == 2) {
            continue;
        }
        if (wk->wu.active_move == tmw->wu.rl_flag) {
            continue;
        }
        if (tmw->wu.routine_no[0] != 1) {
            continue;
        }
        if (Check_Behind(wk, tmw) != 0) {
            continue;
        }

        if (tmw->wu.charset_id == 2) {
            continue;
        }
        if (Check_Ignore_Shell(tmw) != 0) {
            continue;
        }

        xx = Compute_Hit_Time(wk, tmw);

        Shell_Address[wk->wu.id] = tmw;

        break;
    }
    return xx;
}

/** @brief  */
s32 Check_Ignore_Shell(WORK_Other* tmw) {
    if (tmw->wu.type == 0xDE) {
        return 1;
    }
    if ((tmw->wu.type >= 0x24) && (tmw->wu.type < 0x28)) {
        return 1;
    }
    if ((tmw->wu.type >= 0xD) && (tmw->wu.type < 0x10)) {
        return 1;
    }
    if ((tmw->wu.type == 0x54) || (tmw->wu.type == 0x55)) {
        return 1;
    }
    if ((tmw->wu.type >= 0x4D) && (tmw->wu.type < 0x51)) {
        return 1;
    }
    if ((tmw->wu.type >= 0x7A) && (tmw->wu.type < 0x7F)) {
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Compute_Hit_Time(PLW* wk, WORK_Other* tmw) {
    s32 lx1;
    s32 divsp;
    s16 x2;

    lx1 = get_att_head_position(&tmw->wu);
    lx1 <<= 16;

    lx1 -= wk->wu.xyz[0].cal;

    if (tmw->wu.mvxy.a[0].sp != 0) {
        divsp = tmw->wu.mvxy.a[0].sp;
    } else {
        divsp = 0x48000;
    }

    x2 = lx1 / divsp;

    if (x2 < 0) {
        x2 = x2 * -1;
    }

    return x2;
}

/** @brief  */
s32 Decide_Shell_Guard(PLW* wk, WORK_Other* tmw) {
    s16 xx;

    Lv = Setup_Lv10(0);

    if (g_state.Break_Into_CPU == 2) {
        Lv = 9;
    }
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 2;
    }

    Rnd = random_32_com();

    xx = Setup_EM_Rank_Index(wk);
    if (Shell_Guard_Data[xx][emLevelRemake(Lv, 0xA, 0)] > Rnd) {
        return 0;
    }
    g_state.Shell_Ignore_Timer[wk->wu.id] = 0x3C;
    return 1;
}

/** @brief  */
void Guard_or_Jump_VS_Shell(PLW* wk, WORK_Other* tmw, s16 xx) {
    if (xx <= Shell_Dodge_Data[0][wk->player_number]) {
        if (Check_Flip_Term(wk, &tmw->wu) != 0) {
            g_state.Pattern_Index[wk->wu.id] = 9;
        } else {
            g_state.Pattern_Index[wk->wu.id] = 0;
        }
    } else {
        switch (CurrentSave()->Difficulty) {
        case 7:
            if (wk->wu.vital_new < 4) {
                if (!(random_32_com() & 0xF)) {
                    g_state.Pattern_Index[wk->wu.id] = 9;
                    break;
                }
            }
            /* fallthrough */
        case 6:
            if (wk->wu.vital_new < 2) {
                if (!(random_32_com() & 7)) {
                    g_state.Pattern_Index[wk->wu.id] = 9;
                    break;
                }
            }
            /* fallthrough */
        default:
            g_state.Pattern_Index[wk->wu.id] =
                Decide_Shell_Reaction(wk, tmw, Shell_Change_Data_For_Reaction[tmw->wu.type]);
            break;
        }
    }

    Setup_Shell_Disposal(wk, tmw);
}

/** @brief  */
void Setup_Shell_Disposal(PLW* wk, WORK_Other* tmw) {
    switch (g_state.Pattern_Index[wk->wu.id]) {
    case 0:
        Next_Be_Shell_Guard(wk, &tmw->wu);
        break;
    case 9:
        g_state.Passive_Flag[wk->wu.id] = 1;
        break;
    case 10:
        g_state.Pattern_Index[wk->wu.id] = Decide_Shell_Reaction(wk, tmw, 0);

        if (g_state.Pattern_Index[wk->wu.id] == 0) {
            Next_Be_Shell_Guard(wk, &tmw->wu);
        }

        break;
    }
}

/** @brief  */
void Next_Be_Shell_Guard(PLW* wk, WORK* tmw) {
    g_state.CP_No[wk->wu.id][0] = 9;
    g_state.CP_No[wk->wu.id][1] = 0;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;
    g_state.Timer_00[wk->wu.id] = 0xA;

    dash_flag_clear(wk->wu.id);
    g_state.Guard_Type[wk->wu.id] = 0;
    Check_Guard_Type(wk, tmw);
}

/** @brief  */
s32 Decide_Shell_Reaction(PLW* wk, WORK_Other* tmw, u16 dir_step) {
    if (dir_step == 0xFF) {
        return 0;
    }

    Lv = Setup_Lv08(0);

    if (g_state.Break_Into_CPU == 2) {
        Lv = 7;
    }
    if ((g_state.Demo_Flag == 0) && (g_state.Weak_PL == wk->wu.id)) {
        Lv = 4;
    }

    Rnd = random_16_com();

    return VS_Shell_Active_Data[wk->player_number][dir_step][emLevelRemake(Lv, 8, 0)][Rnd];
}

/** @brief  */
s32 Ck_Distance_XX(s16 x1, s16 x2) {
    s16 xx;

    xx = x1 - x2;
    if (xx < 0) {
        xx = xx * -1;
    }

    return xx;
}

/** @brief  */
s32 Check_Behind(PLW* wk, WORK_Other* tmw) {
    if (wk->wu.active_move == 0) {
        if (wk->wu.xyz[0].disp.pos < tmw->wu.xyz[0].disp.pos) {
            return 1;
        }
        return 0;
    } else {
        if (wk->wu.xyz[0].disp.pos > tmw->wu.xyz[0].disp.pos) {
            return 1;
        }
        return 0;
    }
}

typedef s32 (*Term_Tbl_t)(PLW* wk, WORK* em);
const Term_Tbl_t Exit_Term_Tbl[9] = { Exit_Term_0000, Exit_Term_0001, Exit_Term_0002, Exit_Term_0003, Exit_Term_0004,
                                      Exit_Term_0005, Exit_Term_0006, Exit_Term_0007, Exit_Term_0008 };

/** @brief  */
void Setup_Lever_LR(PLW* wk, s16 PL_id, s16 Lever) {
    if (Lever == 0) {
        g_state.Lever_LR[PL_id] = 0;
    } else {
        if (Lever & 0x1000) {
            g_state.Lever_LR[PL_id] = 1;
        }

        else if (Lever & 0x2000) {
            g_state.Lever_LR[PL_id] |= 2;
        }

        if (Lever & 0x4000) {
            g_state.Lever_LR[PL_id] |= Setup_Guard_Lever(wk, 0);
        } else {
            if (Lever & 0x8000) {
                g_state.Lever_LR[PL_id] |= Setup_Guard_Lever(wk, 1);
            }
        }
    }
}

/** @brief  */
s32 Check_Exit_Term(PLW* wk, WORK* em, s16 arg_Exit_No) {
    s16 xx;

    if (VS_Jump_Term(wk, em, &xx) != 0) {
        return xx;
    }
    return Exit_Term_Tbl[arg_Exit_No](wk, em);
}

/** @brief  */
s32 VS_Jump_Term(PLW* wk, WORK* em, s16* xx) {
    if (g_state.Attack_Flag[wk->wu.id] == 0) {
        return 0;
    }

    switch (g_state.My_char[em->id]) {
    case 1:
        if (Check_F_Cross_Chop(wk, em, 0xF) != 0) {
            return *xx = 3;
        }
        break;
    case 3:
    case 10:
        if (Check_Special_Technique(wk, em, 0, 0, 0x2b, -1, -1) != 0) {
            return *xx = 2;
        }
        break;
    case 5:
        if (Check_Special_Technique(wk, em, 0, 0, 0x2A, -1, -1) != 0) {
            return *xx = 2;
        }
        if (Check_Limited_Jump_Attack(wk, em, 0x14, 4) != 0) {
            g_state.VS_Tech[wk->wu.id] = 0xF;
            return *xx = 3;
        }
        break;
    case 8:
        if (Check_Limited_Jump_Attack(wk, em, 0x14, 5) != 0) {
            g_state.VS_Tech[wk->wu.id] = 0xF;
            return *xx = 3;
        }
        break;
    case 9:
        if (Check_Special_Technique(wk, em, 0xF, 8, 0x2C, 1, -1) != 0) {
            return *xx = 3;
        }
        break;
    }
    return 0;
}

/** @brief  */
s32 Exit_Term_0000(PLW* wk, WORK* em) {
    switch (g_state.Term_No[wk->wu.id]) {
    case 0:
        g_state.Term_No[wk->wu.id]++;
        g_state.Timer_00[wk->wu.id] = 0x1E;
        break;
    default:
        if (--g_state.Timer_00[wk->wu.id]) {
            break;
        }
        g_state.Timer_00[wk->wu.id] = 1;
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Exit_Term_0001(PLW* wk, WORK* em) {
    return 1;
}

/** @brief  */
s32 Exit_Term_0002(PLW* wk, WORK* em) {
    if (em->routine_no[1] == 2) {
        return 1;
    }
    if (em->id != 0xD) {
        return 1;
    }
    return 0;
}

/** @brief  */
s32 Exit_Term_0003(PLW* wk, WORK* em) {
    switch (g_state.Term_No[wk->wu.id]) {
    case 0:
        g_state.Term_No[wk->wu.id]++;
        g_state.Timer_00[wk->wu.id] = 0xA;
        break;
    default:
        if (--g_state.Timer_00[wk->wu.id]) {
            break;
        }
        if (Check_SHINRYU(wk)) {
            return 0;
        }
        g_state.Timer_00[wk->wu.id] = 1;
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Exit_Term_0004(PLW* wk, WORK* em) {
    switch (g_state.Term_No[wk->wu.id]) {
    case 0:
        g_state.Term_No[wk->wu.id]++;
        g_state.Timer_00[wk->wu.id] = 1;
        break;
    default:
        if (--g_state.Timer_00[wk->wu.id]) {
            break;
        }
        if (Check_SHINRYU(wk)) {
            return 0;
        }
        g_state.Timer_00[wk->wu.id] = 1;
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Exit_Term_0005(PLW* wk, WORK* em) {
    switch (g_state.Term_No[wk->wu.id]) {
    case 0:
        g_state.Term_No[wk->wu.id]++;
        g_state.Timer_00[wk->wu.id] = 5;
        break;
    default:
        if (--g_state.Timer_00[wk->wu.id]) {
            break;
        }
        if (Check_SHINRYU(wk)) {
            return 0;
        }
        g_state.Timer_00[wk->wu.id] = 1;
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Exit_Term_0006(PLW* wk, WORK* em) {
    switch (g_state.Term_No[wk->wu.id]) {
    case 0:
        g_state.Term_No[wk->wu.id]++;
        g_state.Timer_00[wk->wu.id] = 0x3C;
        break;
    default:
        if (--g_state.Timer_00[wk->wu.id]) {
            break;
        }
        if (Check_SHINRYU(wk)) {
            return 0;
        }
        g_state.Timer_00[wk->wu.id] = 1;
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Exit_Term_0007(PLW* wk, WORK* em) {
    switch (g_state.Term_No[wk->wu.id]) {
    case 0:
        g_state.Term_No[wk->wu.id]++;
        g_state.Timer_00[wk->wu.id] = 0x1E;
        /* fallthrough */
    default:
        if (Check_Drop_Term(em, 0x28) != 0) {
            if (g_state.Area_Number[wk->wu.id] >= 3) {
                return 1;
            }
            g_state.VS_Tech[wk->wu.id] = 0x1A;
            return -1;
        }
        if (--g_state.Timer_00[wk->wu.id]) {
            break;
        }
        g_state.Timer_00[wk->wu.id] = 1;
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Exit_Term_0008(PLW* wk, WORK* em) {
    switch (g_state.Term_No[wk->wu.id]) {
    case 0:
        g_state.Term_No[wk->wu.id]++;
        g_state.Timer_00[wk->wu.id] = 0xb4;
        /* fallthough */
    default:
        if ((wk->wu.old_pos[1] == 0) && (wk->wu.xyz[1].disp.pos == 0) && (wk->wu.routine_no[1] != 4)) {
            return 1;
        }
        if (--g_state.Timer_00[wk->wu.id] == 0) {
            return 1;
        }
    }

    return 0;
}

/** @brief  */
s32 Check_Drop_Term(WORK* em, s16 Y) {
    if (em->mvxy.a[1].real.h >= 0) {
        return 0;
    }
    if (em->xyz[1].disp.pos >= Y) {
        return 0;
    }
    return 1;
}

/** @brief  */
s32 Check_SHINRYU(PLW* wk) {
    PLW* em;

    em = (PLW*)wk->wu.target_adrs;

    if (em->sa->ok != -1) {
        return 0;
    }
    if (g_state.My_char[em->wu.id] != 0xB) {
        return 0;
    }
    if (g_state.plw[em->wu.id].sa->kind_of_arts != 1) {
        return 0;
    }
    return 1;
}

const Term_Tbl_t Check_Misc_Cond_Tbl[10] = { Check_Misc_Cond_0000, Check_Misc_Cond_0001, Check_Misc_Cond_0002, Check_Misc_Cond_0003, Check_Misc_Cond_0004,
                                      Check_Misc_Cond_0005, Check_Misc_Cond_0006, Check_Misc_Cond_0007, Check_Misc_Cond_0008, Check_Misc_Cond_0009 };

/** @brief  */
void Check_BOSS(PLW* wk, u32 Next_Action, u16 Next_Menu) {
    if (g_state.Break_Into_CPU == 1) {
        g_state.Disposal_Again[wk->wu.id] = 1;
        g_state.CP_Index[wk->wu.id][0]++;
        g_state.CP_Index[wk->wu.id][1] = 0;
        g_state.CP_Index[wk->wu.id][2] = 0;
        g_state.CP_Index[wk->wu.id][3] = 0;

        g_state.Flip_Flag[wk->wu.id] = 0;
        g_state.Limited_Flag[wk->wu.id] = 0;
    } else {
        g_state.Disposal_Again[wk->wu.id] = 1;
        Next_Another_Menu(wk, Next_Action, Next_Menu);
    }
}

/** @brief  */
void Check_BOSS_EX(PLW* wk, u32 Next_Action, u16 Next_Menu) {
    if (g_state.Break_Into_CPU != 1) {
        g_state.Disposal_Again[wk->wu.id] = 1;
        g_state.CP_Index[wk->wu.id][0]++;
        g_state.CP_Index[wk->wu.id][1] = 0;
        g_state.CP_Index[wk->wu.id][2] = 0;
        g_state.CP_Index[wk->wu.id][3] = 0;

        g_state.Flip_Flag[wk->wu.id] = 0;
        g_state.Limited_Flag[wk->wu.id] = 0;
    } else {
        g_state.Disposal_Again[wk->wu.id] = 1;
        Next_Another_Menu(wk, Next_Action, Next_Menu);
    }
}

/** @brief  */
void Check_Miscellaneous_Conditions(PLW* wk, s16 arg_Exit_No, u32 Next_Action, u16 Next_Menu) {
    s16 xx;
    WORK* em = (WORK*)wk->wu.target_adrs;

    xx = Check_Misc_Cond_Tbl[arg_Exit_No](wk, em);

    if (xx == -1) {
        return;
    }

    if (xx) {
        g_state.Disposal_Again[wk->wu.id] = 1;
        g_state.CP_Index[wk->wu.id][0]++;
        g_state.CP_Index[wk->wu.id][1] = 0;
        g_state.CP_Index[wk->wu.id][2] = 0;
        g_state.CP_Index[wk->wu.id][3] = 0;

        g_state.Flip_Flag[wk->wu.id] = 0;
        g_state.Limited_Flag[wk->wu.id] = 0;
    } else {
        g_state.Disposal_Again[wk->wu.id] = 1;
        Next_Another_Menu(wk, Next_Action, Next_Menu);
    }
}

/** @brief  */
s32 Check_Misc_Cond_0000(PLW* wk, WORK* em) {
    if (Check_VS_Squat(wk, em, 0x1D, 0x21, 0x20) != 0) {
        return 1;
    }
    return 0;
}

/** @brief  */
s32 Check_Misc_Cond_0001(PLW* wk, WORK* em) {
    if (wk->sa->ok != -1) {
        return 1;
    }
    if (g_state.My_char[wk->wu.id] != 9) {
        return 1;
    }
    if (g_state.plw[wk->wu.id].sa->kind_of_arts) {
        return 1;
    }
    return 0;
}

/** @brief  */
s32 Check_Misc_Cond_0002(PLW* wk, WORK* em) {
    if (g_state.plw[wk->wu.id].wu.vital_new < 0x31) {
        return 1;
    }
    return 0;
}

/** @brief  */
s32 Check_Misc_Cond_0003(PLW* wk, WORK* em) {
    if ((em->pat_status != 0x20) && (em->pat_status != 0x21) && (em->pat_status != 2)) {
        return 1;
    }
    return 0;
}

/** @brief  */
s32 Check_Misc_Cond_0004(PLW* wk, WORK* em) {
    if (em->vital_new >= 0x50) {
        return 0;
    }
    if (g_state.plw[wk->wu.id].wu.vital_new < 0x78) {
        return 0;
    }
    return 1;
}

/** @brief  */
s32 Check_Misc_Cond_0005(PLW* wk, WORK* em) {
    if (em->vital_new < g_state.plw[wk->wu.id].wu.vital_new) {
        return 1;
    }
    return 0;
}

/** @brief  */
s32 Check_Misc_Cond_0006(PLW* wk, WORK* em) {
    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        g_state.CP_Index[wk->wu.id][1]++;
        g_state.Timer_00[wk->wu.id] = 0x78;
        /* fallthrough */
    case 1:
        if (--g_state.Timer_00[wk->wu.id] == 0) {
            return 1;
        }
        if (g_state.PL_Distance[wk->wu.id] < 0x70) {
            return 1;
        }
        g_state.Lever_Buff[wk->wu.id] = 0x40;
        return -1;
    }
    return -1;
}

/** @brief  */
s32 Check_Misc_Cond_0007(PLW* wk, WORK* em) {
    if (g_state.plw[wk->wu.id].sa->kind_of_arts == 2) {
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Check_Misc_Cond_0008(PLW* wk, WORK* em) {
    if (g_state.plw[wk->wu.id].sa->kind_of_arts == 1) {
        return 1;
    }

    return 0;
}

/** @brief  */
s32 Check_Misc_Cond_0009(PLW* wk, WORK* em) {
    if (g_state.plw[wk->wu.id].sa->kind_of_arts == 0) {
        return 1;
    }

    return 0;
}

/** @brief  */
s32 emLevelRemake(s32 now, s32 max, s32 exd) {
    s32 RemakeLevelForDifficulty[8] = { -30, -10, 0, 0, 0, 0, 20, 60 };

    now += (max - exd) * RemakeLevelForDifficulty[CurrentSave()->Difficulty] / 100;

    if (now < 0) {
        now = 0;
    }
    if (now >= max) {
        now = max - 1;
    }

    return now;
}

/** @brief  */
s32 emGetMaxBlocking() {
    s32 RapidBlockingTimes[8] = { 2, 2, 3, 3, 3, 4, 6, 10 };

    return RapidBlockingTimes[CurrentSave()->Difficulty];
}
