/**
 * @file eff96.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_96_suicide_handler.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"

void effect_96_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.char_table[0] = _ef13_char_table;
        set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        /* fallthrough */

    case 1:
        if (ewk->wu.death_timer == 1 || g_state.Suicide[6] != 0) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0]++;
            break;
        }

        if (ewk->wu.hit_stop) {
            ewk->wu.hit_stop--;
        } else if (g_state.execute_flag == 0 && g_state.Game_pause == 0) {
            char_move(&ewk->wu);

            if (ewk->wu.cg_type == 0xFF) {
                ewk->wu.disp_flag = 0;
                ewk->wu.routine_no[0]++;
                break;
            }
        }

        sort_push_request(&ewk->wu);
        break;

    case 2:
        ewk->wu.routine_no[0] = 3;
        break;

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_96_init(State* wk, u8 chix, s8 dspf, s32 /* unused */) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(3)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 96;
    ewk->wu.work_id = 16;
    ewk->wu.blink_timing = wk->blink_timing;
    ewk->wu.char_index = chix;
    ewk->wu.disp_flag = dspf;
    ewk->wu.hit_stop = 0;
    ewk->wu.facing_flag = wk->facing_flag;
    ewk->wu.my_family = wk->my_family;
    ewk->wu.graphic_rom_type = wk->graphic_rom_type;
    ewk->wu.my_col_mode = wk->my_col_mode;
    ewk->wu.my_col_code = wk->my_col_code;
    ewk->wu.my_sprite_sheet = 14;
    ewk->wu.position_x = wk->position_x;
    ewk->wu.position_y = wk->position_y;
    ewk->wu.position_z = 26;

    if (wk->work_id == 1) {
        ewk->master_player = ((PlayerEntity*)wk)->player_number;
        ewk->master_id = wk->id;
    } else {
        ewk->master_player = ((State_Other*)wk)->master_player;
        ewk->master_id = ((State_Other*)wk)->master_id;
    }

    return 0;
}
