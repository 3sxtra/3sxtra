/**
 * @file ai_player_control.c
 * @brief CPU-controlled character AI main loop and state machine.
 *
 * Top-level AI entry point for CPU players. Manages the AI state machine
 * that cycles through: Initialize → Free → Active/Follow/Passive → Guard →
 * Damage/Float/Flip/Caught/Catch states. Dispatches to per-character AI
 * handlers in the active/, follow/, passive/, and shell/ subdirectories.
 *
 * Part of the COM (computer player) AI module.
 */

#include "sf33rd/Source/Game/com/ai_player_control.h"
#include "game_state.h"
#include "common.h"

#define COM_STATE_COUNT 16
#define CHAR_COUNT 20
#define DAMAGE_STATE_COUNT 10
#define FLOAT_STATE_COUNT 4
#define FLIP_STATE_COUNT 5
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/Source/Game/com/active/ai_active_gill.h"
#include "sf33rd/Source/Game/com/active/ai_active_alex.h"
#include "sf33rd/Source/Game/com/active/ai_active_ryu.h"
#include "sf33rd/Source/Game/com/active/ai_active_yun.h"
#include "sf33rd/Source/Game/com/active/ai_active_dudley.h"
#include "sf33rd/Source/Game/com/active/ai_active_necro.h"
#include "sf33rd/Source/Game/com/active/ai_active_hugo.h"
#include "sf33rd/Source/Game/com/active/ai_active_ibuki.h"
#include "sf33rd/Source/Game/com/active/ai_active_elena.h"
#include "sf33rd/Source/Game/com/active/ai_active_oro.h"
#include "sf33rd/Source/Game/com/active/ai_active_yang.h"
#include "sf33rd/Source/Game/com/active/ai_active_ken.h"
#include "sf33rd/Source/Game/com/active/ai_active_sean.h"
#include "sf33rd/Source/Game/com/active/ai_active_urien.h"
#include "sf33rd/Source/Game/com/active/ai_active_akuma.h"
#include "sf33rd/Source/Game/com/active/ai_active_chun_li.h"
#include "sf33rd/Source/Game/com/active/ai_active_makoto.h"
#include "sf33rd/Source/Game/com/active/ai_active_q.h"
#include "sf33rd/Source/Game/com/active/ai_active_twelve.h"
#include "sf33rd/Source/Game/com/active/ai_active_remy.h"
#include "sf33rd/Source/Game/com/ai_passive_check.h"
#include "sf33rd/Source/Game/com/ai_data_tables.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "sf33rd/Source/Game/com/follow/ai_follow_action_02.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_gill.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_alex.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_ryu.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_yun.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_dudley.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_necro.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_hugo.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_ibuki.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_elena.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_oro.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_yang.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_ken.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_sean.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_urien.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_akuma.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_chun_li.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_makoto.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_q.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_twelve.h"
#include "sf33rd/Source/Game/com/passive/ai_passive_remy.h"
#include "sf33rd/Source/Game/com/shell/ai_shell_gill.h"
#include "sf33rd/Source/Game/com/shell/ai_shell_alex.h"
#include "sf33rd/Source/Game/com/shell/ai_shell_group_a.h"
#include "sf33rd/Source/Game/com/shell/ai_shell_dudley.h"
#include "sf33rd/Source/Game/com/shell/ai_shell_necro.h"
#include "sf33rd/Source/Game/com/shell/ai_shell_ibuki.h"
#include "sf33rd/Source/Game/com/shell/ai_shell_group_b.h"
#include "sf33rd/Source/Game/com/shell/ai_shell_sean.h"
#include "sf33rd/Source/Game/com/shell/ai_shell_urien.h"
#include "sf33rd/Source/Game/com/shell/ai_shell_akuma.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/engine/cmd_data.h"
#include "sf33rd/Source/Game/engine/cmd_main.h"
#include "sf33rd/Source/Game/engine/getup.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_main.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/training/training_dummy.h"

void Main_Program(PlayerEntity* wk);

static u16 CPU_Sub(PlayerEntity* wk);
static s32 Check_Counter_Attack(PlayerEntity* wk);
static s16 Check_Spam_Trap(PlayerEntity* wk);
static s32 Check_No12_Shell_Guard(PlayerEntity* wk, State_Other* tmw);
static s32 Ck_Exit_Guard(PlayerEntity* wk, State* em);
static s32 Ck_Exit_Guard_Sub(PlayerEntity* wk, State* em);

void Com_Initialize(PlayerEntity* wk);
void Com_Free(PlayerEntity* wk);
void Com_Active(PlayerEntity* wk);
void Com_Before_Follow(PlayerEntity* wk);
void Com_Follow(PlayerEntity* wk);
void Com_Before_Passive(PlayerEntity* wk);
void Com_Passive(PlayerEntity* wk);
void Com_Guard(PlayerEntity* wk);
void Com_VS_Shell(PlayerEntity* wk);
void Com_Guard_VS_Shell(PlayerEntity* wk);
void Com_Damage(PlayerEntity* wk);
void Com_Float(PlayerEntity* wk);
void Com_Flip(PlayerEntity* wk);
void Com_Caught(PlayerEntity* wk);
void Com_Wait_Lie(PlayerEntity* wk);
void Com_Catch(PlayerEntity* wk);

void Damage_1st(PlayerEntity* wk);
void Damage_2nd(PlayerEntity* wk);
void Damage_3rd(PlayerEntity* /* unused */);
void Damage_4th(PlayerEntity* /* unused */);
void Damage_5th(PlayerEntity* wk);
void Damage_6th(PlayerEntity* wk);
void Damage_7th(PlayerEntity* wk);
void Damage_8th(PlayerEntity* wk);

void Exit_Damage_Sub(PlayerEntity* wk);
static s32 Check_Damage(PlayerEntity* wk);

void Float_2nd(PlayerEntity* wk);
void Float_3rd(PlayerEntity* wk);
void Float_4th(PlayerEntity* wk);

void Flip_Zero(PlayerEntity* wk);
void Flip_1st(PlayerEntity* wk);
void Flip_2nd(PlayerEntity* wk);
void Flip_3rd(PlayerEntity* wk);
void Flip_4th(PlayerEntity* wk);

static s32 Check_Shell_Flip(PlayerEntity* wk);
s32 Check_Flip(PlayerEntity* wk);
static s32 Check_Flip_Attack(PlayerEntity* wk);
static s16 Decide_Exit_Catch(PlayerEntity* wk);
s32 Com_Rapid_Sub(PlayerEntity* wk, s16 Shot, u8* dir_step);
static s32 Check_Caught(PlayerEntity* wk);
s32 Command_Attack_SP(PlayerEntity* wk, s8 Pl_Number, s16 Tech_Number, s16 Power_Level);
void Next_Be_Float(PlayerEntity* wk);
void Clear_Com_Flag(PlayerEntity* wk);
void Check_At_Count(PlayerEntity* wk);
void Shift_Resume_Lv(PlayerEntity* wk);
void Check_Store_Lv(PlayerEntity* wk);
void Store_LR_Sub(PlayerEntity* wk);
void Setup_Bullet_Counter(PlayerEntity* wk);
void Pattern_Insurance(PlayerEntity* wk, s16 Kind_Of_Insurance, s16 Forced_Number);

const u16 Correct_Lv_Data[16] = { 0, 1, 2, 2, 4, 5, 6, 5, 8, 9, 10, 9, 8, 5, 10, 0 };

/** @brief Top-level CPU AI entry point — returns joystick input for this frame. */
u16 cpu_algorithm(PlayerEntity* wk) {
    u16 sw = CPU_Sub(wk);

    if (g_state.Play_Mode == 1 && g_state.Replay_Status[wk->wu.id] != 99) {
        if (wk->wu.id) {
            p2sw_0 = sw;
        } else {
            p1sw_0 = sw;
        }

        if (g_state.CPU_Time_Lag[wk->wu.id]) {
            g_state.CPU_Rec[wk->wu.id] = 1;
            return sw;
        }

        g_state.CPU_Rec[wk->wu.id] = 1;

        if (Debug_w[DEBUG_DISP_REC_STATUS]) {
            flPrintColor(0xFFFFFFFF);
            flPrintL(16, 9, "CPU REC!");
        }

        Check_Replay_Status(wk->wu.id, 1);
    }

    return sw;
}

