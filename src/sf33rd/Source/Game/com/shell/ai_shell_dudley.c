/**
 * @file ai_shell_dudley.c
 * COM Shell: Dudley
 */

#include "sf33rd/Source/Game/com/shell/ai_shell_dudley.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "sf33rd/Source/Game/engine/state_user.h"

static void (*const Shell04_Tbl[12])(PlayerEntity*);

/** @brief Dudley shell (projectile response) AI entry point. */
void Shell04(PlayerEntity* wk) {
    Shell04_Tbl[(s16)g_state.Pattern_Index[wk->wu.id]](wk);
}

static void Shell04_0000(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell04_0001(PlayerEntity* wk) {
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

static void Shell04_0002(PlayerEntity* wk) {
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

static void Shell04_0003(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FD0, 9, 0x200, 0, -0x7FB0, -1, 0x200);
        break;

    case 2:
        Normal_Attack(wk, 0xB, 0x20);
        break;

    case 3:
        Jump_Command_Attack(wk, 8, 0x1C, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell04_0004(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FD0, 0xB, 0x200, 0, -0x7FB0, -1, 0x20);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell04_0005(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7FA8, -0x7FC0, 0xB, 0x200, 0, -0x7FB0, -1, 0x400);
        break;

    case 2:
        Jump_Command_Attack(wk, 8, 0x1C, 0xA, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell04_0006(PlayerEntity* wk) {
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

static void Shell04_0007(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 3, 2, 1, -1, -1);
        break;

    case 1:
        Command_Attack(wk, 0xD, 0x20, 0x30A, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell04_0008(PlayerEntity* wk) {
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

static void Shell04_0009(PlayerEntity* wk) {
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

static void Shell04_0010(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell04_0011(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 3, 2, 1, -1, -1);
        break;

    case 1:
        Command_Attack(wk, 0xD, 0x20, 0x60A, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void (*const Shell04_Tbl[12])(PlayerEntity*) = { Shell04_0000, Shell04_0001, Shell04_0002, Shell04_0003,
                                               Shell04_0004, Shell04_0005, Shell04_0006, Shell04_0007,
                                               Shell04_0008, Shell04_0009, Shell04_0010, Shell04_0011 };
