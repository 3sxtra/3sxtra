/**
 * @file eff83.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/eff83.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/calculate_direction.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/stage/ta_sub.h"

void effect_83_move(State_Other* ewk) {
    State* oya_ptr = (State*)ewk->my_master;
    s16 work;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        setup_shadow_of_the_Effy(&ewk->wu);
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        ewk->wu.old_routine_no[0] = 50;
        cal_all_speed_data(&ewk->wu, ewk->wu.old_routine_no[0], oya_ptr->xyz[0].disp.pos, ewk->wu.xyz[1].disp.pos, 2, 2);
        break;

    case 1:
        if (!g_state.EXE_flag && !g_state.Game_pause) {
            ewk->wu.old_routine_no[0]--;
            add_x_sub(&ewk->wu);
            add_y_sub(&ewk->wu);
            work = ewk->wu.xyz[0].disp.pos - oya_ptr->xyz[0].disp.pos;

            if (work < 0) {
                work = -work;
            }

            if (work < 113) {
                ewk->wu.routine_no[0]++;
                Sound_SE((ewk->master_id * 768) + 350);
                char_move_z(&ewk->wu);
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 2:
        if (!g_state.EXE_flag && !g_state.Game_pause) {
            char_move(&ewk->wu);
            add_x_sub(&ewk->wu);
            add_y_sub(&ewk->wu);
            ewk->wu.old_routine_no[0]--;

            if (ewk->wu.old_routine_no[0] <= 0) {
                ewk->wu.routine_no[0]++;
                oya_ptr->script_register_bank[1] = 9;
                set_char_move_init(&ewk->wu, 0, 35);
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 3:
        if (!g_state.EXE_flag && !g_state.Game_pause) {
            char_move(&ewk->wu);
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;
    }
}

s32 effect_83_init(State* wk) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 83;
    ewk->master_id = wk->id;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.my_col_mode = wk->my_col_mode;
    ewk->wu.my_col_code = wk->my_col_code + 6;
    ewk->wu.my_family = wk->my_family;
    ewk->my_master = wk;
    ewk->wu.rl_flag = wk->rl_flag;

    if (wk->rl_flag) {
        if (wk->xyz[0].disp.pos > g_state.bg_w.bgw[1].wxy[0].disp.pos) {
            ewk->wu.xyz[0].disp.pos = wk->xyz[0].disp.pos + 256;
        } else {
            ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].wxy[0].disp.pos + (g_state.bg_w.pos_offset + 32);
        }

        ewk->wu.old_routine_no[2] = wk->xyz[0].disp.pos + 56;
    } else {
        if (wk->xyz[0].disp.pos < g_state.bg_w.bgw[1].wxy[0].disp.pos) {
            ewk->wu.xyz[0].disp.pos = wk->xyz[0].disp.pos - 256;
        } else {
            ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].wxy[0].disp.pos - (g_state.bg_w.pos_offset + 32);
        }

        ewk->wu.old_routine_no[2] = wk->xyz[0].disp.pos - 56;
    }

    ewk->wu.xyz[1].disp.pos = wk->xyz[1].disp.pos - 12;
    ewk->wu.my_priority = wk->my_priority - 12;
    ewk->wu.position_z = ewk->wu.my_priority - 12;
    *ewk->wu.char_table = _etc2_char_table;
    ewk->wu.char_index = 34;
    ewk->wu.sync_bg_strip = 0;
    suzi_offset_set(ewk);
    ewk->wu.my_mts = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_mts);
    return 0;
}
