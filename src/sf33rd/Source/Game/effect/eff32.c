/**
 * @file eff32.c
 * Effect: Stage Object / ETC3 Character
 */

#include "sf33rd/Source/Game/effect/eff32.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/caldir.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/stage/ta_sub.h"

void effect_32_move(WORK_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        ewk->wu.shadow_flag = 1;
        ewk->wu.shadow_x = 0;
        ewk->wu.shadow_y = -4;
        ewk->wu.shadow_prio = 71;
        ewk->wu.shadow_char = 16;
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        ewk->wu.old_routine_no[0] = 120;
        cal_initial_speed(&ewk->wu, ewk->wu.old_routine_no[0], ewk->wu.old_routine_no[1], ewk->wu.xyz[1].disp.pos);
        break;

    case 1:
        if (!g_state.EXE_flag && !g_state.Game_pause) {
            char_move(&ewk->wu);
            add_x_sub(&ewk->wu);
            add_y_sub(&ewk->wu);
            ewk->wu.old_routine_no[0]--;

            if (ewk->wu.old_routine_no[0] < 1) {
                ewk->wu.routine_no[0]++;
                set_char_move_init(&ewk->wu, 0, 1);
            }
        }

        suzi_sync_pos_set(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 2:
        if (!g_state.EXE_flag && !g_state.Game_pause && (char_move(&ewk->wu), ewk->wu.cg_type == 0xFF)) {
            ewk->wu.routine_no[0]++;
            set_char_move_init(&ewk->wu, 0, 5);
        }

        suzi_sync_pos_set(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 3:
        if (!g_state.EXE_flag && !g_state.Game_pause && (char_move(&ewk->wu), ewk->wu.cg_type == 10)) {
            ewk->wu.cg_type = 0;
            ewk->wu.shadow_x -= 8;
        }

        suzi_sync_pos_set(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 4:
        ewk->wu.disp_flag = 0;
        ewk->wu.shadow_flag = 0;
        ewk->wu.routine_no[0]++;
        break;

    case 5:
        ewk->wu.routine_no[0]++;
        break;

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_32_init(WORK* wk) {
    WORK_Other* ewk;
    PLW* twk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (WORK_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 32;
    ewk->wu.work_id = 16;
    ewk->master_id = wk->id;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.my_col_mode = wk->my_col_mode;
    ewk->wu.my_col_code = wk->my_col_code + 1;
    ewk->wu.my_family = wk->my_family;
    ewk->my_master = wk;
    ewk->wu.target_adrs = wk->target_adrs;
    ewk->wu.rl_flag = wk->rl_flag;
    twk = (PLW*)wk->target_adrs;

    if (wk->rl_flag) {
        if (wk->xyz[0].disp.pos < g_state.bg_w.bgw[1].wxy[0].disp.pos) {
            ewk->wu.xyz[0].disp.pos = wk->xyz[0].disp.pos - 256;
        } else {
            ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].wxy[0].disp.pos - (g_state.bg_w.pos_offset + 32);
        }

        ewk->wu.old_routine_no[1] = twk->wu.xyz[0].disp.pos - 144;
    } else {
        if (wk->xyz[0].disp.pos > g_state.bg_w.bgw[1].wxy[0].disp.pos) {
            ewk->wu.xyz[0].disp.pos = wk->xyz[0].disp.pos + 256;
        } else {
            ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].wxy[0].disp.pos + (g_state.bg_w.pos_offset + 32);
        }

        ewk->wu.old_routine_no[1] = twk->wu.xyz[0].disp.pos + 144;
    }

    ewk->wu.xyz[1].disp.pos = wk->xyz[1].disp.pos - 4;
    ewk->wu.my_priority = 36;
    ewk->wu.position_z = 36;
    ewk->wu.char_table[0] = _etc3_char_table;
    ewk->wu.char_index = 0;
    ewk->wu.sync_suzi = 0;
    suzi_offset_set(ewk);
    ewk->wu.my_mts = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_mts);
    return 0;
}
