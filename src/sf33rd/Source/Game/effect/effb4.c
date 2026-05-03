/**
 * @file effb4.c
 * Effect: Mark Table Effect
 */

#include "sf33rd/Source/Game/effect/effb4.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"

const s16 s_mark_tbl[32] = { 0, 1, 0, 1, 0, 3, 0, 0, 0, 2, 4, 0, 0, 1, 0, 2,
                             6, 3, 0, 2, 0, 0, 6, 3, 0, 4, 0, 0, 6, 0, 0, 4 };

void effect_B4_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        ewk->wu.old_routine_no[0] = random_16();
        ewk->wu.old_routine_no[0] &= 0x1F;
        ewk->wu.char_index += s_mark_tbl[ewk->wu.old_routine_no[0]];
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        break;

    case 1:
        if (!g_state.EXE_flag && !g_state.Game_pause) {
            char_move(&ewk->wu);

            if (ewk->wu.cg_type) {
                ewk->wu.routine_no[0]++;
            }
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 2:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 0;
        break;

    case 3:
        ewk->wu.routine_no[0]++;
        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        return;
    }
}

s32 effect_B4_init(State_Other* oya) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 114;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->my_master = oya;
    ewk->master_id = 0;
    ewk->wu.my_col_mode = 0;
    ewk->wu.my_col_code = 0x20;
    ewk->wu.my_family = 2;
    ewk->wu.position_z = oya->wu.position_z - 1;
    ewk->wu.char_table[0] = _etc_char_table;
    ewk->wu.char_index = 34;
    ewk->wu.sync_bg_strip = 0;
    ewk->wu.rl_flag = 0;
    ewk->wu.xyz[0].disp.pos = oya->wu.xyz[0].disp.pos;
    ewk->wu.xyz[1].disp.pos = oya->wu.xyz[1].disp.pos;
    suzi_offset_set(ewk);
    ewk->wu.my_mts = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_mts);
    return 0;
}