/** @brief Core AI tick — updates state, runs the main program, and returns lever data. */
static u16 CPU_Sub(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (g_state.Allow_a_battle_f == 0 || g_state.pcon_dp_flag) {
        return 0;
    }

    // When Lua drives the dummy, skip the COM AI entirely.
    // Lua has already written g_state.Lever_Buff[id] via joypad.set()
    // in emu.registerbefore() (fired from update_training_state).
    if (g_lua_dummy_active) {
        g_state.Lever_Buff[wk->wu.id] = check_illegal_lever_data(g_state.Lever_Buff[wk->wu.id]);
        Check_Store_Lv(wk);
        Shift_Resume_Lv(wk);
        Disp_Lever(&g_state.Lever_Buff[wk->wu.id], wk->wu.id, 1);
        Disp_Mode(wk);
        return g_state.Lever_Buff[wk->wu.id];
    }

    g_state.Lever_Buff[wk->wu.id] = 0;

    if (em->pat_status == 0x26) {
        g_state.Lie_Flag[wk->wu.id] = 1;
    } else {
        g_state.Lie_Flag[wk->wu.id] = 0;
    }

    g_state.Last_Pattern_Index[wk->wu.id] = g_state.Pattern_Index[wk->wu.id];
    Main_Program(wk);
    g_state.Lever_Buff[wk->wu.id] = check_illegal_lever_data(g_state.Lever_Buff[wk->wu.id]);

    // TRAINING MODE OVERRIDE (C dummy — only when Lua is not active)
    if (g_training_state.is_in_match) {
        training_dummy_update_input(wk, wk->wu.id);
    }

    Check_Store_Lv(wk);
    Shift_Resume_Lv(wk);
    Disp_Lever(&g_state.Lever_Buff[wk->wu.id], wk->wu.id, 1);
    Disp_Mode(wk);
    return g_state.Lever_Buff[wk->wu.id];
}

/** @brief AI state machine dispatcher — calls the handler for the current g_state.CP_No state. */
void Main_Program(PlayerEntity* wk) {
    void (*Com_Jmp_Tbl[COM_STATE_COUNT])(PlayerEntity*) = { Com_Initialize, Com_Free,           Com_Active,   Com_Before_Follow,
                                                   Com_Follow,     Com_Before_Passive, Com_Passive,  Com_Guard,
                                                   Com_VS_Shell,   Com_Guard_VS_Shell, Com_Damage,   Com_Float,
                                                   Com_Flip,       Com_Caught,         Com_Wait_Lie, Com_Catch };

    Ck_Distance(wk);
    g_state.Area_Number[wk->wu.id] = Ck_Area(wk);
    g_state.Attack_Flag[wk->wu.id] = g_state.plw[wk->wu.id ^ 1].caution_flag;
    Check_At_Count(wk);
    g_state.Disposal_Again[wk->wu.id] = 0;

    if ((u32)g_state.CP_No[wk->wu.id][0] >= COM_STATE_COUNT) {
        return;
    }

    Com_Jmp_Tbl[g_state.CP_No[wk->wu.id][0]](wk);

    if (g_state.Disposal_Again[wk->wu.id]) {
        if ((u32)g_state.CP_No[wk->wu.id][0] < COM_STATE_COUNT) {
            Com_Jmp_Tbl[g_state.CP_No[wk->wu.id][0]](wk);
        }
    }
}

/** @brief AI state 0: Initialize all CPU player variables at round start. */
void Com_Initialize(PlayerEntity* wk) {
    const s16* xx;
    s16 i;

    time_check_ix = 0;

    for (i = 0; i < 4; i++) {
        time_check[i] = -1;
    }

    g_state.CP_No[wk->wu.id][0] = 1;
    g_state.CP_No[wk->wu.id][1] = 0;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;
    g_state.Lever_Squat[wk->wu.id] = 0;
    g_state.Lever_Store[wk->wu.id][0] = 0;
    g_state.Lever_Store[wk->wu.id][1] = 0;
    g_state.Lever_Store[wk->wu.id][2] = 0;
    g_state.Attack_Counter[wk->wu.id] = 0;
    g_state.Bullet_No[wk->wu.id] = 0;
    g_state.Last_Attack_Counter[wk->wu.id] = -1;
    g_state.Guard_Counter[wk->wu.id] = -1;
    g_state.Turn_Over_Timer[wk->wu.id] = 1;
    g_state.Attack_Count_Index[wk->wu.id] = 0;
    g_state.Flip_Counter[wk->wu.id] = 0;
    g_state.Lever_LR[0] = 0;
    g_state.Lever_LR[1] = 0;
    xx = Area_Unit_Data[wk->player_number];
    g_state.Separate_Area[wk->wu.id][0] = xx[0];
    g_state.Separate_Area[wk->wu.id][1] = xx[1];
    g_state.Separate_Area[wk->wu.id][2] = xx[2];
    xx = Shell_Area_Unit_Data[wk->player_number];
    g_state.Shell_Separate_Area[wk->wu.id][0] = xx[0];
    g_state.Shell_Separate_Area[wk->wu.id][1] = xx[1];
    g_state.Shell_Separate_Area[wk->wu.id][2] = xx[2];
    g_state.Com_Width_Data[wk->wu.id] = PL_Body_Width_Data[wk->player_number];
    Clear_Com_Flag(wk);
    g_state.Standing_Master_Timer[wk->wu.id] = Setup_Next_Stand_Timer(wk);
    g_state.Squat_Master_Timer[wk->wu.id] = Setup_Next_Squat_Timer(wk);
    g_state.Squat_Master_Timer[wk->wu.id] = 0;
    Setup_Bullet_Counter(wk);

    for (i = 0; i < 20; i++) {
        g_state.Resume_Lever[wk->wu.id][i] = 0;
    }

    for (i = 0; i < 3; i++) {
        g_state.Attack_Count_Buff[wk->wu.id][i] = -1;
    }
}

/** @brief AI state 1: Free state — select an active behavior pattern. */
void Com_Free(PlayerEntity* wk) {
    s16 xx;

    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    g_state.CP_No[wk->wu.id][0] = 2;
    g_state.CP_No[wk->wu.id][1] = 0;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;

    if (g_state.Before_Look[wk->wu.id]) {
        xx = g_state.Standing_Timer[wk->wu.id];
    } else {
        xx = 0;
    }

    Clear_Com_Flag(wk);
    g_state.Standing_Timer[wk->wu.id] = xx;

    for (xx = 0; xx <= 7; xx++) {
        g_state.CP_Index[wk->wu.id][xx] = 0;
    }

    Select_Active(wk);
}

