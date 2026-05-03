/**
 * @file effg7.c
 * Effect: Ending Data Effect
 */

#include "sf33rd/Source/Game/effect/effect_g7_ending_data.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/ending/end_data.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

void effect_G7_move(State_Other* ewk) {
    if (ewk->wu.old_routine_no[0] < g_state.end_w.r_no_2) {
        ewk->wu.routine_no[0] = 99;
    }

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        set_char_move_init2(&ewk->wu, 0, 51, 1, 0);
        disp_pos_trans_entry(ewk);
        break;

    case 1:
        char_move(&ewk->wu);

        if (ewk->wu.cg_type == 0xFF) {
            ewk->wu.routine_no[0]++;
        }

        disp_pos_trans_entry(ewk);
        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_G7_init(s32 /* unused */, s32 /* unused */) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.id = 167;
    ewk->wu.active_flag = 1;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.old_routine_no[0] = g_state.end_w.r_no_2;
    ewk->wu.my_col_mode = 0x4200;
    *ewk->wu.char_table = _end_char_table;
    ewk->wu.my_family = 3;
    ewk->wu.my_col_code = 0x12C;
    ewk->wu.xyz[0].disp.pos = 688;
    ewk->wu.xyz[1].disp.pos = 0;
    ewk->wu.my_priority = ewk->wu.position_z = 74;
    ewk->wu.my_sprite_sheet = 8;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    return 0;
}
