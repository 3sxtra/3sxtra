/**
 * @file ai_shell_group_b.c
 * COM Shell: Ryu, Ken, Chun-Li, Makoto, Q, Twelve, Remy
 */

#include "sf33rd/Source/Game/com/shell/ai_shell_group_b.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "sf33rd/Source/Game/engine/state_user.h"

static void (*const Shell11_Tbl[14])(PlayerEntity*);

/** @brief Ryu shell (projectile response) AI entry point. */
void Shell11(PlayerEntity* wk) {
    Shell11_Tbl[(s16)g_state.Pattern_Index[wk->wu.id]](wk);
}

static void Shell11_0000(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0001(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 1, 2, 1, -1, -1);
        break;

    case 1:
        Jump(wk, 2);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0002(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Jump(wk, 0);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0003(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7F20, -0x7FD0, 9, 0x400, 0, -0x7FB0, -1, 0x200);
        break;

    case 2:
        Normal_Attack(wk, 0xB, 0x20);
        break;

    case 3:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0004(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7F20, -0x7FD0, 0xB, 0x400, 0, -0x7FB0, -1, 0x20);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0005(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7F20, -0x7FC0, 0xB, 0x400, 0, -0x7FB0, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0006(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 1, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -1, 0x30, 8, 0x400, 2, -1, -1, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0007(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1D, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0008(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FD0, 0xB, 0x20, 0, -0x7FB0, -1, 0x200);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0009(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Lever_Off(wk);
        break;

    case 1:
        Check_Projectile_Impact_Time(wk, 2, 2, 1, -1, -1);
        break;

    case 2:
        Next_Be_Flip(wk, 8);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0010(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0011(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1D, 0xA, 0x70);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0012(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x1E, 0xA, -1, -1, 0x30, 0, -1, -1, 0xFFFF);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell11_0013(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Jump_Command_Attack_Term(wk, 8, 0x1E, 0xA, 0x700, -1, 0x30, 0, -1, -1, 0xFFFF);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void (*const Shell11_Tbl[14])(PlayerEntity*) = { Shell11_0000, Shell11_0001, Shell11_0002, Shell11_0003,
                                                        Shell11_0004, Shell11_0005, Shell11_0006, Shell11_0007,
                                                        Shell11_0008, Shell11_0009, Shell11_0010, Shell11_0011,
                                                        Shell11_0012, Shell11_0013 };
