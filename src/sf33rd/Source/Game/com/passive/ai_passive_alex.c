/**
 * @file ai_passive_alex.c
 * COM Passive: Alex
 */

#include "sf33rd/Source/Game/com/passive/ai_passive_alex.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "sf33rd/Source/Game/engine/state_user.h"

void (*const Passive01_Tbl[176])();

void Passive01(PlayerEntity* wk) {
    Passive01_Tbl[(s16)g_state.Pattern_Index[wk->wu.id]](wk);
}

void Passive01_0000(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xD, g_state.M_Lv[wk->wu.id]);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0001(PlayerEntity* wk) {
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

void Passive01_0002(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 3, -1);
        break;

    case 1:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 2:
        Command_Attack(wk, 8, 0, 0xB, -1);
        break;

    case 3:
        Check_Enemy_Distance(wk, 0x7FFF, -1, 1, 1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0003(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        VS_Jump_Guard(wk);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0004(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 6, 0x53);
        break;

    case 1:
        Command_Attack(wk, 8, 1, 0xB, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0005(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Forced_Guard(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0006(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x83, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, 0x7FFF, -1, 1, 1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0007(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x83, 2);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0008(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Command_Attack(wk, 0xC, 0, 0xB, -1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1E, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0009(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0010(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Short_Range_Attack(wk, 8, 0x40, 6, 0x1D);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0011(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -1, -0x7FB0, 0xB, 0x20, 2, -0x7FA0, -1, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0012(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0013(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 6, 0x6C);
        break;

    case 1:
        Command_Attack(wk, 8, 1, 0xB, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0014(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait(wk, 3);
        break;

    case 1:
        Branch_By_Distance(wk, 6, 0x59, 0x5A, 0x5B, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0015(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0016(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 6, 0x6C);
        break;

    case 1:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 2:
        Keep_Away(wk, 0x8D, 0);
        break;

    case 3:
        Wait_Get_Up(wk, 3, -1);
        break;

    case 4:
        Branch_By_Distance(wk, 6, 0x69, 0x6A, 1, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0017(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 3:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0018(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 6, 0x6C);
        break;

    case 1:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 2:
        Keep_Away(wk, 0x8D, 0);
        break;

    case 3:
        Wait_Get_Up(wk, 3, -1);
        break;

    case 4:
        Normal_Attack(wk, 8, 0x12);
        break;

    case 5:
        Branch_By_Distance(wk, 6, 0x69, 0x6A, 1, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0019(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 3, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0020(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 3, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0021(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, -1);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1E, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0022(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7F80, -0x7FB0, 0xB, 0x100, 0, -0x7FA0, -1, 0x200);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 3:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 4:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0023(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F60, -0x7FB0, 0xB, 0x20, 0, -0x7FA0, -1, 0x200);
        break;

    case 1:
        Wait_Get_Up(wk, 0, -1);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0024(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 6, 0x6C);
        break;

    case 1:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 2:
        Keep_Away(wk, 0x8D, 0);
        break;

    case 3:
        Wait_Get_Up(wk, 0, -1);
        break;

    case 4:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0025(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 6, 0x6C);
        break;

    case 1:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 2:
        Keep_Away(wk, 0x8D, 0);
        break;

    case 3:
        Wait_Get_Up(wk, 3, -1);
        break;

    case 4:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 5:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 6:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0026(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FB0, 6, 1, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x42);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0027(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0028(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Attack(wk, 8, 0xA, 0x400, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0029(PlayerEntity* wk) {
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

void Passive01_0030(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F80, -0x7FB8, 6, 1, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0031(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Walk(wk, 1, 0x20, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0032(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Super_Art_Conditions(wk, 0x2E, 0x2F, 0xFFFF, 0x59);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0033(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x9D, 0x9D, 0x9E, 0x9E, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0034(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xC, 0x10);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0035(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0036(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 0xB, 0x1E, 0xA, -1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x42, 0x46, 0x4A, 0x4A, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0037(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 1, -1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x9D, 0x9D, 0x9E, 0x9E, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0038(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x4F, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FD8, 6, 1, -1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1D, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0039(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FB8, 6, 1, -1);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1E, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0040(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 1, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x100);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x100);
        break;

    case 3:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0041(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x83, 3);
        break;

    case 1:
        Check_Enemy_Distance(wk, -0x7F80, -0x7FC0, 6, 1, -1);
        break;

    case 2:
        Normal_Attack(wk, 0xB, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0042(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Attack(wk, 8, 0xC, 0x42, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0043(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F80, -0x7FB0, 6, 1, -1);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0044(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x4F, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FD8, 6, 1, -1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1D, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0045(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F80, -0x7FD8, 6, 1, -1);
        break;

    case 1:
        Lever_Attack(wk, 8, 1, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0046(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Status(wk, 2, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0047(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x102);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0048(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0049(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F80, -1, 6, 1, -1);
        break;

    case 1:
        Adjust_Attack(wk, 8, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0050(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F80, -1, 6, 1, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0051(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x220);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x102);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0052(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0053(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0054(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Branch_By_Distance(wk, 6, 0x68, 0x6A, 1, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0055(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1E, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0056(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F60, -0x7FA0, 8, 0x200, 0, -0x7FA0, -1, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0057(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F60, -0x7FA0, 8, 0x20, 0, -0x7FA0, -1, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0058(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Status(wk, 2, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0059(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0060(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Forced_Guard(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0061(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0x2F, 0xFFFF, 0);
        break;

    case 1:
        Approach_Walk(wk, 0x59, 2);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x2E, 0xFFFF, 0xFFFF, 0x59);
        break;

    case 3:
        Approach_Walk(wk, 0x4F, 2);
        break;

    case 4:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0062(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x3B, 2);
        break;

    case 1:
        Check_Miscellaneous_Conditions(wk, 0, 2, 0xD);
        break;

    case 2:
        Lever_Attack(wk, 8, 0, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0063(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Miscellaneous_Conditions(wk, 0, 6, 0x21);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x10);
        break;

    case 2:
        Lever_Attack(wk, 8, 1, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0064(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0065(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x3B, 2);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    case 2:
        AI_Random_Action_Select(wk, 6, 0x42, 0x46, 0x4A, 0x4A, 1);
        break;

    case 3:
        Lever_Attack(wk, 8, 0, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0066(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x4F, 2);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0067(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x83, 2);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0068(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F60, -0x7FA0, 8, 0x200, 0, -0x7FA0, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0069(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Attack(wk, 0xC, 0xC, 0x42, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0070(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x83, 2);
        break;

    case 1:
        Lever_Attack(wk, 8, 1, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0071(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x3B, 2);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x9D, 0x9D, 0x9E, 0x9E, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0072(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Command_Attack(wk, 0xC, 0, 0xB, -1);
        break;

    case 2:
        Approach_Walk(wk, 0x4F, 2);
        break;

    case 3:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0073(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xC3, 2);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x12);
        break;

    case 2:
        Branch_By_Distance(wk, 6, 0x69, 0x6A, 1, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0074(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Super_Art_Conditions(wk, 0x2E, 0x2F, 0x30, 0x59);
        break;

    case 1:
        Approach_Walk(wk, 0x4F, 2);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0075(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0x2F, 0x30, 0);
        break;

    case 1:
        Approach_Walk(wk, 0x59, 2);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x2E, 0xFFFF, 0xFFFF, 0x59);
        break;

    case 3:
        Approach_Walk(wk, 0x4F, 2);
        break;

    case 4:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0076(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0xFFFF, 0x30, 0);
        break;

    case 1:
        Approach_Walk(wk, 0xC3, 2);
        break;

    case 2:
        Jump_Attack(wk, 8, 0xC, 0x42, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0077(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F60, -0x7FA0, 0xB, 0x40, 0, -0x7FA0, -1, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0078(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F60, -0x7FB0, 0xB, 0x100, 0, -0x7FA0, -1, 0x20);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0079(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F60, -0x7FB0, 0xB, 0x20, 0, -0x7FA0, -1, 0x20);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0080(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -0x7F60, -0x7FA0, 0xB, 0x200, 0, -0x7FA0, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0081(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -0x7F60, -0x7FA0, 0xB, 0x100, 0, -0x7FA0, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0082(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -0x7F60, -0x7FA0, 0xB, 0x20, 0, -0x7FA0, -1, 0x20);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0083(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -1, -0x7FA0, 0xB, 0x200, 0, -0x7FA0, -1, 0x20);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x100);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x100);
        break;

    case 3:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0084(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack(wk, 0xC, 0xC, 0x42, 0);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 3:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0085(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait(wk, 0xA);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 5, 9, 0x35, 1, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0086(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x59, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FD8, 6, 1, -1);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x2E, 0xFFFF, 0xFFFF, 0x59);
        break;

    case 3:
        Approach_Walk(wk, 0x4F, 2);
        break;

    case 4:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0087(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x59, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FD8, 6, 1, -1);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x2E, 0x2F, 0xFFFF, 0x59);
        break;

    case 3:
        Approach_Walk(wk, 0x4F, 2);
        break;

    case 4:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0088(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FA0, 6, 1, -1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0xFFFF, 0x30, 0);
        break;

    case 2:
        Branch_By_Distance(wk, 6, 0x59, 0x5A, 0x5B, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0089(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1C, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0090(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1C, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0091(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1C, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0092(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x83, 3);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FA0, 6, 1, -1);
        break;

    case 2:
        Branch_By_Distance(wk, 6, 0x59, 0x5A, 0x5B, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0093(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x12);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0094(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 8, 0, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0095(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x12);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x12);
        break;

    case 2:
        Branch_By_Distance(wk, 6, 0x68, 0x69, 0x6A, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0096(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x83, 3);
        break;

    case 1:
        Check_Enemy_Distance(wk, -0x7F80, -0x7FD8, 6, 1, -1);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0097(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x3B, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 1, -1);
        break;

    case 2:
        AI_Random_Action_Select(wk, 6, 0x9D, 0x9D, 0x9E, 0x9E, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0098(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x3B, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FC0, 6, 1, -1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    case 3:
        AI_Random_Action_Select(wk, 6, 0x42, 0x46, 0x4A, 0x4A, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0099(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -1, -0x7FA0, 0xB, 0x200, 0, -0x7FA0, -1, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x100);
        break;

    case 2:
        Lever_Attack(wk, 8, 0, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0100(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x4F, 2);
        break;

    case 1:
        Wait_Get_Up(wk, 3, -1);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0x2F, 0x30, 0);
        break;

    case 3:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0101(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x100);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x2E, 0x2F, 0x30, 0x59);
        break;

    case 2:
        Branch_By_Distance(wk, 6, 0x69, 0x6A, 1, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0102(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F80, -1, 6, 1, -1);
        break;

    case 1:
        Lever_Attack(wk, 8, 0, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0103(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x42);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0104(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1F, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0105(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1F, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0106(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0107(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 6, 0x6C);
        break;

    case 1:
        Walk(wk, 1, 0x20, 0);
        break;

    case 2:
        Wait_Get_Up(wk, 0, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0108(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump(wk, 1, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0109(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 6, 0x6C);
        break;

    case 1:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 2:
        Command_Attack(wk, 8, 1, 0xA, -1);
        break;

    case 3:
        Wait_Get_Up(wk, 3, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0110(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x60, 6, 0x6C);
        break;

    case 1:
        Walk(wk, 1, 0x38, 0);
        break;

    case 2:
        Wait_Get_Up(wk, 0, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0111(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FB0, 6, 1, -1);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x42);
        break;

    case 2:
        AI_Random_Action_Select(wk, 6, 0x4B, 0x36, 0x3B, 0x70, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0112(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 8, 1, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0113(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x42);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x4B, 0x36, 0x3B, 0x70, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0114(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FB0, 6, 1, -1);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1E, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0115(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -1, -0x7FA0, 0xB, 0x200, 0, -0x7FA0, -1, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0116(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -1, -0x7FA0, 0xB, 0x400, 0, -0x7FA0, -1, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0117(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Attack(wk, 8, 0xC, 0x100, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0118(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0119(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_On(wk, 1, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FD0, 6, 1, -1);
        break;

    case 2:
        Check_Store_Lever(wk, 0x1F, 1, -1);
        break;

    case 3:
        Branch_By_Distance(wk, 6, 0x69, 0x6A, 1, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0120(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -1, 0x20, 8, 0x400, 1, -1, 0x20, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0121(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -1, 0x20, 8, 0x20, 1, -1, 0x20, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0122(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -1, 0x20, 8, 0x40, 1, -1, 0x20, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0123(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -1, 0x20, 8, 0x400, 2, -1, 0x20, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0124(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -1, 0x20, 8, 0x20, 2, -1, 0x20, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0125(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -1, 0x20, 8, 0x40, 2, -1, 0x20, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0126(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, 0x30, 6, 1, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x42);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0127(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0128(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_EX(wk, 6, 0x36);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1F, 9, 0x700);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0129(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0130(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x20, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0131(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x20, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0132(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Branch_By_Distance(wk, 6, 0x81, 0x82, 0x82, 0x83);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0133(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_EX(wk, 6, 0x84);
        break;

    case 1:
        Command_Attack(wk, 8, 0x20, 0xA, 0x700);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0134(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1E, 0xA, 0x70);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0135(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_EX(wk, 6, 1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1F, 0xA, 0x700);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0136(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_EX(wk, 6, 0x36);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1F, 0xA, 0x700);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0137(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FB0, 6, 1, -1);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1E, 0xA, 0x70);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0138(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_EX(wk, 6, 0x22);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1F, 0xA, 0x700);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0139(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Attack(wk, 0xC, 0xA, 0x42, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0140(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0141(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FB0, 6, 1, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0142(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Provoke(wk, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0143(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Provoke(wk, 1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x85, 0x86, 0x7F, 0x70, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0144(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Provoke(wk, 1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x2E, 0x2F, 0x30, 0x59);
        break;

    case 2:
        AI_Random_Action_Select(wk, 6, 0x85, 0x86, 0x7F, 0x70, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0145(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Provoke(wk, 1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0xFFFF, 0x30, 0xFFFF);
        break;

    case 2:
        AI_Random_Action_Select(wk, 6, 0x85, 0x73, 0x92, 0x93, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0146(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Provoke(wk, 1);
        break;

    case 1:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 2:
        Command_Attack(wk, 0xC, 0, 0xB, -1);
        break;

    case 3:
        Command_Attack(wk, 0xC, 0, 0xB, -1);
        break;

    case 4:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0147(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_EX(wk, 6, 0x85);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1F, 0xA, 0x700);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0148(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump(wk, 1);
        break;

    case 1:
        Look(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0149(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F70, -1, 8, 0x200, 1, -1, 0x20, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0150(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F70, -0x7FB0, 8, 0x20, 2, -1, -0x7FB0, 0x20);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0151(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x102);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0152(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F70, -1, 6, 1, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x102);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x102);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0153(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F70, -1, 6, 1, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0154(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F70, -1, 6, 1, -1);
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

void Passive01_0155(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait(wk, 6);
        break;

    case 1:
        Branch_By_Distance(wk, 6, 0x59, 0x5A, 0x5B, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0156(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_EX(wk, 6, 0x9B);
        break;

    case 1:
        Wait(wk, 4);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1F, 9, 0x700);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0157(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 8, 0, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0158(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 8, 1, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0159(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0160(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x30, 2, 0xF);
        break;

    case 1:
        Walk(wk, 1, 0x20, 0);
        break;

    case 2:
        Wait(wk, 3);
        break;

    case 3:
        Walk(wk, 0, 0x30, 0);
        break;

    case 4:
        Wait(wk, 9);
        break;

    case 5:
        Walk(wk, 0, 0x20, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0161(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x20, 2, 0x1B);
        break;

    case 1:
        Walk(wk, 1, 0x18, 0);
        break;

    case 2:
        Wait(wk, 8);
        break;

    case 3:
        Check_Safe_Retreat_Space(wk, 0x30, 2, 0x1B);
        break;

    case 4:
        Walk(wk, 1, 0x20, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0162(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Walk(wk, 0, 0x20, 0);
        break;

    case 1:
        Check_Safe_Retreat_Space(wk, 0x30, 2, 6);
        break;

    case 2:
        Walk(wk, 1, 0x28, 0);
        break;

    case 3:
        Wait(wk, 8);
        break;

    case 4:
        Walk(wk, 0, 0x20, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0163(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 8, 0, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0164(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F80, -1, 0, 1, -1);
        break;

    case 1:
        Lever_Attack(wk, 8, 0, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0165(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Branch_By_Distance(wk, 6, 0xA6, 0xA7, 0xA8, 1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0166(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x21, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0167(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x21, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0168(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x21, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0169(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FC0, 6, 1, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x21, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0170(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FC0, 6, 1, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x21, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0171(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0172(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x202);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0173(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -0x7F60, -0x7FA0, 0xB, 0x100, 0, -0x7FA0, -1, 0x400);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0174(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -0x7F60, -0x7FA0, 0xB, 0x20, 0, -0x7FA0, -1, 0x20);
        break;

    case 1:
        Lever_Attack(wk, 8, 0, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive01_0175(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Hi_Jump_Attack_Term(wk, -0x7F60, -0x7FA0, 0xB, 0x20, 0, -0x7FA0, -1, 0x20);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x2E, 0x2F, 0x30, -1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void (*const Passive01_Tbl[176])(PlayerEntity*) = {
    Passive01_0000, Passive01_0001, Passive01_0002, Passive01_0003, Passive01_0004, Passive01_0005, Passive01_0006,
    Passive01_0007, Passive01_0008, Passive01_0009, Passive01_0010, Passive01_0011, Passive01_0012, Passive01_0013,
    Passive01_0014, Passive01_0015, Passive01_0016, Passive01_0017, Passive01_0018, Passive01_0019, Passive01_0020,
    Passive01_0021, Passive01_0022, Passive01_0023, Passive01_0024, Passive01_0025, Passive01_0026, Passive01_0027,
    Passive01_0028, Passive01_0029, Passive01_0030, Passive01_0031, Passive01_0032, Passive01_0033, Passive01_0034,
    Passive01_0035, Passive01_0036, Passive01_0037, Passive01_0038, Passive01_0039, Passive01_0040, Passive01_0041,
    Passive01_0042, Passive01_0043, Passive01_0044, Passive01_0045, Passive01_0046, Passive01_0047, Passive01_0048,
    Passive01_0049, Passive01_0050, Passive01_0051, Passive01_0052, Passive01_0053, Passive01_0054, Passive01_0055,
    Passive01_0056, Passive01_0057, Passive01_0058, Passive01_0059, Passive01_0060, Passive01_0061, Passive01_0062,
    Passive01_0063, Passive01_0064, Passive01_0065, Passive01_0066, Passive01_0067, Passive01_0068, Passive01_0069,
    Passive01_0070, Passive01_0071, Passive01_0072, Passive01_0073, Passive01_0074, Passive01_0075, Passive01_0076,
    Passive01_0077, Passive01_0078, Passive01_0079, Passive01_0080, Passive01_0081, Passive01_0082, Passive01_0083,
    Passive01_0084, Passive01_0085, Passive01_0086, Passive01_0087, Passive01_0088, Passive01_0089, Passive01_0090,
    Passive01_0091, Passive01_0092, Passive01_0093, Passive01_0094, Passive01_0095, Passive01_0096, Passive01_0097,
    Passive01_0098, Passive01_0099, Passive01_0100, Passive01_0101, Passive01_0102, Passive01_0103, Passive01_0104,
    Passive01_0105, Passive01_0106, Passive01_0107, Passive01_0108, Passive01_0109, Passive01_0110, Passive01_0111,
    Passive01_0112, Passive01_0113, Passive01_0114, Passive01_0115, Passive01_0116, Passive01_0117, Passive01_0118,
    Passive01_0119, Passive01_0120, Passive01_0121, Passive01_0122, Passive01_0123, Passive01_0124, Passive01_0125,
    Passive01_0126, Passive01_0127, Passive01_0128, Passive01_0129, Passive01_0130, Passive01_0131, Passive01_0132,
    Passive01_0133, Passive01_0134, Passive01_0135, Passive01_0136, Passive01_0137, Passive01_0138, Passive01_0139,
    Passive01_0140, Passive01_0141, Passive01_0142, Passive01_0143, Passive01_0144, Passive01_0145, Passive01_0146,
    Passive01_0147, Passive01_0148, Passive01_0149, Passive01_0150, Passive01_0151, Passive01_0152, Passive01_0153,
    Passive01_0154, Passive01_0155, Passive01_0156, Passive01_0157, Passive01_0158, Passive01_0159, Passive01_0160,
    Passive01_0161, Passive01_0162, Passive01_0163, Passive01_0164, Passive01_0165, Passive01_0166, Passive01_0167,
    Passive01_0168, Passive01_0169, Passive01_0170, Passive01_0171, Passive01_0172, Passive01_0173, Passive01_0174,
    Passive01_0175
};
