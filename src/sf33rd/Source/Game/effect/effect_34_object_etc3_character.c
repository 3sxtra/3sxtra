/**
 * @file eff34.c
 * Effect: Stage Object / ETC3 Character
 */

#include "sf33rd/Source/Game/effect/effect_34_object_etc3_character.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/calculate_direction.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

void effect_34_move(State_Other* ewk) {
    State* oya_ptr = (State*)ewk->my_master;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        ewk->wu.shadow_flag = 1;
        ewk->wu.shadow_x = 0;
        ewk->wu.shadow_y = -10;
        ewk->wu.shadow_prio = 71;
        ewk->wu.shadow_char = 16;
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        ewk->wu.old_routine_no[0] = 60;
        cal_initial_speed(&ewk->wu, ewk->wu.old_routine_no[0], ewk->wu.old_routine_no[1], ewk->wu.xyz[1].disp.pos);
        break;

    case 1:
        if (g_state.execute_flag || g_state.Game_pause || g_state.bg_w.bgw[1].xy[1].disp.pos >= 104) {
            sync_bg_strip_position(ewk);
            sort_push_request(&ewk->wu);
            break;
        }

        char_move(&ewk->wu);
        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);

        if (ewk->wu.cg_type == 1) {
            ewk->wu.routine_no[0]++;
            ewk->wu.cg_type = 0;
            oya_ptr->script_register_bank[1] = 9;
        }

        break;

    case 2:
        if (g_state.execute_flag || g_state.Game_pause) {
            sync_bg_strip_position(ewk);
            sort_push_request(&ewk->wu);
            break;
        }

        char_move(&ewk->wu);
        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);

        if (ewk->wu.cg_type == 0xFF) {
            ewk->wu.routine_no[0]++;
            ewk->wu.facing_flag = ewk->wu.facing_flag ? 0 : 1;
            set_char_move_init(&ewk->wu, 0, 0);
        }

        break;

    case 3:
        if (g_state.execute_flag || g_state.Game_pause) {
            sync_bg_strip_position(ewk);
            sort_push_request(&ewk->wu);
            break;
        }

        if (ewk->wu.old_routine_no[0]--) {
            char_move(&ewk->wu);
            add_x_sub(&ewk->wu);
            sync_bg_strip_position(ewk);
            sort_push_request(&ewk->wu);
            break;
        }

        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 0;
        break;

    case 4:
        ewk->wu.routine_no[0]++;
        break;

    case 5:
    default:
        Release_Effect(&ewk->wu);
        break;
    }
}
s32 effect_34_init(State* wk, s32 /* unused */) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 34;
    ewk->wu.work_id = 16;
    ewk->master_id = wk->id;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.my_col_mode = wk->my_col_mode;
    ewk->wu.my_col_code = wk->my_col_code + 1;
    ewk->wu.my_family = wk->my_family;
    ewk->my_master = wk;
    ewk->wu.facing_flag = wk->facing_flag;
    ewk->wu.xyz[0].disp.pos = wk->xyz[0].disp.pos;
    ewk->wu.xyz[0].disp.pos += wk->facing_flag ? -48 : 48;
    ewk->wu.xyz[1].disp.pos = wk->xyz[1].disp.pos - 12;
    ewk->wu.my_priority = wk->my_priority - 12;
    ewk->wu.position_z = ewk->wu.my_priority - 12;
    ewk->wu.char_table[0] = _etc3_char_table;
    ewk->wu.sync_bg_strip = 0;
    ewk->wu.char_index = g_state.bg_w.stage == 6 ? 4 : 8;

    if (wk->facing_flag) {
        ewk->wu.old_routine_no[1] = g_state.bg_w.bgw[1].wxy[0].disp.pos - (g_state.bg_w.pos_offset + 32);
    } else {
        ewk->wu.old_routine_no[1] = g_state.bg_w.bgw[1].wxy[0].disp.pos + (g_state.bg_w.pos_offset + 32);
    }

    suzi_offset_set(ewk);
    ewk->wu.my_sprite_sheet = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    return 0;
}
