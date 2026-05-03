/**
 * @file ai_passive_check.c
 * @brief Passive AI logic — per-character reaction checks.
 *
 * Contains the character-specific passive AI decision trees. Each character
 * has 8 VS_ functions (AS/A/BS/B/CS/C/DS/D) that handle anti-special checks
 * and counter responses at different distance zones (close/mid/far/very far).
 * Also includes shared check functions for throws, jumps, dashes, and other
 * common situations.
 *
 * Part of the COM (computer player) AI module.
 */

#include "sf33rd/Source/Game/com/ai_passive_check.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/com/ai_data_tables.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/system/work_sys.h"

static s32 Check_PL_Unit_AS(PlayerEntity* wk);
static s32 Check_PL_Unit_A(PlayerEntity* wk);
static s32 Check_PL_Unit_BS(PlayerEntity* wk);
static s32 Check_PL_Unit_B(PlayerEntity* wk);
static s32 Check_PL_Unit_CS(PlayerEntity* wk);
static s32 Check_PL_Unit_C(PlayerEntity* wk);
static s32 Check_PL_Unit_DS(PlayerEntity* wk);
static s32 Check_PL_Unit_D(PlayerEntity* wk);

s8 PASSIVE_X;

/** @brief Check if passive AI reaction conditions are met. */
s32 Ck_Passive_Term(PlayerEntity* wk) {
    PASSIVE_X = 0;
    Passive_jmp_tbl[((PlayerEntity*)wk->wu.target_adrs)->player_number](wk);
    return PASSIVE_X;
}

/** @brief Ken-specific passive AI pattern dispatcher. */
void KEN_vs(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    switch (g_state.Passive_Mode + g_state.Area_Number[wk->wu.id]) {
    case 0:
        Check_Dash(wk, em, 1);
        break;

    case 1:
        Check_Dash(wk, em, 1);
        break;

    case 2:
        Check_Dash(wk, em, 1);
        break;

    case 3:
        Check_Dash(wk, em, 1);
        break;

    case 4:
        if (g_state.Attack_Flag[wk->wu.id]) {
            if (Check_PL_Unit_A(wk)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 12, 32, 3, 0)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 7, 32, 5, 0)) {
                break;
            }

            if (Check_Special_Technique(wk, em, 15, 0, 33, 1, -1)) {
                break;
            }
        } else {
            if (Check_PL_Unit_AS(wk)) {
                break;
            }

            if (Check_After_Attack(wk, em, 28)) {
                break;
            }

            if (Check_VS_Squat(wk, em, 29, 33, 32)) {
                break;
            }

            if (Check_Stand(wk, em, 4105)) {
                break;
            }
        }

        if (Check_VS_Jump(wk, (PlayerEntity*)em, 16)) {
            break;
        }

        Check_Personal_Action(wk, em);
        break;

    case 5:
        if (g_state.Attack_Flag[wk->wu.id]) {
            if (Check_PL_Unit_B(wk)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 12, 32, 3, 0)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 7, 32, 5, 0)) {
                break;
            }

            if (Check_Special_Technique(wk, em, 15, 0, 33, 1, -1)) {
                break;
            }
        } else {
            if (Check_PL_Unit_BS(wk)) {
                break;
            }

            if (Check_After_Attack(wk, em, 28)) {
                break;
            }

            if (Check_VS_Squat(wk, em, 29, 33, 32)) {
                break;
            }

            if (Check_Stand(wk, em, 4105)) {
                break;
            }
        }

        if (Check_VS_Jump(wk, (PlayerEntity*)em, 32)) {
            break;
        }

        Check_Personal_Action(wk, em);
        break;

    case 6:
        if (g_state.Attack_Flag[wk->wu.id]) {
            if (Check_PL_Unit_C(wk)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 12, 32, 3, 0)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 7, 32, 5, 0)) {
                break;
            }
        } else {
            if (Check_PL_Unit_CS(wk)) {
                break;
            }

            if (Check_After_Attack(wk, em, 28)) {
                break;
            }

            if (Check_Stand(wk, em, 4105)) {
                break;
            }
        }

        if (Check_VS_Jump(wk, (PlayerEntity*)em, 64)) {
            break;
        }

        Check_Personal_Action(wk, em);

        break;

    default:
        if (g_state.Attack_Flag[wk->wu.id]) {
            if (Check_PL_Unit_D(wk)) {
                break;
            }
        } else {
            if (Check_PL_Unit_DS(wk)) {
                break;
            }

            if (Check_Stand(wk, em, 4105)) {
                break;
            }
        }

        Check_Personal_Action(wk, em);
        break;
    }
}

