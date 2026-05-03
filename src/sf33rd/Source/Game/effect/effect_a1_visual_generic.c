/**
 * @file effa1.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_a1_visual_generic.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/stage/bg.h"

void effect_A1_move(State_Other* ewk) {
    s16 x;

    ewk->wu.disp_flag = 0;

    if (g_state.Menu_Suicide[ewk->master_player]) {
        Release_Effect(&ewk->wu);
        return;
    }

    if (ewk->wu.be_flag == 0) {
        return;
    }

    ewk->wu.my_clear_level = g_state.Flash_Synchro;

    if (ewk->wu.type) {
        x = g_state.Cursor_Limit[1] - g_state.Cursor_Limit[0];

        if (x > 4) {
            ewk->wu.disp_flag = 1;
        }
    } else if (g_state.Cursor_Limit[0] > 0) {
        ewk->wu.disp_flag = 1;
    }

    sort_push_request4(&ewk->wu);
}