/** @brief AI state 3: Wait before transitioning to follow-up combo execution. */
void Com_Before_Follow(PlayerEntity* wk) {
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Guard(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    if (--g_state.Timer_00[wk->wu.id] != 0) {
        return;
    }

    Decide_Follow_Menu(wk);
    g_state.CP_No[wk->wu.id][0] = 4;
    g_state.CP_No[wk->wu.id][1] = 0;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;
    g_state.CP_Index[wk->wu.id][0] = 0;
    g_state.CP_Index[wk->wu.id][1] = 0;
    g_state.CP_Index[wk->wu.id][2] = 0;
    g_state.CP_Index[wk->wu.id][3] = 0;
    Clear_Com_Flag(wk);
}

/** @brief AI state 5: Wait before transitioning to passive reaction execution. */
void Com_Before_Passive(PlayerEntity* wk) {
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    if (!g_state.Limited_Flag[wk->wu.id] && !g_state.Counter_Attack[wk->wu.id]) {
        if (Check_Guard(wk)) {
            return;
        }
    }

    if (--g_state.Timer_00[wk->wu.id] != 0) {
        return;
    }

    g_state.CP_No[wk->wu.id][0] = 6;
    g_state.CP_No[wk->wu.id][1] = 0;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;
    g_state.CP_Index[wk->wu.id][0] = 0;
    g_state.CP_Index[wk->wu.id][1] = 0;
    g_state.CP_Index[wk->wu.id][2] = 0;
    g_state.CP_Index[wk->wu.id][3] = 0;
}

/** @brief AI state 7: Guard state — decide whether to continue blocking or counter-attack. */
void Com_Guard(PlayerEntity* wk) {
    State* em;

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    if (wk->wu.routine_no[1] == 1 && PL_Blow_Off_Data[wk->wu.routine_no[2]] == 2) {
        Next_Be_Float(wk);
        return;
    }

    em = (State*)wk->wu.target_adrs;

    if (Ck_Exit_Guard(wk, em)) {
        Check_Guard_Type(wk, em);
        return;
    }

    g_state.Passive_Flag[wk->wu.id] = 0;
    g_state.Passive_Mode = 4;

    if (Ck_Passive_Term(wk)) {
        Select_Passive(wk);
        g_state.Counter_Attack[wk->wu.id] |= 2;
        return;
    }

    if (!Check_Counter_Attack(wk)) {
        Next_Be_Free(wk);
        return;
    }

    if (Select_Passive(wk) == -1) {
        Next_Be_Free(wk);
    }
}

/** @brief Check whether the CPU should attempt a counter-attack based on attack type. */
static s32 Check_Counter_Attack(PlayerEntity* wk) {
    s16 xx;

    if (g_state.Area_Number[wk->wu.id] >= 3) {
        return 0;
    }

    xx = g_state.Type_of_Attack[wk->wu.id] & 0xF8;

    if (xx == 8) {
        g_state.VS_Tech[wk->wu.id] = 28;
        return 1;
    }

    if (xx == 24) {
        g_state.VS_Tech[wk->wu.id] = 14;
        return 1;
    }

    if (xx == 32) {
        g_state.VS_Tech[wk->wu.id] = 14;
        return 1;
    }

    if (xx == 48) {
        g_state.VS_Tech[wk->wu.id] = 14;
        return 1;
    }

    return Check_Spam_Trap(wk);
}

/** @brief Check if the opponent is repeating the same attack ("spam_trap" trap detection). */
static s16 Check_Spam_Trap(PlayerEntity* wk) {
    u8 tech;
    s16 Rnd;
    s16 limit;
    s16 xx;

    if (g_state.Area_Number[wk->wu.id] >= 2) {
        return 0;
    }

    tech = g_state.Attack_Count_Buff[wk->wu.id][0];
    Rnd = random_32_com() & 1;
    limit = Rnd + 3;

    if (((PlayerEntity*)wk->wu.target_adrs)->player_number == 4 && tech == 3) {
        limit--;
    } else if (tech != 0 && tech != 1) {
        return 0;
    }

    for (xx = 1; xx < limit; xx++) {
        if (tech != g_state.Attack_Count_Buff[wk->wu.id][xx]) {
            return 0;
        }
    }

    return g_state.VS_Tech[wk->wu.id] = 32;
}

/** @brief AI state 9: Guard against incoming projectiles (shells). */
void Com_Guard_VS_Shell(PlayerEntity* wk) {
    State_Other* tmw;

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    tmw = (State_Other*)Shell_Address[wk->wu.id];

    Check_Guard_Type(wk, &tmw->wu);

    if (g_state.Timer_00[wk->wu.id] == 0) {
        if (wk->player_number != 18) {
            if (wk->wu.routine_no[1] != 1) {
                Exit_Damage_Sub(wk);
            }
        } else if (Check_No12_Shell_Guard(wk, tmw) != 0) {
            Exit_Damage_Sub(wk);
        }

        if (tmw->wu.routine_no[0] == 2) {
            Exit_Damage_Sub(wk);
        }

        if (tmw->wu.id != 13) {
            Exit_Damage_Sub(wk);
        }

        g_state.Timer_00[wk->wu.id] = 1;
        return;
    }

    g_state.Timer_00[wk->wu.id]--;
}

/** @brief Check if Twelve (NO12) should continue guarding against a projectile by position. */
static s32 Check_No12_Shell_Guard(PlayerEntity* wk, State_Other* tmw) {
    s16 pos_x;

    if (wk->wu.facing_flag) {
        pos_x = wk->wu.xyz[0].disp.pos - 48;

        if (tmw->wu.xyz[0].disp.pos < pos_x) {
            return 1;
        }
    } else {
        pos_x = wk->wu.xyz[0].disp.pos + 48;

        if (tmw->wu.xyz[0].disp.pos > pos_x) {
            return 1;
        }
    }

    return 0;
}

/** @brief Set the guard lever input based on the current guard type (stand/crouch/auto). */
void Check_Guard_Type(PlayerEntity* wk, State* em) {
    g_state.Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 1);

    switch (g_state.Guard_Type[wk->wu.id]) {
    case 0:
        if (em->pat_status >= 0xE && em->pat_status <= 0x1E) {
            break;
        }

        if (em->att.guard & 16 || !(em->att.guard & 8)) {
            break;
        }

        g_state.Lever_Buff[wk->wu.id] |= 2;
        break;

    case 1:
        break;

    case 2:
        g_state.Lever_Buff[wk->wu.id] |= 2;
        break;
    }
}

/** @brief Check whether the CPU should remain in guard state or exit. */
static s32 Ck_Exit_Guard(PlayerEntity* wk, State* em) {
    s16 Lv;

    if (--g_state.Timer_00[wk->wu.id]) {
        return 1;
    }

    g_state.Timer_00[wk->wu.id] = 1;

    if (Ck_Exit_Guard_Sub(wk, em)) {
        if (g_state.Guard_Counter[wk->wu.id] == g_state.Attack_Counter[wk->wu.id]) {
            return 1;
        }

        g_state.Guard_Counter[wk->wu.id] = g_state.Attack_Counter[wk->wu.id];
        Lv = Setup_Lv10(0);

        if (g_state.Break_Into_CPU == 2) {
            Lv = 10;
        }

        if (g_state.Demo_Flag == 0 && g_state.Weak_PL == wk->wu.id) {
            Lv = 2;
        }

        Lv += g_state.CC_Value[0];
        Lv = emLevelRemake(Lv, 11, 1);

        if (g_state.EM_Rank != 0) {
            g_state.Guard_Type[wk->wu.id] = Guard_Data[17][Lv][random_16_com()];
        } else {
            g_state.Guard_Type[wk->wu.id] = Guard_Data[wk->player_number][Lv][random_16_com()];
        }

        return 1;
    }

    return 0;
}

/** @brief Sub-check for guard exit — tests whether the opponent is still attacking. */
static s32 Ck_Exit_Guard_Sub(PlayerEntity* wk, State* em) {
    if (g_state.Attack_Flag[wk->wu.id] == 0) {
        return 0;
    }

    if (wk->wu.routine_no[1] == 1) {
        if (wk->wu.routine_no[3] == 0) {
            return 1;
        }

        if (wk->wu.routine_no[2] >= 4 && wk->wu.routine_no[2] < 8 && wk->wu.script_register_bank[0xE] == 0 &&
            g_state.Attack_Flag[wk->wu.id] == 0) {
            return 0;
        }

        return 1;
    }

    if (em->routine_no[1] != 4) {
        return 0;
    }

    if (g_state.Attack_Flag[wk->wu.id] == 0) {
        return 0;
    }

    return 1;
}

/**
 * @brief Shared character-dispatch helper for AI states.
 *
 * All four character-dispatch AI states (Active, Follow, Passive, VS_Shell)
 * share exactly the same structure: guard checks, pattern insurance, bounds
 * check, then dispatch through a per-character table.
 *
 * @param wk         Player work area.
 * @param table      Per-character function dispatch table (CHAR_COUNT entries).
 * @param ins_kind   Kind_Of_Insurance parameter for Pattern_Insurance.
 * @param ins_forced Forced_Number parameter for Pattern_Insurance.
 */
static void com_dispatch_char(PlayerEntity* wk, void (*const table[CHAR_COUNT])(PlayerEntity*), s16 ins_kind, s16 ins_forced) {
    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    Pattern_Insurance(wk, ins_kind, ins_forced);

    if ((u32)wk->player_number >= CHAR_COUNT) {
        return;
    }

    table[wk->player_number](wk);
}

