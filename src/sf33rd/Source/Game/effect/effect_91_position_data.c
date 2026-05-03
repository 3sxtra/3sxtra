/**
 * @file eff91.c
 * Effect: Position Data Effect
 */

#include "sf33rd/Source/Game/effect/effect_91_position_data.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/bg.h"

const s16 EFF91_Pos_Data[2][3][2] = { { { -88, 95 }, { -94, 71 }, { -88, 47 } },
                                      { { 104, 95 }, { 98, 71 }, { 104, 47 } } };

void effect_91_move(State_Other* ewk) {
    if (g_state.Menu_Suicide[ewk->master_player]) {
        Release_Effect(&ewk->wu);
        return;
    }

    if (ewk->wu.be_flag == 0) {
        return;
    }

    if ((ewk->wu.type == 1 && !Debug_w[DEBUG_CPU_REPLAY_TEST]) &&
        (g_state.Round_Operator[0] == 0 || g_state.Round_Operator[1] == 0)) {
        ewk->wu.my_clear_level = 205;
        sort_push_request4(&ewk->wu);
        return;
    }

    if (g_state.Menu_Cursor_Y[ewk->master_id] == ewk->wu.type) {
        if (g_state.Menu_Cursor_X[ewk->master_id] != 0) {
            ewk->wu.my_clear_level = 0;
        } else {
            ewk->wu.my_clear_level = g_state.Flash_Synchro;
        }
    } else {
        ewk->wu.routine_no[1] = 0;
        ewk->wu.my_clear_level = 128;
    }

    sort_push_request4(&ewk->wu);
}

s32 effect_91_init(s16 master_id, s16 type, s16 target_bg, s16 char_ix, s16 char_ix2, s16 master_player) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.disp_flag = 1;
    ewk->wu.id = 91;
    ewk->wu.work_id = 16;
    ewk->wu.my_col_code = 428;
    ewk->master_id = master_id;
    ewk->wu.my_family = target_bg + 1;
    ewk->wu.char_table[0] = _sel_pl_char_table;
    ewk->wu.type = type;
    ewk->master_player = master_player;
    ewk->wu.my_mts = 13;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_mts);
    ewk->wu.position_x =
        g_state.bg_w.bgw[ewk->wu.my_family - 1].wxy[0].disp.pos + EFF91_Pos_Data[master_id][ewk->wu.type][0];
    ewk->wu.position_y =
        g_state.bg_w.bgw[ewk->wu.my_family - 1].wxy[1].disp.pos + EFF91_Pos_Data[master_id][ewk->wu.type][1];
    ewk->wu.position_z = 68;
    set_char_move_init2(&ewk->wu, 0, char_ix, char_ix2 + 1, 0);
    return 0;
}
