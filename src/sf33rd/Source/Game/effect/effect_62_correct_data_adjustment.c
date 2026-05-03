/**
 * @file eff62.c
 * Effect: Correct Data / Adjustment Effect
 */

#include "sf33rd/Source/Game/effect/effect_62_correct_data_adjustment.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "game_state.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"

const s16 EFF62_Correct_Data[1][2] = { { 0, 0 } };

void effect_62_move(State_Other* ewk) {
    State_Other* mwk = (State_Other*)ewk->my_master;

    if (mwk->wu.active_flag == 0) {
        Release_Effect(&ewk->wu);
        return;
    }

    ewk->wu.position_x = mwk->wu.position_x + ewk->wu.vital_new;
    ewk->wu.position_y = mwk->wu.position_y + ewk->wu.vital_old;
    ewk->wu.position_z = mwk->wu.position_z + 1;
    sort_push_requestA(&ewk->wu);
}

s32 effect_62_init(State_Other* mwk, s16 arg_ID) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.disp_flag = 1;
    ewk->wu.id = 62;
    ewk->wu.work_id = 16;
    ewk->wu.my_col_code = 0x2000;
    *ewk->wu.char_table = _sel_pl_char_table;
    ewk->my_master = mwk;
    ewk->wu.damage_vitality = arg_ID;
    ewk->wu.my_family = mwk->wu.my_family;
    ewk->wu.vital_new = EFF62_Correct_Data[arg_ID][0];
    ewk->wu.vital_old = EFF62_Correct_Data[arg_ID][1];
    ewk->wu.my_sprite_sheet = 13;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    ewk->wu.my_col_code = 1;
    ewk->wu.my_clear_level = 179;
    ewk->wu.shell_ix[0] = -190;
    ewk->wu.shell_ix[1] = 380;
    ewk->wu.shell_ix[2] = -46;
    ewk->wu.shell_ix[3] = 30;
    return 0;
}