static void (*const Active_Char_Tbl[CHAR_COUNT])(PlayerEntity*) = { Computer00, Computer01, Computer02, Computer03, Computer04,
                                                           Computer05, Computer06, Computer07, Computer08, Computer09,
                                                           Computer10, Computer11, Computer12, Computer13, Computer14,
                                                           Computer15, Computer16, Computer17, Computer18, Computer19 };

/** @brief AI state 2: Execute the active AI pattern for the current character. */
void Com_Active(PlayerEntity* wk) {
    com_dispatch_char(wk, Active_Char_Tbl, 0, 0);
}

static void (*const Follow_Char_Tbl[CHAR_COUNT])(PlayerEntity*) = { Follow02, Follow02, Follow02, Follow02, Follow02,
                                                           Follow02, Follow02, Follow02, Follow02, Follow02,
                                                           Follow02, Follow02, Follow02, Follow02, Follow02,
                                                           Follow02, Follow02, Follow02, Follow02, Follow02 };

/** @brief AI state 4: Execute follow-up combo pattern for the current character. */
void Com_Follow(PlayerEntity* wk) {
    com_dispatch_char(wk, Follow_Char_Tbl, 3, 2);
}

static void (*const Passive_Char_Tbl[CHAR_COUNT])(PlayerEntity*) = { Passive00, Passive01, Passive02, Passive03, Passive04,
                                                            Passive05, Passive06, Passive07, Passive08, Passive09,
                                                            Passive10, Passive11, Passive12, Passive13, Passive14,
                                                            Passive15, Passive16, Passive17, Passive18, Passive19 };

/** @brief AI state 6: Execute passive reaction pattern for the current character. */
void Com_Passive(PlayerEntity* wk) {
    com_dispatch_char(wk, Passive_Char_Tbl, 1, 1);
}

static void (*const VS_Shell_Char_Tbl[CHAR_COUNT])(PlayerEntity*) = { Shell00, Shell01, Shell11, Shell03, Shell04,
                                                             Shell05, Shell03, Shell07, Shell03, Shell03,
                                                             Shell03, Shell11, Shell12, Shell13, Shell14,
                                                             Shell11, Shell11, Shell11, Shell11, Shell11 };

/** @brief AI state 8: Execute projectile response pattern for the current character. */
void Com_VS_Shell(PlayerEntity* wk) {
    com_dispatch_char(wk, VS_Shell_Char_Tbl, 2, 0);
}

/** @brief AI state 10: Handle taking damage — dispatches through damage sub-states. */
void Com_Damage(PlayerEntity* wk) {
    void (*Damage_Jmp_Tbl[DAMAGE_STATE_COUNT])(PlayerEntity*) = { Damage_1st, Damage_2nd, Damage_3rd, Damage_4th, Damage_5th,
                                                         Damage_6th, Damage_7th, Damage_7th, Damage_7th, Damage_8th };

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    if ((u32)g_state.CP_No[wk->wu.id][1] >= DAMAGE_STATE_COUNT) {
        return;
    }

    Damage_Jmp_Tbl[g_state.CP_No[wk->wu.id][1]](wk);
}

/** @brief Damage sub-state 0: Initial damage reaction — decide blocking and get-up action. */
void Damage_1st(PlayerEntity* wk) {
    u8 Lv;
    u8 Rnd;
    u8 xx;
    State* em;

    g_state.Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 1);
    g_state.Lever_Buff[wk->wu.id] |= 2;

    switch (g_state.CP_No[wk->wu.id][2]) {
    case 0:
        if (wk->py->flag) {
            g_state.CP_No[wk->wu.id][1] = 9;
            break;
        }

        if (PL_Blow_Off_Data[wk->wu.routine_no[2]] == 0) {
            g_state.CP_No[wk->wu.id][1] = 1;
            break;
        }

        g_state.CP_No[wk->wu.id][2]++;
        Lv = Setup_Lv08(0);

        if (g_state.Break_Into_CPU == 2) {
            Lv = 7;
        }

        if (g_state.Demo_Flag == 0 && g_state.Weak_PL == wk->wu.id) {
            Lv = 0;
        }

        Rnd = random_32_com();
        xx = Setup_EM_Rank_Index(wk);

        if (Receive_Data[xx][emLevelRemake(Lv, 8, 0)] > Rnd) {
            g_state.Receive_Flag[wk->wu.id] = 1;
            break;
        }

        break;

    case 1:
        if (wk->wu.routine_no[3] == 0) {
            g_state.CP_No[wk->wu.id][2] = 0;
            break;
        }

        Lv = Setup_Lv04(0);

        if (g_state.Break_Into_CPU == 2) {
            Lv = 3;
        }

        if (g_state.Demo_Flag == 0 && g_state.Weak_PL == wk->wu.id) {
            Lv = 0;
        }

        Rnd = random_32_com();
        g_state.CP_No[wk->wu.id][1] = Get_Up_Data[wk->player_number][emLevelRemake(Lv, 4, 0)][Rnd] + 1;
        g_state.CP_No[wk->wu.id][2] = 0;

        if (Get_Up_Action_Check_Data[wk->player_number][g_state.CP_No[wk->wu.id][1] - 1]
                                    [g_state.Area_Number[wk->wu.id]] == -1) {
            g_state.CP_No[wk->wu.id][1] = Get_Up_Action_Check_Data[wk->player_number][g_state.CP_No[wk->wu.id][1]][4];
        }

        if (g_state.CP_No[wk->wu.id][1] != 0) {
            break;
        }

        Lv = Setup_Lv10(0);

        if (g_state.Break_Into_CPU == 2) {
            Lv = 10;
        }

        if (g_state.Demo_Flag == 0 && g_state.Weak_PL == wk->wu.id) {
            Lv = 0;
        }

        Rnd = random_16_com();
        Lv += g_state.CC_Value[0];
        Lv = emLevelRemake(Lv, 11, 1);
        em = (State*)wk->wu.target_adrs;

        if (g_state.EM_Rank != 0) {
            g_state.Guard_Type[wk->wu.id] = Guard_Data[17][Lv][Rnd];
        } else {
            g_state.Guard_Type[wk->wu.id] = Guard_Data[wk->player_number][Lv][Rnd];
        }

        Check_Guard_Type(wk, em);
        break;
    }
}

/** @brief Damage sub-state 1: Continue guarding after hit; check for ukemi (tech) opportunity. */
void Damage_2nd(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    Check_Guard_Type(wk, em);

    if (wk->wu.routine_no[2] == 0x19) {
        g_state.CP_No[wk->wu.id][1] = 9;
        g_state.CP_No[wk->wu.id][2] = 0;
        return;
    }

    if (g_state.Receive_Flag[wk->wu.id] != 0 && g_state.plw[wk->wu.id].ukemi_cooldown_ok != 0) {
        g_state.Lever_Buff[wk->wu.id] = 2;
    }

    if (wk->wu.routine_no[1] != 1) {
        Exit_Damage_Sub(wk);
    }
}

/** @brief Damage sub-state 2: No-op placeholder. */
void Damage_3rd(PlayerEntity* /* unused */) {}

/** @brief Damage sub-state 3: No-op placeholder. */
void Damage_4th(PlayerEntity* /* unused */) {}

/** @brief Damage sub-state 4: Super art reversal during get-up. */
void Damage_5th(PlayerEntity* wk) {
    if (wk->wu.routine_no[3] == 0) {
        g_state.CP_No[wk->wu.id][1] = 0;
        g_state.CP_No[wk->wu.id][2] = 0;
        return;
    }

    switch (g_state.CP_No[wk->wu.id][2]) {
    case 0:
        if (wk->wu.routine_no[1] != 1) {
            Exit_Damage_Sub(wk);
            break;
        }

        if (wk->wu.cg_type == 9) {
            g_state.CP_No[wk->wu.id][2]++;
            g_state.CP_Index[wk->wu.id][1] = 0;
        }

        break;

    case 1:
        if (Command_Attack_SP(wk, wk->player_number, 46, 8)) {
            g_state.CP_No[wk->wu.id][2]++;
        }

        break;

    default:
        if (wk->wu.routine_no[1] != 4 || wk->wu.cg_type == 64) {
            Exit_Damage_Sub(wk);
        }

        break;
    }
}

