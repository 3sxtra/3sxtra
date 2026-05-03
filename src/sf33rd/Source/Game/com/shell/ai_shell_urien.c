/**
 * @file ai_shell_urien.c
 * COM Shell: Urien
 */

#include "sf33rd/Source/Game/com/shell/ai_shell_urien.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "sf33rd/Source/Game/engine/state_user.h"

static void (*const Shell13_Tbl[12])(PlayerEntity*);

/** @brief Remy shell (projectile response) AI entry point. */
void Shell13(PlayerEntity* wk) {
    Shell13_Tbl[(s16)g_state.Pattern_Index[wk->wu.id]](wk);
}

static void Shell13_0000(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell13_0001(PlayerEntity* wk) {
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

static void Shell13_0002(PlayerEntity* wk) {
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

static void Shell13_0003(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7F78, -0x7FD0, 9, 0x200, 0, -0x7FB0, -1, 0x200);
        break;

    case 2:
        Normal_Attack(wk, 0xB, 0x20);
        break;

    case 3:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell13_0004(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7F78, -0x7FD0, 0xB, 0x200, 0, -0x7FB0, -1, 0x20);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell13_0005(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7F78, -0x7FC0, 0xB, 0x200, 0, -0x7FB0, -1, 0x400);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x40);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell13_0006(PlayerEntity* wk) {
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

static void Shell13_0007(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1E, 8, -1);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell13_0008(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Check_Projectile_Impact_Time(wk, 0, 2, 1, -1, -1);
        break;

    case 1:
        Check_Jump_Attack_Conditions(wk, -0x7F78, -0x7FD0, 0xB, 0x20, 0, -0x7FB0, -1, 0x200);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x202);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell13_0009(PlayerEntity* wk) {
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

static void Shell13_0010(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    default:
        End_Pattern(wk);
        break;
    }
}

static void Shell13_0011(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Command_Attack(wk, 8, 0x1E, 8, 0x70);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void (*const Shell13_Tbl[12])(PlayerEntity*) = { Shell13_0000, Shell13_0001, Shell13_0002, Shell13_0003,
                                               Shell13_0004, Shell13_0005, Shell13_0006, Shell13_0007,
                                               Shell13_0008, Shell13_0009, Shell13_0010, Shell13_0011 };