/** @brief Hugo-specific passive AI pattern dispatcher. */
void HUGO_vs(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    switch (g_state.Passive_Mode + g_state.Area_Number[wk->wu.id]) {
    case 0:
        Check_Dash(wk, em, 1);
        break;

    case 1:
        Check_Dash(wk, em, 1);
        break;

    case 2:
        Check_Dash(wk, em, 1);
        break;

    case 3:
        Check_Dash(wk, em, 1);
        break;

    case 4:
        if (g_state.Attack_Flag[wk->wu.id]) {
            if (Check_PL_Unit_A(wk)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 7, 32, 3, 0)) {
                break;
            }

            if (Check_Special_Technique(wk, em, 15, 0, 33, 1, -1)) {
                break;
            }
        } else {
            if (Check_PL_Unit_AS(wk)) {
                break;
            }

            if (Check_After_Attack(wk, em, 28)) {
                break;
            }

            if (Check_VS_Squat(wk, em, 29, 33, 32)) {
                break;
            }

            if (Check_Stand(wk, em, 4105)) {
                break;
            }
        }

        Check_VS_Jump(wk, (PlayerEntity*)em, 16);
        break;

    case 5:
        if (g_state.Attack_Flag[wk->wu.id]) {
            if (Check_PL_Unit_B(wk)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 7, 32, 3, 0)) {
                break;
            }

            if (Check_Special_Technique(wk, em, 15, 0, 33, 1, -1)) {
                break;
            }
        } else {
            if (Check_PL_Unit_BS(wk)) {
                break;
            }

            if (Check_After_Attack(wk, em, 28)) {
                break;
            }

            if (Check_VS_Squat(wk, em, 29, 33, 32)) {
                break;
            }

            if (Check_Stand(wk, em, 4105)) {
                break;
            }
        }

        Check_VS_Jump(wk, (PlayerEntity*)em, 32);
        break;

    case 6:
        if (g_state.Attack_Flag[wk->wu.id]) {
            if (Check_PL_Unit_C(wk)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 7, 32, 3, 0)) {
                break;
            }
        } else {
            if (Check_PL_Unit_CS(wk)) {
                break;
            }

            if (Check_After_Attack(wk, em, 28)) {
                break;
            }

            if (Check_Stand(wk, em, 4105)) {
                break;
            }
        }

        Check_VS_Jump(wk, (PlayerEntity*)em, 64);
        break;

    default:
        if (g_state.Attack_Flag[wk->wu.id]) {
            Check_PL_Unit_D(wk);
            break;
        } else {
            if (Check_PL_Unit_DS(wk)) {
                break;
            }

            Check_Stand(wk, em, 4105);
            break;
        }
    }
}

/** @brief Gill-specific passive AI pattern dispatcher. */
void GILL_vs(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    switch (g_state.Passive_Mode + g_state.Area_Number[wk->wu.id]) {
    case 0:
        Check_Dash(wk, em, 1);
        break;

    case 1:
        Check_Dash(wk, em, 1);
        break;

    case 2:
        Check_Dash(wk, em, 1);
        break;

    case 3:
        Check_Dash(wk, em, 1);
        break;

    case 4:
        if (g_state.Attack_Flag[wk->wu.id]) {
            if (Check_PL_Unit_A(wk)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 12, 32, 3, 0)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 7, 32, 5, 0)) {
                break;
            }

            if (Check_Special_Technique(wk, em, 15, 0, 33, 1, -1)) {
                break;
            }
        } else {
            if (Check_PL_Unit_AS(wk)) {
                break;
            }

            if (Check_After_Attack(wk, em, 28)) {
                break;
            }

            if (Check_VS_Squat(wk, em, 29, 33, 32)) {
                break;
            }

            if (Check_Stand(wk, em, 4105)) {
                break;
            }
        }

        Check_VS_Jump(wk, (PlayerEntity*)em, 16);
        break;

    case 5:
        if (g_state.Attack_Flag[wk->wu.id]) {
            if (Check_PL_Unit_B(wk)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 12, 32, 3, 0)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 7, 32, 5, 0)) {
                break;
            }

            if (Check_Special_Technique(wk, em, 15, 0, 33, 1, -1)) {
                break;
            }
        } else {
            if (Check_PL_Unit_BS(wk)) {
                break;
            }

            if (Check_After_Attack(wk, em, 28)) {
                break;
            }

            if (Check_VS_Squat(wk, em, 29, 33, 32)) {
                break;
            }

            if (Check_Stand(wk, em, 4105)) {
                break;
            }
        }

        Check_VS_Jump(wk, (PlayerEntity*)em, 32);
        break;

    case 6:
        if (g_state.Attack_Flag[wk->wu.id]) {
            if (Check_PL_Unit_C(wk)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 12, 32, 3, 0)) {
                break;
            }

            if (Check_Limited_Attack(wk, em, 7, 32, 5, 0)) {
                break;
            }
        } else {
            if (Check_PL_Unit_CS(wk)) {
                break;
            }

            if (Check_After_Attack(wk, em, 28)) {
                break;
            }

            if (Check_Stand(wk, em, 4105)) {
                break;
            }
        }

        Check_VS_Jump(wk, (PlayerEntity*)em, 64);
        break;

    default:
        if (g_state.Attack_Flag[wk->wu.id]) {
            Check_PL_Unit_D(wk);
            break;
        } else {
            if (Check_PL_Unit_DS(wk)) {
                break;
            }

            if (Check_Stand(wk, em, 4105)) {
                break;
            }
        }

        Check_VS_Squat(wk, em, 7, 33, 32);
        break;
    }
}

/** @brief Check if the opponent is performing a specific special technique. */
s32 Check_Special_Technique(PlayerEntity* wk, State* em, s16 VS_Technique, u8 Kind_of_Tech, u8 SP_Tech_ID, s16 Option,
                            s16 Option2) {
    u8 xx;

    if (Option == 8 && g_state.Attack_Flag[wk->wu.id] != 0) {
        return 0;
    }

    if (VS_Technique != 23 && Check_Attack_Direction(wk, em)) {
        return 0;
    }

    if (g_state.Last_Attack_Counter[wk->wu.id] == g_state.Attack_Counter[wk->wu.id]) {
        return 0;
    }

    xx = em->attack_type & 0xF8;

    if (xx == Kind_of_Tech && (em->sp_tech_id == SP_Tech_ID)) {
        if ((Option2 == -1 || !(Option2 & 8))) {
            if (Option2 == (em->attack_type & 6)) {
                g_state.Last_Attack_Counter[(wk->wu.id)] = g_state.Attack_Counter[(wk->wu.id)];
                return 0;
            }
        } else if (!((Option2 & 6) & (em->attack_type & 6))) {
            return 0;
        }

        if (Option == 8) {
            g_state.Counter_Attack[(wk->wu.id)] = 1;
        }

        if (Option == 1) {
            g_state.Counter_Attack[(wk->wu.id)] = 1;
        }

        g_state.VS_Tech[wk->wu.id] = VS_Technique;

        return PASSIVE_X = 1;
    }

    return 0;
}