/** @brief Damage sub-state 5: Get-up action with command attack reversal. */
void Damage_6th(PlayerEntity* wk) {
    u8 Lv;
    u8 Rnd;

    if (wk->wu.routine_no[3] == 0) {
        g_state.CP_No[wk->wu.id][1] = 0;
        g_state.CP_No[wk->wu.id][2] = 0;
        return;
    }

    if (wk->wu.routine_no[2] == 0x19) {
        g_state.CP_No[wk->wu.id][1] = 9;
        g_state.CP_No[wk->wu.id][2] = 0;
        return;
    }

    g_state.Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 1);
    g_state.Lever_Buff[wk->wu.id] |= 2;

    switch (g_state.CP_No[wk->wu.id][2]) {
    case 0:
        if (wk->wu.routine_no[1] != 1) {
            Exit_Damage_Sub(wk);
            break;
        }

        if (wk->wu.cg_type == 12) {
            if (Get_Up_Action_Check_Data[wk->player_number][g_state.CP_No[wk->wu.id][1] - 1]
                                        [g_state.Area_Number[wk->wu.id]] == -1) {
                g_state.CP_No[wk->wu.id][1] =
                    Get_Up_Action_Check_Data[wk->player_number][g_state.CP_No[wk->wu.id][1]][4];
            }

            g_state.CP_No[wk->wu.id][2]++;
            g_state.CP_Index[wk->wu.id][1] = 0;
            Lv = Setup_Lv04(0);

            if (g_state.Break_Into_CPU == 2) {
                Lv = 3;
            }

            if (g_state.Demo_Flag == 0 && g_state.Weak_PL == wk->wu.id) {
                Lv = 0;
            }

            Lv = emLevelRemake(Lv, 4, 0);
            Rnd = random_32_com() & 3;
            Rnd *= 2;

            g_state.CP_Index[wk->wu.id][0] = Get_Up_Action_Tech_Data[wk->player_number][Lv][Rnd];
            g_state.CP_Index[wk->wu.id][7] = Get_Up_Action_Tech_Data[wk->player_number][Lv][Rnd + 1];

            if (g_state.CP_Index[wk->wu.id][0] == 0xFF) {
                g_state.CP_Index[wk->wu.id][0] = Get_Up_Action_Tech_Data[wk->player_number][Lv][0];
                g_state.CP_Index[wk->wu.id][7] = 8;

                if (g_state.plw[wk->wu.id].sa->can_activate &&
                    Arts_Super_Name_Data[wk->player_number][g_state.plw[wk->wu.id].sa->kind_of_arts] != -1) {
                    g_state.CP_Index[wk->wu.id][0] =
                        Arts_Super_Name_Data[wk->player_number][g_state.plw[wk->wu.id].sa->kind_of_arts];
                }
            }
        }

        break;

    case 1:
        if (Command_Attack_SP(wk, wk->player_number, g_state.CP_Index[wk->wu.id][0], g_state.CP_Index[wk->wu.id][7])) {
            g_state.CP_No[wk->wu.id][2]++;
        }

        break;

    default:
        if (Command_Attack_SP(wk, wk->player_number, g_state.CP_Index[wk->wu.id][0], g_state.CP_Index[wk->wu.id][7])) {
            Exit_Damage_Sub(wk);
        }

        break;
    }
}

/** @brief Damage sub-state 6/7/8: Guard on wake-up with guard type selection. */
void Damage_7th(PlayerEntity* wk) {
    State* em;

    switch (g_state.CP_No[wk->wu.id][2]) {
    case 0:
        if (wk->wu.routine_no[1] != 1) {
            Exit_Damage_Sub(wk);
            break;
        }

        g_state.CP_No[wk->wu.id][2]++;

        switch (g_state.CP_No[wk->wu.id][1]) {
        case 6:
            g_state.Guard_Type[wk->wu.id] = 0;
            break;

        case 7:
            g_state.Guard_Type[wk->wu.id] = 1;
            break;

        default:
            g_state.Guard_Type[wk->wu.id] = 2;
            break;
        }

        break;

    default:
        em = (State*)wk->wu.target_adrs;
        Check_Guard_Type(wk, em);

        if (wk->wu.cg_type != 0x40 && wk->wu.routine_no[1] != 0) {
            break;
        }

        if (g_state.Attack_Flag[wk->wu.id] != 0) {
            break;
        }

        if (g_state.Attack_Flag[wk->wu.id] == 0) {
            Exit_Damage_Sub(wk);
            break;
        }

        if (wk->throw_invuln_flag == 0) {
            Exit_Damage_Sub(wk);
        }

        break;
    }
}

/** @brief Damage sub-state 9: Stun mash — rapidly input to escape dizzy. */
void Damage_8th(PlayerEntity* wk) {
    s16 Rnd;
    s16 Lv;

    if (wk->wu.routine_no[1] != 1) {
        Exit_Damage_Sub(wk);
        return;
    }

    switch (g_state.CP_No[wk->wu.id][2]) {
    case 0:
        if (wk->wu.routine_no[2] == 0x19 && wk->wu.routine_no[3] != 0) {
            g_state.CP_No[wk->wu.id][2] += 1;
            g_state.Timer_00[wk->wu.id] = 1;
            Lv = Setup_Lv08(0);

            if (g_state.Break_Into_CPU == 2) {
                Lv = 7;
            }

            if (g_state.Demo_Flag == 0 && g_state.Weak_PL == wk->wu.id) {
                Lv = 0;
            }

            g_state.Timer_01[wk->wu.id] = Faint_Rapid_Data[emLevelRemake(Lv, 8, 0)][(Rnd = random_16_com() & 7)];
        }

        break;

    case 1:
        g_state.Lever_Buff[wk->wu.id] = Com_Rapid_Sub(wk, 0, &g_state.CP_No[wk->wu.id][3]);
        break;
    }
}

/** @brief Exit damage state — clear flags and transition to passive or free. */
void Exit_Damage_Sub(PlayerEntity* wk) {
    Clear_Com_Flag(wk);

    if (Check_Passive(wk)) {
        return;
    }

    Next_Be_Free(wk);
}

/** @brief Check if the CPU player is currently being hit and should enter damage state. */
static s32 Check_Damage(PlayerEntity* wk) {
    if (g_state.Counter_Attack[wk->wu.id] & 2) {
        return 0;
    }

    if (wk->wu.routine_no[1] == 1 && g_state.CP_No[wk->wu.id][0] != 7 && g_state.CP_No[wk->wu.id][0] != 9 &&
        g_state.Guard_Flag[wk->wu.id] == 0) {
        g_state.CP_No[wk->wu.id][0] = 10;
        g_state.CP_No[wk->wu.id][1] = 0;
        g_state.CP_No[wk->wu.id][2] = 0;
        g_state.CP_No[wk->wu.id][3] = 0;
        g_state.Receive_Flag[wk->wu.id] = 0;
        g_state.Lever_Buff[wk->wu.id] = 2;
        Clear_Com_Flag(wk);
        return 1;
    }

    return 0;
}

/** @brief AI state 11: Float (juggle) state — dispatch to float sub-handlers. */
void Com_Float(PlayerEntity* wk) {
    void (*Float_Jmp_Tbl[FLOAT_STATE_COUNT])(PlayerEntity*) = { Damage_2nd, Float_2nd, Float_3rd, Float_4th };

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    if ((u32)g_state.CP_No[wk->wu.id][1] >= FLOAT_STATE_COUNT) {
        return;
    }

    Float_Jmp_Tbl[g_state.CP_No[wk->wu.id][1]](wk);
}

/** @brief Float sub-state 1: Air recovery — input neutral then check for landing. */
void Float_2nd(PlayerEntity* wk) {
    switch (g_state.CP_No[wk->wu.id][2]) {
    case 0:
        g_state.CP_No[wk->wu.id][2]++;
        g_state.Lever_Buff[wk->wu.id] = 16;
        break;

    default:
        if (wk->wu.routine_no[1] == 0) {
            Next_Be_Free(wk);
            break;
        }

        Check_Damage(wk);
        break;
    }
}

