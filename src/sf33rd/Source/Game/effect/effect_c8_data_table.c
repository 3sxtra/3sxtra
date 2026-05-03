/**
 * @file effc8.c
 * Effect: Data Table Effect
 */

#include "sf33rd/Source/Game/effect/effect_c8_data_table.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

const s32 effc8_data_tbl[4] = { 0x30000, 0x200, 0, -0x1800 };

void effect_C8_move(State_Other* ewk) {
    PLW* oya_pl = (PLW*)ewk->my_master;
    s16 work;
    const s32* ptr;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        set_char_move_init(&ewk->wu, 0, 12);
        break;

    case 1:
        switch (oya_pl->wu.routine_no[3]) {
        case 1:
            break;

        case 0:
        case 2:
            if (oya_pl->wu.graphic_index != ewk->wu.graphic_index) {
                work = oya_pl->wu.graphic_index / oya_pl->wu.char_graphic_data_type + 1;
                set_char_move_init2(&ewk->wu, 0, 12, work + 1, 0);
                ewk->wu.graphic_index = oya_pl->wu.graphic_index;
            }

            break;

        case 3:
            ewk->wu.routine_no[0]++;
            break;
        }

        pl_eff_trans_entry(ewk);
        break;

    case 2:
        ewk->wu.routine_no[0]++;
        set_char_move_init(&ewk->wu, 0, 13);
        ptr = effc8_data_tbl;

        if (oya_pl->wu.id) {
            ewk->wu.xyz[0].disp.pos += 61;
            ewk->wu.mvxy.a[0].sp = *ptr++;
            ewk->wu.mvxy.d[0].sp = *ptr++;
            ewk->wu.mvxy.a[1].sp = *ptr++;
            ewk->wu.mvxy.d[1].sp = *ptr++;
        } else {
            ewk->wu.xyz[0].disp.pos -= 61;
            ewk->wu.mvxy.a[0].sp = -*ptr;
            ptr++;
            ewk->wu.mvxy.d[0].sp = -*ptr;
            ptr++;
            ewk->wu.mvxy.a[1].sp = *ptr++;
            ewk->wu.mvxy.d[1].sp = *ptr++;
        }

        ewk->wu.xyz[1].disp.pos = 137;
        pl_eff_trans_entry(ewk);
        break;

    case 3:
        if (!g_state.EXE_flag && !g_state.Game_pause) {
            add_x_sub(&ewk->wu);
            add_y_sub(&ewk->wu);
            char_move(&ewk->wu);

            if (ewk->wu.cg_type) {
                ewk->wu.routine_no[0]++;
                ewk->wu.disp_flag = 0;
            }
        }

        pl_eff_trans_entry(ewk);
        break;

    case 4:
        ewk->wu.routine_no[0]++;
        break;

    case 5:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_C8_init(PLW* wk) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(2)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 128;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.my_family = 2;
    ewk->wu.rl_flag = wk->wu.rl_flag;
    ewk->wu.my_col_mode = 0x4200;
    ewk->wu.my_priority = ewk->wu.position_z = 20;
    *ewk->wu.char_table = _etc_char_table;
    ewk->wu.my_col_code = wk->wu.my_col_code;
    ewk->my_master = wk;
    ewk->wu.position_x = ewk->wu.xyz[0].disp.pos = wk->wu.xyz[0].disp.pos;
    ewk->wu.position_y = ewk->wu.xyz[1].disp.pos = wk->wu.xyz[1].disp.pos;
    ewk->wu.xyz[0].disp.low = ewk->wu.xyz[1].disp.low = 0;
    ewk->wu.my_mts = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_mts);
    return 0;
}
