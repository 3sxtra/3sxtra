/**
 * @file eff80.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_80_visual_generic.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "port/sdl/rmlui/rmlui_char_select.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"

void effect_80_move(State_Other* ewk) {
    State_Other* mwk = (State_Other*)ewk->my_master;

    if (mwk->wu.active_flag == 0) {
        ewk->wu.disp_flag = 0;
        ewk->wu.routine_no[0] = 3;
        return;
    }

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        set_char_move_init2(&ewk->wu, 0, ewk->wu.char_index, ewk->wu.dir_step + 1, 0);
        break;

    case 1:
        if (Ck_Range_Out_S(ewk, 2, 96)) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0]++;
            return;
        }

        break;

    case 2:
        ewk->wu.routine_no[0] = 99;
        return;

    default:
        Release_Effect(&ewk->wu);
        return;
    }

    ewk->wu.disp_flag = g_state.Disp_Command_Name[ewk->master_id][ewk->master_player];
    ewk->wu.position_x = ewk->wu.xyz[0].disp.pos = mwk->wu.position_x;
    ewk->wu.position_y = ewk->wu.xyz[1].disp.pos = mwk->wu.position_y;
    ewk->wu.position_z = ewk->wu.xyz[2].disp.pos = mwk->wu.position_z - 1;
    if (!rmlui_char_select_visible)
        sort_push_request4(&ewk->wu);
}

s32 effect_80_init(State_Other* mwk, s16 PL_id, s16 Plate_id, s16 Target_BG) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 80;
    ewk->wu.work_id = 16;
    ewk->wu.my_col_code = 0x90;
    ewk->wu.my_family = Target_BG + 1;
    *ewk->wu.char_table = _sel_pl_char_table;
    ewk->master_id = PL_id;
    ewk->master_player = Plate_id;
    ewk->my_master = mwk;
    ewk->wu.my_sprite_sheet = 13;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    ewk->wu.char_index = 7;
    ewk->wu.dir_step = Plate_id + (g_state.My_char[PL_id] * 3) + (PL_id * 60);
    return 0;
}