/** @brief Float sub-state 2: Hold back to air guard while floating. */
void Float_3rd(PlayerEntity* wk) {
    if (wk->wu.routine_no[1] != 1) {
        Next_Be_Free(wk);
    }

    switch (g_state.CP_No[wk->wu.id][2]) {
    case 0:
        g_state.CP_No[wk->wu.id][2]++;
        g_state.Timer_00[wk->wu.id] = 4;
        g_state.Lever_Pool[wk->wu.id] = Setup_Guard_Lever(wk, 0);
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
        break;

    default:
        if (--g_state.Timer_00[wk->wu.id] != 0) {
            break;
        }

        g_state.Timer_00[wk->wu.id] = 3;
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
        break;
    }
}

/** @brief Float sub-state 3: Hold crouch guard while floating. */
void Float_4th(PlayerEntity* wk) {
    if (wk->wu.routine_no[1] != 1) {
        Next_Be_Free(wk);
    }

    switch (g_state.CP_No[wk->wu.id][2]) {
    case 0:
        g_state.CP_No[wk->wu.id][2]++;
        g_state.Timer_00[wk->wu.id] = 4;
        g_state.Lever_Pool[wk->wu.id] = Setup_Guard_Lever(wk, 1);
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
        break;

    default:
        if (--g_state.Timer_00[wk->wu.id] != 0) {
            break;
        }

        g_state.Timer_00[wk->wu.id] = 3;
        g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
        break;
    }
}

/** @brief AI state 12: Flip (parry) state — dispatch to flip sub-handlers. */
void Com_Flip(PlayerEntity* wk) {
    void (*Flip_Jmp_Tbl[FLIP_STATE_COUNT])(PlayerEntity*) = { Flip_Zero, Flip_1st, Flip_2nd, Flip_3rd, Flip_4th };

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    if ((u32)g_state.CP_No[wk->wu.id][1] >= FLIP_STATE_COUNT) {
        return;
    }

    Flip_Jmp_Tbl[g_state.CP_No[wk->wu.id][1]](wk);
}

/** @brief Flip sub-state 0: Ground parry — wait for attack hit, then guard. */
void Flip_Zero(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    switch (g_state.CP_No[wk->wu.id][2]) {
    case 0:
        if (em->routine_no[1] != 4) {
            Exit_Damage_Sub(wk);
            break;
        }

        if (!Check_Flip_GO(wk, 0)) {
            break;
        }

        g_state.CP_No[wk->wu.id][2]++;
        g_state.Timer_00[wk->wu.id] = 9;
        break;

    case 1:
        if (Check_Flip(wk)) {
            break;
        }

        if (--g_state.Timer_00[wk->wu.id] != 0) {
            break;
        }

        Exit_Damage_Sub(wk);
        break;
    }
}

/** @brief Check if parry input should be committed — sets guard lever if attack is incoming. */
s32 Check_Flip_GO(PlayerEntity* wk, s16 xx) {
    State* em = (State*)wk->wu.target_adrs;

    if (em->att_hit_ok || xx) {
        if (em->pat_status == 0x21 || em->pat_status == 0x20) {
            g_state.Lever_Buff[wk->wu.id] = 2;
        } else {
            g_state.Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 0);
        }

        if (xx == 0 && g_state.Resume_Lever[wk->wu.id][0] == g_state.Lever_Buff[wk->wu.id]) {
            Next_Be_Guard(wk, em, 0);
            g_state.Flip_Counter[wk->wu.id] = 255;
            return 0;
        }

        g_state.Flip_Counter[wk->wu.id]++;
        return 1;
    }

    return 0;
}

/** @brief Flip sub-state 1: Air parry — wait until landing. */
void Flip_1st(PlayerEntity* wk) {
    if (wk->wu.xyz[1].disp.pos <= 0) {
        Exit_Damage_Sub(wk);
    }
}

/** @brief Flip sub-state 2: After parry — decide whether to counter-attack. */
void Flip_2nd(PlayerEntity* wk) {
    if (PL_Damage_Data[wk->wu.routine_no[2]] != 0) {
        return;
    }

    if (Check_Flip_Attack(wk) != 0) {
        if (Select_Passive(wk) == -1) {
            Exit_Damage_Sub(wk);
        }
    } else {
        Exit_Damage_Sub(wk);
    }
}

/** @brief Flip sub-state 3: Post-parry against projectile — decide next action. */
void Flip_3rd(PlayerEntity* wk) {
    s16 next_disposal;

    if (PL_Damage_Data[wk->wu.routine_no[2]] == 0) {
        return;
    }

    next_disposal = Check_Shell_Flip(wk);

    switch (next_disposal) {
    case 0:
        g_state.CP_No[wk->wu.id][1] = 2;
        return;

    case 1:
        g_state.Timer_00[wk->wu.id] = 15;
        /* fallthrough */

    case 3:
        g_state.CP_No[wk->wu.id][1] = 4;
        return;

    case 2:
        g_state.CP_No[wk->wu.id][0] = 9;
        g_state.CP_No[wk->wu.id][1] = 0;
        g_state.CP_No[wk->wu.id][2] = 0;
        g_state.CP_No[wk->wu.id][3] = 0;
        g_state.Timer_00[wk->wu.id] = 10;
        g_state.Flip_Counter[wk->wu.id] = 255;
        dash_flag_clear(wk->wu.id);
        g_state.Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 1);

        if (((State*)wk->wu.dmg_adrs)->att.guard & 0x10) {
            break;
        }

        g_state.Lever_Buff[wk->wu.id] |= 2;
        break;

    default:
        g_state.Flip_Counter[wk->wu.id] = 255;
        Next_Be_Free(wk);
        break;
    }
}

/** @brief Flip sub-state 4: Wait timer then attempt another shell parry or exit. */
void Flip_4th(PlayerEntity* wk) {
    if (--g_state.Timer_00[wk->wu.id] != 0) {
        return;
    }

    if (SetShellFlipLever(wk) == 0) {
        g_state.Flip_Counter[wk->wu.id] = 255;
        Next_Be_Free(wk);
        return;
    }

    g_state.CP_No[wk->wu.id][1] = 0;
    g_state.CP_No[wk->wu.id][2] = 1;
    g_state.Timer_00[wk->wu.id] = 9;
}

/** @brief Set the guard lever for parrying an incoming projectile. Returns 0 if no shell. */
s32 SetShellFlipLever(PlayerEntity* wk) {
    State* tmw;

    g_state.Lever_Buff[wk->wu.id] = 0;
    tmw = (State*)Shell_Address[wk->wu.id];

    if (tmw == NULL) {
        return 0;
    }

    if (tmw->active_flag == 0 || tmw->id != 13) {
        return 0;
    }

    if (!(tmw->att.guard & 3)) {
        return 0;
    }

    g_state.Lever_Buff[wk->wu.id] = 2;

    if (tmw->att.guard & 2) {
        g_state.Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 0);
    }

    return 1;
}

/** @brief Decide the next action after parrying a projectile (continue, guard, or exit). */
static s32 Check_Shell_Flip(PlayerEntity* wk) {
    State* shell;
    s32 Rnd;
    s32 Lv;
    s32 xx;
    s32 res;

    res = 0;
    g_state.Flip_Counter[wk->wu.id]++;

    if (g_state.Timer_01[wk->wu.id] != 8) {
        return 0;
    }

    shell = (State*)wk->wu.dmg_adrs;

    if (shell == NULL) {
        res = 1;
    } else if (shell->active_flag != 0 && shell->id == 13) {
        // do nothing
    } else {
        res = 1;
    }

    if (res || shell->vital_new < 256) {
        if ((xx = Check_Shell_Another_in_Flip(wk)) == 0) {
            if (res) {
                return -1;
            }

            return 0;
        }

        if (xx > 16) {
            return 0;
        }

        res = 1;
        shell = (State*)Shell_Address[wk->wu.id];
        wk->wu.dmg_adrs = shell;
    }

    Rnd = random_32_com();
    Rnd -= Flip_Term_Correct(wk);
    Lv = emLevelRemake(Setup_Lv08(0), 8, 0);

    if (Rnd >= Shell_Renzoku_Flip_Data[wk->player_number][Lv]) {
        return 2;
    }

    if (g_state.Flip_Counter[wk->wu.id] < emGetMaxBlocking()) {
        if (res == 0) {
            return 1;
        }

        xx -= 8;

        if (xx > 0) {
            g_state.Timer_00[wk->wu.id] = xx;
            return 3;
        }
    }

    return 0;
}

