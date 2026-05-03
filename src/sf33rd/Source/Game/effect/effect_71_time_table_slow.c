/**
 * @file eff71.c
 * Effect: Time Table / Slow Effect
 */

#include "sf33rd/Source/Game/effect/effect_71_time_table_slow.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_72_visual_generic.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

const s16 eff71_time_tbl[8] = { 2, 8, 12, 9, 4, 6, 50, 3 };

void effect_71_move(State_Other* ewk) {
    s16 work;

    if (obr_no_disp_check()) {
        return;
    }

    switch (ewk->wu.routine_no[0]) {
    case 0:
        if (!g_state.execute_flag && !g_state.Game_pause && !g_state.EXE_obroll) {
            ewk->wu.old_routine_no[0]--;

            if (ewk->wu.old_routine_no[0] <= 0) {
                ewk->wu.routine_no[0]++;
                ewk->wu.disp_flag = 1;
                ewk->wu.old_routine_no[1] = 0;
                effect_72_init(ewk, 0);
                effect_72_init(ewk, 1);
            }
        }

        break;

    case 1:
        if (!g_state.execute_flag && !g_state.Game_pause && !g_state.EXE_obroll) {
            ewk->wu.routine_no[0] = 0;
            ewk->wu.old_routine_no[1] = 0;
            work = random_16();
            work &= 7;
            ewk->wu.old_routine_no[0] = eff71_time_tbl[work];
        }

        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_71_init() {
    State_Other* ewk;
    s16 ix;

    if (g_state.EXE_obroll) {
        return 0;
    }

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 71;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.facing_flag = 0;
    ewk->wu.disp_flag = 0;
    ewk->wu.old_routine_no[0] = 0;
    return 0;
}
