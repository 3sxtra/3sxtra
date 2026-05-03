/**
 * @file eff56.c
 * Effect: Color / Bonus Stage Effect
 */

#include "sf33rd/Source/Game/effect/eff56.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

const u8 ci_color_tbl[26] = { 21, 2,  22, 2,  21, 2,  20, 2,  21, 2,  22, 2,  21,
                              2,  20, 2,  21, 2,  22, 2,  21, 2,  20, 2,  20, 255 };

const u8 bonus_ci_color_tbl[6] = { 20, 12, 20, 12, 20, 255 };

void effect_56_move(WORK_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        if (g_state.Bonus_Game_Flag && g_state.bg_w.stage == 20) {
            g_state.ci_pointer = bonus_ci_color_tbl;
            g_state.ci_col = *g_state.ci_pointer++;
            g_state.ci_timer = *g_state.ci_pointer++;
        } else {
            g_state.ci_pointer = ci_color_tbl;
            g_state.ci_col = *g_state.ci_pointer++;
            g_state.ci_timer = *g_state.ci_pointer++;
        }

        /* fallthrough */

    case 1:
        if (ewk->wu.type < 7) {
            ci_set(ewk->wu.type, g_state.ci_col);
        } else {
            nw_set(ewk->wu.type - 7, g_state.ci_col);
        }

        break;

    default:
    case 2:
        if (g_state.Message_Suicide[ewk->wu.charset_id]) {
            Release_Effect(&ewk->wu);
            return;
        }

        if (ewk->wu.type < 7) {
            ci_set(ewk->wu.type, 20);
        } else {
            nw_set(ewk->wu.type - 7, 20);
        }

        return;
    }

    if (g_state.ci_timer > 1) {
        g_state.ci_timer--;
        return;
    }

    g_state.ci_col = *g_state.ci_pointer++;
    g_state.ci_timer = *g_state.ci_pointer++;

    if (g_state.ci_timer == 0xFF) {
        ewk->wu.routine_no[0]++;
    }
}

s32 effect_56_init(u8 type, u8 kill) {
    WORK_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (WORK_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 56;
    ewk->wu.work_id = 16;
    ewk->wu.type = type;
    ewk->wu.charset_id = kill;
    return 0;
}
