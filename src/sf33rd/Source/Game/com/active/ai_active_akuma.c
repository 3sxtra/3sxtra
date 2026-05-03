/**
 * @file ai_active_akuma.c
 * COM Active: Akuma/Gouki
 */

#include "sf33rd/Source/Game/com/active/ai_active_akuma.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "sf33rd/Source/Game/engine/state_user.h"

static void (*const Pattern14_Tbl[151])();

/** @brief Makoto active AI pattern entry point. */
void Computer14(PlayerEntity* wk) {
    Pattern14_Tbl[(s16)g_state.Pattern_Index[wk->wu.id]](wk);
}

static void Pattern14_0000(PlayerEntity* wk) {
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

static void Pattern14_0001(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Wait(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0002(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x10);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0003(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0004(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0005(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x100);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0006(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0007(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0008(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x12);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0009(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x22);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0010(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x42);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0011(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x102);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0012(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0013(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0014(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1F, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0015(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1F, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0016(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0017(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1F, 8, -1);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0018(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0019(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0020(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x20, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0021(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x20, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0022(PlayerEntity* wk) {
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

static void Pattern14_0023(PlayerEntity* wk) {
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

static void Pattern14_0024(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0x7F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0025(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Keep_Away(wk, 0xBF, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0026(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Approach_Walk(wk, 0xbf, 3);
        break;

    case 2:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 8, -1, -1, 0x30, 0, -0x7F80, -1, 0x400);
        break;

    case 3:
        Normal_Attack(wk, 0xb, 0x202);
        break;

    case 4:
        Jump_Command_Attack(wk, 0xb, 0x20, 8, -1);
        break;

    case 5:
        Wait(wk, 5);
        break;

    case 6:
        Check_Super_Art_Conditions(wk, 0x2f, 0xffff, 0x31, 0x7f);
        break;

    case 7:
        Jump_Command_Attack(wk, 8, 0x1e, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0027(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Attack(wk, 8, 0, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0028(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 8, -1, -1, 0x30, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Lever_Attack(wk, 8, 0, 0x20);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0029(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 0xB, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xC, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0030(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xbf, 3);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, 0x8058, 0x8038, 0xb, 0x400, 0, 0x8080, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xb, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xc, 0x1f, 10, -1);
        break;

    case 4:
        Wait(wk, 1);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2f, 0x30, 0x31, 0x7f);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0031(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC8, 0xB, 0x400, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xC, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0032(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xbf, 2);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, 0x8058, 0x8038, 0xb, 0x400, 0, 0x8080, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xb, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xc, 0x1f, 10, -1);
        break;

    case 4:
        Wait(wk, 1);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2f, 0x30, 0x31, 0x7f);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0033(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Approach_Walk(wk, 0xbf, 2);
        break;

    case 2:
        Jump_Command_Attack_Term(wk, 8, 0x2e, 8, -1, -1, 0x30, 0, -0x7F80, -1, 0x400);
        break;

    case 3:
        Normal_Attack(wk, 0xb, 0x202);
        break;

    case 4:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 5:
        Wait(wk, 3);
        break;

    case 6:
        Jump_Command_Attack(wk, 8, 0x1e, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0034(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 2:
        Jump_Command_Attack_Term(wk, 0xB, 0x2F, 0xA, -1, -1, 0x30, 0, -1, -1, -1);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0035(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Hi_Jump_Attack_Term(wk, -1, 0x61, 8, 0x202, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 3:
        Command_Attack(wk, 8, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0036(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Hi_Jump_Attack_Term(wk, -1, 0x61, 8, 0x202, 0, 0x8080, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xb, 0x202);
        break;

    case 3:
        Command_Attack(wk, 0xc, 0x1f, 10, -1);
        break;

    case 4:
        Wait(wk, 1);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2f, 0x30, 0x31, 0x7f);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0037(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Hi_Jump_Attack_Term(wk, -1, 0x61, 8, 0x202, 0, 0x8080, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 0xb, 0x202);
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
        Jump_Command_Attack(wk, 8, 0x1e, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0038(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Miscellaneous_Conditions(wk, 3, 2, 0);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0039(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Miscellaneous_Conditions(wk, 3, 2, 0);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0040(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Miscellaneous_Conditions(wk, 3, 2, 0);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0041(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Miscellaneous_Conditions(wk, 3, 2, 0);
        break;

    case 1:
        Jump_Command_Attack(wk, 0xB, 0x20, 8, -1);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0042(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Miscellaneous_Conditions(wk, 3, 2, 0);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 2:
        Command_Attack(wk, 0xC, 0x1F, 0xA, -1);
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

static void Pattern14_0043(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Miscellaneous_Conditions(wk, 3, 2, 0);
        break;

    case 1:
        Normal_Attack(wk, 0xB, 0x202);
        break;

    case 2:
        Command_Attack(wk, 8, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0044(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x71, 0x71, 0x72, 0x73, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0045(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x74, 0x74, 0x75, 0x76, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0046(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0047(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x8014, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0048(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x8015, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0049(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack(wk, 8, 0x8016, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0050(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Branch_By_Distance(wk, 2, 0x37, 0x37, 0x36, 0x35);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0051(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x8016, 0xA, -1, -0x7FB0, -0x7FC0, 0, -1, -1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0052(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_SA_Full(wk, 6, 0x7c);
        break;

    case 1:
        Check_Enemy_Distance(wk, 0x80d0, -1, 5, 2, 0);
        break;

    case 2:
        Only_Shot(wk, 0x10);
        break;

    case 3:
        Wait(wk, 1);
        break;

    case 4:
        Only_Shot(wk, 0x10);
        break;

    case 5:
        Wait(wk, 1);
        break;

    case 6:
        Lever_On(wk, 0, 0);
        break;

    case 7:
        Wait(wk, 1);
        break;

    case 8:
        Only_Shot(wk, 0x100);
        break;

    case 9:
        Wait(wk, 1);
        break;

    case 10:
        Only_Shot(wk, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0053(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x8014, 8, -1, -1, 0x20, 0, -1, -1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0054(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x8014, 8, -1, -1, 0x20, 2, -1, -1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0055(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x8014, 8, -1, -1, 0x20, 1, -1, -1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0056(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x32, 0xFFFF, 0xFFFF, 0xBF);
        break;

    case 2:
        AI_Random_Action_Select(wk, 6, 0x74, 0x74, 0x75, 0x76, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0057(PlayerEntity* wk) {
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

static void Pattern14_0058(PlayerEntity* wk) {
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
        Command_Attack(wk, 0xC, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0059(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xbf, 2);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, 0x8058, 0x8038, 9, 0x400, 0, 0x8080, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 3:
        Normal_Attack(wk, 0xc, 0x202);
        break;

    case 4:
        Command_Attack(wk, 0xc, 0x1f, 10, -1);
        break;

    case 5:
        Wait(wk, 1);
        break;

    case 6:
        Check_Super_Art_Conditions(wk, 0x2f, 0x30, 0x31, 0x7f);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0060(PlayerEntity* wk) {
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

static void Pattern14_0061(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xbf, 2);
        break;

    case 1:
        Turn_Over_On(wk);
        break;

    case 2:
        Hi_Jump_Attack_Term(wk, -1, 0x61, 9, 0x202, 0, 0x8080, -1, 0x400);
        break;

    case 3:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x32, 0xffff, 0xffff, 0xbf);
        break;

    case 5:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0062(PlayerEntity* wk) {
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

static void Pattern14_0063(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xbf, 2);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2e, 8, -1, -1, 0x34, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 3:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 4:
        Normal_Attack(wk, 0xc, 0x202);
        break;

    case 5:
        Command_Attack(wk, 0xc, 0x1f, 10, -1);
        break;

    case 6:
        Wait(wk, 1);
        break;

    case 7:
        Check_Super_Art_Conditions(wk, 0x2f, 0x30, 0x31, 0x7f);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0064(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xbf, 2);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 8, -1, -1, 0x34, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Lever_Attack(wk, 9, 0, 0x20);
        break;

    case 3:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 4:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 5:
        Normal_Attack(wk, 0xc, 0x202);
        break;

    case 6:
        Command_Attack(wk, 0xc, 0x1f, 10, -1);
        break;

    case 7:
        Wait(wk, 1);
        break;

    case 8:
        Check_Super_Art_Conditions(wk, 0x2f, 0x30, 0x31, 0x7f);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0065(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xbf, 2);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2e, 8, -1, -1, 0x34, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 3:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 4:
        Check_Super_Art_Conditions(wk, 0x32, -1, -1, 0xbf);
        break;

    case 5:
        AI_Random_Action_Select(wk, 6, 0x77, 0x77, 0x78, 0x79, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0066(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2E, 8, -1, -1, 0x34, 0, -0x7F80, -1, 0x400);
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

static void Pattern14_0067(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 1:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 2:
        Jump_Command_Attack_Term(wk, 0xB, 0x2F, 0xA, -1, -1, 0x40, 0, -1, -1, -1);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0068(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0, -1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0069(PlayerEntity* wk) {
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

static void Pattern14_0070(PlayerEntity* wk) {
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

static void Pattern14_0071(PlayerEntity* wk) {
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

static void Pattern14_0072(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0xB2, 0xB3, 0xB4, 0xB6, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0073(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0xB9, 0xBB, 0xBC, 0xBD, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0074(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 6, 0xB5, 0xB7, 0xBF, 0xC0, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0075(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x7F, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0076(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x202);
        break;

    case 2:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7F);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0077(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x12);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0078(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x200);
        break;

    case 3:
        Command_Attack(wk, 8, 0x1F, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0079(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 3:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 4:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0080(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0081(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0082(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 0xD, 0x20);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x102);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0083(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 4:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 5:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0084(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 4:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 5:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 6:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 7:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0085(PlayerEntity* wk) {
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
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0086(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x12);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 3:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0087(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x220);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 3:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 4:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 5:
        Lever_On(wk, 1, 2);
        break;

    case 6:
        Wait(wk, 2);
        break;

    case 7:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0088(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7F);
        break;

    case 2:
        Approach_Walk(wk, 0x10, 2);
        break;

    case 3:
        Lever_Attack(wk, 8, 0, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0089(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x100);
        break;

    case 1:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7F);
        break;

    case 2:
        Approach_Walk(wk, 0x10, 2);
        break;

    case 3:
        Lever_Attack(wk, 8, 1, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0090(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x100);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 3:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7f);
        break;

    case 4:
        Approach_Walk(wk, 0x10, 2);
        break;

    case 5:
        Lever_Attack(wk, 8, 0, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0091(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x100);
        break;

    case 1:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 2:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x20, 8, -1);
        break;

    case 4:
        Normal_Attack(wk, 9, 0x102);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x34, 0x34, 0x34, 0x7f);
        break;

    case 6:
        Approach_Walk(wk, 0x10, 2);
        break;

    case 7:
        Lever_Attack(wk, 8, 1, 0x110);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0092(PlayerEntity* wk) {
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

static void Pattern14_0093(PlayerEntity* wk) {
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

static void Pattern14_0094(PlayerEntity* wk) {
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

static void Pattern14_0095(PlayerEntity* wk) {
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

static void Pattern14_0096(PlayerEntity* wk) {
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

static void Pattern14_0097(PlayerEntity* wk) {
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

static void Pattern14_0098(PlayerEntity* wk) {
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

static void Pattern14_0099(PlayerEntity* wk) {
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

static void Pattern14_0100(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2F, 0xA, -1, -0x7F60, 0x50, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x4C, 0x4D, 0x4E, 0x4F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0101(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2F, 0xA, -1, -0x7F60, 0x50, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x50, 0x51, 0x52, 0x53, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0102(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2F, 0xA, -1, -0x7F60, 0x50, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x54, 0x55, 0x56, 0x57, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0103(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 3);
        break;

    case 1:
        Jump_Command_Attack_Term(wk, 8, 0x2F, 0xA, -1, -0x7F60, 0x50, 0, -0x7F80, -1, 0x400);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x58, 0x59, 0x5A, 0x5B, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0104(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x4C, 0x4D, 0x4E, 0x4F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0105(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x50, 0x51, 0x52, 0x53, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0106(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x54, 0x55, 0x56, 0x57, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0107(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x58, 0x59, 0x5A, 0x5B, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0108(PlayerEntity* wk) {
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

static void Pattern14_0109(PlayerEntity* wk) {
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

static void Pattern14_0110(PlayerEntity* wk) {
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

static void Pattern14_0111(PlayerEntity* wk) {
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

static void Pattern14_0112(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 1, -1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0113(PlayerEntity* wk) {
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

static void Pattern14_0114(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x300, 2, 0x76);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
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

static void Pattern14_0115(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x80, 2, 0x76);
        break;

    case 1:
        Command_Attack(wk, 8, 0x1D, 0xA, -1);
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

static void Pattern14_0116(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x200, 2, 0x77);
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

static void Pattern14_0117(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Safe_Retreat_Space(wk, 0x100, 2, 0x77);
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

static void Pattern14_0118(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1C, 0xA, -1);
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

static void Pattern14_0119(PlayerEntity* wk) {
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

static void Pattern14_0120(PlayerEntity* wk) {
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

static void Pattern14_0121(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x72, 2);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x4C, 0x4D, 0x4E, 0x4F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0122(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x72, 2);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x50, 0x51, 0x52, 0x53, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0123(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x72, 2);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x54, 0x55, 0x56, 0x57, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0124(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x72, 2);
        break;

    case 1:
        AI_Random_Action_Select(wk, 2, 0x58, 0x59, 0x5A, 0x5B, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0125(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x21, 0x3F, 0x40, 0x41, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0126(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x23, 0x24, 0x25, 0x3D, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0127(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x22, 0x43, 0x3C, 0x3E, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0128(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0xBF, 2);
        break;

    case 1:
        Look(wk, 2);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x4C, 0x4D, 0x4E, 0x4F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0129(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x72, 2);
        break;

    case 1:
        Look(wk, 2);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x4C, 0x4D, 0x4E, 0x4F, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0130(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x71, 0x72, 0x71, 0x72, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0131(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x1A, 0x1C, 0x1D, 0x1E, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0132(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x78, 2);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0133(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x86, 2);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x402);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0134(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x77, 2);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0135(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x84, 2);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x400);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0136(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x84, 0x85, 0x86, 0x87, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0137(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 0xc, 0x202);
        break;

    case 2:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 3:
        Jump_Command_Attack(wk, 0xb, 0x20, 8, -1);
        break;

    case 4:
        Wait(wk, 5);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2f, -1, 0x31, 0x7f);
        break;

    case 6:
        Jump_Command_Attack(wk, 8, 0x1e, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0138(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x202);
        break;

    case 1:
        Normal_Attack(wk, 0xc, 0x202);
        break;

    case 2:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 3:
        Jump_Command_Attack(wk, 0xb, 0x20, 8, -1);
        break;

    case 4:
        Wait(wk, 5);
        break;

    case 5:
        Check_Super_Art_Conditions(wk, 0x2f, -1, 0x31, 0x7f);
        break;

    case 6:
        Jump_Command_Attack(wk, 8, 0x1e, 10, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0139(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 0xc, 0x202);
        break;

    case 2:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 3:
        Jump_Command_Attack(wk, 0xb, 0x20, 9, -1);
        break;

    case 4:
        Wait(wk, 3);
        break;

    case 5:
        Jump_Command_Attack(wk, 8, 0x1e, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0140(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 9, 0x200);
        break;

    case 1:
        Normal_Attack(wk, 9, 0x200);
        break;

    case 2:
        Enable_Overhead_Attack_Flag(wk);
        break;

    case 3:
        Jump_Command_Attack(wk, 0xb, 0x20, 9, -1);
        break;

    case 4:
        Wait(wk, 3);
        break;

    case 5:
        Jump_Command_Attack(wk, 8, 0x1e, 9, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0141(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x89, 0x8A, 0x8B, 0x8C, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0142(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x57, 3);
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

static void Pattern14_0143(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Turn_Over_On(wk);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -1, 0x49, 9, 0x202, 0, -0x7F80, -1, 0x40);
        break;

    case 2:
        AI_Random_Action_Select(wk, 2, 0x89, 0x8A, 0x8B, 0x8C, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0144(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        AI_Random_Action_Select(wk, 2, 0x26, 0x27, 0x28, 0x29, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Pattern14_0145(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x57, 3);
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

static void Pattern14_0146(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x57, 3);
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

static void Pattern14_0147(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x57, 3);
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

static void Pattern14_0148(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Approach_Walk(wk, 0x57, 3);
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

static void Pattern14_0149(PlayerEntity* wk) {
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

static void Pattern14_0150(PlayerEntity* wk) {
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

static void (*const Pattern14_Tbl[151])(PlayerEntity*) = {
    Pattern14_0000, Pattern14_0001, Pattern14_0002, Pattern14_0003, Pattern14_0004, Pattern14_0005, Pattern14_0006,
    Pattern14_0007, Pattern14_0008, Pattern14_0009, Pattern14_0010, Pattern14_0011, Pattern14_0012, Pattern14_0013,
    Pattern14_0014, Pattern14_0015, Pattern14_0016, Pattern14_0017, Pattern14_0018, Pattern14_0019, Pattern14_0020,
    Pattern14_0021, Pattern14_0022, Pattern14_0023, Pattern14_0024, Pattern14_0025, Pattern14_0026, Pattern14_0027,
    Pattern14_0028, Pattern14_0029, Pattern14_0030, Pattern14_0031, Pattern14_0032, Pattern14_0033, Pattern14_0034,
    Pattern14_0035, Pattern14_0036, Pattern14_0037, Pattern14_0038, Pattern14_0039, Pattern14_0040, Pattern14_0041,
    Pattern14_0042, Pattern14_0043, Pattern14_0044, Pattern14_0045, Pattern14_0046, Pattern14_0047, Pattern14_0048,
    Pattern14_0049, Pattern14_0050, Pattern14_0051, Pattern14_0052, Pattern14_0053, Pattern14_0054, Pattern14_0055,
    Pattern14_0056, Pattern14_0057, Pattern14_0058, Pattern14_0059, Pattern14_0060, Pattern14_0061, Pattern14_0062,
    Pattern14_0063, Pattern14_0064, Pattern14_0065, Pattern14_0066, Pattern14_0067, Pattern14_0068, Pattern14_0069,
    Pattern14_0070, Pattern14_0071, Pattern14_0072, Pattern14_0073, Pattern14_0074, Pattern14_0075, Pattern14_0076,
    Pattern14_0077, Pattern14_0078, Pattern14_0079, Pattern14_0080, Pattern14_0081, Pattern14_0082, Pattern14_0083,
    Pattern14_0084, Pattern14_0085, Pattern14_0086, Pattern14_0087, Pattern14_0088, Pattern14_0089, Pattern14_0090,
    Pattern14_0091, Pattern14_0092, Pattern14_0093, Pattern14_0094, Pattern14_0095, Pattern14_0096, Pattern14_0097,
    Pattern14_0098, Pattern14_0099, Pattern14_0100, Pattern14_0101, Pattern14_0102, Pattern14_0103, Pattern14_0104,
    Pattern14_0105, Pattern14_0106, Pattern14_0107, Pattern14_0108, Pattern14_0109, Pattern14_0110, Pattern14_0111,
    Pattern14_0112, Pattern14_0113, Pattern14_0114, Pattern14_0115, Pattern14_0116, Pattern14_0117, Pattern14_0118,
    Pattern14_0119, Pattern14_0120, Pattern14_0121, Pattern14_0122, Pattern14_0123, Pattern14_0124, Pattern14_0125,
    Pattern14_0126, Pattern14_0127, Pattern14_0128, Pattern14_0129, Pattern14_0130, Pattern14_0131, Pattern14_0132,
    Pattern14_0133, Pattern14_0134, Pattern14_0135, Pattern14_0136, Pattern14_0137, Pattern14_0138, Pattern14_0139,
    Pattern14_0140, Pattern14_0141, Pattern14_0142, Pattern14_0143, Pattern14_0144, Pattern14_0145, Pattern14_0146,
    Pattern14_0147, Pattern14_0148, Pattern14_0149, Pattern14_0150
};
