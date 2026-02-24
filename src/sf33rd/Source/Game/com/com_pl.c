/**
 * @file com_pl.c
 * @brief CPU-controlled character AI main loop and state machine.
 *
 * Top-level AI entry point for CPU players. Manages the AI state machine
 * that cycles through: Initialize → Free → Active/Follow/Passive → Guard →
 * Damage/Float/Flip/Caught/Catch states. Dispatches to per-character AI
 * handlers in the active/, follow/, passive/, and shell/ subdirectories.
 *
 * Part of the COM (computer player) AI module.
 */

#include "sf33rd/Source/Game/com/com_pl.h"
#include "common.h"

#define COM_STATE_COUNT 16
#define CHAR_COUNT 20
#define DAMAGE_STATE_COUNT 10
#define FLOAT_STATE_COUNT 4
#define FLIP_STATE_COUNT 5
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/Source/Game/com/active/active00.h"
#include "sf33rd/Source/Game/com/active/active01.h"
#include "sf33rd/Source/Game/com/active/active02.h"
#include "sf33rd/Source/Game/com/active/active03.h"
#include "sf33rd/Source/Game/com/active/active04.h"
#include "sf33rd/Source/Game/com/active/active05.h"
#include "sf33rd/Source/Game/com/active/active06.h"
#include "sf33rd/Source/Game/com/active/active07.h"
#include "sf33rd/Source/Game/com/active/active08.h"
#include "sf33rd/Source/Game/com/active/active09.h"
#include "sf33rd/Source/Game/com/active/active10.h"
#include "sf33rd/Source/Game/com/active/active11.h"
#include "sf33rd/Source/Game/com/active/active12.h"
#include "sf33rd/Source/Game/com/active/active13.h"
#include "sf33rd/Source/Game/com/active/active14.h"
#include "sf33rd/Source/Game/com/active/active15.h"
#include "sf33rd/Source/Game/com/active/active16.h"
#include "sf33rd/Source/Game/com/active/active17.h"
#include "sf33rd/Source/Game/com/active/active18.h"
#include "sf33rd/Source/Game/com/active/active19.h"
#include "sf33rd/Source/Game/com/ck_pass.h"
#include "sf33rd/Source/Game/com/com_data.h"
#include "sf33rd/Source/Game/com/com_sub.h"
#include "sf33rd/Source/Game/com/follow/follow02.h"
#include "sf33rd/Source/Game/com/passive/pass00.h"
#include "sf33rd/Source/Game/com/passive/pass01.h"
#include "sf33rd/Source/Game/com/passive/pass02.h"
#include "sf33rd/Source/Game/com/passive/pass03.h"
#include "sf33rd/Source/Game/com/passive/pass04.h"
#include "sf33rd/Source/Game/com/passive/pass05.h"
#include "sf33rd/Source/Game/com/passive/pass06.h"
#include "sf33rd/Source/Game/com/passive/pass07.h"
#include "sf33rd/Source/Game/com/passive/pass08.h"
#include "sf33rd/Source/Game/com/passive/pass09.h"
#include "sf33rd/Source/Game/com/passive/pass10.h"
#include "sf33rd/Source/Game/com/passive/pass11.h"
#include "sf33rd/Source/Game/com/passive/pass12.h"
#include "sf33rd/Source/Game/com/passive/pass13.h"
#include "sf33rd/Source/Game/com/passive/pass14.h"
#include "sf33rd/Source/Game/com/passive/pass15.h"
#include "sf33rd/Source/Game/com/passive/pass16.h"
#include "sf33rd/Source/Game/com/passive/pass17.h"
#include "sf33rd/Source/Game/com/passive/pass18.h"
#include "sf33rd/Source/Game/com/passive/pass19.h"
#include "sf33rd/Source/Game/com/shell/shell00.h"
#include "sf33rd/Source/Game/com/shell/shell01.h"
#include "sf33rd/Source/Game/com/shell/shell03.h"
#include "sf33rd/Source/Game/com/shell/shell04.h"
#include "sf33rd/Source/Game/com/shell/shell05.h"
#include "sf33rd/Source/Game/com/shell/shell07.h"
#include "sf33rd/Source/Game/com/shell/shell11.h"
#include "sf33rd/Source/Game/com/shell/shell12.h"
#include "sf33rd/Source/Game/com/shell/shell13.h"
#include "sf33rd/Source/Game/com/shell/shell14.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/engine/cmd_data.h"
#include "sf33rd/Source/Game/engine/cmd_main.h"
#include "sf33rd/Source/Game/engine/getup.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/plmain.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/training/training_dummy.h"

void Main_Program(PLW* wk);

static u16 CPU_Sub(PLW* wk);
static s32 Check_Counter_Attack(PLW* wk);
static s16 Check_Hamari(PLW* wk);
static s32 Check_No12_Shell_Guard(PLW* wk, WORK_Other* tmw);
static s32 Ck_Exit_Guard(PLW* wk, WORK* em);
static s32 Ck_Exit_Guard_Sub(PLW* wk, WORK* em);

void Com_Initialize(PLW* wk);
void Com_Free(PLW* wk);
void Com_Active(PLW* wk);
void Com_Before_Follow(PLW* wk);
void Com_Follow(PLW* wk);
void Com_Before_Passive(PLW* wk);
void Com_Passive(PLW* wk);
void Com_Guard(PLW* wk);
void Com_VS_Shell(PLW* wk);
void Com_Guard_VS_Shell(PLW* wk);
void Com_Damage(PLW* wk);
void Com_Float(PLW* wk);
void Com_Flip(PLW* wk);
void Com_Caught(PLW* wk);
void Com_Wait_Lie(PLW* wk);
void Com_Catch(PLW* wk);

void Damage_1st(PLW* wk);
void Damage_2nd(PLW* wk);
void Damage_3rd(PLW* /* unused */);
void Damage_4th(PLW* /* unused */);
void Damage_5th(PLW* wk);
void Damage_6th(PLW* wk);
void Damage_7th(PLW* wk);
void Damage_8th(PLW* wk);

void Exit_Damage_Sub(PLW* wk);
static s32 Check_Damage(PLW* wk);

void Float_2nd(PLW* wk);
void Float_3rd(PLW* wk);
void Float_4th(PLW* wk);

void Flip_Zero(PLW* wk);
void Flip_1st(PLW* wk);
void Flip_2nd(PLW* wk);
void Flip_3rd(PLW* wk);
void Flip_4th(PLW* wk);

static s32 Check_Shell_Flip(PLW* wk);
s32 Check_Flip(PLW* wk);
static s32 Check_Flip_Attack(PLW* wk);
static s16 Decide_Exit_Catch(PLW* wk);
s32 Com_Rapid_Sub(PLW* wk, s16 Shot, u8* dir_step);
static s32 Check_Caught(PLW* wk);
s32 Command_Attack_SP(PLW* wk, s8 Pl_Number, s16 Tech_Number, s16 Power_Level);
void Next_Be_Float(PLW* wk);
void Clear_Com_Flag(PLW* wk);
void Check_At_Count(PLW* wk);
void Shift_Resume_Lv(PLW* wk);
void Check_Store_Lv(PLW* wk);
void Store_LR_Sub(PLW* wk);
void Setup_Bullet_Counter(PLW* wk);
void Pattern_Insurance(PLW* wk, s16 Kind_Of_Insurance, s16 Forced_Number);

