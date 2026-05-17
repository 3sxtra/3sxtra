/**
 * @file eff25.c
 * Effect: Stage / Background Effect
 */

#include "sf33rd/Source/Game/effect/effect_25_background.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_05_background.h"
#include "sf33rd/Source/Game/effect/effect_26_background.h"
#include "sf33rd/Source/Game/effect/effect_27_screen_object_piece_data.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

const s16 eff25_data_0000[16] = { 0, 2, 300, 160, 32, 71, 6, 0, 0, 0, 6, 6, 66, 9, 0, -1 };

const s16* scr_obj_data25[1] = { eff25_data_0000 };

void (*eff25_jp_tbl[10])(State_Other* ewk) = { eff25_00, eff25_00, eff25_02, eff25_02, eff25_04,
                                               eff25_04, eff25_06, eff25_06, eff25_08, eff25_08 };

void effect_25_move(State_Other* ewk) {
    if (compel_dead_check(ewk)) {
        ewk->wu.routine_no[0] = 99;
        ewk->wu.disp_flag = 0;
        return;
    }

    switch (ewk->wu.routine_no[0]) {
    case 0:
        if (!g_state.execute_flag && !g_state.Game_pause) {
            eff25_jp_tbl[ewk->wu.old_routine_no[2]](ewk);
        }

        disp_pos_trans_entry_rs(ewk);
        break;

    case 1:
    case 2:
    case 3:
        ewk->wu.disp_flag = 0;
        ewk->wu.routine_no[0]++;
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

void eff25_00(State_Other* ewk) {
    switch (ewk->wu.routine_no[1]) {
    case 0:
        if (g_state.eff_hit_flag[ewk->wu.type]) {
            ewk->wu.routine_no[0] = 4;
            break;
        }

        eff25_char_set(ewk);
        break;

    case 1:
        if (eff_hit_check(ewk, ewk->wu.old_routine_no[4])) {
            piece_set(ewk);
            set_char_move_init(&ewk->wu, 0, ewk->wu.old_routine_no[1]);
            ewk->wu.routine_no[1]++;
            break;
        }

        if (ewk->wu.hit_stop && !g_state.EXE_obroll) {
            char_move(&ewk->wu);
        }

        break;

    case 2:
        if (!g_state.EXE_obroll) {
            char_move(&ewk->wu);
        }

        if (ewk->wu.cg_type) {
            ewk->wu.routine_no[1]++;
        }

        /* fallthrough */

    case 3:
        ewk->wu.routine_no[0]++;
        break;
    }
}

void eff25_02(State_Other* ewk) {
    switch (ewk->wu.routine_no[1]) {
    case 0:
        if (g_state.eff_hit_flag[ewk->wu.type]) {
            ewk->wu.disp_flag = 1;
            ewk->wu.routine_no[1] = 4;
            set_char_move_init(&ewk->wu, 0, ewk->wu.old_routine_no[7]);
            break;
        }

        eff25_char_set(ewk);
        break;

    case 1:
        if (eff_hit_check(ewk, ewk->wu.old_routine_no[4])) {
            piece_set(ewk);
            ewk->wu.routine_no[1]++;
            set_char_move_init(&ewk->wu, 0, ewk->wu.old_routine_no[1]);
            break;
        }

        if (ewk->wu.hit_stop && !g_state.EXE_obroll) {
            char_move(&ewk->wu);
        }

        break;

    case 2:
        if (!g_state.EXE_obroll) {
            char_move(&ewk->wu);
        }

        if (ewk->wu.cg_type) {
            ewk->wu.routine_no[1]++;
        }

        break;

    case 3:
        break;

    case 4:
        if (!g_state.EXE_obroll) {
            char_move(&ewk->wu);
        }

        break;
    }
}

void eff25_04(State_Other* ewk) {
    switch (ewk->wu.routine_no[1]) {
    case 0:
        if (g_state.eff_hit_flag[ewk->wu.type]) {
            ewk->wu.routine_no[0] = 4;
            break;
        }

        eff25_char_set(ewk);
        break;

    case 1:
        if (eff_hit_check(ewk, ewk->wu.old_routine_no[4])) {
            piece_set(ewk);
            ewk->wu.routine_no[1]++;
            break;
        }

        if (ewk->wu.hit_stop && !g_state.EXE_obroll) {
            char_move(&ewk->wu);
        }

        break;

    case 2:
        ewk->wu.routine_no[0]++;
        break;
    }
}

void eff25_06(State_Other* ewk) {
    switch (ewk->wu.routine_no[1]) {
    case 0:
        if (g_state.eff_hit_flag[ewk->wu.type]) {
            ewk->wu.disp_flag = 1;
            set_char_move_init(&ewk->wu, 0, ewk->wu.old_routine_no[7]);
            ewk->wu.routine_no[1] = 4;
            break;
        }

        eff25_char_set(ewk);
        break;

    case 1:
        if (eff_hit_check(ewk, ewk->wu.old_routine_no[4])) {
            piece_set(ewk);
            ewk->wu.routine_no[1]++;
            set_char_move_init(&ewk->wu, 0, ewk->wu.old_routine_no[1]);
            break;
        }

        if (ewk->wu.hit_stop && !g_state.EXE_obroll) {
            char_move(&ewk->wu);
        }

        break;

    case 2:
        if (!g_state.EXE_obroll) {
            char_move(&ewk->wu);
        }

        if (ewk->wu.cg_type) {
            set_char_move_init(&ewk->wu, 0, ewk->wu.old_routine_no[3]);
            ewk->wu.routine_no[1]++;
            break;
        }

        break;

    case 3:
        break;

    case 4:
        if (!g_state.EXE_obroll) {
            char_move(&ewk->wu);
        }

        break;
    }
}

void eff25_08(State_Other* ewk) {
    switch (ewk->wu.routine_no[1]) {
    case 0:
        if (g_state.eff_hit_flag[ewk->wu.type]) {
            ewk->wu.routine_no[0] = 4;
            break;
        }

        eff25_char_set(ewk);
        break;

    case 1:
        if (eff_hit_check(ewk, ewk->wu.old_routine_no[4])) {
            piece_set(ewk);
            ewk->wu.routine_no[1]++;
            break;
        }

        if (ewk->wu.hit_stop && !g_state.EXE_obroll) {
            char_move(&ewk->wu);
        }

        break;

    case 2:
        ewk->wu.routine_no[0]++;
        break;
    }
}

void eff25_char_set(State_Other* ewk) {
    ewk->wu.routine_no[1]++;
    ewk->wu.disp_flag = 1;
    set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
}

void piece_set(State_Other* ewk) {
    if (!(ewk->wu.old_routine_no[2] & 1)) {
        return;
    }

    if (ewk->wu.old_routine_no[0] < 0) {
        return;
    }

    effect_27_init(ewk, ewk->wu.old_routine_no[0]);
}

s32 effect_25_init(s8 num) {
    State_Other* ewk;
    s16 ix;
    const s16* data_ptr = scr_obj_data25[num];

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 25;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.facing_flag = 0;
    ewk->wu.my_col_mode = 0x4200;
    ewk->wu.type = num;
    ewk->wu.death_timer = *data_ptr++;
    ewk->wu.my_family = *data_ptr++;
    ewk->wu.my_col_code = *data_ptr++;
    ewk->wu.xyz[0].disp.pos = *data_ptr++;
    ewk->wu.xyz[1].disp.pos = *data_ptr++;
    ewk->wu.my_priority = *data_ptr++;
    ewk->wu.char_index = *data_ptr++;
    ewk->wu.hit_stop = *data_ptr++;
    ewk->wu.sync_bg_strip = *data_ptr++;
    ewk->wu.old_routine_no[0] = *data_ptr++;
    ewk->wu.old_routine_no[1] = *data_ptr++;
    ewk->wu.old_routine_no[7] = *data_ptr++;
    ewk->wu.old_routine_no[3] = *data_ptr++;
    ewk->wu.old_routine_no[2] = *data_ptr++;
    ewk->wu.old_routine_no[4] = *data_ptr++;
    ewk->wu.position_z = ewk->wu.my_priority;

    if (*data_ptr >= 0) {
        effect_26_init(ewk, *data_ptr);
    }

    data_ptr++;
    ewk->wu.char_table[0] = char_add[g_state.bg_w.bg_index];
    suzi_offset_set(ewk);
    ewk->wu.my_sprite_sheet = 7;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    return 0;
}
