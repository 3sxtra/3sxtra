/**
 * @file effc5.c
 * Effect: Appear / Entry Effect
 */

#include "sf33rd/Source/Game/effect/effect_c5_appear_entry.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/animation/appear.h"
#include "sf33rd/Source/Game/effect/effect_c6_visual_generic.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/calculate_direction.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/sound/sound_effects.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

void effect_C5_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            ewk->wu.routine_no[0]++;
            ewk->wu.disp_flag = 1;
            set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
            Sound_SE(ewk->master_id * 0x300 + 0x134);
        }

        break;

    case 1:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            char_move(&ewk->wu);
            ewk->wu.old_routine_no[0]--;

            if (ewk->wu.old_routine_no[0] < 1) {
                ewk->wu.routine_no[0]++;
                g_state.Appear_car_stop[ewk->master_id] = 1;
                set_char_move_init(&ewk->wu, 0, 9);

                if (g_state.Demo_Flag != 0) {
                    SsRequestPan(0x135, 0x40, 0x40, 0, 2);
                }
            } else {
                add_x_sub(&ewk->wu);
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 2:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            char_move(&ewk->wu);

            if (ewk->wu.cg_type == 1) {
                ewk->wu.routine_no[0]++;
                ewk->wu.old_routine_no[0] = 20;
            } else if (ewk->wu.cg_type == 2) {
                g_state.demo_car_flag[ewk->master_id] = 1;
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 3:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            ewk->wu.old_routine_no[0]--;

            if (ewk->wu.old_routine_no[0] < 0) {
                ewk->wu.routine_no[0]++;
                ewk->wu.old_routine_no[0] = 48;

                if (ewk->wu.facing_flag) {
                    ewk->wu.mvxy.a[0].sp = -0x20000;
                    ewk->wu.mvxy.d[0].sp = -0x1000;
                } else {
                    ewk->wu.mvxy.a[0].sp = 0x20000;
                    ewk->wu.mvxy.d[0].sp = 0x1000;
                }
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 4:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            ewk->wu.old_routine_no[0]--;

            if (ewk->wu.old_routine_no[0] < 0) {
                ewk->wu.routine_no[0]++;
            } else {
                add_x_sub(&ewk->wu);
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 5:
        ewk->wu.routine_no[0]++;
        g_state.demo_car_flag[ewk->master_id] = 0;
        ewk->wu.disp_flag = 0;
        break;

    case 6:
        ewk->wu.routine_no[0]++;
        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_C5_init(PlayerEntity* oya, s16 reverse_f) {
    State_Other* ewk;
    s16 ix;
    s16 work;
    s16 id_num;

    if ((ix = Acquire_Effect(3)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    g_state.demo_car_flag[oya->wu.id] = 0;
    g_state.Appear_car_stop[oya->wu.id] = 0;
    ewk->wu.active_flag = 1;
    ewk->wu.id = 125;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.disp_flag = 0;
    ewk->wu.my_family = 2;
    ewk->wu.char_index = 8;
    ewk->wu.my_col_mode = 0x4200;
    ewk->wu.my_priority = ewk->wu.position_z = 57;
    *ewk->wu.char_table = _etc_char_table;
    ewk->wu.my_col_code = oya->wu.my_col_code + 6;
    ewk->wu.sync_bg_strip = 0;
    ewk->master_id = oya->wu.id;
    ewk->wu.my_sprite_sheet = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    id_num = oya->wu.id ^ reverse_f;

    if (id_num) {
        ewk->wu.xyz[0].disp.low = 0;
        ewk->wu.xyz[1].cal = 0;
        ewk->wu.facing_flag = 0;
        ewk->wu.old_routine_no[0] = 40;
        work = (g_state.bg_w.bgw[1].pos_x_work + 192) & 0xFFFF;
        ewk->wu.xyz[0].disp.pos = (g_state.bg_w.bgw[1].pos_x_work + 320) & 0xFFFF;
        cal_all_speed_data(&ewk->wu, ewk->wu.old_routine_no[0], work, 0, 1, 1);
    } else {
        ewk->wu.xyz[1].cal = 0;
        ewk->wu.xyz[0].disp.low = 0;
        ewk->wu.facing_flag = 1;
        ewk->wu.old_routine_no[0] = 40;
        work = (g_state.bg_w.bgw[1].pos_x_work - 192) & 0xFFFF;
        ewk->wu.xyz[0].disp.pos = (g_state.bg_w.bgw[1].pos_x_work - 320) & 0xFFFF;
        cal_all_speed_data(&ewk->wu, ewk->wu.old_routine_no[0], work, 0, 1, 1);
    }

    suzi_offset_set(ewk);
    effect_C6_init(ewk);
    return 0;
}
