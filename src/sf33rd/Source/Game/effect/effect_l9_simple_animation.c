/**
 * @file effl9.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_l9_simple_animation.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/bg.h"

void effect_L9_move(State_Other* ewk) {
    State_Other* oya = (State_Other*)ewk->my_master;

    switch (ewk->wu.routine_no[1]) {
    case 0:
        ewk->wu.routine_no[1] += 1;
        ewk->wu.disp_flag = 1;
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        break;

    case 1:
        char_move(&ewk->wu);

        if (oya->wu.routine_no[0] >= 3) {
            ewk->wu.routine_no[1] += 1;
            ewk->wu.disp_flag = 0;
        }

        ewk->wu.position_x = ewk->wu.xyz[0].disp.pos & 0xFFFF;
        ewk->wu.position_y = ewk->wu.xyz[1].disp.pos & 0xFFFF;
        sort_push_request4(&ewk->wu);
        break;

    case 2:
        ewk->wu.routine_no[1] += 1;
        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_L9_init(State_Other* oya, u8 ten_type) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->my_master = oya;
    ewk->master_id = oya->wu.id;
    ewk->wu.type = ten_type;
    ewk->wu.active_flag = 1;
    ewk->wu.id = 219;
    ewk->wu.work_id = 16;
    ewk->wu.my_priority = 64;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.facing_flag = 0;
    ewk->wu.my_col_mode = 0x4200;
    ewk->wu.char_table[0] = _etc2_char_table;
    ewk->wu.my_family = 8;
    ewk->wu.my_col_code = 56;
    ewk->wu.position_x = ewk->wu.xyz[0].disp.pos & 0xFFFF;
    ewk->wu.position_y = ewk->wu.xyz[1].disp.pos & 0xFFFF;
    ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].wxy[0].disp.pos;

    if (ewk->wu.type) {
        ewk->wu.char_index = 53;

        if (g_state.plw[g_state.Winner_id].wu.facing_flag) {
            ewk->wu.xyz[0].disp.pos = 191;
        } else {
            ewk->wu.xyz[0].disp.pos = 192;
        }

        ewk->wu.my_priority = ewk->wu.position_z = 9;
        ewk->wu.xyz[1].disp.pos = 24;
    } else {
        ewk->wu.char_index = 48;
        ewk->wu.my_priority = ewk->wu.position_z = 17;
        ewk->wu.xyz[0].disp.pos = 192;
        ewk->wu.xyz[1].disp.pos = 8;
    }

    ewk->wu.my_sprite_sheet = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    return 0;
}
