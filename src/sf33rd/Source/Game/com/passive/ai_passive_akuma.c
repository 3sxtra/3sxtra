/**
 * @file ai_passive_akuma.c
 * COM Passive: Akuma/Gouki
 */

#include "sf33rd/Source/Game/com/passive/ai_passive_akuma.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "sf33rd/Source/Game/engine/state_user.h"

void (*const Passive14_Tbl[253])();

void Passive14(PlayerEntity* wk) {
    Passive14_Tbl[(s16)g_state.Pattern_Index[wk->wu.id]](wk);
}

void Passive14_0000(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xD, g_state.M_Lv[wk->wu.id]);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0001(PlayerEntity* wk) {
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

void Passive14_0002(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 1, -1);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x10);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0003(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FDC, 0, 1, -1);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0004(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FE0, 0, 1, -1);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0005(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x10, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 1, -1);
        break;

    case 2:
        Lever_Attack(wk, 8, 0, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0006(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x10, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 1, -1);
        break;

    case 2:
        Lever_Attack(wk, 8, 1, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0007(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 2:
        Normal_Attack(wk, 0xD, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0008(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0009(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 3:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0010(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x3F, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x1E, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0011(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 3:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 5:
        Wait(wk, 3);
        break;

    case 6:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0012(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x20, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0013(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7F00, 6, 6, 0x7C);
        break;

    case 1:
        Command_Attack(wk, 8, 0x21, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0014(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FC8, 6, 6, 0x7C);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1F, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0015(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x7F, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FD0, 6, 6, 0x7C);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0016(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 3:
        Jump_Command_Attack(wk, 9, 0x20, 8, -1);
        break;

    case 4:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 5:
        Wait(wk, 3);
        break;

    case 6:
        Jump_Command_Attack(wk, 0xC, 0x1E, 10, -1);
        break;

    case 7:
        Wait(wk, 3);
        break;

    case 8:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0xFFFF, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0017(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 3:
        Jump_Command_Attack(wk, 0xC, 0x1E, 10, -1);
        break;

    case 4:
        Wait(wk, 4);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0018(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x20, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0019(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 1, -1);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x42);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1E, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0020(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x7F, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF0, 0, 1, -1);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0021(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7F80, 6, 6, 0x7C);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0022(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FB0, 6, 6, 0x7C);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x20, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0023(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2F, 10, -1, -1, 0x30, 0, -1, -1, 0xFFFF);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0024(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0025(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x10, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 1, -1);
        break;

    case 3:
        Lever_Attack(wk, 8, 0, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0026(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x10, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 1, -1);
        break;

    case 3:
        Lever_Attack(wk, 8, 1, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0027(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 3:
        Normal_Attack(wk, 0xD, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0028(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 3:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 4:
        Jump_Command_Attack(wk, 0xC, 0x1E, 10, -1);
        break;

    case 5:
        Wait(wk, 4);
        break;

    case 6:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0029(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF0, 6, 1, -1);
        break;

    case 3:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 4:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    case 5:
        Wait(wk, 1);
        break;

    case 6:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0030(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 3:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 4:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 5:
        Jump_Command_Attack(wk, 0xB, 0x20, 8, -1);
        break;

    case 6:
        Wait(wk, 3);
        break;

    case 7:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0031(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 3:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x20, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0032(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x7F, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FD0, 6, 6, 0x7C);
        break;

    case 3:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0033(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 3:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 4:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0034(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x3F, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 3:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    case 5:
        Jump_Command_Attack(wk, 8, 0x1E, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0035(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 3:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 4:
        Jump_Command_Attack(wk, 0xB, 0x20, 8, -1);
        break;

    case 5:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 6:
        Wait(wk, 3);
        break;

    case 7:
        Jump_Command_Attack(wk, 0xC, 0x1E, 10, -1);
        break;

    case 8:
        Wait(wk, 3);
        break;

    case 9:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0xFFFF, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0036(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0x30, 0x31, 0x7F);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0037(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7FA0, -0x7FA0, 6, 6, 0x35);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0x30, 0x31, 0x7F);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0038(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0x30, 0x31, 0x7F);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0039(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FB0, 6, 6, 0x35);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0x30, 0x31, 0x7F);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0040(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait(wk, 3);
        break;

    case 1:
        Check_Enemy_Distance(wk, -0x7FC0, -0x7FC8, 6, 6, 0x35);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0041(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x7F, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FD0, 6, 6, 0x35);
        break;

    case 3:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0042(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 1, -1, -1);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0043(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA0, 0x38, 8, 0x200, 2, -0x7FA0, -1, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0044(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA0, 0x38, 8, 0x400, 2, -0x7FA0, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0045(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2F, 10, -1, -1, 0x10, 2, -1, -1, 0xFFFF);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0xFFFF, 0xFFFF, 0x31, 0x7F);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1E, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0046(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x7F, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF0, 6, 1, -1);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    case 4:
        Wait(wk, 1);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0047(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF0, 6, 1, -1);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Jump_Command_Attack(wk, 0xC, 0x1E, 10, -1);
        break;

    case 4:
        Wait(wk, 3);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0048(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7F80, 0x20, 8, 0x400, 1, -0x7F80, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0049(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7FB0, -0x7FB0, 6, 6, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0050(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7FC0, -0x7FF0, 6, 1, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x100);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0051(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7FB0, -0x7FC0, 0, 1, -1);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x102);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0052(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0053(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Forced_Guard(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0054(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0055(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 3, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0056(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0057(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, -1);
        break;

    case 1:
        Lever_Attack(wk, 8, 0, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0058(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 3, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0059(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 3, -1);
        break;

    case 1:
        Lever_Attack(wk, 8, 0, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0060(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0xBF, 0);
        break;

    case 1:
        Wait_Get_Up(wk, 0, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0061(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0xBF, 0);
        break;

    case 1:
        Wait_Get_Up(wk, 3, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0062(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0xBF, 0);
        break;

    case 1:
        Wait_Get_Up(wk, 0, -1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0063(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Miscellaneous_Conditions(wk, 4, 6, 0x37);
        break;

    case 1:
        Provoke(wk, -1);
        break;

    case 2:
        Next_Another_Menu(wk, 6, 0x39);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0064(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0x7F, 0);
        break;

    case 1:
        Wait_Get_Up(wk, 0, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0065(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0x7F, 0);
        break;

    case 1:
        Wait_Get_Up(wk, 3, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0066(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0x7F, 0);
        break;

    case 1:
        Wait_Get_Up(wk, 0, -1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0067(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 3, -1);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0068(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, 0);
        break;

    case 1:
        Check_Safe_Retreat_Space(wk, 0xE0, 6, 0x45);
        break;

    case 2:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FD0, 0xB, 0x200, 0, -0x7F80, -1, 0x400);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1E, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0069(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, 0);
        break;

    case 1:
        Branch_Wait_Area(wk, 0x14, 0xF, 5, 1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1F, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0070(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, 0);
        break;

    case 1:
        Branch_Wait_Area(wk, 0x14, 0xF, 5, 1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0071(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x7F, 2);
        break;

    case 1:
        Wait_Get_Up(wk, 0, -1);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0072(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x7F, 2);
        break;

    case 1:
        Wait_Get_Up(wk, 0, -1);
        break;

    case 2:
        Lever_Attack(wk, 8, 0, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0073(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, 0);
        break;

    case 1:
        Branch_Wait_Area(wk, 10, 7, 3, 1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x21, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0074(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, 0);
        break;

    case 1:
        Branch_Wait_Area(wk, 10, 7, 3, 1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x21, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0075(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, 0);
        break;

    case 1:
        Branch_Wait_Area(wk, 10, 7, 3, 1);
        break;

    case 2:
        Command_Attack(wk, 8, 0x21, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0076(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, 0);
        break;

    case 1:
        Branch_Wait_Area(wk, 0xF, 10, 5, 1);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x20, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0077(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 0, 0);
        break;

    case 1:
        Check_Safe_Retreat_Space(wk, 0xE0, 6, 0x45);
        break;

    case 2:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FE0, 0xB, 0x200, 0, -0x7F80, -1, 0x400);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x20, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0078(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Miscellaneous_Conditions(wk, 4, 6, 0x37);
        break;

    case 1:
        Provoke(wk, -1);
        break;

    case 2:
        Next_Another_Menu(wk, 6, 0x46);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0079(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0080(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0081(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x300, 6, 0xE3);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 10, -1);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x47);
        break;

    case 3:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0082(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x80, 6, 0xE3);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 10, -1);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x47);
        break;

    case 3:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0083(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x200, 6, 0xE4);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 8, -1);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x47);
        break;

    case 3:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0084(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x100, 6, 0xE4);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 8, -1);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x47);
        break;

    case 3:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0085(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 1:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    case 2:
        Wait(wk, 1);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0086(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0087(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x102);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x20, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0088(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x102);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0089(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x102);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0090(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x20, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0091(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0092(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1E, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0093(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0094(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 3:
        Command_Attack(wk, 8, 0x1F, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0095(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 2:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    case 3:
        Wait(wk, 1);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0096(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 2:
        Jump_Command_Attack(wk, 0xC, 0x1E, 10, -1);
        break;

    case 3:
        Wait(wk, 3);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0097(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0xBF, 1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x15, 0x1D, 0x1E, 0x1F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0098(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0099(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0100(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0101(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 10, 0x202);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0102(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 10, 0x102);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0103(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 10, 0x202);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0104(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 10, 0x202);
        break;

    case 2:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    case 3:
        Wait(wk, 1);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0105(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 1, -1, -1);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0106(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 1, -1, -1);
        break;

    case 1:
        Command_Attack(wk, 8, 0x21, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0107(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0108(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0109(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0110(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0111(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0112(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x20);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1E, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0113(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 8, -1, -1, 0x20, 2, -0x7F80, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0114(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 9, -1, -1, 0x20, 2, -0x7F80, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0115(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 0xA, -1, -1, 0x20, 2, -0x7F80, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0116(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 8, -1, -1, 0x20, 0, -0x7F80, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0117(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 9, -1, -1, 0x20, 0, -0x7F80, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0118(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 0xA, -1, -1, 0x20, 0, -0x7F80, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0119(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 8, -1, -1, 0x20, 1, -0x7F80, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0120(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 9, -1, -1, 0x20, 1, -0x7F80, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0121(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 0xA, -1, -1, 0x20, 1, -0x7F80, -1, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0122(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x71, 0x71, 0x72, 0x73, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0123(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x74, 0x74, 0x75, 0x76, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0124(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0125(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x57, 0x57, 0x58, 0x59, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0126(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x5A, 0x5B, 0x5C, 0x5D, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0127(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x57, 0x57, 0x58, 0x59, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0128(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x5A, 0x5B, 0x5C, 0x5D, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0129(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x10, 2);
        break;

    case 1:
        Lever_Attack(wk, 8, 0, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0130(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x10, 2);
        break;

    case 1:
        Lever_Attack(wk, 8, 1, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0131(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x81, 0x81, 0x82, 0x82, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0132(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1F, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0133(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1F, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0134(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0135(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x21, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0136(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x21, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0137(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x21, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0138(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x84, 0x84, 0x85, 0x86, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0139(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x87, 0x87, 0x88, 0x89, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0140(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 9, 0x20, 8, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0141(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 9, 0x20, 9, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0142(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 9, 0x20, 10, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0143(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x8C, 0x8C, 0x8D, 0x8E, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0144(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0145(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x90, 0xFF, 0xFF, 0xFF, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0146(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 0xC, 0x2E, 8, -1, -1, 0x34, 0, -1, -1, 0xFFFF);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x2F, 0xFFFF, 0xFFFF, 0xBF);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0147(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_SA_Full(wk, 6, 0x76);
        break;

    case 1:
        Only_Shot(wk, 0x10);
        break;

    case 2:
        Wait(wk, 1);
        break;

    case 3:
        Only_Shot(wk, 0x10);
        break;

    case 4:
        Wait(wk, 1);
        break;

    case 5:
        Lever_On(wk, 0, 0);
        break;

    case 6:
        Wait(wk, 1);
        break;

    case 7:
        Only_Shot(wk, 0x100);
        break;

    case 8:
        Wait(wk, 1);
        break;

    case 9:
        Only_Shot(wk, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0148(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_SA(wk, 6, 0x76);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x8014, 10, -1, -1, 0x20, 0, -1, -1, 0xFFFF);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0149(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x92, 0x92, 0x93, 0x94, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0150(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x81, 0x82, 0x81, 0x82, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0151(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, 8, 0x49, 0xB, 0x202, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 2:
        Command_Attack(wk, 0xB, 0x1F, 10, -1);
        break;

    case 3:
        Wait(wk, 1);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x2F, 0x34, 0x34, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0152(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Hi_Jump_Attack_Term(wk, -1, 0x61, 0xB, 0x202, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xB, 0x1F, 10, -1);
        break;

    case 4:
        Wait(wk, 1);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2F, 0x34, 0x34, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0153(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 8, 0, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0154(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 1);
        break;

    case 1:
        Normal_Attack(wk, 0xD, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0155(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 0xB, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0156(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 0xB, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xB, 0x1F, 10, -1);
        break;

    case 4:
        Wait(wk, 1);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2F, 0x34, 0x34, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0157(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 0xB, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0158(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 0xB, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xB, 0x1F, 10, -1);
        break;

    case 4:
        Wait(wk, 1);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2F, 0x34, 0x34, 0x7F);
        break;

    case 6:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0159(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 0xB, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 2:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0160(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 0xB, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 2:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    case 3:
        Wait(wk, 1);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0161(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 0xB, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 2:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0162(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 0xB, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 2:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    case 3:
        Wait(wk, 1);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0163(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 1, -1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0164(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 1, -1, -1);
        break;

    case 1:
        Wait_Get_Up(wk, 3, 0);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x32, 0xFFFF, 0xFFFF, 0xBF);
        break;

    case 3:
        AI_Random_Action_Select(wk, 6, 0x71, 0x71, 0x72, 0x73, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0165(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Wait_Get_Up(wk, 3, 0);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x32, 0xFFFF, 0xFFFF, 0xBF);
        break;

    case 3:
        AI_Random_Action_Select(wk, 6, 0x74, 0x74, 0x75, 0x76, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0166(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x84, 0x84, 0x85, 0x86, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0167(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 3:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 4:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0168(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 3:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 4:
        Command_Attack(wk, 0xB, 0x1F, 10, -1);
        break;

    case 5:
        Wait(wk, 1);
        break;

    case 6:
        Check_Super_Art_Conditions(wk, 0x2F, 0x34, 0x34, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0169(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x32, 0xFFFF, 0xFFFF, 0xBF);
        break;

    case 4:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0170(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Turn_Over_On(wk);
        break;

    case 2:
        Hi_Jump_Attack_Term(wk, -1, 0x61, 9, 0x202, 0, -0x7F80, -1, 0x400);
        break;

    case 3:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x32, 0xFFFF, 0xFFFF, 0xBF);
        break;

    case 5:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0171(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7F);
        break;

    case 4:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0172(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0173(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    case 4:
        Wait(wk, 1);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0174(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x32, 0xFFFF, 0xFFFF, 0xBF);
        break;

    case 3:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0175(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Hi_Jump_Attack_Term(wk, -1, 0x61, 9, 0x202, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x32, 0xFFFF, 0xFFFF, 0xBF);
        break;

    case 4:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0176(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7F);
        break;

    case 3:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0177(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x3F, 0x40, 0x41, 0x42, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0178(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0179(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xB, 0x1F, 10, -1);
        break;

    case 4:
        Wait(wk, 1);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2F, 0x34, 0x34, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0180(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x32, 0xFFFF, 0xFFFF, 0xBF);
        break;

    case 3:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0181(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Hi_Jump_Attack_Term(wk, -1, 0x61, 9, 0x202, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x32, -1, -1, 0xBF);
        break;

    case 4:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0182(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7F);
        break;

    case 3:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0183(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Hi_Jump_Attack_Term(wk, -1, 0x61, 9, 0x202, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7F);
        break;

    case 4:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0184(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Command_Attack(wk, 0xB, 0x1F, 10, -1);
        break;

    case 2:
        Wait(wk, 1);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x2F, 0xFFFF, 0xFFFF, 0xFFFF);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0185(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 2:
        Command_Attack(wk, 0xB, 0x1F, 10, -1);
        break;

    case 3:
        Wait(wk, 1);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x2F, 0x34, 0x34, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0186(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0xB2, 0xB3, 0xB4, 0xB6, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0187(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 2:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0188(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x32, 0xFFFF, 0xFFFF, 0xBF);
        break;

    case 3:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0189(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 9, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7F);
        break;

    case 3:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0190(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0xB9, 0xBB, 0xBC, 0xBD, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0191(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Hi_Jump_Attack_Term(wk, -1, 0x61, 0xB, 0x202, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x2F, 0x34, 0x34, 0x7F);
        break;

    case 4:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0192(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Hi_Jump_Attack_Term(wk, -1, 0x61, 0xB, 0x202, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 4:
        Jump_Command_Attack(wk, 0xB, 0x20, 8, -1);
        break;

    case 5:
        Wait(wk, 3);
        break;

    case 6:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0193(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0xB5, 0xB7, 0xBF, 0xC0, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0194(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0xB2, 0xB3, 0xB4, 0xB6, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0195(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0xB9, 0xBB, 0xBC, 0xBD, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0196(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0xB5, 0xB7, 0xBF, 0xC0, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0197(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Approach_Walk(wk, 0x47, 2);
        break;

    case 2:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 0x7C);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7F);
        break;

    case 4:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 5:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 6:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 7:
        Wait(wk, 3);
        break;

    case 8:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0198(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0199(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Attack_Complete(wk, 1, 1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0200(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x4C, 0x4D, 0x4E, 0x4F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0201(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x50, 0x51, 0x52, 0x53, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0202(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x54, 0x55, 0x56, 0x57, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0203(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x58, 0x59, 0x5A, 0x5B, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0204(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x4C, 0x4D, 0x4E, 0x4F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0205(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x50, 0x51, 0x52, 0x53, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0206(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x54, 0x55, 0x56, 0x57, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0207(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x58, 0x59, 0x5A, 0x5B, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0208(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xB5, 3);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2F, 10, -1, -0x7F60, 0x50, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x4C, 0x4D, 0x4E, 0x4F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0209(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2F, 10, -1, -0x7F60, 0x50, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x50, 0x51, 0x52, 0x53, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0210(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xAD, 3);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2F, 10, -1, -0x7F60, 0x50, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x54, 0x55, 0x56, 0x57, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0211(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2F, 10, -1, -0x7F60, 0x50, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x58, 0x59, 0x5A, 0x5B, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0212(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Super_Art_Conditions(wk, 0x32, 0xFFFF, 0xFFFF, 0xBF);
        break;

    case 1:
        AI_Random_Action_Select(wk, 6, 0x71, 0x71, 0x72, 0x73, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0213(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x4C, 0x4D, 0x4E, 0x4F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0214(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x50, 0x51, 0x52, 0x53, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0215(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x54, 0x55, 0x56, 0x57, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0216(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 1);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x58, 0x59, 0x5A, 0x5B, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0217(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 1, -1, -1);
        break;

    case 1:
        Turn_Over_On(wk);
        break;

    case 2:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 3:
        AI_Random_Action_Select(wk, 2, 0x4C, 0x4D, 0x4E, 0x4F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0218(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 1, -1, -1);
        break;

    case 1:
        Turn_Over_On(wk);
        break;

    case 2:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 3:
        AI_Random_Action_Select(wk, 2, 0x50, 0x51, 0x52, 0x53, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0219(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 1, -1, -1);
        break;

    case 1:
        Turn_Over_On(wk);
        break;

    case 2:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 3:
        AI_Random_Action_Select(wk, 2, 0x54, 0x55, 0x56, 0x57, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0220(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 1, -1, -1);
        break;

    case 1:
        Turn_Over_On(wk);
        break;

    case 2:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 3:
        AI_Random_Action_Select(wk, 2, 0x58, 0x59, 0x5A, 0x5B, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0221(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x300, 6, 0xE1);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 10, -1);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x47);
        break;

    case 3:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0222(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x80, 6, 0xE1);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 10, -1);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x47);
        break;

    case 3:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0223(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x200, 6, 0xE2);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 8, -1);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x47);
        break;

    case 3:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0224(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x100, 6, 0xE2);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 8, -1);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x47);
        break;

    case 3:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0225(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1C, 10, -1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x47);
        break;

    case 2:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0226(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1C, 8, -1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x47);
        break;

    case 2:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0227(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1C, 10, -1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x47);
        break;

    case 2:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0228(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1C, 8, -1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x47);
        break;

    case 2:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0229(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x22, 8, -1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x47);
        break;

    case 2:
        Check_Enemy_Distance(wk, -0x7FB0, -1, 5, 6, 1);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0230(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 1);
        break;

    case 1:
        Check_SA_Full(wk, 6, 0x11);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7F);
        break;

    case 3:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 4:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 5:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 6:
        Wait(wk, 3);
        break;

    case 7:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0231(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    case 1:
        Check_SA_Full(wk, 6, 0x11);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0xBF);
        break;

    case 3:
        Normal_Attack(wk, 0xC, 0x40);
        break;

    case 4:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 5:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 6:
        Wait(wk, 3);
        break;

    case 7:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0232(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Look(wk, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0233(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Look(wk, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0234(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Attack_Complete(wk, 1, 1);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 6, 6, 1);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 8, 0x1F, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0235(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F80, -1, 6, 6, 1);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0236(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F68, -1, 6, 6, 1);
        break;

    case 1:
        Check_Enemy_Distance(wk, -1, -0x7FF8, 4, 6, 0xEB);
        break;

    case 2:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xC, 0x1F, 10, -1);
        break;

    case 4:
        Wait(wk, 1);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2F, 0x30, 0x31, 0x7F);
        break;
    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0237(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Enemy_Distance(wk, -0x7F80, -1, 6, 6, 1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1E, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0238(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x10);
        break;

    case 1:
        Normal_Attack(wk, 0xC, 0x202);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0239(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0240(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 3, 0);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x84, 0x85, 0x86, 0x87, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0241(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x57, 3);
        break;

    case 1:
        Wait_Get_Up(wk, 3, 0);
        break;

    case 2:
        Turn_Over_On(wk);
        break;

    case 3:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 4:
        AI_Random_Action_Select(wk, 2, 0x89, 0x8A, 0x8B, 0x8C, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0242(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait_Get_Up(wk, 3, 0);
        break;

    case 1:
        Turn_Over_On(wk);
        break;

    case 2:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 3:
        AI_Random_Action_Select(wk, 2, 0x89, 0x8A, 0x8B, 0x8C, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0243(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x26, 0x27, 0x28, 0x29, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0244(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x57, 3);
        break;

    case 1:
        Wait_Get_Up(wk, 3, 0);
        break;

    case 2:
        Turn_Over_On(wk);
        break;

    case 3:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 4:
        AI_Random_Action_Select(wk, 2, 0x4C, 0x4D, 0x4E, 0x4F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0245(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x57, 3);
        break;

    case 1:
        Wait_Get_Up(wk, 3, 0);
        break;

    case 2:
        Turn_Over_On(wk);
        break;

    case 3:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 4:
        AI_Random_Action_Select(wk, 2, 0x50, 0x51, 0x52, 0x53, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0246(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x57, 3);
        break;

    case 1:
        Wait_Get_Up(wk, 3, 0);
        break;

    case 2:
        Turn_Over_On(wk);
        break;

    case 3:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 4:
        AI_Random_Action_Select(wk, 2, 0x54, 0x55, 0x56, 0x57, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0247(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x57, 3);
        break;

    case 1:
        Wait_Get_Up(wk, 3, 0);
        break;

    case 2:
        Turn_Over_On(wk);
        break;

    case 3:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 4:
        AI_Random_Action_Select(wk, 2, 0x58, 0x59, 0x5A, 0x5B, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0248(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x89, 0x8A, 0x8B, 0x8C, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0249(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xC, 0x402);
        break;

    case 1:
        Command_Attack(wk, 0xC, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0250(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0251(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x8019, 0xA, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 0xB, 0x20, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void Passive14_0252(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_SA_Full(wk, 6, 0x7C);
        break;

    case 1:
        Check_Enemy_Distance(wk, -0x7F30, -1, 5, 2, 0);
        break;

    case 2:
        Command_Attack(wk, 8, 0x8019, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

void (*const Passive14_Tbl[253])(PlayerEntity*) = {
    Passive14_0000, Passive14_0001, Passive14_0002, Passive14_0003, Passive14_0004, Passive14_0005, Passive14_0006,
    Passive14_0007, Passive14_0008, Passive14_0009, Passive14_0010, Passive14_0011, Passive14_0012, Passive14_0013,
    Passive14_0014, Passive14_0015, Passive14_0016, Passive14_0017, Passive14_0018, Passive14_0019, Passive14_0020,
    Passive14_0021, Passive14_0022, Passive14_0023, Passive14_0024, Passive14_0025, Passive14_0026, Passive14_0027,
    Passive14_0028, Passive14_0029, Passive14_0030, Passive14_0031, Passive14_0032, Passive14_0033, Passive14_0034,
    Passive14_0035, Passive14_0036, Passive14_0037, Passive14_0038, Passive14_0039, Passive14_0040, Passive14_0041,
    Passive14_0042, Passive14_0043, Passive14_0044, Passive14_0045, Passive14_0046, Passive14_0047, Passive14_0048,
    Passive14_0049, Passive14_0050, Passive14_0051, Passive14_0052, Passive14_0053, Passive14_0054, Passive14_0055,
    Passive14_0056, Passive14_0057, Passive14_0058, Passive14_0059, Passive14_0060, Passive14_0061, Passive14_0062,
    Passive14_0063, Passive14_0064, Passive14_0065, Passive14_0066, Passive14_0067, Passive14_0068, Passive14_0069,
    Passive14_0070, Passive14_0071, Passive14_0072, Passive14_0073, Passive14_0074, Passive14_0075, Passive14_0076,
    Passive14_0077, Passive14_0078, Passive14_0079, Passive14_0080, Passive14_0081, Passive14_0082, Passive14_0083,
    Passive14_0084, Passive14_0085, Passive14_0086, Passive14_0087, Passive14_0088, Passive14_0089, Passive14_0090,
    Passive14_0091, Passive14_0092, Passive14_0093, Passive14_0094, Passive14_0095, Passive14_0096, Passive14_0097,
    Passive14_0098, Passive14_0099, Passive14_0100, Passive14_0101, Passive14_0102, Passive14_0103, Passive14_0104,
    Passive14_0105, Passive14_0106, Passive14_0107, Passive14_0108, Passive14_0109, Passive14_0110, Passive14_0111,
    Passive14_0112, Passive14_0113, Passive14_0114, Passive14_0115, Passive14_0116, Passive14_0117, Passive14_0118,
    Passive14_0119, Passive14_0120, Passive14_0121, Passive14_0122, Passive14_0123, Passive14_0124, Passive14_0125,
    Passive14_0126, Passive14_0127, Passive14_0128, Passive14_0129, Passive14_0130, Passive14_0131, Passive14_0132,
    Passive14_0133, Passive14_0134, Passive14_0135, Passive14_0136, Passive14_0137, Passive14_0138, Passive14_0139,
    Passive14_0140, Passive14_0141, Passive14_0142, Passive14_0143, Passive14_0144, Passive14_0145, Passive14_0146,
    Passive14_0147, Passive14_0148, Passive14_0149, Passive14_0150, Passive14_0151, Passive14_0152, Passive14_0153,
    Passive14_0154, Passive14_0155, Passive14_0156, Passive14_0157, Passive14_0158, Passive14_0159, Passive14_0160,
    Passive14_0161, Passive14_0162, Passive14_0163, Passive14_0164, Passive14_0165, Passive14_0166, Passive14_0167,
    Passive14_0168, Passive14_0169, Passive14_0170, Passive14_0171, Passive14_0172, Passive14_0173, Passive14_0174,
    Passive14_0175, Passive14_0176, Passive14_0177, Passive14_0178, Passive14_0179, Passive14_0180, Passive14_0181,
    Passive14_0182, Passive14_0183, Passive14_0184, Passive14_0185, Passive14_0186, Passive14_0187, Passive14_0188,
    Passive14_0189, Passive14_0190, Passive14_0191, Passive14_0192, Passive14_0193, Passive14_0194, Passive14_0195,
    Passive14_0196, Passive14_0197, Passive14_0198, Passive14_0199, Passive14_0200, Passive14_0201, Passive14_0202,
    Passive14_0203, Passive14_0204, Passive14_0205, Passive14_0206, Passive14_0207, Passive14_0208, Passive14_0209,
    Passive14_0210, Passive14_0211, Passive14_0212, Passive14_0213, Passive14_0214, Passive14_0215, Passive14_0216,
    Passive14_0217, Passive14_0218, Passive14_0219, Passive14_0220, Passive14_0221, Passive14_0222, Passive14_0223,
    Passive14_0224, Passive14_0225, Passive14_0226, Passive14_0227, Passive14_0228, Passive14_0229, Passive14_0230,
    Passive14_0231, Passive14_0232, Passive14_0233, Passive14_0234, Passive14_0235, Passive14_0236, Passive14_0237,
    Passive14_0238, Passive14_0239, Passive14_0240, Passive14_0241, Passive14_0242, Passive14_0243, Passive14_0244,
    Passive14_0245, Passive14_0246, Passive14_0247, Passive14_0248, Passive14_0249, Passive14_0250, Passive14_0251,
    Passive14_0252
};