const u16 Correct_Lv_Data[16] = { 0, 1, 2, 2, 4, 5, 6, 5, 8, 9, 10, 9, 8, 5, 10, 0 };

/** @brief Top-level CPU AI entry point — returns joystick input for this frame. */
u16 cpu_algorithm(PLW* wk) {
    u16 sw = CPU_Sub(wk);

    if (Play_Mode == 1 && Replay_Status[wk->wu.id] != 99) {
        if (wk->wu.id) {
            p2sw_0 = sw;
        } else {
            p1sw_0 = sw;
        }

        if (CPU_Time_Lag[wk->wu.id]) {
            CPU_Rec[wk->wu.id] = 1;
            return sw;
        }

        CPU_Rec[wk->wu.id] = 1;

        if (Debug_w[DEBUG_DISP_REC_STATUS]) {
            flPrintColor(0xFFFFFFFF);
            flPrintL(16, 9, "CPU REC!");
        }

        Check_Replay_Status(wk->wu.id, 1);
    }

    return sw;
}

/** @brief Core AI tick — updates state, runs the main program, and returns lever data. */
static u16 CPU_Sub(PLW* wk) {
    WORK* em = (WORK*)wk->wu.target_adrs;

    if (Allow_a_battle_f == 0 || pcon_dp_flag) {
        return 0;
    }

    Lever_Buff[wk->wu.id] = 0;

    if (em->pat_status == 0x26) {
        Lie_Flag[wk->wu.id] = 1;
    } else {
        Lie_Flag[wk->wu.id] = 0;
    }

    Last_Pattern_Index[wk->wu.id] = Pattern_Index[wk->wu.id];
    Main_Program(wk);
    Lever_Buff[wk->wu.id] = check_illegal_lever_data(Lever_Buff[wk->wu.id]);

    // TRAINING MODE OVERRIDE
    if (g_training_state.is_in_match) {
        training_dummy_update_input(wk, wk->wu.id);
        // Dummy writes Lever_Buff[id] directly — no sync needed
    }

    Check_Store_Lv(wk);
    Shift_Resume_Lv(wk);
    Disp_Lever(&Lever_Buff[wk->wu.id], wk->wu.id, 1);
    Disp_Mode(wk);
    return Lever_Buff[wk->wu.id];
}

/** @brief AI state machine dispatcher — calls the handler for the current CP_No state. */
void Main_Program(PLW* wk) {
    void (*Com_Jmp_Tbl[COM_STATE_COUNT])(PLW*) = { Com_Initialize, Com_Free,           Com_Active,   Com_Before_Follow,
                                                   Com_Follow,     Com_Before_Passive, Com_Passive,  Com_Guard,
                                                   Com_VS_Shell,   Com_Guard_VS_Shell, Com_Damage,   Com_Float,
                                                   Com_Flip,       Com_Caught,         Com_Wait_Lie, Com_Catch };

    Ck_Distance(wk);
    Area_Number[wk->wu.id] = Ck_Area(wk);
    Attack_Flag[wk->wu.id] = plw[wk->wu.id ^ 1].caution_flag;
    Check_At_Count(wk);
    Disposal_Again[wk->wu.id] = 0;

    if ((u32)CP_No[wk->wu.id][0] >= COM_STATE_COUNT) {
        return;
    }

    Com_Jmp_Tbl[CP_No[wk->wu.id][0]](wk);

    if (Disposal_Again[wk->wu.id]) {
        if ((u32)CP_No[wk->wu.id][0] < COM_STATE_COUNT) {
            Com_Jmp_Tbl[CP_No[wk->wu.id][0]](wk);
        }
    }
}

/** @brief AI state 0: Initialize all CPU player variables at round start. */
void Com_Initialize(PLW* wk) {
    const s16* xx;
    s16 i;

    time_check_ix = 0;

    for (i = 0; i < 4; i++) {
        time_check[i] = -1;
    }

    CP_No[wk->wu.id][0] = 1;
    CP_No[wk->wu.id][1] = 0;
    CP_No[wk->wu.id][2] = 0;
    CP_No[wk->wu.id][3] = 0;
    Lever_Squat[wk->wu.id] = 0;
    Lever_Store[wk->wu.id][0] = 0;
    Lever_Store[wk->wu.id][1] = 0;
    Lever_Store[wk->wu.id][2] = 0;
    Attack_Counter[wk->wu.id] = 0;
    Bullet_No[wk->wu.id] = 0;
    Last_Attack_Counter[wk->wu.id] = -1;
    Guard_Counter[wk->wu.id] = -1;
    Turn_Over_Timer[wk->wu.id] = 1;
    Attack_Count_Index[wk->wu.id] = 0;
    Flip_Counter[wk->wu.id] = 0;
    Lever_LR[0] = 0;
    Lever_LR[1] = 0;
    xx = Area_Unit_Data[wk->player_number];
    Separate_Area[wk->wu.id][0] = xx[0];
    Separate_Area[wk->wu.id][1] = xx[1];
    Separate_Area[wk->wu.id][2] = xx[2];
    xx = Shell_Area_Unit_Data[wk->player_number];
    Shell_Separate_Area[wk->wu.id][0] = xx[0];
    Shell_Separate_Area[wk->wu.id][1] = xx[1];
    Shell_Separate_Area[wk->wu.id][2] = xx[2];
    Com_Width_Data[wk->wu.id] = PL_Body_Width_Data[wk->player_number];
    Clear_Com_Flag(wk);
    Standing_Master_Timer[wk->wu.id] = Setup_Next_Stand_Timer(wk);
    Squat_Master_Timer[wk->wu.id] = Setup_Next_Squat_Timer(wk);
    Squat_Master_Timer[wk->wu.id] = 0;
    Setup_Bullet_Counter(wk);

    for (i = 0; i < 20; i++) {
        Resume_Lever[wk->wu.id][i] = 0;
    }

    for (i = 0; i < 3; i++) {
        Attack_Count_Buff[wk->wu.id][i] = -1;
    }
}

/** @brief AI state 1: Free state — select an active behavior pattern. */
void Com_Free(PLW* wk) {
    s16 xx;

    Lever_Buff[wk->wu.id] = Lever_LR[wk->wu.id];

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    CP_No[wk->wu.id][0] = 2;
    CP_No[wk->wu.id][1] = 0;
    CP_No[wk->wu.id][2] = 0;
    CP_No[wk->wu.id][3] = 0;

    if (Before_Look[wk->wu.id]) {
        xx = Standing_Timer[wk->wu.id];
    } else {
        xx = 0;
    }

    Clear_Com_Flag(wk);
    Standing_Timer[wk->wu.id] = xx;

    for (xx = 0; xx <= 7; xx++) {
        CP_Index[wk->wu.id][xx] = 0;
    }

    Select_Active(wk);
}

/** @brief AI state 3: Wait before transitioning to follow-up combo execution. */
void Com_Before_Follow(PLW* wk) {
    Lever_Buff[wk->wu.id] = Lever_LR[wk->wu.id];

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

    if (--Timer_00[wk->wu.id] != 0) {
        return;
    }

    Decide_Follow_Menu(wk);
    CP_No[wk->wu.id][0] = 4;
    CP_No[wk->wu.id][1] = 0;
    CP_No[wk->wu.id][2] = 0;
    CP_No[wk->wu.id][3] = 0;
    CP_Index[wk->wu.id][0] = 0;
    CP_Index[wk->wu.id][1] = 0;
    CP_Index[wk->wu.id][2] = 0;
    CP_Index[wk->wu.id][3] = 0;
    Clear_Com_Flag(wk);
}