/** @brief Check the direction of the opponent's attack for guard. */
s32 Check_Attack_Direction(PlayerEntity* wk, State* em) {
    if (wk->wu.xyz[0].disp.pos < em->xyz[0].disp.pos) {
        if (em->xyz[0].disp.pos > em->old_pos[0]) {
            return 1;
        }
    } else if (em->xyz[0].disp.pos < em->old_pos[0]) {
        return 1;
    }

    return 0;
}

/** @brief Check if opponent is jumping, decide counter response. */
s32 Check_VS_Jump(PlayerEntity* wk, PlayerEntity* em, s16 Height) {
    if (em->wu.routine_no[1] == 1) {
        return 0;
    }

    if (em->wu.sp_tech_id == 33) {
        return 0;
    }

    if (g_state.Jump_Pass_Timer[wk->wu.id][g_state.Area_Number[wk->wu.id]]) {
        g_state.Jump_Pass_Timer[wk->wu.id][g_state.Area_Number[wk->wu.id]]--;
        return 0;
    }

    if ((em->wu.mvxy.a[1].real.h) < 0 && (em->wu.xyz[1].disp.pos <= Height)) {
        return 0;
    }

    if (Check_Specific_Term(wk, &em->wu, 4099, 14, 20, 26)) {
        return g_state.Counter_Attack[wk->wu.id] = 1;
    }

    if (em->wu.xyz[1].disp.pos == 0) {
        return 0;
    }

    if ((em->wu.xyz[1].disp.pos < 32) && (em->wu.mvxy.a[1].real.h > 0)) {
        return 0;
    }

    if (em->close_proximity_flag) {
        g_state.VS_Tech[wk->wu.id] = 18;
        return PASSIVE_X = 1;
    }

    if (Check_Specific_Term(wk, &em->wu, 18, 22, 28, 16)) {
        return 1;
    }

    g_state.VS_Tech[wk->wu.id] = 0;
    return 0;
}

/** @brief Check if the opponent is performing a rolling attack. */
s32 Check_Rolling(PlayerEntity* wk, State* em) {
    if (em->pat_status != 34) {
        return 0;
    }

    if (Check_Attack_Direction(wk, em)) {
        g_state.VS_Tech[wk->wu.id] = 6;
    } else {
        g_state.VS_Tech[wk->wu.id] = 5;
    }

    return PASSIVE_X = 1;
}

/** @brief Check if opponent is performing a personal action. */
s32 Check_Personal_Action(PlayerEntity* wk, State* em) {
    if (em->routine_no[1] != 4) {
        return 0;
    }
    if (em->routine_no[2] != 30) {
        return 0;
    }

    g_state.VS_Tech[wk->wu.id] = 4105;

    return PASSIVE_X = 1;
}

/** @brief Check for a specific technique from the opponent and react. */
s32 Check_Specific_Term(PlayerEntity* wk, State* em, s16 VS_Technique, u8 Status_00, u8 Status_01, u8 Status_02) {
    g_state.VS_Tech[wk->wu.id] = VS_Technique;

    if (em->pat_status == Status_00) {
        return PASSIVE_X = 1;
    }

    if (em->pat_status == Status_01) {
        return PASSIVE_X = 1;
    }

    if (em->pat_status == Status_02) {
        return PASSIVE_X = 1;
    }

    return 0;
}

/** @brief Check if the opponent is dashing, decide counter response. */
s32 Check_Dash(PlayerEntity* wk, State* em, s16 VS_Technique) {
    if ((em->routine_no[1] == 0) && (em->routine_no[2] == 5) && (em->routine_no[3] != 0)) {
        g_state.VS_Tech[wk->wu.id] = VS_Technique;

        return PASSIVE_X = 1;
    }

    return 0;
}

/** @brief Check if opponent is doing a limited (restricted) attack. */
s32 Check_Limited_Attack(PlayerEntity* wk, State* em, s16 VS_Technique, u8 PL_Status, s8 Status_00, s16 Limit_Number) {
    s16 xx;

    if (g_state.Attack_Flag[wk->wu.id] == 0) {
        return 0;
    }

    if (g_state.Last_Attack_Counter[wk->wu.id] == g_state.Attack_Counter[wk->wu.id]) {
        return 0;
    }

    if ((em->pat_status != PL_Status) || em->attack_type != Status_00) {
        return 0;
    }

    xx = (em->graphic_index / em->char_graphic_data_type);

    if ((((PlayerEntity*)em)->player_number == 17) && (VS_Technique == 7)) {
        Limit_Number += 1;
    }

    if ((((PlayerEntity*)em)->player_number == 10) && (VS_Technique == 7)) {
        Limit_Number += 1;
    }

    if ((((PlayerEntity*)em)->player_number == 3) && (VS_Technique == 7)) {
        Limit_Number += 2;
    }

    if (xx > Limit_Number) {
        return 0;
    }

    g_state.VS_Tech[wk->wu.id] = VS_Technique;
    g_state.Limited_Flag[wk->wu.id] = 1;
    g_state.Counter_Attack[wk->wu.id] = 1;

    return PASSIVE_X = 1;
}

/** @brief Check if opponent is doing a limited jump attack. */
s32 Check_Limited_Jump_Attack(PlayerEntity* wk, State* em, u8 PL_Status, s8 Status_00) {
    if ((em->pat_status != PL_Status) || (em->attack_type != Status_00)) {
        return 0;
    }

    return 1;
}

/** @brief Check if opponent is standing, decide AI response. */
s32 Check_Stand(PlayerEntity* wk, State* em, s16 VS_Technique) {
    if (g_state.Attack_Flag[wk->wu.id]) {
        return 0;
    }

    if (em->routine_no[1] != 0) {
        return 0;
    }

    if ((g_state.Standing_Timer[wk->wu.id] += 1) < g_state.Standing_Master_Timer[wk->wu.id]) {
        return 0;
    }

    g_state.Standing_Master_Timer[wk->wu.id] = Setup_Next_Stand_Timer(wk);
    g_state.VS_Tech[wk->wu.id] = VS_Technique;

    return PASSIVE_X = 1;
}

