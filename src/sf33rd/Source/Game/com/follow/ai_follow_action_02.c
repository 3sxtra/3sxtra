/**
 * @file ai_follow_action_02.c
 * COM Follow
 */

#include "sf33rd/Source/Game/com/follow/ai_follow_action_02.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/com/ai_subroutines.h"
#include "sf33rd/Source/Game/engine/state_user.h"

static void (*const Follow02_Tbl[4])(PlayerEntity*);

/** @brief Follow-up combo AI pattern entry point. */
void Follow02(PlayerEntity* wk) {
    Follow02_Tbl[(s16)g_state.Pattern_Index[wk->wu.id]](wk);
}

static void Follow02_0000(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x20);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Follow02_0001(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x20);
        break;

    case 1:
        Command_Attack(wk, 2, 8, 0x1C, 8);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Follow02_0002(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x10);
        break;

    case 1:
        Normal_Attack(wk, 8, 0x20);
        break;

    case 2:
        Normal_Attack(wk, 8, 0x200);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void Follow02_0003(PlayerEntity* wk) {
    switch (g_state.CP_Index[wk->wu.id][0]) {
    case 0:
        Normal_Attack(wk, 8, 0x20);
        break;

    case 1:
        Command_Attack(wk, 2, 8, 0x1C, 8);
        break;

    default:
        End_Pattern(wk);
        break;
    }
}

static void (*const Follow02_Tbl[4])(PlayerEntity*) = {
    Follow02_0000,
    Follow02_0001,
    Follow02_0002,
    Follow02_0003,
};
