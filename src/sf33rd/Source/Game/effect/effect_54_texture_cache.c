/**
 * @file eff54.c
 * Effect: Texture / Cache Effect
 */

#include "sf33rd/Source/Game/effect/effect_54_texture_cache.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

const s16 eff29_data_tbl[8] = { 687, 128, 82, 7, 415, 152, 79, 8 };

void effect_54_move(State_Other* ewk) {
    State_Other* oya = (State_Other*)ewk->my_master;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        break;

    case 1:
        ewk->wu.disp_flag = oya->wu.disp_flag;
        disp_pos_trans_entry_rs(ewk);
        break;
    }
}

s32 effect_54_init(State_Other* oya) {
    State_Other* ewk;
    s16 ix;
    s16 i;
    const s16* data_ptr = eff29_data_tbl;

    for (i = 0; i < 2; i++) {
        if ((ix = Acquire_Effect(4)) == -1) {
            return -1;
        }

        ewk = (State_Other*)frw[ix];
        ewk->wu.be_flag = 1;
        ewk->wu.id = 54;
        ewk->wu.work_id = 16;
        ewk->wu.graphic_rom_type = 1;
        ewk->wu.rl_flag = 0;
        ewk->my_master = oya;
        ewk->wu.my_col_mode = 0x4200;
        ewk->wu.type = i;
        ewk->wu.dead_f = 0;
        ewk->wu.my_family = 2;
        ewk->wu.my_col_code = 8492;
        ewk->wu.my_mts = 7;
        ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_mts);
        ewk->wu.xyz[0].disp.pos = *data_ptr++;
        ewk->wu.xyz[1].disp.pos = *data_ptr++;
        ewk->wu.my_priority = ewk->wu.position_z = *data_ptr++;
        ewk->wu.char_index = *data_ptr++;
        ewk->wu.sync_bg_strip = 0;
        ewk->wu.char_table[0] = _eng_char_table;
        suzi_offset_set(ewk);
    }

    return 0;
}