/** @brief Calculate the next standing timer duration. */
s32 Setup_Next_Stand_Timer(PlayerEntity* wk) {
    if (g_state.EM_Rank != 0) {
        return Standing_Time_Data[17][g_state.Area_Number[wk->wu.id]][(random_16_com() & 7)];
    }

    return Standing_Time_Data[wk->player_number][g_state.Area_Number[wk->wu.id]][(random_16_com() & 7)];
}

/** @brief Check if opponent is crouching, decide AI response. */
s32 Check_VS_Squat(PlayerEntity* wk, State* em, s16 VS_Technique, u8 Status_00, u8 Status_01) {
    if (g_state.Attack_Flag[wk->wu.id]) {
        return g_state.Squat_Timer[wk->wu.id] = 0;
    }

    if (em->routine_no[1] != 0) {
        return g_state.Squat_Timer[wk->wu.id] = 0;
    }

    if (em->xyz[1].disp.pos) {
        return g_state.Squat_Timer[wk->wu.id] = 0;
    }

    if (em->pat_status != Status_00 && em->pat_status != Status_01) {
        return g_state.Squat_Timer[wk->wu.id] = 0;
    }

    if ((g_state.Squat_Timer[wk->wu.id] += 1) < g_state.Squat_Master_Timer[wk->wu.id]) {
        return 0;
    }

    g_state.Squat_Master_Timer[wk->wu.id] = Setup_Next_Squat_Timer(wk);
    g_state.VS_Tech[wk->wu.id] = VS_Technique;

    return PASSIVE_X = 1;
}

/** @brief Calculate the next crouching timer duration. */
s32 Setup_Next_Squat_Timer(PlayerEntity* wk) {
    return Squat_Time_Data[Setup_Lv08(0)][(random_16_com() & 7)];
}

/** @brief Check if opponent performed a throw (tech window). */
s32 Check_Thrown(PlayerEntity* wk, State* em) {
    s16 Rnd;
    s16 x;

    if (em->xyz[1].disp.pos) {
        return 0;
    }

    x = Setup_VS_Catch_Data(wk);
    Rnd = random_32_com();

    if (x < Rnd) {
        return 0;
    }

    switch (g_state.Area_Number[wk->wu.id]) {
    case 0:
        if (Check_Catch(wk, em, 25)) {
            return 1;
        }

        break;

    case 1:
        if (Check_Catch(wk, em, 25)) {
            return 1;
        }

        break;

    default:
        break;
    }

    return 0;
}

/** @brief Check if the CPU should attempt a throw (catch). */
s32 Check_Catch(PlayerEntity* wk, State* em, s16 VS_Technique) {
    u16 xx;

    if (g_state.Demo_Flag == 0) {
        return 0;
    }

    if (em->routine_no[1] != 0) {
        return 0;
    }

    if (em->xyz[1].disp.pos) {
        return 0;
    }

    if (wk->wu.id == 0) {
        xx = p2sw_0;
    } else {
        xx = p1sw_0;
    }

    if (wk->wu.active_move) {
        if (!(xx & 4)) {
            return 0;
        }
    } else if (!(xx & 8)) {
        return 0;
    }

    g_state.Counter_Attack[wk->wu.id] = 1;
    g_state.VS_Tech[wk->wu.id] = VS_Technique;

    return PASSIVE_X = 1;
}

/** @brief Check if opponent is lying down (knocked down). */
s32 Check_Lie(PlayerEntity* wk) {
    State* em;
    PlayerEntity* enemy;

    em = (State*)wk->wu.target_adrs;
    enemy = (PlayerEntity*)wk->wu.target_adrs;

    if (Check_Faint(wk, enemy, 2)) {
        return Select_Passive(wk);
    }

    if (Check_Specific_Term(wk, em, 0, 38, 38, 38)) {
        return Select_Passive(wk);
    }

    return 0;
}

/** @brief Check if opponent is stunned (faint/dizzy). */
s32 Check_Faint(PlayerEntity* wk, PlayerEntity* enemy, s16 VS_Technique) {
    g_state.Counter_Attack[wk->wu.id] = 1;
    g_state.VS_Tech[wk->wu.id] = VS_Technique;

    if ((enemy->wu.routine_no[1] == 1) && (enemy->wu.routine_no[2] == 25)) {
        return 1;
    }

    return g_state.Counter_Attack[wk->wu.id] = 0;
}

/** @brief Check if opponent has been blown off (launched). */
s32 Check_Blow_Off(PlayerEntity* wk, State* em, s16 VS_Technique) {
    if (em->routine_no[1] != 1) {
        return 0;
    }

    if (PL_Blow_Off_Data[em->routine_no[2]] == 0) {
        return 0;
    }

    if (em->xyz[1].disp.pos == 0) {
        return 0;
    }

    g_state.VS_Tech[(wk->wu.id)] = VS_Technique;

    return PASSIVE_X = 1;
}

/** @brief Check what opponent did after an attack (recovery window). */
s32 Check_After_Attack(PlayerEntity* wk, State* em, s16 VS_Technique) {
    u8 xx;

    if (g_state.CP_No[wk->wu.id][0] == 7) {
        return 0;
    }

    if (g_state.Last_Attack_Counter[wk->wu.id] == g_state.Attack_Counter[wk->wu.id]) {
        return 0;
    }

    if (em->xyz[1].disp.pos) {
        return 0;
    }

    if (em->routine_no[1] != 4) {
        return 0;
    }

    g_state.Last_Attack_Counter[wk->wu.id] = g_state.Attack_Counter[wk->wu.id];

    if (!(em->attack_type & 32) && !(em->attack_type & 48) && !(em->attack_type & 40) && !(em->attack_type & 56) &&
        !(em->attack_type & 8)) {
        xx = em->attack_type & 6;

        if (xx == 0) {
            return 0;
        }

        if (xx == 2) {
            return 0;
        }
    }

    g_state.VS_Tech[wk->wu.id] = VS_Technique;

    return PASSIVE_X = 1;
}