/** @brief Check if the CPU player has been parried and should enter flip state. */
s32 Check_Flip(PlayerEntity* wk) {
    if (g_state.Flip_Flag[wk->wu.id]) {
        return 0;
    }

    if (wk->wu.routine_no[1] != 0) {
        return 0;
    }

    if (PL_Damage_Data[wk->wu.routine_no[2]] == 0) {
        return 0;
    }

    if (g_state.Flip_Counter[wk->wu.id] == 0xFF) {
        return 0;
    }

    g_state.CP_No[wk->wu.id][0] = 12;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;
    g_state.Timer_00[wk->wu.id] = 15;

    if (g_state.Timer_01[wk->wu.id] == 8) {
        g_state.CP_No[wk->wu.id][1] = 3;
    } else {
        g_state.CP_No[wk->wu.id][1] = 2;
    }

    if (wk->wu.xyz[1].disp.pos > 0) {
        g_state.CP_No[wk->wu.id][1] = 1;
    }

    return 1;
}

/** @brief Decide whether to counter-attack after a successful parry based on difficulty. */
static s32 Check_Flip_Attack(PlayerEntity* wk) {
    s16 Lv = Setup_Lv08(0);
    s16 Rnd;
    s16 xx;

    if (g_state.Break_Into_CPU == 2) {
        Lv = 7;
    }

    if (g_state.Demo_Flag == 0 && g_state.Weak_PL == wk->wu.id) {
        Lv = 0;
    }

    Rnd = random_32_com();
    Rnd -= Flip_Term_Correct(wk);
    xx = Setup_EM_Rank_Index(wk);

    if (Rnd >= Flip_Attack_Data[xx][emLevelRemake(Lv, 8, 0)]) {
        return 0;
    }

    g_state.Flip_Flag[wk->wu.id] = 0;
    g_state.VS_Tech[wk->wu.id] = 13;
    g_state.Counter_Attack[wk->wu.id] = 1;
    return 1;
}

/** @brief AI state 13: Being thrown — mash to escape or take the throw. */
void Com_Caught(PlayerEntity* wk) {
    s16 Rnd;
    s16 Lv;
    State* em = (State*)wk->wu.target_adrs;

    switch (g_state.CP_No[wk->wu.id][1]) {
    case 0:
        g_state.CP_No[wk->wu.id][1]++;
        g_state.CP_No[wk->wu.id][2] = 0;

        if (em->sp_tech_id == 1) {
            g_state.Timer_00[wk->wu.id] = 12;
            Lv = Setup_Lv08(0);

            if (g_state.Break_Into_CPU == 2) {
                Lv = 7;
            }

            if (g_state.Demo_Flag == 0 && g_state.Weak_PL == wk->wu.id) {
                Lv = 0;
            }

            g_state.Timer_01[wk->wu.id] = Rapid_Exit_Data[emLevelRemake(Lv, 8, 0)][(Rnd = random_16_com() & 7)];
            break;
        }

        g_state.Timer_00[wk->wu.id] = Decide_Exit_Catch(wk);
        g_state.Timer_01[wk->wu.id] = 1;
        break;

    case 1:
        if (wk->wu.routine_no[1] != 3) {
            if (wk->wu.routine_no[1] == 0) {
                Next_Be_Free(wk);
                break;
            }

            Check_Damage(wk);
            break;
        }

        g_state.Lever_Buff[wk->wu.id] = Com_Rapid_Sub(wk, 0xFF0, &g_state.CP_No[wk->wu.id][2]);
        break;
    }
}

/** @brief Decide whether the CPU escapes a throw based on difficulty level. */
static s16 Decide_Exit_Catch(PlayerEntity* wk) {
    s16 Rnd;
    s16 xx;
    s16 Lv = Setup_Lv18(CurrentSave()->Difficulty + 0);

    Lv += g_state.CC_Value[0];

    if (g_state.Break_Into_CPU == 2) {
        Lv = 17;
    }

    Rnd = (u8)random_32_com();
    xx = Setup_EM_Rank_Index(wk);

    if (Rnd >= Exit_Throw_Data[xx][emLevelRemake(Lv, 18, 0)]) {
        return 0;
    }

    return 1;
}

const u8 Rapid_Lever_Data[2] = { 8, 4 };

/** @brief Generate rapid button-mash input for throw escape or stun recovery. */
s32 Com_Rapid_Sub(PlayerEntity* wk, s16 Shot, u8* dir_step) {
    u16 xx;

    if (--g_state.Timer_00[wk->wu.id] == 0) {
        g_state.Timer_00[wk->wu.id] = g_state.Timer_01[wk->wu.id];
        xx = Rapid_Lever_Data[dir_step[0]];
        xx |= Shot;
        dir_step[0]++;
        dir_step[0] &= 1;
        return xx;
    }

    return 0;
}

/** @brief Check if the CPU player has been grabbed and should enter caught state. */
static s32 Check_Caught(PlayerEntity* wk) {
    if (wk->wu.routine_no[1] == 3) {
        g_state.CP_No[wk->wu.id][0] = 13;
        g_state.CP_No[wk->wu.id][1] = 0;
        g_state.CP_No[wk->wu.id][2] = 0;
        g_state.CP_No[wk->wu.id][3] = 0;
        Clear_Com_Flag(wk);
        return 1;
    }

    return 0;
}

/** @brief AI state 15: Catching the opponent — mash buttons during throw animation. */
void Com_Catch(PlayerEntity* wk) {
    State* em;
    s16 Rnd;
    s16 Lv;

    switch (g_state.CP_No[wk->wu.id][1]) {
    case 0:
        g_state.CP_No[wk->wu.id][1]++;
        g_state.CP_No[wk->wu.id][2] = 0;
        g_state.Timer_00[wk->wu.id] = 1;
        Lv = Setup_Lv04(0);

        if (g_state.Break_Into_CPU == 2) {
            Lv = 3;
        }

        g_state.Timer_01[wk->wu.id] = Rapid_Hit_Data[emLevelRemake(Lv, 4, 0)][(Rnd = random_16_com() & 7)];
        break;

    case 1:
        em = (State*)wk->wu.target_adrs;

        if (wk->wu.routine_no[1] != 2 || em->routine_no[1] != 3) {
            Next_Be_Free(wk);
            break;
        }

        g_state.Lever_Buff[wk->wu.id] = Com_Rapid_Sub(wk, 0xFF0, &g_state.CP_No[wk->wu.id][2]);
        break;
    }
}

/** @brief Transition into the catch (throwing opponent) state. */
void Be_Catch(PlayerEntity* wk) {
    g_state.CP_No[wk->wu.id][0] = 15;
    g_state.CP_No[wk->wu.id][1] = 0;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;
    Clear_Com_Flag(wk);
}

/** @brief AI state 14: Lying on ground — check for opponent blow-off then exit damage. */
void Com_Wait_Lie(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;

    if (Check_Blow_Off(wk, em, 0)) {
        return;
    }

    Exit_Damage_Sub(wk);
}

/** @brief Execute a command attack (special/super) by feeding the input sequence frame-by-frame. */
s32 Command_Attack_SP(PlayerEntity* wk, s8 Pl_Number, s16 Tech_Number, s16 Power_Level) {
    switch (g_state.CP_Index[wk->wu.id][1]) {
    case 0:
        g_state.CP_Index[wk->wu.id][1]++;
        dash_flag_clear(wk->wu.id);
        Tech_Address[wk->wu.id] = player_cmd[Pl_Number][Tech_Number & 0xFF];
        g_state.Tech_Index[wk->wu.id] = 0xC;
        Check_Rapid(wk, Tech_Number);
        g_state.Rapid_Index[wk->wu.id] = 0x110;
        g_state.Lever_Pool[wk->wu.id] = 0x110;
        break;

    case 1:
        switch (Tech_Address[wk->wu.id][g_state.Tech_Index[wk->wu.id]]) {
        default:
        case 1:
        case 10:
            if (Command_Type_00(wk, Power_Level & 0xF, Tech_Number, -1) == -1) {
                g_state.CP_Index[wk->wu.id][1] = 99;
            }

            break;

        case 2:
            if (Command_Type_01(wk, Power_Level & 0xF, -1)) {
                g_state.CP_Index[wk->wu.id][1]++;
            }

            break;
        }

        if (g_state.CP_Index[wk->wu.id][1] == 2) {
            return 1;
        }

        break;

    case 2:
        if (wk->wu.cg_type == 64) {
            g_state.Lever_Buff[wk->wu.id] = g_state.Lever_Pool[wk->wu.id];
            g_state.CP_Index[wk->wu.id][1]++;
        }

        /* fallthrough */

    default:
        Rapid_Sub(wk);

        if (wk->wu.routine_no[1] == 0 && g_state.plw[wk->wu.id].caution_flag == 0) {
            return 1;
        }
    }

    return 0;
}

