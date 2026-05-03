/**
 * @file eff86.c
 * Effect: Data Table Effect
 */

#include "sf33rd/Source/Game/effect/effect_86_data_table.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"

const s16 eff86_data_tbl00[7] = { 0, 2, 8224, 511, 56, 10, 18 };

const s16* eff86_adrs_tbl[1] = { eff86_data_tbl00 };

void effect_86_move(State_Other* ewk) {
    void (*eff86_jp_tbl[1])(State_Other*) = { eff86_0000 };

    switch (ewk->wu.routine_no[0]) {
    case 0:
        if (!g_state.execute_flag && !g_state.Game_pause && !g_state.EXE_obroll) {
            eff86_jp_tbl[ewk->wu.routine_no[1]](ewk);
        }

        disp_pos_trans_entry(ewk);
        return;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
    }
}

void eff86_0000(State_Other* ewk) {
    s16 work;

    switch (ewk->wu.routine_no[2]) {
    case 0:
        ewk->wu.routine_no[2]++;
        ewk->wu.disp_flag = 1;
        work = g_state.plw[0].wu.position_x + g_state.plw[1].wu.position_x;
        work >>= 1;
        ewk->wu.xyz[0].disp.pos = work;
        work = g_state.plw[1].wu.position_y + g_state.plw[1].wu.position_y;
        work >>= 1;
        ewk->wu.xyz[1].disp.pos = work + 92;
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        break;

    case 1:
        char_move(&ewk->wu);

        if (ewk->wu.cg_type == 0xFF) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0]++;
        }

        break;
    }
}

s32 effect_86_init(s16 type86) {
    State_Other* ewk;
    s16 ix;
    const s16* data_ptr;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    data_ptr = eff86_adrs_tbl[type86];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 86;
    ewk->wu.work_id = 16;
    ewk->wu.type = type86;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.facing_flag = 0;
    ewk->wu.death_timer = 1;
    ewk->wu.routine_no[1] = *data_ptr++;
    ewk->wu.my_col_mode = 0x4200;
    ewk->wu.my_family = *data_ptr++;
    ewk->wu.my_col_code = *data_ptr++;
    ewk->wu.xyz[0].disp.pos = *data_ptr++;
    ewk->wu.xyz[1].disp.pos = *data_ptr++;
    ewk->wu.my_priority = ewk->wu.position_z = *data_ptr++;
    ewk->wu.char_index = *data_ptr++;
    ewk->wu.char_table[0] = _etc_char_table;
    ewk->wu.my_sprite_sheet = 14;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_sprite_sheet);
    return 0;
}
