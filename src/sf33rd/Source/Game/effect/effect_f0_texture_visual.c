/**
 * @file efff0.c
 * Effect: Texture / Visual Effect
 */

#include "sf33rd/Source/Game/effect/effect_f0_texture_visual.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"

void effect_F0_move(State_Other* ewk) {
    State* mwk = (State*)ewk->my_master;

    if (!ewk->wu.routine_no[0]) {
        if ((ewk->wu.death_timer == 1) || (ewk->wu.dir_old != mwk->current_char_type)) {
            ewk->wu.disp_flag = 0;
            Release_Effect(&ewk->wu);
            return;
        }

        ewk->wu.disp_flag = ((mwk->cg_number >= 0x1B59) && (mwk->cg_number < 0x1B5E)) ? 1 : 0;
        ewk->wu.cg_number = mwk->cg_number;
        ewk->wu.position_x = mwk->position_x;
        ewk->wu.position_y = mwk->position_y;
        ewk->wu.position_z = mwk->position_z - 1;
        ewk->wu.facing_flag = mwk->facing_flag;
        ewk->wu.my_col_code = mwk->my_col_code;
        sort_push_request(&ewk->wu);
        return;
    }

    Release_Effect(&ewk->wu);
}

s32 effect_F0_init(State* wk) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 150;
    ewk->wu.work_id = 16;
    ewk->wu.my_family = wk->my_family;
    ewk->my_master = wk;
    ewk->master_work_id = wk->work_id;
    ewk->master_id = wk->id;
    ewk->wu.graphic_rom_type = wk->graphic_rom_type;
    ewk->wu.my_sprite_sheet = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    ewk->wu.disp_flag = 0;
    ewk->wu.dir_old = wk->current_char_type;
    effect_F0_move(ewk);
    return 0;
}