/** @brief Check for Hugo's Flying Cross Chop approach. */
s32 Check_F_Cross_Chop(PlayerEntity* wk, State* em, s16 VS_Technique) {
    if (g_state.Last_Attack_Counter[wk->wu.id] == g_state.Attack_Counter[wk->wu.id]) {
        return 0;
    }

    if ((em->attack_type) != 4) {
        return 0;
    }

    if ((em->pat_status != 22) && (em->pat_status != 20) && (em->pat_status != 26) && (em->pat_status != 28)) {
        return 0;
    }

    g_state.VS_Tech[wk->wu.id] = VS_Technique;
    g_state.Counter_Attack[wk->wu.id] = 1;

    return PASSIVE_X = 1;
}

static s32 Check_PL_Unit_AS(PlayerEntity* wk) {
    return Passive_AS_tbl[((PlayerEntity*)wk->wu.target_adrs)->player_number](wk);
}

/** @brief Passive AI check (anti-special) vs Gill. */
s32 VS_GILL_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 61, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Alex. */
s32 VS_ALEX_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 24, 23, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 24, 22, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 56, 37, -1, -1)) {
        return 1;
    }

    if (Check_Rolling(wk, em)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Ryu. */
s32 VS_RYU_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 32, 4, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 32, 5, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Yun. */
s32 VS_YUN_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 29, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 36, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 24, 74, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Dudley. */
s32 VS_DUDLEY_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 2, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 13, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 11, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 12, -1, 0)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Necro. */
s32 VS_NECRO_AS(PlayerEntity* wk) {
    return 0;
}

/** @brief Passive AI check (anti-special) vs Hugo. */
s32 VS_HUGO_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 14, 8, 61, -1, -1)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 5, 32767)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 4, 32767)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 19, 24, 58, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 24, 61, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Ibuki. */
s32 VS_IBUKI_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 28, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 25, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Elena. */
s32 VS_ELENA_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 17, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 14, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 15, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 2, 48, 16, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Oro. */
s32 VS_ORO_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 39, -1, 0)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Ken. */
s32 VS_KEN_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 8, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 6, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Sean. */
s32 VS_SEAN_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 32, 8, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Urien. */
s32 VS_URIEN_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 65, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Gouki. */
s32 VS_GOUKI_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 69, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Chun-Li. */
s32 VS_CHUN_LI_AS(PlayerEntity* wk) {
    return 0;
}

/** @brief Passive AI check (anti-special) vs Makoto. */
s32 VS_MAKOTO_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 14, 24, 95, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Q. */
s32 VS_Q_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 25, 24, 88, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI check (anti-special) vs Twelve. */
s32 VS_NO12_AS(PlayerEntity* wk) {
    return 0;
}

/** @brief Passive AI check (anti-special) vs Remy. */
s32 VS_REMY_AS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 101, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    return 0;
}

static s32 Check_PL_Unit_A(PlayerEntity* wk) {
    return Passive_A_tbl[((PlayerEntity*)wk->wu.target_adrs)->player_number](wk);
}

/** @brief Passive AI response (counter) vs Gill. */
s32 VS_GILL_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 63, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 61, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Alex. */
s32 VS_ALEX_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 25, 24, 22, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 24, 115, 1, 0)) {
        return 1;
    }

    if (Check_F_Cross_Chop(wk, em, 15)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 73, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 30, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 40, 72, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Ryu. */
s32 VS_RYU_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 48, 3, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 16, 32, 4, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 32, 5, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Yun. */
s32 VS_YUN_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 20, 8, 52, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 25, 24, 74, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Dudley. */
s32 VS_DUDLEY_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 2, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 18, 8, 75, 1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 12, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Necro. */
s32 VS_NECRO_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 38, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 40, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 17, 24, 24, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 32, 41, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Hugo. */
s32 VS_HUGO_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 19, 24, 58, 1, -1)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 5, 32767)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 4, 32767)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 19, 24, 62, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 19, 56, 55, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 24, 61, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Ibuki. */
s32 VS_IBUKI_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 24, 26, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Elena. */
s32 VS_ELENA_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 8, 8, 18, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 2, 48, 16, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 19, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Oro. */
s32 VS_ORO_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 15, 8, 44, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Ken. */
s32 VS_KEN_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Sean. */
s32 VS_SEAN_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 21, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 32, 1, -1)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 0, 5, 32767)) {
        return 1;
    }

    if (Check_Rolling(wk, em)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Urien. */
s32 VS_URIEN_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 8, 8, 65, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 63, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Gouki. */
s32 VS_GOUKI_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 16, 32, 4, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 13, 64, 47, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Chun-Li. */
s32 VS_CHUN_LI_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 78, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 114, 1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 21, 8, 77, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Makoto. */
s32 VS_MAKOTO_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 25, 24, 95, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Q. */
s32 VS_Q_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 25, 24, 88, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Twelve. */
s32 VS_NO12_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 17, 8, 105, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 22, 8, 107, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Passive AI response (counter) vs Remy. */
s32 VS_REMY_A(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    return 0;
}

static s32 Check_PL_Unit_BS(PlayerEntity* wk) {
    return Passive_BS_tbl[((PlayerEntity*)wk->wu.target_adrs)->player_number](wk);
}

/** @brief Standing passive check (anti-special) vs Gill. */
s32 VS_GILL_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 61, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Alex. */
s32 VS_ALEX_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 24, 23, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 24, 22, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 56, 37, -1, -1)) {
        return 1;
    }

    if (Check_Rolling(wk, em)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Ryu. */
s32 VS_RYU_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 2, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 48, 3, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 16, 32, 4, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 32, 5, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Yun. */
s32 VS_YUN_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 29, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 36, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 24, 74, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Dudley. */
s32 VS_DUDLEY_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 13, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 11, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 12, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Necro. */
s32 VS_NECRO_BS(PlayerEntity* wk) {
    return 0;
}

/** @brief Standing passive check (anti-special) vs Hugo. */
s32 VS_HUGO_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 61, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 61, -1, -1)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 5, 32767)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 4, 32767)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 19, 24, 58, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 24, 61, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Ibuki. */
s32 VS_IBUKI_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 28, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 25, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Elena. */
s32 VS_ELENA_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 17, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 14, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 15, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 2, 48, 16, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Oro. */
s32 VS_ORO_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 39, -1, 0)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Ken. */
s32 VS_KEN_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 2, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 8, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 6, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Sean. */
s32 VS_SEAN_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 32, 8, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Urien. */
s32 VS_URIEN_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 65, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Gouki. */
s32 VS_GOUKI_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 2, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 16, 32, 4, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 69, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Chun-Li. */
s32 VS_CHUN_LI_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 78, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 76, -1, 0)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Makoto. */
s32 VS_MAKOTO_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 8, 8, 92, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 24, 95, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Q. */
s32 VS_Q_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 25, 24, 88, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive check (anti-special) vs Twelve. */
s32 VS_NO12_BS(PlayerEntity* wk) {
    return 0;
}

