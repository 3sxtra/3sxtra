/**
 * @file eff81.c
 * Effect: Quake Effect
 */

#include "sf33rd/Source/Game/effect/effect_81_quake.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"

void effect_81_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        if (--ewk->wu.dir_timer) {
            return;
        }

        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        ewk->wu.mvxy.a[0].sp = 0xE0000;
        ewk->wu.mvxy.d[0].sp = 0x10000;
        ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].xy[0].disp.pos - 416;
        ewk->wu.xyz[1].disp.pos = g_state.bg_w.bgw[1].xy[1].disp.pos - 24;
        ewk->wu.hit_quake = g_state.bg_w.bgw[1].xy[0].disp.pos - 16;
        set_char_move_init2(&ewk->wu, 0, ewk->wu.char_index, ewk->wu.dir_step + 1, 0);
        break;

    case 1:
        ewk->wu.xyz[0].cal += ewk->wu.mvxy.a[0].sp;
        ewk->wu.mvxy.a[0].sp += ewk->wu.mvxy.d[0].sp;

        if (ewk->wu.hit_quake <= ewk->wu.xyz[0].disp.pos) {
            ewk->wu.routine_no[0]++;
            ewk->wu.dir_timer = 39;
        }

        break;

    case 2:
        if (--ewk->wu.dir_timer == 0) {
            ewk->wu.routine_no[0]++;
        }

        break;

    case 3:
        ewk->wu.xyz[0].cal += ewk->wu.mvxy.a[0].sp;
        ewk->wu.mvxy.a[0].sp += ewk->wu.mvxy.d[0].sp;

        if (Ck_Range_Out_S(ewk, ewk->wu.my_family - 1, 240)) {
            ewk->wu.routine_no[0]++;
            ewk->wu.disp_flag = 0;
            return;
        }

        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        return;
    }

    ewk->wu.position_x = ewk->wu.xyz[0].disp.pos & 0xFFFF;
    ewk->wu.position_y = ewk->wu.xyz[1].disp.pos + g_state.base_y_pos + 0xFFFF;
    sort_push_request4(&ewk->wu);
}

s32 effect_81_init(s16 Time) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 81;
    ewk->wu.work_id = 16;
    ewk->wu.my_col_code = 8336;
    ewk->wu.my_family = 2;
    ewk->wu.char_index = 16;
    ewk->wu.dir_step = 7;
    ewk->wu.char_table[0] = (u32*)_sel_pl_char_table;
    ewk->wu.dir_timer = Time;
    ewk->wu.position_z = 8;
    g_state.Appear_Q = 1;
    ewk->wu.my_sprite_sheet = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    ewk->wu.mirror_flag = 1;
    ewk->wu.mirror_scale.size.x = 127;
    ewk->wu.mirror_scale.size.y = 127;
    return 0;
}
