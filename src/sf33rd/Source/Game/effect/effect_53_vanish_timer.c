/**
 * @file eff53.c
 * Effect: Vanish / Timer Effect
 */

#include "sf33rd/Source/Game/effect/effect_53_vanish_timer.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_54_texture_cache.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

const s16 eff53_vanish_time[8] = { 480, 600, 900, 1440, 480, 1080, 1500, 600 };

void effect_53_move(State_Other* ewk) {
    s16 work;

    if (obr_no_disp_check()) {
        return;
    }

    if (g_state.execute_flag || g_state.Game_pause || !g_state.EXE_obroll) {
        return;
    }

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.old_routine_no[2]--;

        if (ewk->wu.old_routine_no[2] <= 0) {
            ewk->wu.routine_no[0]++;
            ewk->wu.old_routine_no[0] = 30;
            ewk->wu.old_routine_no[1] = 0;
            ewk->wu.disp_flag = 1;
        }

        break;

    case 1:
        ewk->wu.old_routine_no[0]--;

        if (ewk->wu.old_routine_no[0] > 0) {
            break;
        }

        ewk->wu.disp_flag ^= 1;
        ewk->wu.old_routine_no[0] = 30;

        if (ewk->wu.disp_flag) {
            break;
        }

        ewk->wu.old_routine_no[1]++;

        if (ewk->wu.old_routine_no[1] < 6) {
            break;
        }

        ewk->wu.routine_no[0] = 0;
        work = random_16();
        work &= 7;
        ewk->wu.old_routine_no[2] = eff53_vanish_time[work];
        ewk->wu.disp_flag = 0;
        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_53_init() {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 53;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.disp_flag = 0;
    ewk->wu.old_routine_no[2] = 0;
    effect_54_init(ewk);
    return 0;
}
