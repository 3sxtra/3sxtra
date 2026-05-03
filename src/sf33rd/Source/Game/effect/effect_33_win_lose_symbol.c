/**
 * @file eff33.c
 * Effect: Win/Lose Symbol Effect
 */

#include "sf33rd/Source/Game/effect/effect_33_win_lose_symbol.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"

const s16 WinLoseID[2][2];

void effect_33_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        ewk->wu.shadow_flag = 1;
        ewk->wu.shadow_x = 0;
        ewk->wu.shadow_y = -10;
        ewk->wu.shadow_prio = 71;
        ewk->wu.shadow_char = 16;
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        break;

    case 1:
        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);

        if (g_state.EXE_flag || g_state.Game_pause || g_state.pcon_rno[2] != 1 || g_state.Event_Judge_Gals != -1 ||
            !g_state.Complete_Judgement) {
            break;
        }

        ewk->wu.routine_no[0]++;
        break;

    case 2:
        if (!g_state.EXE_flag && !g_state.Game_pause) {
            ewk->wu.routine_no[0]++;
            ewk->wu.char_index = WinLoseID[ewk->master_id][g_state.Winner_id] + 10;
            set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 3:
        if (!g_state.EXE_flag && !g_state.Game_pause) {
            if (ewk->wu.dead_f == 1 || g_state.Suicide[0] != 0) {
                ewk->wu.disp_flag = 0;
                ewk->wu.routine_no[0]++;
                break;
            }

            char_move(&ewk->wu);
        }

        sync_bg_strip_position(ewk);
        sort_push_request(&ewk->wu);
        break;

    case 4:
        ewk->wu.routine_no[0]++;
        break;

    case 5:
    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_33_init(State* wk) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 33;
    ewk->wu.work_id = 16;
    ewk->master_id = wk->id;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.my_col_mode = wk->my_col_mode;
    ewk->wu.my_col_code = wk->my_col_code + 1;
    ewk->wu.my_family = wk->my_family;
    ewk->my_master = wk;
    ewk->wu.rl_flag = wk->rl_flag;
    ewk->wu.xyz[0].disp.pos = wk->xyz[0].disp.pos;
    ewk->wu.xyz[0].disp.pos += wk->rl_flag ? -48 : 48;
    ewk->wu.xyz[1].disp.pos = wk->xyz[1].disp.pos - 12;
    ewk->wu.my_priority = wk->my_priority - 12;
    ewk->wu.position_z = ewk->wu.my_priority - 12;
    ewk->wu.char_table[0] = _etc3_char_table;
    ewk->wu.char_index = 7;
    ewk->wu.sync_bg_strip = 0;
    suzi_offset_set(ewk);
    ewk->wu.my_mts = 0xE;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_mts);
    return 0;
}

const s16 WinLoseID[2][2] = { { 1, 0 }, { 0, 1 } };
