/**
 * @file eff72.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_72_visual_generic.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

void effect_72_move(State_Other* ewk) {
    if (obr_no_disp_check()) {
        return;
    }

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        break;

    case 1:
        if (!g_state.EXE_flag && !g_state.Game_pause) {
            char_move(&ewk->wu);

            if (ewk->wu.cg_type) {
                ewk->wu.routine_no[0]++;
                ewk->wu.disp_flag = 0;
            }
        }

        disp_pos_trans_entry_r(ewk);
        break;

    default:
    case 2:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_72_init(State_Other* oya, u8 type_id) {
    State_Other* ewk;
    s16 ix;

    if (g_state.EXE_obroll) {
        return 0;
    }

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 72;
    ewk->wu.type = type_id;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.rl_flag = 0;
    ewk->wu.my_col_mode = 0x4200;
    ewk->my_master = oya;
    ewk->wu.my_family = 2;
    ewk->wu.my_col_code = 8492;
    ewk->wu.my_mts = 7;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_mts);
    ewk->wu.char_table[0] = _hkg_char_table;
    ewk->wu.xyz[0].disp.pos = 511;
    ewk->wu.xyz[1].disp.pos = 0;

    if (type_id) {
        ewk->wu.position_z = ewk->wu.my_priority = 10;
        ewk->wu.char_index = 8;
    } else {
        ewk->wu.position_z = ewk->wu.my_priority = 72;
        ewk->wu.char_index = 9;
    }

    ewk->wu.sync_bg_strip = 0;
    suzi_offset_set(ewk);
    return 0;
}
