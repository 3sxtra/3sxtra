/**
 * @file eff17.c
 * Effect: Effect Dispatch / General
 */

#include "sf33rd/Source/Game/effect/effect_17_dispatch_general.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/stage/bg.h"

void effect_17_move(State_Other* ewk) {
    if (g_state.Menu_Suicide[ewk->master_player]) {
        Release_Effect(&ewk->wu);
    } else if (g_state.Menu_Cursor_Y[0] == ewk->wu.type) {
        EFF17_Bowan(ewk);
    } else {
        ewk->wu.my_clear_level = 128;
        ewk->wu.routine_no[1] = 0;
    }

    sort_push_request4(&ewk->wu);
}

void EFF17_Bowan(State_Other* ewk) {
    switch (ewk->wu.routine_no[1]) {
    case 0:
        ewk->wu.my_clear_level -= 4;

        if (ewk->wu.my_clear_level < 20) {
            ewk->wu.routine_no[1]++;
            ewk->wu.my_clear_level = 20;
        }

        break;

    case 1:
        ewk->wu.my_clear_level += 4;

        if (ewk->wu.my_clear_level > 127) {
            ewk->wu.routine_no[1]--;
            ewk->wu.my_clear_level = 127;
        }

        break;
    }
}