/** @brief AI state 5: Wait before transitioning to passive reaction execution. */
void Com_Before_Passive(PLW* wk) {
    Lever_Buff[wk->wu.id] = Lever_LR[wk->wu.id];

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    if (!Limited_Flag[wk->wu.id] && !Counter_Attack[wk->wu.id]) {
        if (Check_Guard(wk)) {
            return;
        }
    }

    if (--Timer_00[wk->wu.id] != 0) {
        return;
    }

    CP_No[wk->wu.id][0] = 6;
    CP_No[wk->wu.id][1] = 0;
    CP_No[wk->wu.id][2] = 0;
    CP_No[wk->wu.id][3] = 0;
    CP_Index[wk->wu.id][0] = 0;
    CP_Index[wk->wu.id][1] = 0;
    CP_Index[wk->wu.id][2] = 0;
    CP_Index[wk->wu.id][3] = 0;
}

/** @brief AI state 7: Guard state — decide whether to continue blocking or counter-attack. */
void Com_Guard(PLW* wk) {
    WORK* em;

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

    em = (WORK*)wk->wu.target_adrs;

    if (Ck_Exit_Guard(wk, em)) {
        Check_Guard_Type(wk, em);
        return;
    }

    Passive_Flag[wk->wu.id] = 0;
    Passive_Mode = 4;

    if (Ck_Passive_Term(wk)) {
        Select_Passive(wk);
        Counter_Attack[wk->wu.id] |= 2;
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
static s32 Check_Counter_Attack(PLW* wk) {
    s16 xx;

    if (Area_Number[wk->wu.id] >= 3) {
        return 0;
    }

    xx = Type_of_Attack[wk->wu.id] & 0xF8;

    if (xx == 8) {
        VS_Tech[wk->wu.id] = 28;
        return 1;
    }

    if (xx == 24) {
        VS_Tech[wk->wu.id] = 14;
        return 1;
    }

    if (xx == 32) {
        VS_Tech[wk->wu.id] = 14;
        return 1;
    }

    if (xx == 48) {
        VS_Tech[wk->wu.id] = 14;
        return 1;
    }

    return Check_Hamari(wk);
}

/** @brief Check if the opponent is repeating the same attack ("hamari" trap detection). */
static s16 Check_Hamari(PLW* wk) {
    u8 tech;
    s16 Rnd;
    s16 limit;
    s16 xx;

    if (Area_Number[wk->wu.id] >= 2) {
        return 0;
    }

    tech = Attack_Count_Buff[wk->wu.id][0];
    Rnd = random_32_com() & 1;
    limit = Rnd + 3;

    if (((PLW*)wk->wu.target_adrs)->player_number == 4 && tech == 3) {
        limit--;
    } else if (tech != 0 && tech != 1) {
        return 0;
    }

    for (xx = 1; xx < limit; xx++) {
        if (tech != Attack_Count_Buff[wk->wu.id][xx]) {
            return 0;
        }
    }

    return VS_Tech[wk->wu.id] = 32;
}

/** @brief AI state 9: Guard against incoming projectiles (shells). */
void Com_Guard_VS_Shell(PLW* wk) {
    WORK_Other* tmw;

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    tmw = (WORK_Other*)Shell_Address[wk->wu.id];

    Check_Guard_Type(wk, &tmw->wu);

    if (Timer_00[wk->wu.id] == 0) {
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

        Timer_00[wk->wu.id] = 1;
        return;
    }

    Timer_00[wk->wu.id]--;
}

/** @brief Check if Twelve (NO12) should continue guarding against a projectile by position. */
static s32 Check_No12_Shell_Guard(PLW* wk, WORK_Other* tmw) {
    s16 pos_x;

    if (wk->wu.rl_flag) {
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
void Check_Guard_Type(PLW* wk, WORK* em) {
    Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 1);

    switch (Guard_Type[wk->wu.id]) {
    case 0:
        if (em->pat_status >= 0xE && em->pat_status <= 0x1E) {
            break;
        }

        if (em->att.guard & 16 || !(em->att.guard & 8)) {
            break;
        }

        Lever_Buff[wk->wu.id] |= 2;
        break;

    case 1:
        break;

    case 2:
        Lever_Buff[wk->wu.id] |= 2;
        break;
    }
}

/** @brief Check whether the CPU should remain in guard state or exit. */
static s32 Ck_Exit_Guard(PLW* wk, WORK* em) {
    s16 Lv;

    if (--Timer_00[wk->wu.id]) {
        return 1;
    }

    Timer_00[wk->wu.id] = 1;

    if (Ck_Exit_Guard_Sub(wk, em)) {
        if (Guard_Counter[wk->wu.id] == Attack_Counter[wk->wu.id]) {
            return 1;
        }

        Guard_Counter[wk->wu.id] = Attack_Counter[wk->wu.id];
        Lv = Setup_Lv10(0);

        if (Break_Into_CPU == 2) {
            Lv = 10;
        }

        if (Demo_Flag == 0 && Weak_PL == wk->wu.id) {
            Lv = 2;
        }

        Lv += CC_Value[0];
        Lv = emLevelRemake(Lv, 11, 1);

        if (EM_Rank != 0) {
            Guard_Type[wk->wu.id] = Guard_Data[17][Lv][random_16_com()];
        } else {
            Guard_Type[wk->wu.id] = Guard_Data[wk->player_number][Lv][random_16_com()];
        }

        return 1;
    }

    return 0;
}

/** @brief Sub-check for guard exit — tests whether the opponent is still attacking. */
static s32 Ck_Exit_Guard_Sub(PLW* wk, WORK* em) {
    if (Attack_Flag[wk->wu.id] == 0) {
        return 0;
    }

    if (wk->wu.routine_no[1] == 1) {
        if (wk->wu.routine_no[3] == 0) {
            return 1;
        }

        if (wk->wu.routine_no[2] >= 4 && wk->wu.routine_no[2] < 8 && wk->wu.cmwk[0xE] == 0 &&
            Attack_Flag[wk->wu.id] == 0) {
            return 0;
        }

        return 1;
    }

    if (em->routine_no[1] != 4) {
        return 0;
    }

    if (Attack_Flag[wk->wu.id] == 0) {
        return 0;
    }

    return 1;
}

/** @brief AI state 2: Execute the active AI pattern for the current character. */
void Com_Active(PLW* wk) {
    void (*Char_Jmp_Tbl[CHAR_COUNT])(PLW*) = { Computer00, Computer01, Computer02, Computer03, Computer04,
                                               Computer05, Computer06, Computer07, Computer08, Computer09,
                                               Computer10, Computer11, Computer12, Computer13, Computer14,
                                               Computer15, Computer16, Computer17, Computer18, Computer19 };

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    Pattern_Insurance(wk, 0, 0);

    if ((u32)wk->player_number >= CHAR_COUNT) {
        return;
    }

    Char_Jmp_Tbl[wk->player_number](wk);
}

/** @brief AI state 4: Execute follow-up combo pattern for the current character. */
void Com_Follow(PLW* wk) {
    void (*Follow_Jmp_Tbl[CHAR_COUNT])(PLW*) = { Follow02, Follow02, Follow02, Follow02, Follow02, Follow02, Follow02,
                                                 Follow02, Follow02, Follow02, Follow02, Follow02, Follow02, Follow02,
                                                 Follow02, Follow02, Follow02, Follow02, Follow02, Follow02 };

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    Pattern_Insurance(wk, 3, 2);

    if ((u32)wk->player_number >= CHAR_COUNT) {
        return;
    }

    Follow_Jmp_Tbl[wk->player_number](wk);
}

/** @brief AI state 6: Execute passive reaction pattern for the current character. */
void Com_Passive(PLW* wk) {
    void (*Passive_Jmp_Tbl[CHAR_COUNT])(PLW*) = { Passive00, Passive01, Passive02, Passive03, Passive04,
                                                  Passive05, Passive06, Passive07, Passive08, Passive09,
                                                  Passive10, Passive11, Passive12, Passive13, Passive14,
                                                  Passive15, Passive16, Passive17, Passive18, Passive19 };

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    Pattern_Insurance(wk, 1, 1);

    if ((u32)wk->player_number >= CHAR_COUNT) {
        return;
    }

    Passive_Jmp_Tbl[wk->player_number](wk);
}

/** @brief AI state 8: Execute projectile response pattern for the current character. */
void Com_VS_Shell(PLW* wk) {
    void (*VS_Shell_Jmp_Tbl[CHAR_COUNT])(PLW*) = { Shell00, Shell01, Shell11, Shell03, Shell04, Shell05, Shell03,
                                                   Shell07, Shell03, Shell03, Shell03, Shell11, Shell12, Shell13,
                                                   Shell14, Shell11, Shell11, Shell11, Shell11, Shell11 };

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    Pattern_Insurance(wk, 2, 0);

    if ((u32)wk->player_number >= CHAR_COUNT) {
        return;
    }

    VS_Shell_Jmp_Tbl[wk->player_number](wk);
}

/** @brief AI state 10: Handle taking damage — dispatches through damage sub-states. */
void Com_Damage(PLW* wk) {
    void (*Damage_Jmp_Tbl[DAMAGE_STATE_COUNT])(PLW*) = { Damage_1st, Damage_2nd, Damage_3rd, Damage_4th, Damage_5th,
                                                         Damage_6th, Damage_7th, Damage_7th, Damage_7th, Damage_8th };

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    if ((u32)CP_No[wk->wu.id][1] >= DAMAGE_STATE_COUNT) {
        return;
    }

    Damage_Jmp_Tbl[CP_No[wk->wu.id][1]](wk);
}

/** @brief Damage sub-state 0: Initial damage reaction — decide blocking and get-up action. */
void Damage_1st(PLW* wk) {
    u8 Lv;
    u8 Rnd;
    u8 xx;
    WORK* em;

    Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 1);
    Lever_Buff[wk->wu.id] |= 2;

    switch (CP_No[wk->wu.id][2]) {
    case 0:
        if (wk->py->flag) {
            CP_No[wk->wu.id][1] = 9;
            break;
        }

        if (PL_Blow_Off_Data[wk->wu.routine_no[2]] == 0) {
            CP_No[wk->wu.id][1] = 1;
            break;
        }

        CP_No[wk->wu.id][2]++;
        Lv = Setup_Lv08(0);

        if (Break_Into_CPU == 2) {
            Lv = 7;
        }

        if (Demo_Flag == 0 && Weak_PL == wk->wu.id) {
            Lv = 0;
        }

        Rnd = random_32_com();
        xx = Setup_EM_Rank_Index(wk);

        if (Receive_Data[xx][emLevelRemake(Lv, 8, 0)] > Rnd) {
            Receive_Flag[wk->wu.id] = 1;
            break;
        }

        break;

    case 1:
        if (wk->wu.routine_no[3] == 0) {
            CP_No[wk->wu.id][2] = 0;
            break;
        }

        Lv = Setup_Lv04(0);

        if (Break_Into_CPU == 2) {
            Lv = 3;
        }

        if (Demo_Flag == 0 && Weak_PL == wk->wu.id) {
            Lv = 0;
        }

        Rnd = random_32_com();
        CP_No[wk->wu.id][1] = Get_Up_Data[wk->player_number][emLevelRemake(Lv, 4, 0)][Rnd] + 1;
        CP_No[wk->wu.id][2] = 0;

        if (Get_Up_Action_Check_Data[wk->player_number][CP_No[wk->wu.id][1] - 1][Area_Number[wk->wu.id]] == -1) {
            CP_No[wk->wu.id][1] = Get_Up_Action_Check_Data[wk->player_number][CP_No[wk->wu.id][1]][4];
        }

        if (CP_No[wk->wu.id][1] != 0) {
            break;
        }

        Lv = Setup_Lv10(0);

        if (Break_Into_CPU == 2) {
            Lv = 10;
        }

        if (Demo_Flag == 0 && Weak_PL == wk->wu.id) {
            Lv = 0;
        }

        Rnd = random_16_com();
        Lv += CC_Value[0];
        Lv = emLevelRemake(Lv, 11, 1);
        em = (WORK*)wk->wu.target_adrs;

        if (EM_Rank != 0) {
            Guard_Type[wk->wu.id] = Guard_Data[17][Lv][Rnd];
        } else {
            Guard_Type[wk->wu.id] = Guard_Data[wk->player_number][Lv][Rnd];
        }

        Check_Guard_Type(wk, em);
        break;
    }
}

/** @brief Damage sub-state 1: Continue guarding after hit; check for ukemi (tech) opportunity. */
void Damage_2nd(PLW* wk) {
    WORK* em = (WORK*)wk->wu.target_adrs;

    Check_Guard_Type(wk, em);

    if (wk->wu.routine_no[2] == 0x19) {
        CP_No[wk->wu.id][1] = 9;
        CP_No[wk->wu.id][2] = 0;
        return;
    }

    if (Receive_Flag[wk->wu.id] != 0 && plw[wk->wu.id].uot_cd_ok_flag != 0) {
        Lever_Buff[wk->wu.id] = 2;
    }

    if (wk->wu.routine_no[1] != 1) {
        Exit_Damage_Sub(wk);
    }
}

/** @brief Damage sub-state 2: No-op placeholder. */
void Damage_3rd(PLW* /* unused */) {}

/** @brief Damage sub-state 3: No-op placeholder. */
void Damage_4th(PLW* /* unused */) {}

/** @brief Damage sub-state 4: Super art reversal during get-up. */
void Damage_5th(PLW* wk) {
    if (wk->wu.routine_no[3] == 0) {
        CP_No[wk->wu.id][1] = 0;
        CP_No[wk->wu.id][2] = 0;
        return;
    }

    switch (CP_No[wk->wu.id][2]) {
    case 0:
        if (wk->wu.routine_no[1] != 1) {
            Exit_Damage_Sub(wk);
            break;
        }

        if (wk->wu.cg_type == 9) {
            CP_No[wk->wu.id][2]++;
            CP_Index[wk->wu.id][1] = 0;
        }

        break;

    case 1:
        if (Command_Attack_SP(wk, wk->player_number, 46, 8)) {
            CP_No[wk->wu.id][2]++;
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
void Damage_6th(PLW* wk) {
    u8 Lv;
    u8 Rnd;

    if (wk->wu.routine_no[3] == 0) {
        CP_No[wk->wu.id][1] = 0;
        CP_No[wk->wu.id][2] = 0;
        return;
    }

    if (wk->wu.routine_no[2] == 0x19) {
        CP_No[wk->wu.id][1] = 9;
        CP_No[wk->wu.id][2] = 0;
        return;
    }

    Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 1);
    Lever_Buff[wk->wu.id] |= 2;

    switch (CP_No[wk->wu.id][2]) {
    case 0:
        if (wk->wu.routine_no[1] != 1) {
            Exit_Damage_Sub(wk);
            break;
        }

        if (wk->wu.cg_type == 12) {
            if (Get_Up_Action_Check_Data[wk->player_number][CP_No[wk->wu.id][1] - 1][Area_Number[wk->wu.id]] == -1) {
                CP_No[wk->wu.id][1] = Get_Up_Action_Check_Data[wk->player_number][CP_No[wk->wu.id][1]][4];
            }

            CP_No[wk->wu.id][2]++;
            CP_Index[wk->wu.id][1] = 0;
            Lv = Setup_Lv04(0);

            if (Break_Into_CPU == 2) {
                Lv = 3;
            }

            if (Demo_Flag == 0 && Weak_PL == wk->wu.id) {
                Lv = 0;
            }

            Lv = emLevelRemake(Lv, 4, 0);
            Rnd = random_32_com() & 3;
            Rnd *= 2;

            CP_Index[wk->wu.id][0] = Get_Up_Action_Tech_Data[wk->player_number][Lv][Rnd];
            CP_Index[wk->wu.id][7] = Get_Up_Action_Tech_Data[wk->player_number][Lv][Rnd + 1];

            if (CP_Index[wk->wu.id][0] == 0xFF) {
                CP_Index[wk->wu.id][0] = Get_Up_Action_Tech_Data[wk->player_number][Lv][0];
                CP_Index[wk->wu.id][7] = 8;

                if (plw[wk->wu.id].sa->ok &&
                    Arts_Super_Name_Data[wk->player_number][plw[wk->wu.id].sa->kind_of_arts] != -1) {
                    CP_Index[wk->wu.id][0] = Arts_Super_Name_Data[wk->player_number][plw[wk->wu.id].sa->kind_of_arts];
                }
            }
        }

        break;

    case 1:
        if (Command_Attack_SP(wk, wk->player_number, CP_Index[wk->wu.id][0], CP_Index[wk->wu.id][7])) {
            CP_No[wk->wu.id][2]++;
        }

        break;

    default:
        if (Command_Attack_SP(wk, wk->player_number, CP_Index[wk->wu.id][0], CP_Index[wk->wu.id][7])) {
            Exit_Damage_Sub(wk);
        }

        break;
    }
}

/** @brief Damage sub-state 6/7/8: Guard on wake-up with guard type selection. */
void Damage_7th(PLW* wk) {
    WORK* em;

    switch (CP_No[wk->wu.id][2]) {
    case 0:
        if (wk->wu.routine_no[1] != 1) {
            Exit_Damage_Sub(wk);
            break;
        }

        CP_No[wk->wu.id][2]++;

        switch (CP_No[wk->wu.id][1]) {
        case 6:
            Guard_Type[wk->wu.id] = 0;
            break;

        case 7:
            Guard_Type[wk->wu.id] = 1;
            break;

        default:
            Guard_Type[wk->wu.id] = 2;
            break;
        }

        break;

    default:
        em = (WORK*)wk->wu.target_adrs;
        Check_Guard_Type(wk, em);

        if (wk->wu.cg_type != 0x40 && wk->wu.routine_no[1] != 0) {
            break;
        }

        if (Attack_Flag[wk->wu.id] != 0) {
            break;
        }

        if (Attack_Flag[wk->wu.id] == 0) {
            Exit_Damage_Sub(wk);
            break;
        }

        if (wk->tsukamarenai_flag == 0) {
            Exit_Damage_Sub(wk);
        }

        break;
    }
}

/** @brief Damage sub-state 9: Stun mash — rapidly input to escape dizzy. */
void Damage_8th(PLW* wk) {
    s16 Rnd;
    s16 Lv;

    if (wk->wu.routine_no[1] != 1) {
        Exit_Damage_Sub(wk);
        return;
    }

    switch (CP_No[wk->wu.id][2]) {
    case 0:
        if (wk->wu.routine_no[2] == 0x19 && wk->wu.routine_no[3] != 0) {
            CP_No[wk->wu.id][2] += 1;
            Timer_00[wk->wu.id] = 1;
            Lv = Setup_Lv08(0);

            if (Break_Into_CPU == 2) {
                Lv = 7;
            }

            if (Demo_Flag == 0 && Weak_PL == wk->wu.id) {
                Lv = 0;
            }

            Timer_01[wk->wu.id] = Faint_Rapid_Data[emLevelRemake(Lv, 8, 0)][(Rnd = random_16_com() & 7)];
        }

        break;

    case 1:
        Lever_Buff[wk->wu.id] = Com_Rapid_Sub(wk, 0, &CP_No[wk->wu.id][3]);
        break;
    }
}

/** @brief Exit damage state — clear flags and transition to passive or free. */
void Exit_Damage_Sub(PLW* wk) {
    Clear_Com_Flag(wk);

    if (Check_Passive(wk)) {
        return;
    }

    Next_Be_Free(wk);
}

/** @brief Check if the CPU player is currently being hit and should enter damage state. */
static s32 Check_Damage(PLW* wk) {
    if (Counter_Attack[wk->wu.id] & 2) {
        return 0;
    }

    if (wk->wu.routine_no[1] == 1 && CP_No[wk->wu.id][0] != 7 && CP_No[wk->wu.id][0] != 9 &&
        Guard_Flag[wk->wu.id] == 0) {
        CP_No[wk->wu.id][0] = 10;
        CP_No[wk->wu.id][1] = 0;
        CP_No[wk->wu.id][2] = 0;
        CP_No[wk->wu.id][3] = 0;
        Receive_Flag[wk->wu.id] = 0;
        Lever_Buff[wk->wu.id] = 2;
        Clear_Com_Flag(wk);
        return 1;
    }

    return 0;
}

/** @brief AI state 11: Float (juggle) state — dispatch to float sub-handlers. */
void Com_Float(PLW* wk) {
    void (*Float_Jmp_Tbl[FLOAT_STATE_COUNT])(PLW*) = { Damage_2nd, Float_2nd, Float_3rd, Float_4th };

    if (Check_Caught(wk)) {
        return;
    }

    if (Check_Flip(wk)) {
        return;
    }

    if ((u32)CP_No[wk->wu.id][1] >= FLOAT_STATE_COUNT) {
        return;
    }

    Float_Jmp_Tbl[CP_No[wk->wu.id][1]](wk);
}

/** @brief Float sub-state 1: Air recovery — input neutral then check for landing. */
void Float_2nd(PLW* wk) {
    switch (CP_No[wk->wu.id][2]) {
    case 0:
        CP_No[wk->wu.id][2]++;
        Lever_Buff[wk->wu.id] = 16;
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
void Float_3rd(PLW* wk) {
    if (wk->wu.routine_no[1] != 1) {
        Next_Be_Free(wk);
    }

    switch (CP_No[wk->wu.id][2]) {
    case 0:
        CP_No[wk->wu.id][2]++;
        Timer_00[wk->wu.id] = 4;
        Lever_Pool[wk->wu.id] = Setup_Guard_Lever(wk, 0);
        Lever_Buff[wk->wu.id] = Lever_Pool[wk->wu.id];
        break;

    default:
        if (--Timer_00[wk->wu.id] != 0) {
            break;
        }

        Timer_00[wk->wu.id] = 3;
        Lever_Buff[wk->wu.id] = Lever_Pool[wk->wu.id];
        break;
    }
}

/** @brief Float sub-state 3: Hold crouch guard while floating. */
void Float_4th(PLW* wk) {
    if (wk->wu.routine_no[1] != 1) {
        Next_Be_Free(wk);
    }

    switch (CP_No[wk->wu.id][2]) {
    case 0:
        CP_No[wk->wu.id][2]++;
        Timer_00[wk->wu.id] = 4;
        Lever_Pool[wk->wu.id] = Setup_Guard_Lever(wk, 1);
        Lever_Buff[wk->wu.id] = Lever_Pool[wk->wu.id];
        break;

    default:
        if (--Timer_00[wk->wu.id] != 0) {
            break;
        }

        Timer_00[wk->wu.id] = 3;
        Lever_Buff[wk->wu.id] = Lever_Pool[wk->wu.id];
        break;
    }
}

/** @brief AI state 12: Flip (parry) state — dispatch to flip sub-handlers. */
void Com_Flip(PLW* wk) {
    void (*Flip_Jmp_Tbl[FLIP_STATE_COUNT])(PLW*) = { Flip_Zero, Flip_1st, Flip_2nd, Flip_3rd, Flip_4th };

    if (Check_Damage(wk)) {
        return;
    }

    if (Check_Caught(wk)) {
        return;
    }

    if ((u32)CP_No[wk->wu.id][1] >= FLIP_STATE_COUNT) {
        return;
    }

    Flip_Jmp_Tbl[CP_No[wk->wu.id][1]](wk);
}

/** @brief Flip sub-state 0: Ground parry — wait for attack hit, then guard. */
void Flip_Zero(PLW* wk) {
    WORK* em = (WORK*)wk->wu.target_adrs;

    switch (CP_No[wk->wu.id][2]) {
    case 0:
        if (em->routine_no[1] != 4) {
            Exit_Damage_Sub(wk);
            break;
        }

        if (!Check_Flip_GO(wk, 0)) {
            break;
        }

        CP_No[wk->wu.id][2]++;
        Timer_00[wk->wu.id] = 9;
        break;

    case 1:
        if (Check_Flip(wk)) {
            break;
        }

        if (--Timer_00[wk->wu.id] != 0) {
            break;
        }

        Exit_Damage_Sub(wk);
        break;
    }
}

/** @brief Check if parry input should be committed — sets guard lever if attack is incoming. */
s32 Check_Flip_GO(PLW* wk, s16 xx) {
    WORK* em = (WORK*)wk->wu.target_adrs;

    if (em->att_hit_ok || xx) {
        if (em->pat_status == 0x21 || em->pat_status == 0x20) {
            Lever_Buff[wk->wu.id] = 2;
        } else {
            Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 0);
        }

        if (xx == 0 && Resume_Lever[wk->wu.id][0] == Lever_Buff[wk->wu.id]) {
            Next_Be_Guard(wk, em, 0);
            Flip_Counter[wk->wu.id] = 255;
            return 0;
        }

        Flip_Counter[wk->wu.id]++;
        return 1;
    }

    return 0;
}

/** @brief Flip sub-state 1: Air parry — wait until landing. */
void Flip_1st(PLW* wk) {
    if (wk->wu.xyz[1].disp.pos <= 0) {
        Exit_Damage_Sub(wk);
    }
}

/** @brief Flip sub-state 2: After parry — decide whether to counter-attack. */
void Flip_2nd(PLW* wk) {
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
void Flip_3rd(PLW* wk) {
    s16 next_disposal;

    if (PL_Damage_Data[wk->wu.routine_no[2]] == 0) {
        return;
    }

    next_disposal = Check_Shell_Flip(wk);

    switch (next_disposal) {
    case 0:
        CP_No[wk->wu.id][1] = 2;
        return;

    case 1:
        Timer_00[wk->wu.id] = 15;
        /* fallthrough */

    case 3:
        CP_No[wk->wu.id][1] = 4;
        return;

    case 2:
        CP_No[wk->wu.id][0] = 9;
        CP_No[wk->wu.id][1] = 0;
        CP_No[wk->wu.id][2] = 0;
        CP_No[wk->wu.id][3] = 0;
        Timer_00[wk->wu.id] = 10;
        Flip_Counter[wk->wu.id] = 255;
        dash_flag_clear(wk->wu.id);
        Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 1);

        if (((WORK*)wk->wu.dmg_adrs)->att.guard & 0x10) {
            break;
        }

        Lever_Buff[wk->wu.id] |= 2;
        break;

    default:
        Flip_Counter[wk->wu.id] = 255;
        Next_Be_Free(wk);
        break;
    }
}

/** @brief Flip sub-state 4: Wait timer then attempt another shell parry or exit. */
void Flip_4th(PLW* wk) {
    if (--Timer_00[wk->wu.id] != 0) {
        return;
    }

    if (SetShellFlipLever(wk) == 0) {
        Flip_Counter[wk->wu.id] = 255;
        Next_Be_Free(wk);
        return;
    }

    CP_No[wk->wu.id][1] = 0;
    CP_No[wk->wu.id][2] = 1;
    Timer_00[wk->wu.id] = 9;
}

/** @brief Set the guard lever for parrying an incoming projectile. Returns 0 if no shell. */
s32 SetShellFlipLever(PLW* wk) {
    WORK* tmw;

    Lever_Buff[wk->wu.id] = 0;
    tmw = (WORK*)Shell_Address[wk->wu.id];

    if (tmw == NULL) {
        return 0;
    }

    if (tmw->be_flag == 0 || tmw->id != 13) {
        return 0;
    }

    if (!(tmw->att.guard & 3)) {
        return 0;
    }

    Lever_Buff[wk->wu.id] = 2;

    if (tmw->att.guard & 2) {
        Lever_Buff[wk->wu.id] = Setup_Guard_Lever(wk, 0);
    }

    return 1;
}

/** @brief Decide the next action after parrying a projectile (continue, guard, or exit). */
static s32 Check_Shell_Flip(PLW* wk) {
    WORK* shell;
    s32 Rnd;
    s32 Lv;
    s32 xx;
    s32 res;

    res = 0;
    Flip_Counter[wk->wu.id]++;

    if (Timer_01[wk->wu.id] != 8) {
        return 0;
    }

    shell = (WORK*)wk->wu.dmg_adrs;

    if (shell == NULL) {
        res = 1;
    } else if (shell->be_flag != 0 && shell->id == 13) {
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
        shell = (WORK*)Shell_Address[wk->wu.id];
        wk->wu.dmg_adrs = shell;
    }

    Rnd = random_32_com();
    Rnd -= Flip_Term_Correct(wk);
    Lv = emLevelRemake(Setup_Lv08(0), 8, 0);

    if (Rnd >= Shell_Renzoku_Flip_Data[wk->player_number][Lv]) {
        return 2;
    }

    if (Flip_Counter[wk->wu.id] < emGetMaxBlocking()) {
        if (res == 0) {
            return 1;
        }

        xx -= 8;

        if (xx > 0) {
            Timer_00[wk->wu.id] = xx;
            return 3;
        }
    }

    return 0;
}

/** @brief Check if the CPU player has been parried and should enter flip state. */
s32 Check_Flip(PLW* wk) {
    if (Flip_Flag[wk->wu.id]) {
        return 0;
    }

    if (wk->wu.routine_no[1] != 0) {
        return 0;
    }

    if (PL_Damage_Data[wk->wu.routine_no[2]] == 0) {
        return 0;
    }

    if (Flip_Counter[wk->wu.id] == 0xFF) {
        return 0;
    }

    CP_No[wk->wu.id][0] = 12;
    CP_No[wk->wu.id][2] = 0;
    CP_No[wk->wu.id][3] = 0;
    Timer_00[wk->wu.id] = 15;

    if (Timer_01[wk->wu.id] == 8) {
        CP_No[wk->wu.id][1] = 3;
    } else {
        CP_No[wk->wu.id][1] = 2;
    }

    if (wk->wu.xyz[1].disp.pos > 0) {
        CP_No[wk->wu.id][1] = 1;
    }

    return 1;
}

/** @brief Decide whether to counter-attack after a successful parry based on difficulty. */
static s32 Check_Flip_Attack(PLW* wk) {
    s16 Lv = Setup_Lv08(0);
    s16 Rnd;
    s16 xx;

    if (Break_Into_CPU == 2) {
        Lv = 7;
    }

    if (Demo_Flag == 0 && Weak_PL == wk->wu.id) {
        Lv = 0;
    }

    Rnd = random_32_com();
    Rnd -= Flip_Term_Correct(wk);
    xx = Setup_EM_Rank_Index(wk);

    if (Rnd >= Flip_Attack_Data[xx][emLevelRemake(Lv, 8, 0)]) {
        return 0;
    }

    Flip_Flag[wk->wu.id] = 0;
    VS_Tech[wk->wu.id] = 13;
    Counter_Attack[wk->wu.id] = 1;
    return 1;
}

/** @brief AI state 13: Being thrown — mash to escape or take the throw. */
void Com_Caught(PLW* wk) {
    s16 Rnd;
    s16 Lv;
    WORK* em = (WORK*)wk->wu.target_adrs;

    switch (CP_No[wk->wu.id][1]) {
    case 0:
        CP_No[wk->wu.id][1]++;
        CP_No[wk->wu.id][2] = 0;

        if (em->sp_tech_id == 1) {
            Timer_00[wk->wu.id] = 12;
            Lv = Setup_Lv08(0);

            if (Break_Into_CPU == 2) {
                Lv = 7;
            }

            if (Demo_Flag == 0 && Weak_PL == wk->wu.id) {
                Lv = 0;
            }

            Timer_01[wk->wu.id] = Rapid_Exit_Data[emLevelRemake(Lv, 8, 0)][(Rnd = random_16_com() & 7)];
            break;
        }

        Timer_00[wk->wu.id] = Decide_Exit_Catch(wk);
        Timer_01[wk->wu.id] = 1;
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

        Lever_Buff[wk->wu.id] = Com_Rapid_Sub(wk, 0xFF0, &CP_No[wk->wu.id][2]);
        break;
    }
}

/** @brief Decide whether the CPU escapes a throw based on difficulty level. */
static s16 Decide_Exit_Catch(PLW* wk) {
    s16 Rnd;
    s16 xx;
    s16 Lv = Setup_Lv18(save_w[Present_Mode].Difficulty + 0);

    Lv += CC_Value[0];

    if (Break_Into_CPU == 2) {
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
s32 Com_Rapid_Sub(PLW* wk, s16 Shot, u8* dir_step) {
    u16 xx;

    if (--Timer_00[wk->wu.id] == 0) {
        Timer_00[wk->wu.id] = Timer_01[wk->wu.id];
        xx = Rapid_Lever_Data[dir_step[0]];
        xx |= Shot;
        dir_step[0]++;
        dir_step[0] &= 1;
        return xx;
    }

    return 0;
}

/** @brief Check if the CPU player has been grabbed and should enter caught state. */
static s32 Check_Caught(PLW* wk) {
    if (wk->wu.routine_no[1] == 3) {
        CP_No[wk->wu.id][0] = 13;
        CP_No[wk->wu.id][1] = 0;
        CP_No[wk->wu.id][2] = 0;
        CP_No[wk->wu.id][3] = 0;
        Clear_Com_Flag(wk);
        return 1;
    }

    return 0;
}

/** @brief AI state 15: Catching the opponent — mash buttons during throw animation. */
void Com_Catch(PLW* wk) {
    WORK* em;
    s16 Rnd;
    s16 Lv;

    switch (CP_No[wk->wu.id][1]) {
    case 0:
        CP_No[wk->wu.id][1]++;
        CP_No[wk->wu.id][2] = 0;
        Timer_00[wk->wu.id] = 1;
        Lv = Setup_Lv04(0);

        if (Break_Into_CPU == 2) {
            Lv = 3;
        }

        Timer_01[wk->wu.id] = Rapid_Hit_Data[emLevelRemake(Lv, 4, 0)][(Rnd = random_16_com() & 7)];
        break;

    case 1:
        em = (WORK*)wk->wu.target_adrs;

        if (wk->wu.routine_no[1] != 2 || em->routine_no[1] != 3) {
            Next_Be_Free(wk);
            break;
        }

        Lever_Buff[wk->wu.id] = Com_Rapid_Sub(wk, 0xFF0, &CP_No[wk->wu.id][2]);
        break;
    }
}

/** @brief Transition into the catch (throwing opponent) state. */
void Be_Catch(PLW* wk) {
    CP_No[wk->wu.id][0] = 15;
    CP_No[wk->wu.id][1] = 0;
    CP_No[wk->wu.id][2] = 0;
    CP_No[wk->wu.id][3] = 0;
    Clear_Com_Flag(wk);
}

/** @brief AI state 14: Lying on ground — check for opponent blow-off then exit damage. */
void Com_Wait_Lie(PLW* wk) {
    WORK* em = (WORK*)wk->wu.target_adrs;

    if (Check_Blow_Off(wk, em, 0)) {
        return;
    }

    Exit_Damage_Sub(wk);
}

/** @brief Execute a command attack (special/super) by feeding the input sequence frame-by-frame. */
s32 Command_Attack_SP(PLW* wk, s8 Pl_Number, s16 Tech_Number, s16 Power_Level) {
    switch (CP_Index[wk->wu.id][1]) {
    case 0:
        CP_Index[wk->wu.id][1]++;
        dash_flag_clear(wk->wu.id);
        Tech_Address[wk->wu.id] = player_cmd[Pl_Number][Tech_Number & 0xFF];
        Tech_Index[wk->wu.id] = 0xC;
        Check_Rapid(wk, Tech_Number);
        Rapid_Index[wk->wu.id] = 0x110;
        Lever_Pool[wk->wu.id] = 0x110;
        break;

    case 1:
        switch (Tech_Address[wk->wu.id][Tech_Index[wk->wu.id]]) {
        default:
        case 1:
        case 10:
            if (Command_Type_00(wk, Power_Level & 0xF, Tech_Number, -1) == -1) {
                CP_Index[wk->wu.id][1] = 99;
            }

            break;

        case 2:
            if (Command_Type_01(wk, Power_Level & 0xF, -1)) {
                CP_Index[wk->wu.id][1]++;
            }

            break;
        }

        if (CP_Index[wk->wu.id][1] == 2) {
            return 1;
        }

        break;

    case 2:
        if (wk->wu.cg_type == 64) {
            Lever_Buff[wk->wu.id] = Lever_Pool[wk->wu.id];
            CP_Index[wk->wu.id][1]++;
        }

        /* fallthrough */

    default:
        Rapid_Sub(wk);

        if (wk->wu.routine_no[1] == 0 && plw[wk->wu.id].caution_flag == 0) {
            return 1;
        }
    }

    return 0;
}

/** @brief Transition the AI back to the Free (idle) state. */
void Next_Be_Free(PLW* wk) {
    CP_No[wk->wu.id][0] = 1;
    CP_No[wk->wu.id][1] = 0;
    CP_No[wk->wu.id][2] = 0;
    CP_No[wk->wu.id][3] = 0;
    Lever_Buff[wk->wu.id] = Lever_LR[wk->wu.id];
}

/** @brief Transition the AI into the Float (juggle recovery) state. */
void Next_Be_Float(PLW* wk) {
    s16 Rnd;
    s16 Lv;

    CP_No[wk->wu.id][0] = 11;
    CP_No[wk->wu.id][2] = 0;
    CP_No[wk->wu.id][3] = 0;
    Clear_Com_Flag(wk);
    Lv = Setup_Lv04(0);
    Rnd = random_16_com();
    CP_No[wk->wu.id][1] = Float_Attack_Data[emLevelRemake(Lv, 4, 0)][Rnd];
}

/** @brief Reset all per-frame AI control flags to their defaults. */
void Clear_Com_Flag(PLW* wk) {
    Passive_Flag[wk->wu.id] = 0;
    Flip_Flag[wk->wu.id] = 0;
    Counter_Attack[wk->wu.id] = 0;
    Limited_Flag[wk->wu.id] = 0;
    Guard_Flag[wk->wu.id] = 0;
    Before_Jump[wk->wu.id] = 0;
    Shell_Ignore_Timer[wk->wu.id] = 0;
    Pierce_Menu[wk->wu.id] = 0;
    Continue_Menu[wk->wu.id] = 0;
    Standing_Timer[wk->wu.id] = 0;
    Before_Look[wk->wu.id] = 0;
    Attack_Count_No0[wk->wu.id] = 0;
    Turn_Over[wk->wu.id] = 0;
    Jump_Pass_Timer[wk->wu.id][0] = 0;
    Jump_Pass_Timer[wk->wu.id][1] = 0;
    Jump_Pass_Timer[wk->wu.id][2] = 0;
    Jump_Pass_Timer[wk->wu.id][3] = 0;
    Last_Eftype[wk->wu.id] = 0;
}

/** @brief Track the opponent's attack frequency and type for counter-attack decisions. */
void Check_At_Count(PLW* wk) {
    WORK* em = (WORK*)wk->wu.target_adrs;
    s16 ix;

    if (Attack_Count_No0[wk->wu.id] == 0) {
        if (Attack_Flag[wk->wu.id]) {
            Attack_Counter[wk->wu.id]++;
            Attack_Count_No0[wk->wu.id] = 1;
            Type_of_Attack[wk->wu.id] = em->kind_of_waza;
            Attack_Count_Buff[wk->wu.id][Attack_Count_Index[wk->wu.id]] = em->kind_of_waza;
            Attack_Count_Index[wk->wu.id]++;
            Attack_Count_Index[wk->wu.id] &= 3;
        }
    } else if (Attack_Flag[wk->wu.id] == 0) {
        Attack_Count_No0[wk->wu.id] = 0;
    }

    if (Attack_Flag[wk->wu.id]) {
        Reset_Timer[wk->wu.id] = 120;
        return;
    }

    if (--Reset_Timer[wk->wu.id] == 0) {
        for (ix = 0; ix < 4; ix++) {
            Attack_Count_Buff[wk->wu.id][ix] = ix;
        }
    }
}

/** @brief Shift the lever history buffer — stores the last 20 frames of lever input. */
void Shift_Resume_Lv(PLW* wk) {
    s16 xx;

    for (xx = 18; xx >= 0; xx--) {
        Resume_Lever[wk->wu.id][xx + 1] = Resume_Lever[wk->wu.id][xx];
    }

    Resume_Lever[wk->wu.id][0] = Lever_Buff[wk->wu.id];
}

/** @brief Track consecutive directional inputs for dash/charge detection. */
void Check_Store_Lv(PLW* wk) {
    s16 xx = Lever_Buff[wk->wu.id] & 0xF;

    switch (xx) {
    case 2:
        Lever_Store[wk->wu.id][0]++;
        break;

    case 6:
    case 10:
        Store_LR_Sub(wk);
        Lever_Store[wk->wu.id][0]++;
        break;

    case 4:
    case 8:
        Store_LR_Sub(wk);
        break;

    default:
        Lever_Store[wk->wu.id][0] = 0;
        Lever_Store[wk->wu.id][1] = 0;
        Lever_Store[wk->wu.id][2] = 0;
        break;
    }
}

/** @brief Sub-routine for Store_LR — count left/right directional holds with facing correction. */
void Store_LR_Sub(PLW* wk) {
    if (wk->wu.rl_waza) {
        if (Lever_Buff[wk->wu.id] & 8) {
            Lever_Store[wk->wu.id][1]++;
            Lever_Store[wk->wu.id][2] = 0;
        }

        if (Lever_Buff[wk->wu.id] & 4) {
            Lever_Store[wk->wu.id][1] = 0;
            Lever_Store[wk->wu.id][2]++;
        }
    } else {
        if (Lever_Buff[wk->wu.id] & 4) {
            Lever_Store[wk->wu.id][1]++;
            Lever_Store[wk->wu.id][2] = 0;
        }

        if (Lever_Buff[wk->wu.id] & 8) {
            Lever_Store[wk->wu.id][1] = 0;
            Lever_Store[wk->wu.id][2]++;
        }
    }
}

/** @brief Initialize the bullet counter (limits projectile spam). */
void Setup_Bullet_Counter(PLW* wk) {
    Bullet_Counter[wk->wu.id] = 3;
    Bullet_Counter[wk->wu.id] += random_32_com() & 1;
}

const u8 Pattern_Insurance_Data[20][4] = {
    { 67, 157, 10, 3 }, { 69, 175, 9, 3 },  { 74, 132, 10, 3 }, { 71, 135, 10, 3 },  { 67, 141, 11, 3 },
    { 66, 101, 10, 3 }, { 63, 146, 10, 3 }, { 75, 213, 11, 3 }, { 70, 213, 10, 3 },  { 100, 131, 10, 3 },
    { 69, 137, 10, 3 }, { 89, 254, 13, 3 }, { 85, 230, 10, 3 }, { 80, 167, 11, 3 },  { 150, 252, 12, 3 },
    { 68, 163, 13, 3 }, { 69, 166, 13, 3 }, { 82, 181, 13, 3 }, { 108, 203, 13, 3 }, { 78, 175, 13, 3 }
};

/** @brief Safety check: reset pattern index if it exceeds the valid range for this character. */
void Pattern_Insurance(PLW* wk, s16 Kind_Of_Insurance, s16 Forced_Number) {
    if (Pattern_Insurance_Data[wk->player_number][Kind_Of_Insurance] < Pattern_Index[wk->wu.id]) {
        Pattern_Index[wk->wu.id] = Forced_Number;
    }
}
