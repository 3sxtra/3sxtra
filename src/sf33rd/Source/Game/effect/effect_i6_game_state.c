/**
 * @file effi6.c
 * Effect: Game State Effect
 */

#include "sf33rd/Source/Game/effect/effect_i6_game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "game_state.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

// Forward decls

static void effi6_line_move(State_Other* ewk);

// Funcs

void effect_I6_move(State_Other* ewk) {
    State_Other* oya_ptr = (State_Other*)ewk->my_master;

    switch (oya_ptr->wu.routine_no[0]) {
    case 2:
        effi6_line_move(ewk);
        /* fallthrough */

    case 3:
        ewk->wu.mirror_scale.size.x = oya_ptr->wu.mirror_scale.size.x;
        ewk->wu.mirror_scale.size.y = oya_ptr->wu.mirror_scale.size.y;
        disp_pos_trans_entry5(ewk);
        break;

    case 4:
        ewk->wu.disp_flag = 0;
        disp_pos_trans_entry5(ewk);
        break;

    case 5:
    case 99:
        Release_Effect(&ewk->wu);
        break;
    }
}

static void effi6_line_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[1]) {
    case 0:
        ewk->wu.routine_no[1] += 1;
        ewk->wu.disp_flag = 1;
        ewk->wu.mirror_flag = 1;
        ewk->wu.mirror_scale.size.x = 0;
        ewk->wu.mirror_scale.size.y = 0;
        set_char_move_init2(&ewk->wu, 0, 2, 3, 0);
        /* fallthrough */

    case 1:
        break;
    }
}

s32 effect_I6_init(State_Other* oya) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 0xBA;
    ewk->wu.work_id = 0x10;
    ewk->wu.graphic_rom_type = 1;
    ewk->my_master = oya;
    ewk->wu.my_family = 4;
    ewk->wu.my_col_code = 0x52;
    ewk->wu.my_priority = ewk->wu.position_z = 10;
    ewk->wu.my_sprite_sheet = 0xE;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    *ewk->wu.char_table = (u32*)_etc_char_table;
    ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[ewk->wu.my_family - 1].position_x + g_state.bg_w.pos_offset;
    ewk->wu.position_x = ewk->wu.xyz[0].disp.pos;
    ewk->wu.xyz[1].disp.pos = 0x90;
    return 0;
}
