/**
 * @file effj6.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_j6_visual_generic.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_27_screen_object_piece_data.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

static void effect_j6_hit_sub(State_Other* ewk);

void effect_J6_move(State_Other* ewk) {
    State_Other* oya_ptr;

    if (obr_no_disp_check()) {
        return;
    }

    oya_ptr = (State_Other*)ewk->my_master;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;

        if (g_state.eff_hit_flag[ewk->wu.type]) {
            ewk->wu.routine_no[0] = 4;
            set_char_move_init(&ewk->wu, 0, 3);
        } else {
            set_char_move_init(&ewk->wu, 0, 4);
        }

        break;

    case 1:
        if (oya_ptr->wu.routine_no[0] >= 2) {
            ewk->wu.routine_no[0]++;
        }

        disp_pos_trans_entry_r(ewk);
        break;

    case 2:
        if (!g_state.execute_flag && !g_state.Game_pause && !g_state.EXE_obroll) {
            effect_j6_hit_sub(ewk);
        }

        disp_pos_trans_entry_r(ewk);
        break;

    case 3:
        ewk->wu.routine_no[0]++;
        set_char_move_init(&ewk->wu, 0, 3);
        /* fallthrough */

    case 4:
        disp_pos_trans_entry_r(ewk);
        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

static void effect_j6_hit_sub(State_Other* ewk) {
    if (eff_hit_check(ewk, 0)) {
        ewk->wu.routine_no[0]++;
        effect_27_init(ewk, 1);
    }
}

s32 effect_J6_init(State_Other* oya) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->my_master = oya;
    ewk->wu.active_flag = 1;
    ewk->wu.id = 196;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.facing_flag = 0;
    ewk->wu.my_col_mode = 0x4200;
    ewk->wu.type = 3;
    ewk->wu.death_timer = 0;
    ewk->wu.my_family = 2;
    ewk->wu.my_sprite_sheet = 7;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    ewk->wu.my_col_code = 0x212C;
    *ewk->wu.char_table = _chn_char_table;
    ewk->wu.xyz[0].disp.pos = 904;
    ewk->wu.xyz[1].disp.pos = 16;
    ewk->wu.my_priority = ewk->wu.position_z = 10;
    ewk->wu.char_index = 4;
    ewk->wu.routine_no[1] = 0;
    return 0;
}