/** @brief Transition the AI back to the Free (idle) state. */
void Next_Be_Free(PlayerEntity* wk) {
    g_state.CP_No[wk->wu.id][0] = 1;
    g_state.CP_No[wk->wu.id][1] = 0;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;
    g_state.Lever_Buff[wk->wu.id] = g_state.Lever_LR[wk->wu.id];
}

/** @brief Transition the AI into the Float (juggle recovery) state. */
void Next_Be_Float(PlayerEntity* wk) {
    s16 Rnd;
    s16 Lv;

    g_state.CP_No[wk->wu.id][0] = 11;
    g_state.CP_No[wk->wu.id][2] = 0;
    g_state.CP_No[wk->wu.id][3] = 0;
    Clear_Com_Flag(wk);
    Lv = Setup_Lv04(0);
    Rnd = random_16_com();
    g_state.CP_No[wk->wu.id][1] = Float_Attack_Data[emLevelRemake(Lv, 4, 0)][Rnd];
}

/** @brief Reset all per-frame AI control flags to their defaults. */
void Clear_Com_Flag(PlayerEntity* wk) {
    g_state.Passive_Flag[wk->wu.id] = 0;
    g_state.Flip_Flag[wk->wu.id] = 0;
    g_state.Counter_Attack[wk->wu.id] = 0;
    g_state.Limited_Flag[wk->wu.id] = 0;
    g_state.Guard_Flag[wk->wu.id] = 0;
    g_state.Before_Jump[wk->wu.id] = 0;
    g_state.Shell_Ignore_Timer[wk->wu.id] = 0;
    g_state.Pierce_Menu[wk->wu.id] = 0;
    g_state.Continue_Menu[wk->wu.id] = 0;
    g_state.Standing_Timer[wk->wu.id] = 0;
    g_state.Before_Look[wk->wu.id] = 0;
    g_state.Attack_Count_No0[wk->wu.id] = 0;
    g_state.Turn_Over[wk->wu.id] = 0;
    g_state.Jump_Pass_Timer[wk->wu.id][0] = 0;
    g_state.Jump_Pass_Timer[wk->wu.id][1] = 0;
    g_state.Jump_Pass_Timer[wk->wu.id][2] = 0;
    g_state.Jump_Pass_Timer[wk->wu.id][3] = 0;
    g_state.Last_Eftype[wk->wu.id] = 0;
}

/** @brief Track the opponent's attack frequency and type for counter-attack decisions. */
void Check_At_Count(PlayerEntity* wk) {
    State* em = (State*)wk->wu.target_adrs;
    s16 ix;

    if (g_state.Attack_Count_No0[wk->wu.id] == 0) {
        if (g_state.Attack_Flag[wk->wu.id]) {
            g_state.Attack_Counter[wk->wu.id]++;
            g_state.Attack_Count_No0[wk->wu.id] = 1;
            g_state.Type_of_Attack[wk->wu.id] = em->attack_type;
            g_state.Attack_Count_Buff[wk->wu.id][g_state.Attack_Count_Index[wk->wu.id]] = em->attack_type;
            g_state.Attack_Count_Index[wk->wu.id]++;
            g_state.Attack_Count_Index[wk->wu.id] &= 3;
        }
    } else if (g_state.Attack_Flag[wk->wu.id] == 0) {
        g_state.Attack_Count_No0[wk->wu.id] = 0;
    }

    if (g_state.Attack_Flag[wk->wu.id]) {
        g_state.Reset_Timer[wk->wu.id] = 120;
        return;
    }

    if (--g_state.Reset_Timer[wk->wu.id] == 0) {
        for (ix = 0; ix < 4; ix++) {
            g_state.Attack_Count_Buff[wk->wu.id][ix] = ix;
        }
    }
}

/** @brief Shift the lever history buffer — stores the last 20 frames of lever input. */
void Shift_Resume_Lv(PlayerEntity* wk) {
    s16 xx;

    for (xx = 18; xx >= 0; xx--) {
        g_state.Resume_Lever[wk->wu.id][xx + 1] = g_state.Resume_Lever[wk->wu.id][xx];
    }

    g_state.Resume_Lever[wk->wu.id][0] = g_state.Lever_Buff[wk->wu.id];
}

/** @brief Track consecutive directional inputs for dash/charge detection. */
void Check_Store_Lv(PlayerEntity* wk) {
    s16 xx = g_state.Lever_Buff[wk->wu.id] & 0xF;

    switch (xx) {
    case 2:
        g_state.Lever_Store[wk->wu.id][0]++;
        break;

    case 6:
    case 10:
        Store_LR_Sub(wk);
        g_state.Lever_Store[wk->wu.id][0]++;
        break;

    case 4:
    case 8:
        Store_LR_Sub(wk);
        break;

    default:
        g_state.Lever_Store[wk->wu.id][0] = 0;
        g_state.Lever_Store[wk->wu.id][1] = 0;
        g_state.Lever_Store[wk->wu.id][2] = 0;
        break;
    }
}

/** @brief Sub-routine for Store_LR — count left/right directional holds with facing correction. */
void Store_LR_Sub(PlayerEntity* wk) {
    if (wk->wu.active_move) {
        if (g_state.Lever_Buff[wk->wu.id] & 8) {
            g_state.Lever_Store[wk->wu.id][1]++;
            g_state.Lever_Store[wk->wu.id][2] = 0;
        }

        if (g_state.Lever_Buff[wk->wu.id] & 4) {
            g_state.Lever_Store[wk->wu.id][1] = 0;
            g_state.Lever_Store[wk->wu.id][2]++;
        }
    } else {
        if (g_state.Lever_Buff[wk->wu.id] & 4) {
            g_state.Lever_Store[wk->wu.id][1]++;
            g_state.Lever_Store[wk->wu.id][2] = 0;
        }

        if (g_state.Lever_Buff[wk->wu.id] & 8) {
            g_state.Lever_Store[wk->wu.id][1] = 0;
            g_state.Lever_Store[wk->wu.id][2]++;
        }
    }
}

/** @brief Initialize the bullet counter (limits projectile spam). */
void Setup_Bullet_Counter(PlayerEntity* wk) {
    g_state.Bullet_Counter[wk->wu.id] = 3;
    g_state.Bullet_Counter[wk->wu.id] += random_32_com() & 1;
}

const u8 Pattern_Insurance_Data[20][4] = {
    { 67, 157, 10, 3 }, { 69, 175, 9, 3 },  { 74, 132, 10, 3 }, { 71, 135, 10, 3 },  { 67, 141, 11, 3 },
    { 66, 101, 10, 3 }, { 63, 146, 10, 3 }, { 75, 213, 11, 3 }, { 70, 213, 10, 3 },  { 100, 131, 10, 3 },
    { 69, 137, 10, 3 }, { 89, 254, 13, 3 }, { 85, 230, 10, 3 }, { 80, 167, 11, 3 },  { 150, 252, 12, 3 },
    { 68, 163, 13, 3 }, { 69, 166, 13, 3 }, { 82, 181, 13, 3 }, { 108, 203, 13, 3 }, { 78, 175, 13, 3 }
};

/** @brief Safety check: reset pattern index if it exceeds the valid range for this character. */
void Pattern_Insurance(PlayerEntity* wk, s16 Kind_Of_Insurance, s16 Forced_Number) {
    if (Pattern_Insurance_Data[wk->player_number][Kind_Of_Insurance] < g_state.Pattern_Index[wk->wu.id]) {
        g_state.Pattern_Index[wk->wu.id] = Forced_Number;
    }
}
