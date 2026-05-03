/**
 * @file effl7.c
 * Effect: Win Player / Data Table Effect
 */

#include "sf33rd/Source/Game/effect/effect_l7_win_player_data_table.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/animation/win_pl.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/calculate_direction.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"

// forward declaration
const s16 effl7_data_tbl[16];

void effect_L7_move(State_Other* ewk) {
    State* oya_ptr = (State*)ewk->my_master;

    if (g_state.Suicide[0] || (ewk->wu.dead_f)) {
        ewk->wu.routine_no[0] = 1;
        ewk->wu.disp_flag = 0;
    }

    switch (ewk->wu.routine_no[0]) {
    case 0:
        if ((!g_state.EXE_flag) && (!g_state.Game_pause)) {
            effl7_move(ewk);
        }

        pl_eff_trans_entry(ewk);
        break;

    case 1:
        ewk->wu.routine_no[0] += 1;
        g_state.poison_flag[oya_ptr->id] = 0;
        /* fallthrough */

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

void effl7_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[1]) {
    case 0:
        ewk->wu.routine_no[1] += 1;
        ewk->wu.disp_flag = 1;
        ewk->wu.shadow_flag = 1;
        ewk->wu.shadow_x = 0;
        ewk->wu.shadow_y = -10;
        ewk->wu.shadow_prio = 71;
        ewk->wu.shadow_char = 16;
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        ewk->wu.old_routine_no[0] = 80;
        cal_initial_speed(&ewk->wu, ewk->wu.old_routine_no[0], ewk->wu.old_routine_no[1], ewk->wu.xyz[1].disp.pos);
        break;

    case 1:
        char_move(&ewk->wu);
        add_x_sub(&ewk->wu);
        add_y_sub(&ewk->wu);
        ewk->wu.old_routine_no[0]--;

        if (ewk->wu.old_routine_no[0] <= 0) {
            ewk->wu.routine_no[1] += 1;
            set_char_move_init(&ewk->wu, 0, 1);
        }

        break;

    default:
        // Do nothing
        break;

    case 2:
        char_move(&ewk->wu);

        if (ewk->wu.cg_type == 0xFF) {
            ewk->wu.routine_no[1] += 1;
            set_char_move_init(&ewk->wu, 1, ewk->wu.old_routine_no[2]);
        }

        break;

    case 3:
        char_move(&ewk->wu);

        if (ewk->wu.cg_type == 9) {
            ewk->wu.routine_no[1] += 1;
            ewk->wu.rl_flag ^= 1;
        }

        break;

    case 4:
        char_move(&ewk->wu);

        if (ewk->wu.cg_type == 0xFF) {
            ewk->wu.routine_no[1] += 1;
            set_char_move_init2(&ewk->wu, 0, 0, 3, 1);

            if (ewk->wu.rl_flag) {
                ewk->wu.mvxy.a[0].sp = 0x20000;
            } else {
                ewk->wu.mvxy.a[0].sp = -0x20000;
            }

            ewk->wu.mvxy.a[1].sp = 0;
        }

        break;

    case 5:
        char_move(&ewk->wu);
        add_x_sub(&ewk->wu);

        if (range_x_check3(ewk, 64) == 0) {
            ewk->wu.routine_no[1] += 1;
            ewk->wu.disp_flag = 0;
            ewk->wu.shadow_flag = 0;
        }

        break;

    case 6:
        ewk->wu.routine_no[1] += 1;
        ewk->wu.routine_no[0] += 1;
        break;
    }
}

s32 effect_L7_init(State* wk, s32 /* unused */) {
    State_Other* ewk;
    s16 ix;
    s16 kind_w;

    if ((wk->work_id == 1) && (((PLW*)wk)->player_number != g_state.My_char[wk->id])) {
        return 0;
    }

    if (g_state.poison_flag[wk->id]) {
        return 0;
    }

    if (wk->id) {
        if (!(p2sw_0 & 1)) {
            return 0;
        }
    } else if (!(p1sw_0 & 1)) {
        return 0;
    }

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 217;
    ewk->wu.work_id = 16;
    ewk->master_id = wk->id;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.my_col_mode = wk->my_col_mode;
    ewk->wu.my_col_code = wk->my_col_code + 1;
    ewk->wu.my_family = wk->my_family;
    ewk->my_master = wk;
    ewk->wu.rl_flag = wk->rl_flag;

    if (wk->rl_flag) {
        if (wk->xyz[0].disp.pos < g_state.bg_w.bgw[1].wxy[0].disp.pos) {
            ewk->wu.xyz[0].disp.pos = wk->xyz[0].disp.pos - 256;
        } else {
            ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].wxy[0].disp.pos - (g_state.bg_w.pos_offset + 32);
        }

        ewk->wu.old_routine_no[1] = wk->xyz[0].disp.pos - 32;
    } else {
        if (wk->xyz[0].disp.pos > g_state.bg_w.bgw[1].wxy[0].disp.pos) {
            ewk->wu.xyz[0].disp.pos = wk->xyz[0].disp.pos + 256;
        } else {
            ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].wxy[0].disp.pos + (g_state.bg_w.pos_offset + 32);
        }

        ewk->wu.old_routine_no[1] = wk->xyz[0].disp.pos + 32;
    }

    ewk->wu.xyz[1].disp.pos = wk->xyz[1].disp.pos - 12;
    ewk->wu.my_priority = 28;
    ewk->wu.position_z = 28;
    ewk->wu.char_table[0] = _etc3_char_table;
    ewk->wu.char_table[1] = _etc_char_table;
    ewk->wu.char_index = 0;
    ewk->wu.sync_bg_strip = 0;
    suzi_offset_set(ewk);
    kind_w = random_16();
    ewk->wu.old_routine_no[2] = effl7_data_tbl[kind_w];
    g_state.poison_flag[wk->id] = 1;
    ewk->wu.my_mts = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_mts);
    return 0;
}

const s16 effl7_data_tbl[16] = { 55, 56, 57, 55, 56, 57, 55, 57, 55, 56, 57, 55, 56, 57, 56, 57 };
