/**
 * @file effc1.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_c1_simple_animation.h"
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
#include "sf33rd/Source/Game/sound/sound_effects.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

void effect_C1_move(State_Other* ewk) {
    State* oya_ptr = (State*)ewk->my_master;
    s16 work;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        setup_shadow_of_the_Effy(&ewk->wu);
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        ewk->wu.old_routine_no[0] = 64;

        if (oya_ptr->char_index == 67) {
            work = oya_ptr->xyz[0].disp.pos;
        } else if (oya_ptr->facing_flag) {
            work = oya_ptr->xyz[0].disp.pos + 74;
        } else {
            work = oya_ptr->xyz[0].disp.pos - 74;
        }

        cal_all_speed_data(&ewk->wu, ewk->wu.old_routine_no[0], work, ewk->wu.xyz[1].disp.pos, 2, 2);
        break;

    case 1:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            ewk->wu.old_routine_no[0]--;
            add_x_sub(&ewk->wu);
            add_y_sub(&ewk->wu);
            work = ewk->wu.xyz[0].disp.pos - oya_ptr->xyz[0].disp.pos;

            if (work < 0) {
                work = -work;
            }

            if (work < 0x91) {
                ewk->wu.routine_no[0]++;
                Sound_SE((ewk->master_id * 0x300) + 0x15E);
                char_move_z(&ewk->wu);
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 2:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            char_move(&ewk->wu);
            add_x_sub(&ewk->wu);
            add_y_sub(&ewk->wu);
            ewk->wu.old_routine_no[0]--;

            if (ewk->wu.old_routine_no[0] <= 0) {
                ewk->wu.routine_no[0]++;

                if (oya_ptr->char_index == 67) {
                    set_char_move_init(&ewk->wu, 0, 37);
                } else {
                    set_char_move_init(&ewk->wu, 0, 38);
                }
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 3:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            char_move(&ewk->wu);
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;
    }
}

s32 effect_C1_init(State* wk) {

    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 121;
    ewk->wu.work_id = 16;
    ewk->master_id = wk->id;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.my_col_mode = wk->my_col_mode;
    ewk->wu.my_col_code = wk->my_col_code + 6;
    ewk->wu.my_family = wk->my_family;
    ewk->my_master = wk;
    ewk->wu.facing_flag = wk->facing_flag;

    if (wk->facing_flag) {
        ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].wxy[0].disp.pos + (g_state.bg_w.pos_offset + 16);
    } else {
        ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].wxy[0].disp.pos - (g_state.bg_w.pos_offset + 16);
    }

    ewk->wu.xyz[1].disp.pos = wk->xyz[1].disp.pos - 16;
    ewk->wu.my_priority = wk->my_priority - 12;
    ewk->wu.position_z = ewk->wu.my_priority - 12;
    *ewk->wu.char_table = _etc2_char_table;
    ewk->wu.char_index = 36;
    ewk->wu.sync_bg_strip = 0;
    suzi_offset_set(ewk);
    ewk->wu.my_sprite_sheet = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    return 0;
}