/** @brief Standing passive check (anti-special) vs Remy. */
s32 VS_REMY_BS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 101, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    return 0;
}

static s32 Check_PL_Unit_B(PlayerEntity* wk) {
    return Passive_B_tbl[((PlayerEntity*)wk->wu.target_adrs)->player_number](wk);
}

/** @brief Standing passive response (counter) vs Gill. */
s32 VS_GILL_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 63, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 61, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Alex. */
s32 VS_ALEX_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 25, 24, 22, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 24, 115, 1, 0)) {
        return 1;
    }

    if (Check_F_Cross_Chop(wk, em, 15)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 73, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 30, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 40, 72, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Ryu. */
s32 VS_RYU_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 2, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 16, 32, 4, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 32, 5, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Yun. */
s32 VS_YUN_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 8, 8, 31, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 20, 8, 52, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 25, 24, 74, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Dudley. */
s32 VS_DUDLEY_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 2, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 18, 8, 75, 1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 12, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Necro. */
s32 VS_NECRO_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 38, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 40, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 17, 24, 24, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 32, 41, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Hugo. */
s32 VS_HUGO_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 2, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 61, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 19, 24, 58, 1, -1)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 5, 32767)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 4, 32767)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 19, 24, 62, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 19, 56, 55, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 24, 61, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Ibuki. */
s32 VS_IBUKI_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 24, 26, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Elena. */
s32 VS_ELENA_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 8, 8, 18, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 19, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 2, 48, 16, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Oro. */
s32 VS_ORO_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 15, 8, 44, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Ken. */
s32 VS_KEN_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 2, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Sean. */
s32 VS_SEAN_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 15, 8, 32, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 21, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 20, 1, -1)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 0, 5, 32767)) {
        return 1;
    }

    if (Check_Rolling(wk, em)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Urien. */
s32 VS_URIEN_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 8, 8, 65, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 63, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Gouki. */
s32 VS_GOUKI_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 2, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 16, 32, 4, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 16, 32, 5, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 13, 64, 47, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Chun-Li. */
s32 VS_CHUN_LI_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 78, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 76, 1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 114, 1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 21, 8, 77, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Makoto. */
s32 VS_MAKOTO_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 8, 8, 92, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 25, 24, 95, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Q. */
s32 VS_Q_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 84, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 21, 8, 87, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 25, 24, 88, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Twelve. */
s32 VS_NO12_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 22, 8, 107, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 17, 8, 105, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Standing passive response (counter) vs Remy. */
s32 VS_REMY_B(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    return 0;
}

static s32 Check_PL_Unit_CS(PlayerEntity* wk) {
    return Passive_CS_tbl[((PlayerEntity*)wk->wu.target_adrs)->player_number](wk);
}

/** @brief Crouching passive check (anti-special) vs Gill. */
s32 VS_GILL_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 65, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Alex. */
s32 VS_ALEX_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 24, 23, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 24, 22, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 56, 37, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Ryu. */
s32 VS_RYU_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 48, 3, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 16, 32, 4, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 32, 5, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Yun. */
s32 VS_YUN_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 29, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 36, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 24, 74, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Dudley. */
s32 VS_DUDLEY_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 2, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 13, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 11, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 12, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Necro. */
s32 VS_NECRO_CS(PlayerEntity* wk) {
    return 0;
}

/** @brief Crouching passive check (anti-special) vs Hugo. */
s32 VS_HUGO_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 61, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 61, -1, -1)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 5, 32767)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 4, 32767)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 19, 24, 58, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 24, 61, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Ibuki. */
s32 VS_IBUKI_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 32, 25, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Elena. */
s32 VS_ELENA_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 17, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 14, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 15, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 2, 48, 16, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Oro. */
s32 VS_ORO_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 39, -1, 0)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Ken. */
s32 VS_KEN_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 8, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 6, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Sean. */
s32 VS_SEAN_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 32, 8, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Urien. */
s32 VS_URIEN_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 65, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Gouki. */
s32 VS_GOUKI_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 1, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 16, 32, 4, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 23, 32, 69, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Chun-Li. */
s32 VS_CHUN_LI_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 78, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 76, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Makoto. */
s32 VS_MAKOTO_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 8, 8, 92, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 24, 95, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive check (anti-special) vs Q. */
s32 VS_Q_CS(PlayerEntity* wk) {
    return 0;
}

/** @brief Crouching passive check (anti-special) vs Twelve. */
s32 VS_NO12_CS(PlayerEntity* wk) {
    return 0;
}

