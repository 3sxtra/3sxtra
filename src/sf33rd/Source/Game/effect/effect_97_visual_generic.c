/**
 * @file eff97.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_97_visual_generic.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

void effect_97_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        set_char_move_init(&ewk->wu, 0, 44);
        /* fallthrough */

    case 1:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            char_move(&ewk->wu);

            if (g_state.plw[ewk->master_id].wu.routine_no[2] == 1 &&
                g_state.plw[ewk->master_id].wu.routine_no[3] == 0) {
                ewk->wu.routine_no[0]++;
                ewk->wu.old_routine_no[0] = 16;
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request4(&ewk->wu);
        break;

    case 2:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            ewk->wu.old_routine_no[0]--;

            if (ewk->wu.old_routine_no[0] > 0) {
                ewk->wu.routine_no[0]++;
                ewk->wu.facing_flag ^= 1;
                set_char_move_init(&ewk->wu, 0, 45);
                ewk->wu.old_routine_no[0] = 16;
                ewk->wu.mvxy.a[1].sp = 0xE8000;
                ewk->wu.mvxy.d[1].sp = -0x6000;

                if (g_state.plw[ewk->master_id].wu.id) {
                    ewk->wu.mvxy.a[0].sp = -0xA8000;
                    ewk->wu.mvxy.d[0].sp = -0x1000;
                } else {
                    ewk->wu.mvxy.a[0].sp = 0xA8000;
                    ewk->wu.mvxy.d[0].sp = 0x1000;
                }
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request4(&ewk->wu);
        break;

    case 3:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            ewk->wu.old_routine_no[0]--;

            if (ewk->wu.old_routine_no[0] > 0) {
                add_x_sub(&ewk->wu);
                add_y_sub(&ewk->wu);
            } else {
                ewk->wu.routine_no[0]++;
                ewk->wu.disp_flag = 0;
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request4(&ewk->wu);
        break;

    case 4:
        ewk->wu.routine_no[0]++;
        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_97_init(PlayerEntity* oya) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(3)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 97;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->my_master = oya;
    ewk->master_id = oya->wu.id;
    ewk->wu.my_col_mode = 0x4200;
    ewk->wu.my_col_code = oya->wu.my_col_code;
    ewk->wu.my_family = 2;
    ewk->wu.my_priority = ewk->wu.position_z = oya->wu.my_priority;
    ewk->wu.sync_bg_strip = 0;
    ewk->wu.xyz[1].disp.pos = 40;
    ewk->wu.shadow_flag = 1;
    ewk->wu.shadow_x = 0;
    ewk->wu.shadow_y = 40;
    ewk->wu.shadow_prio = 71;
    ewk->wu.shadow_char = 20;
    *ewk->wu.char_table = _etc_char_table;

    if (oya->wu.id) {
        ewk->wu.facing_flag = 0;
        ewk->wu.xyz[0].disp.pos = oya->wu.xyz[0].disp.pos - 108;
    } else {
        ewk->wu.facing_flag = 1;
        ewk->wu.xyz[0].disp.pos = oya->wu.xyz[0].disp.pos + 108;
    }

    ewk->wu.my_sprite_sheet = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    suzi_offset_set(ewk);
    return 0;
}
