/**
 * @file ai_active_sean.c
 * COM Active: Sean
 */

#include "sf33rd/Source/Game/com/active/ai_active_sean.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "sf33rd/Source/Game/engine/state_user.h"

static void (*const Pattern12_Tbl[86])();

/** @brief Twelve active AI pattern entry point. */
void Computer12(PlayerEntity* wk) {
    Pattern12_Tbl[(s16)g_state.Pattern_Index[wk->wu.id]](wk);
}

static void Pattern12_0000(PlayerEntity* wk) {
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

static void Pattern12_0001(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0002(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x37, 2);
        break;

    case 1:
        Lever_Attack(wk, 8, 0, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0003(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x37, 2);
        break;

    case 1:
        Lever_Attack(wk, 8, 1, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0004(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x37, 2);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0005(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1D, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0006(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1D, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0007(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0008(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0009(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x20, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0010(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x20, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0011(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x10);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0012(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0013(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0014(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x100);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0015(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0016(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0017(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1C, 0x4008, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0018(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1C, 0x4009, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0019(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1C, 0x400A, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0020(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0021(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0022(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0023(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 8, 0, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0024(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0025(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0026(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Walk(wk, 1, 0x20, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0027(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0xBF, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0028(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Jump(wk, 0);
        break;

    case 2:
        AI_Random_Action_Select(wk, 6, 2, 4, 0x20, 0xA, 4);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0029(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Jump(wk, 0);
        break;

    case 2:
        AI_Random_Action_Select(wk, 6, 2, 4, 0, 0xD, 3);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0030(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0xBF, 1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x11, 0, 0x10, 0x2F, 3);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0031(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Walk(wk, 1, 0x20, -1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x11, 0, 0x24, 0x23, 3);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0032(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x20);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0033(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x37, 2);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x30, 0x31, 0x32, 0);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0034(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0xBF, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0035(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1C, 0x4009, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0036(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x400);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1C, 0x400A, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0037(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0038(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1D, 9, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1D, 9, -1);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    case 3:
        AI_Random_Action_Select(wk, 6, 0, 0xA, 0x13, 0x1F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0039(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x37, 2);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x30, 0x31, 0x32, 0);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0040(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC0, 9, 0x20, 0, -0x7FB0, -1, 0x200);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1D, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0041(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC0, 9, 0x200, 0, -0x7FB0, -1, 0x20);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x20);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x20, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0042(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC0, 0xB, 0x200, 0, -1, -1, -1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x30, 0x31, 0x32, 0);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1C, 0x400A, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0043(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC0, 0xB, 0x200, 0, -0x7FB0, -1, 0x40);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x400);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0044(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA0, -0x7FC0, 9, 0x400, 0, -0x7FB0, -1, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0045(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA0, -0x7FC0, 9, 0x40, 0, -0x7FB0, -1, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x40);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0046(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA0, -0x7FC0, 9, 0x40, 0, -0x7FB0, -1, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x40);
        break;

    case 2:
        Command_Attack(wk, 0xC, 0x1C, 0x400A, -1);
        break;

    case 3:
        Wait(wk, 5);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x30, 0x31, 0x32, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0047(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Command_Attack(wk, 0xC, 0, -1, -1);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0048(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x8014, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0049(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x8215, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0050(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x8016, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0051(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0x7F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0052(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x20, 0xA, -1);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0053(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x37, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0054(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x10);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0055(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x10);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0056(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x7F, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0057(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, 0x8058, 0x8040, 9, 0x20, 0, 0x8050, -1, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x12);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 3:
        Normal_Attack(wk, 8, 0x202);
        break;

    case 4:
        Command_Attack(wk, 8, 0x1c, 0x4008, -1);
        break;

    case 5:
        AI_Random_Action_Select(wk, 6, 0x2d, 0xff, 0xff, 0xff, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0058(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC0, 9, 0x200, 0, -0x7FB0, -1, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0059(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0060(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x102);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0061(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x12);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0062(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0x37, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0063(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0x7F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0064(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0xBF, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0065(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0xBF, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0066(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x12);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x12);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1C, 0x400A, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0067(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0068(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x20);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x40);
        break;

    case 3:
        Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0069(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0070(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1E, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0071(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC0, 9, 0x200, 0, -0x7FB0, -1, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x202);
        break;

    case 3:
        Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0072(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA0, -0x7FC0, 9, 0x200, 0, -0x7FB0, -1, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x40);
        break;

    case 2:
        Command_Attack(wk, 0xC, 0x1D, 0xA, -1);
        break;

    case 3:
        Wait(wk, 5);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x30, 0x31, 0x32, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0073(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x40);
        break;

    case 1:
        Command_Attack(wk, 0xC, 0x1C, 9, -1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0074(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x93, 2);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0x31, 0xFFFF, 0);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0075(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x202);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x202);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0x30, 0xFFFF, 0);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0076(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1E, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0077(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0xBF, 0);
        break;

    case 1:
        Keep_Away(wk, 0x7F, 0);
        break;

    case 2:
        Keep_Away(wk, 0xBF, 0);
        break;

    case 3:
        Keep_Away(wk, 0x37, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0078(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x12);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x12);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1C, 0x400A, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0079(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x402);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x402);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0080(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x20);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1D, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0081(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 0xC, 0x20, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0082(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 0xC, 0x20, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0083(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump(wk, 0xC, 1);
        break;

    case 1:
        Command_Attack(wk, 0xC, 0x20, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0084(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Miscellaneous_Conditions(wk, 5, 1, 0xFFFF);
        break;

    case 1:
        Provoke(wk, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern12_0085(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 8, 0, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void (*const Pattern12_Tbl[86])(PlayerEntity*) = {
    Pattern12_0000, Pattern12_0001, Pattern12_0002, Pattern12_0003, Pattern12_0004, Pattern12_0005, Pattern12_0006,
    Pattern12_0007, Pattern12_0008, Pattern12_0009, Pattern12_0010, Pattern12_0011, Pattern12_0012, Pattern12_0013,
    Pattern12_0014, Pattern12_0015, Pattern12_0016, Pattern12_0017, Pattern12_0018, Pattern12_0019, Pattern12_0020,
    Pattern12_0021, Pattern12_0022, Pattern12_0023, Pattern12_0024, Pattern12_0025, Pattern12_0026, Pattern12_0027,
    Pattern12_0028, Pattern12_0029, Pattern12_0030, Pattern12_0031, Pattern12_0032, Pattern12_0033, Pattern12_0034,
    Pattern12_0035, Pattern12_0036, Pattern12_0037, Pattern12_0038, Pattern12_0039, Pattern12_0040, Pattern12_0041,
    Pattern12_0042, Pattern12_0043, Pattern12_0044, Pattern12_0045, Pattern12_0046, Pattern12_0047, Pattern12_0048,
    Pattern12_0049, Pattern12_0050, Pattern12_0051, Pattern12_0052, Pattern12_0053, Pattern12_0054, Pattern12_0055,
    Pattern12_0056, Pattern12_0057, Pattern12_0058, Pattern12_0059, Pattern12_0060, Pattern12_0061, Pattern12_0062,
    Pattern12_0063, Pattern12_0064, Pattern12_0065, Pattern12_0066, Pattern12_0067, Pattern12_0068, Pattern12_0069,
    Pattern12_0070, Pattern12_0071, Pattern12_0072, Pattern12_0073, Pattern12_0074, Pattern12_0075, Pattern12_0076,
    Pattern12_0077, Pattern12_0078, Pattern12_0079, Pattern12_0080, Pattern12_0081, Pattern12_0082, Pattern12_0083,
    Pattern12_0084, Pattern12_0085
};
