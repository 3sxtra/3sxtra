/**
 * @file ai_active_elena.c
 * COM Active: Elena
 */

#include "sf33rd/Source/Game/com/active/ai_active_elena.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "sf33rd/Source/Game/engine/state_user.h"

static void (*const Pattern08_Tbl[71])();

/** @brief Elena active AI pattern entry point. */
void Computer08(PlayerEntity* wk) {
    Pattern08_Tbl[(s16)g_state.Pattern_Index[wk->wu.id]](wk);
}

static void Pattern08_0000(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Off(wk);
        break;

    case 1:
        Look(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0001(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0002(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x100);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x102);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0003(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Adjust_Attack(wk, 9, 0x10);
        break;

    case 1:
        Adjust_Attack(wk, 8, 0x10);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0004(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 0xB, 0x1E, 9, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1C, 9, -1);
        break;

    case 2:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 3:
        Command_Attack(wk, 8, 1, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0005(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 8, 0, 0x110);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1E, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0006(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0007(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x70, 2, 3);
        break;

    case 1:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 2:
        Command_Attack(wk, 8, 1, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0008(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F90, -0x7FC0, 8, 0x200, 0, -0x7F98, -1, 0x10);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0009(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1C, 8, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1C, 8, -1);
        break;

    case 2:
        Jump_Command_Attack(wk, 9, 0x1E, 0xA, -1);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1C, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0010(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0011(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 2:
        Jump_Command_Attack(wk, 9, 0x1E, 0xA, -1);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1C, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0012(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0013(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0014(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA0, -0x7FC8, 8, 0x200, 2, -1, -0x7FF0, 0x100);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0015(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Walk(wk, 0, 0x30, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0016(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Walk(wk, 1, 0x30, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0017(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0018(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 1, -1);
        break;

    case 1:
        Jump(wk, 1);
        break;

    case 2:
        Command_Attack(wk, 8, 0, 0xA, -1);
        break;

    case 3:
        Lever_Off(wk);
        break;

    case 4:
        Look(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0019(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, 0x70, -1, 0, 2, 5);
        break;

    case 1:
        Lever_Attack(wk, 8, 0, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0020(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 9, 0x1D, 0xA, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1C, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0021(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0022(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F98, -0x7FC0, 9, 0x40, 0, -0x7F90, -1, 0x100);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1C, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0023(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x7B, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0024(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Walk(wk, 0, 0x60, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0025(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Walk(wk, 1, 0x60, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0026(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, 0xB, -1);
        break;

    case 1:
        Lever_Off(wk);
        break;

    case 2:
        Look(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0027(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0028(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0029(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1E, 9, -1);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x40);
        break;

    case 3:
        Lever_Attack(wk, 8, 0, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0030(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x100);
        break;

    case 1:
        Lever_Attack(wk, 8, 0, 0x20);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x200);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1C, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0031(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F98, -0x7FC0, 8, 0x200, 0, -0x7F68, -1, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0032(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Adjust_Attack(wk, 0xB, 0x10);
        break;

    case 1:
        Adjust_Attack(wk, 0xB, 0x20);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0033(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 9, 0, 0x20);
        break;

    case 1:
        Lever_Attack(wk, 8, 0, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0034(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xb, 0x100);
        break;

    case 1:
        Normal_Attack(wk, 0xb, 0x40);
        break;

    case 2:
        Normal_Attack(wk, 0xb, 0x402);
        break;

    case 3:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 4:
        Command_Attack(wk, 8, 1, 10, -1);
        break;

    case 5:
        Lever_Off(wk);
        break;

    case 6:
        Look(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0035(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0036(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x42);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0037(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 8, 1, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0038(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Super_Art_Conditions(wk, 0x39, 0x3A, 0xFFFF, 0);
        break;

    case 1:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 2:
        Adjust_Attack(wk, 8, 0x200);
        break;

    case 3:
        Lever_Attack(wk, 8, 0, 0x20);
        break;

    case 4:
        Lever_Attack(wk, 8, 0, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0039(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, 0xB, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1C, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0040(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1C, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0041(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x40);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    case 2:
        Lever_Attack(wk, 8, 0, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0042(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F90, -0x7FC0, 8, 0x200, 0, -0x7F70, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0043(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F98, -0x7FC0, 8, 0x40, 0, -1, -0x7FE0, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0044(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F98, -0x7FC0, 8, 0x40, 0, -0x7F70, -1, 0x200);
        break;
    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;
    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0045(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F98, -0x7FC0, 0xB, 0x400, 0, -0x7F70, -1, 0x20);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x12);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0046(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Walk(wk, 0, 0x15, 0);
        break;

    case 1:
        Walk(wk, 1, 0x13, 0);
        break;

    case 2:
        Walk(wk, 0, 0x10, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0047(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Walk(wk, 1, 0x20, 0);
        break;

    case 1:
        Walk(wk, 0, 0x14, 0);
        break;

    case 2:
        Walk(wk, 1, 0x18, 0);
        break;

    case 3:
        Wait(wk, 0x14);
        break;

    case 4:
        Walk(wk, 0, 0x14, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0048(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait(wk, 0x3C);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0049(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0050(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -0x7F98, -0x7FB8, 8, 0x400, 2, -1, -0x7FF0, 0x10);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0051(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -0x7F98, -0x7FB8, 8, 0x40, 0, -1, -0x7FF0, 0x10);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0052(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -0x7F98, -0x7FB8, 8, 0x40, 0, -1, -0x7FF0, 0x10);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0053(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    case 2:
        Command_Attack(wk, 8, 1, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0054(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1C, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0055(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 1, -1);
        break;

    case 1:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 2:
        Command_Attack(wk, 8, 1, 10, -1);
        break;

    case 3:
        Check_Safe_Retreat_Space(wk, 0x60, 1, -1);
        break;

    case 4:
        Command_Attack(wk, 8, 1, 10, -1);
        break;

    case 5:
        Jump_Command_Attack(wk, 8, 0x1d, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0056(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 1, -1);
        break;

    case 1:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 2:
        Command_Attack(wk, 8, 1, 10, -1);
        break;

    case 3:
        Check_Safe_Retreat_Space(wk, 0x60, 1, -1);
        break;

    case 4:
        Command_Attack(wk, 8, 1, 10, -1);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, -1, -1, 0x3b, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0057(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x8014, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0058(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x8015, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0059(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x8016, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0060(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0061(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 1, 0xA, -1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0xFFFF, 0x3B, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0062(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Super_Art_Conditions(wk, 0x39, 0x3A, 0xFFFF, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0063(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0064(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0065(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 9, 0x1F, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0066(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 9, 0x1F, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0067(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 9, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0068(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 9, 0x1F, 0xA, 0x700);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0069(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump(wk, 0);
        break;

    case 1:
        Command_Attack(wk, 9, 0x1F, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern08_0070(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F98, -0x7FC0, 9, 0x400, 0, -0x7F90, -1, 0x100);
        break;

    case 1:
        Command_Attack(wk, 9, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void (*const Pattern08_Tbl[71])(PlayerEntity*) = {
    Pattern08_0000, Pattern08_0001, Pattern08_0002, Pattern08_0003, Pattern08_0004, Pattern08_0005, Pattern08_0006,
    Pattern08_0007, Pattern08_0008, Pattern08_0009, Pattern08_0010, Pattern08_0011, Pattern08_0012, Pattern08_0013,
    Pattern08_0014, Pattern08_0015, Pattern08_0016, Pattern08_0017, Pattern08_0018, Pattern08_0019, Pattern08_0020,
    Pattern08_0021, Pattern08_0022, Pattern08_0023, Pattern08_0024, Pattern08_0025, Pattern08_0026, Pattern08_0027,
    Pattern08_0028, Pattern08_0029, Pattern08_0030, Pattern08_0031, Pattern08_0032, Pattern08_0033, Pattern08_0034,
    Pattern08_0035, Pattern08_0036, Pattern08_0037, Pattern08_0038, Pattern08_0039, Pattern08_0040, Pattern08_0041,
    Pattern08_0042, Pattern08_0043, Pattern08_0044, Pattern08_0045, Pattern08_0046, Pattern08_0047, Pattern08_0048,
    Pattern08_0049, Pattern08_0050, Pattern08_0051, Pattern08_0052, Pattern08_0053, Pattern08_0054, Pattern08_0055,
    Pattern08_0056, Pattern08_0057, Pattern08_0058, Pattern08_0059, Pattern08_0060, Pattern08_0061, Pattern08_0062,
    Pattern08_0063, Pattern08_0064, Pattern08_0065, Pattern08_0066, Pattern08_0067, Pattern08_0068, Pattern08_0069,
    Pattern08_0070
};