/** @brief Crouching passive check (anti-special) vs Remy. */
s32 VS_REMY_CS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 23, 8, 101, -1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    return 0;
}

static s32 Check_PL_Unit_C(PlayerEntity* wk) {
    return Passive_C_tbl[((PlayerEntity*)wk->wu.target_adrs)->player_number](wk);
}

/** @brief Crouching passive response (counter) vs Gill. */
s32 VS_GILL_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 63, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 61, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Alex. */
s32 VS_ALEX_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 24, 115, 1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 30, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 73, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 40, 72, 1, -1)) {
        return 1;
    }

    if (Check_F_Cross_Chop(wk, em, 15)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Ryu. */
s32 VS_RYU_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 2, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 16, 32, 4, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 32, 5, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Yun. */
s32 VS_YUN_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 8, 8, 31, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 20, 8, 52, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Dudley. */
s32 VS_DUDLEY_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 2, 1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 18, 8, 75, 1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 14, 32, 12, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Necro. */
s32 VS_NECRO_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 38, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 40, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 17, 24, 24, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 32, 41, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Hugo. */
s32 VS_HUGO_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 2, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 61, -1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 19, 24, 58, 1, -1)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 5, 32767)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 32, 4, 32767)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 19, 24, 62, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 19, 56, 55, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 24, 61, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Ibuki. */
s32 VS_IBUKI_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 8, 24, 26, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Elena. */
s32 VS_ELENA_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 8, 8, 18, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 19, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 2, 48, 16, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Oro. */
s32 VS_ORO_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 15, 8, 44, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Ken. */
s32 VS_KEN_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 2, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Sean. */
s32 VS_SEAN_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 15, 8, 32, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 21, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 20, 1, -1)) {
        return 1;
    }

    if (Check_Limited_Attack(wk, em, 24, 0, 5, 32767)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Urien. */
s32 VS_URIEN_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 8, 8, 65, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 64, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 63, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Gouki. */
s32 VS_GOUKI_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 24, 8, 2, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 16, 32, 4, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Chun-Li. */
s32 VS_CHUN_LI_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 76, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 114, 1, 0)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 21, 8, 77, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 11, 8, 78, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Makoto. */
s32 VS_MAKOTO_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 8, 8, 92, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 25, 24, 95, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Q. */
s32 VS_Q_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 24, 8, 84, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 21, 8, 87, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Twelve. */
s32 VS_NO12_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 22, 8, 107, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 17, 8, 105, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Crouching passive response (counter) vs Remy. */
s32 VS_REMY_C(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 8, 0, 1, -1)) {
        return 1;
    }

    return 0;
}

static s32 Check_PL_Unit_DS(PlayerEntity* wk) {
    return Passive_DS_tbl[((PlayerEntity*)wk->wu.target_adrs)->player_number](wk);
}

