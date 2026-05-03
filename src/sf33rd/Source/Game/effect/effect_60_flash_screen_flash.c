/**
 * @file eff60.c
 * Effect: Flash / Screen Flash
 */

#include "sf33rd/Source/Game/effect/effect_60_flash_screen_flash.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_05_background.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

const s16 flash_0000[10] = { 0, 2, 300, 431, 80, 82, 1, 0, 0, 3 };

const s16 flash_0001[10] = { 0, 2, 300, 511, 184, 82, 2, 0, 0, 3 };

const s16 flash_0002[10] = { 0, 2, 300, 431, 64, 83, 2, 0, 0, 2 };

const s16* flash_obj_data61[3] = { flash_0000, flash_0001, flash_0002 };

void effect_60_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        ewk->wu.disp_flag = 1;
        break;

    case 1:
        if (compel_dead_check(ewk)) {
            ewk->wu.routine_no[0]++;
            ewk->wu.disp_flag = 0;
            break;
        }

        if (!g_state.execute_flag && !g_state.Game_pause && !g_state.EXE_obroll) {
            if (ewk->wu.type < 3) {
                ewk->wu.disp_flag = 1;
                char_move(&ewk->wu);
            } else {
                ewk->wu.old_routine_no[1]--;

                if (ewk->wu.old_routine_no[1] <= 0) {
                    ewk->wu.disp_flag ^= 1;
                    ewk->wu.old_routine_no[1] = ewk->wu.old_routine_no[0];

                    if (ewk->wu.hit_stop) {
                        char_move(&ewk->wu);
                    }
                }
            }
        }

        disp_pos_trans_entry_rs(ewk);
        break;

    case 2:
        ewk->wu.routine_no[0]++;
        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_60_init(s16 type) {
    State_Other* ewk;
    s16 ix;
    const s16* data_ptr;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    data_ptr = flash_obj_data61[type];
    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 60;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.facing_flag = 0;
    ewk->wu.my_col_mode = 0x4200;
    ewk->wu.type = type;
    ewk->wu.death_timer = *data_ptr++;
    ewk->wu.my_family = *data_ptr++;
    ewk->wu.my_col_code = *data_ptr++;
    ewk->wu.xyz[0].disp.pos = *data_ptr++;
    ewk->wu.xyz[1].disp.pos = *data_ptr++;
    ewk->wu.my_priority = ewk->wu.position_z = *data_ptr++;
    ewk->wu.char_index = *data_ptr++;
    ewk->wu.hit_stop = *data_ptr++;
    ewk->wu.sync_bg_strip = *data_ptr++;
    ewk->wu.old_routine_no[0] = *data_ptr++;
    ewk->wu.old_routine_no[1] = ewk->wu.old_routine_no[0];
    ewk->wu.char_table[0] = char_add[g_state.bg_w.bg_index];
    suzi_offset_set(ewk);
    ewk->wu.my_sprite_sheet = 7;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    return 0;
}
