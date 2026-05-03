/**
 * @file effl2.c
 * Effect: Direction Table Effect
 */

#include "sf33rd/Source/Game/effect/effect_l2_direction_table.h"
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

const s8 effl2_dir_tbl[2][16] = { { 0, 0, 0, 1, 2, 2, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 },
                                  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 3 } };

void effect_L2_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        effl2_dir_check(ewk);
        set_char_move_init2(&ewk->wu, 0, 0, 1, 0);
        break;

    case 1:
        if (g_state.Allow_a_battle_f == 0 && g_state.Conclusion_Flag == 1 && *g_state.manage_phase >= 2) {
            if (!(g_state.Complete_Victory == 0) && g_state.Conclusion_Flag) {
                ewk->wu.routine_no[0]++;
                ewk->wu.old_routine_no[0] = 0;

                if (g_state.Winner_id != ewk->master_id) {
                    set_char_move_init(&ewk->wu, 0, 2);
                } else {
                    set_char_move_init(&ewk->wu, 0, 1);
                }
            }
        } else if (!g_state.execute_flag && !g_state.Game_pause) {
            effl2_dir_check(ewk);
        }

        ewk->wu.position_x = ewk->wu.xyz[0].disp.pos;
        ewk->wu.position_y = ewk->wu.xyz[1].disp.pos;
        sort_push_request(&ewk->wu);
        break;

    case 2:
        if (g_state.Exec_Wipe) {
            ewk->wu.old_routine_no[0] = 1;
        }

        if (ewk->wu.old_routine_no[0] && !g_state.Exec_Wipe) {
            ewk->wu.routine_no[0] = 0;
        }

        ewk->wu.position_x = ewk->wu.xyz[0].disp.pos;
        ewk->wu.position_y = ewk->wu.xyz[1].disp.pos;
        sort_push_request(&ewk->wu);
        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

void effl2_dir_check(State_Other* ewk) {
    s16 work = (g_state.plw[ewk->master_id].wu.xyz[0].disp.pos);

    work >>= 6;
    work &= 15;

    if (ewk->wu.direction != effl2_dir_tbl[ewk->master_id][work]) {
        ewk->wu.direction = effl2_dir_tbl[ewk->master_id][work];
        set_char_move_init2(&ewk->wu, 0, 0, ewk->wu.direction + 1, 0);
    }
}

s32 effect_L2_init() {
    State_Other* ewk;
    s16 ix;
    s16 oya_id;

    if (g_state.My_char[0] == 10 || g_state.My_char[1] == 10) {
        return -1;
    }

    if (g_state.My_char[0] == 3 && g_state.My_char[1] == 3) {
        return -1;
    }

    if (g_state.My_char[0] == 3) {
        oya_id = 0;
    } else if (g_state.My_char[1] == 3) {
        oya_id = 1;
    } else {
        return -1;
    }

    if ((ix = Acquire_Effect(3)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 212;
    ewk->wu.work_id = 16;
    ewk->master_id = oya_id;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.disp_flag = 1;
    ewk->wu.my_family = 2;
    ewk->my_master = &g_state.plw[oya_id];
    ewk->wu.my_col_mode = 0x4200;
    ewk->wu.my_sprite_sheet = 7;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);

    if (oya_id) {
        ewk->wu.my_col_code = 0x2016;
    } else {
        ewk->wu.my_col_code = 0x2006;
    }

    ewk->wu.my_priority = ewk->wu.position_z = 71;

    if (oya_id) {
        ewk->wu.xyz[0].cal = 0x3000000;
    } else {
        ewk->wu.xyz[0].cal = 0xF00000;
    }

    ewk->wu.xyz[1].cal = 0xA0000;
    ewk->wu.char_table[0] = _direct_03_char_table;
    ewk->wu.shadow_flag = 1;
    ewk->wu.shadow_x = 0;
    ewk->wu.shadow_y = 11;
    ewk->wu.shadow_char = 10;
    ewk->wu.shadow_prio = ewk->wu.position_z + 1;
    ewk->wu.dir_old = 0;
    ewk->wu.direction = 0;
    return 0;
}