/** @brief Downed passive check (anti-special) vs Gill. */
s32 VS_GILL_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Alex. */
s32 VS_ALEX_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Ryu. */
s32 VS_RYU_DS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 32, 5, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Downed passive check (anti-special) vs Yun. */
s32 VS_YUN_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Dudley. */
s32 VS_DUDLEY_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Necro. */
s32 VS_NECRO_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Ibuki. */
s32 VS_IBUKI_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Hugo. */
s32 VS_HUGO_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Elena. */
s32 VS_ELENA_DS(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 2, 48, 16, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Downed passive check (anti-special) vs Oro. */
s32 VS_ORO_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Ken. */
s32 VS_KEN_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Sean. */
s32 VS_SEAN_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Urien. */
s32 VS_URIEN_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Gouki. */
s32 VS_GOUKI_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Chun-Li. */
s32 VS_CHUN_LI_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Makoto. */
s32 VS_MAKOTO_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Q. */
s32 VS_Q_DS(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive check (anti-special) vs Twelve. */
s32 VS_NO12_DS(PlayerEntity* wk) {
    PlayerEntity* em = (PlayerEntity*)wk->wu.target_adrs;

    if (Check_VS_Jump(wk, em, 32)) {
        g_state.VS_Tech[wk->wu.id] = 15;
        return 1;
    }

    return 0;
}

/** @brief Downed passive check (anti-special) vs Remy. */
s32 VS_REMY_DS(PlayerEntity* wk) {
    return 0;
}

static s32 Check_PL_Unit_D(PlayerEntity* wk) {
    return Passive_D_tbl[((PlayerEntity*)wk->wu.target_adrs)->player_number](wk);
}

/** @brief Downed passive response (counter) vs Gill. */
s32 VS_GILL_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Alex. */
s32 VS_ALEX_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Ryu. */
s32 VS_RYU_D(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 11, 32, 5, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Downed passive response (counter) vs Yun. */
s32 VS_YUN_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Dudley. */
s32 VS_DUDLEY_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Necro. */
s32 VS_NECRO_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Hugo. */
s32 VS_HUGO_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Ibuki. */
s32 VS_IBUKI_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Elena. */
s32 VS_ELENA_D(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 2, 48, 16, -1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Downed passive response (counter) vs Oro. */
s32 VS_ORO_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Ken. */
s32 VS_KEN_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Sean. */
s32 VS_SEAN_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Urien. */
s32 VS_URIEN_D(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 12, 8, 63, 1, 12)) {
        return 1;
    }

    return 0;
}

/** @brief Downed passive response (counter) vs Gouki. */
s32 VS_GOUKI_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Chun-Li. */
s32 VS_CHUN_LI_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Makoto. */
s32 VS_MAKOTO_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Q. */
s32 VS_Q_D(PlayerEntity* wk) {
    return 0;
}

/** @brief Downed passive response (counter) vs Twelve. */
s32 VS_NO12_D(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Special_Technique(wk, em, 8, 8, 105, 1, -1)) {
        return 1;
    }

    if (Check_Special_Technique(wk, em, 15, 8, 107, 1, -1)) {
        return 1;
    }

    return 0;
}

/** @brief Downed passive response (counter) vs Remy. */
s32 VS_REMY_D(PlayerEntity* wk) {
    return 0;
}

void (*const Passive_jmp_tbl[21])() = { GILL_vs, KEN_vs, KEN_vs, KEN_vs, KEN_vs, KEN_vs, HUGO_vs,
                                        KEN_vs,  KEN_vs, KEN_vs, KEN_vs, KEN_vs, KEN_vs, KEN_vs,
                                        KEN_vs,  KEN_vs, KEN_vs, KEN_vs, KEN_vs, KEN_vs, KEN_vs };

s32 (*const Passive_AS_tbl[20])(PlayerEntity* wk) = { VS_GILL_AS,    VS_ALEX_AS,   VS_RYU_AS,   VS_YUN_AS,   VS_DUDLEY_AS,
                                             VS_NECRO_AS,   VS_HUGO_AS,   VS_IBUKI_AS, VS_ELENA_AS, VS_ORO_AS,
                                             VS_YUN_AS,     VS_KEN_AS,    VS_SEAN_AS,  VS_URIEN_AS, VS_GOUKI_AS,
                                             VS_CHUN_LI_AS, VS_MAKOTO_AS, VS_Q_AS,     VS_NO12_AS,  VS_REMY_AS };

s32 (*const Passive_A_tbl[20])(PlayerEntity* wk) = { VS_GILL_A,    VS_ALEX_A,   VS_RYU_A,   VS_YUN_A,   VS_DUDLEY_A,
                                            VS_NECRO_A,   VS_HUGO_A,   VS_IBUKI_A, VS_ELENA_A, VS_ORO_A,
                                            VS_YUN_A,     VS_KEN_A,    VS_SEAN_A,  VS_URIEN_A, VS_GOUKI_A,
                                            VS_CHUN_LI_A, VS_MAKOTO_A, VS_Q_A,     VS_NO12_A,  VS_REMY_A };

s32 (*const Passive_BS_tbl[20])(PlayerEntity* wk) = { VS_GILL_BS,    VS_ALEX_BS,   VS_RYU_BS,   VS_YUN_BS,   VS_DUDLEY_BS,
                                             VS_NECRO_BS,   VS_HUGO_BS,   VS_IBUKI_BS, VS_ELENA_BS, VS_ORO_BS,
                                             VS_YUN_BS,     VS_KEN_BS,    VS_SEAN_BS,  VS_URIEN_BS, VS_GOUKI_BS,
                                             VS_CHUN_LI_BS, VS_MAKOTO_BS, VS_Q_BS,     VS_NO12_BS,  VS_REMY_BS };

s32 (*const Passive_B_tbl[20])(PlayerEntity* wk) = { VS_GILL_B,    VS_ALEX_B,   VS_RYU_B,   VS_YUN_B,   VS_DUDLEY_B,
                                            VS_NECRO_B,   VS_HUGO_B,   VS_IBUKI_B, VS_ELENA_B, VS_ORO_B,
                                            VS_YUN_B,     VS_KEN_B,    VS_SEAN_B,  VS_URIEN_B, VS_GOUKI_B,
                                            VS_CHUN_LI_B, VS_MAKOTO_B, VS_Q_B,     VS_NO12_B,  VS_REMY_B };

s32 (*const Passive_CS_tbl[20])(PlayerEntity* wk) = { VS_GILL_CS,    VS_ALEX_CS,   VS_RYU_CS,   VS_YUN_CS,   VS_DUDLEY_CS,
                                             VS_NECRO_CS,   VS_HUGO_CS,   VS_IBUKI_CS, VS_ELENA_CS, VS_ORO_CS,
                                             VS_YUN_CS,     VS_KEN_CS,    VS_SEAN_CS,  VS_URIEN_CS, VS_GOUKI_CS,
                                             VS_CHUN_LI_CS, VS_MAKOTO_CS, VS_Q_CS,     VS_NO12_CS,  VS_REMY_CS };

s32 (*const Passive_C_tbl[20])(PlayerEntity* wk) = { VS_GILL_C,    VS_ALEX_C,   VS_RYU_C,   VS_YUN_C,   VS_DUDLEY_C,
                                            VS_NECRO_C,   VS_HUGO_C,   VS_IBUKI_C, VS_ELENA_C, VS_ORO_C,
                                            VS_YUN_C,     VS_KEN_C,    VS_SEAN_C,  VS_URIEN_C, VS_GOUKI_C,
                                            VS_CHUN_LI_C, VS_MAKOTO_C, VS_Q_C,     VS_NO12_C,  VS_REMY_C };

s32 (*const Passive_DS_tbl[20])(PlayerEntity* wk) = { VS_GILL_DS,    VS_ALEX_DS,   VS_RYU_DS,   VS_YUN_DS,   VS_DUDLEY_DS,
                                             VS_NECRO_DS,   VS_HUGO_DS,   VS_IBUKI_DS, VS_ELENA_DS, VS_ORO_DS,
                                             VS_YUN_DS,     VS_KEN_DS,    VS_SEAN_DS,  VS_URIEN_DS, VS_GOUKI_DS,
                                             VS_CHUN_LI_DS, VS_MAKOTO_DS, VS_Q_DS,     VS_NO12_DS,  VS_REMY_DS };

s32 (*const Passive_D_tbl[20])(PlayerEntity* wk) = { VS_GILL_D,    VS_ALEX_D,   VS_RYU_D,   VS_YUN_D,   VS_DUDLEY_D,
                                            VS_NECRO_D,   VS_HUGO_D,   VS_IBUKI_D, VS_ELENA_D, VS_ORO_D,
                                            VS_YUN_D,     VS_KEN_D,    VS_SEAN_D,  VS_URIEN_D, VS_GOUKI_D,
                                            VS_CHUN_LI_D, VS_MAKOTO_D, VS_Q_D,     VS_NO12_D,  VS_REMY_D };
