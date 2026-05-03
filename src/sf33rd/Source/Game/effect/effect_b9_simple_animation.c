/**
 * @file effb9.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_b9_simple_animation.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"
#include "structs.h"

// sbss

State_Other* oya_p = NULL;

// Funcs

void effect_B9_move(State_Other* ewk) {
    oya_p = (State_Other*)ewk->my_master;

    switch (oya_p->wu.routine_no[0]) {
    case 2:
        switch (ewk->wu.routine_no[1]) {
        case 0:
            ewk->wu.routine_no[1] += 1;
            ewk->wu.disp_flag = 1;
            set_char_move_init2(&ewk->wu, 0, ewk->wu.old_routine_no[0], ewk->wu.char_index, 0);
            ewk->wu.mirror_flag = 1;
            /* fallthrough */

        case 1:
            ewk->wu.mirror_scale.size.x = oya_p->wu.mirror_scale.size.x;
            ewk->wu.mirror_scale.size.y = oya_p->wu.mirror_scale.size.y;
            disp_pos_trans_entry5(ewk);
            break;
        }

        break;

    case 3:
        ewk->wu.mirror_scale.size.x = oya_p->wu.mirror_scale.size.x;
        ewk->wu.mirror_scale.size.y = oya_p->wu.mirror_scale.size.y;
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

s32 effect_B9_init(State_Other* oya) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(3)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 0x77;
    ewk->wu.work_id = 0x10;
    ewk->my_master = oya;
    ewk->wu.my_family = 4;
    ewk->wu.my_col_code = 0x52;
    ewk->wu.my_priority = ewk->wu.position_z = 10;
    *ewk->wu.char_table = _etc_char_table;
    ewk->wu.my_sprite_sheet = 0xE;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);

    if (oya->wu.type) {
        ewk->wu.old_routine_no[0] = 2;
        ewk->wu.char_index = 2;
        ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[ewk->wu.my_family - 1].position_x + g_state.bg_w.pos_offset;
        ewk->wu.xyz[0].disp.pos += 0x50;
        ewk->wu.xyz[1].disp.pos = 0x90;
    } else {
        ewk->wu.old_routine_no[0] = 3;
        ewk->wu.char_index = g_state.Round_num + 1;
        ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[ewk->wu.my_family - 1].position_x + g_state.bg_w.pos_offset;
        ewk->wu.xyz[0].disp.pos += 0x70;
        ewk->wu.xyz[1].disp.pos = 0x90;
    }

    return 0;
}
