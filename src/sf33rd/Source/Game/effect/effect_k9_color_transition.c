/**
 * @file effk9.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_k9_color_transition.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/color_palette.h"
#include "sf33rd/Source/Game/stage/stage_data.h"

void effect_K9_move(State_Other* ewk) {
    State* mwk = (State*)ewk->my_master;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        push_color_trans_req(0x36, 10);
        ewk->wu.my_col_code = 10;
        ewk->wu.my_priority = ewk->wu.position_z = 32;
        ewk->wu.my_clear_level = 0xA0;
        ewk->wu.mirror_flag = 1;
        ewk->wu.mirror_scale.size.x = 127;
        ewk->wu.mirror_scale.size.y = 112;
        set_char_move_init(&ewk->wu, 0, 0x90);
        /* fallthrough */

    case 1:
        if (ewk->wu.death_timer == 0) {
            if (g_state.execute_flag != 0 || g_state.Game_pause != 0 ||
                (ewk->wu.dir_old == mwk->current_char_type && ewk->wu.dir_step == mwk->char_index &&
                 (char_move(&ewk->wu), ewk->wu.cg_type != 0xFF))) {
                sort_push_request(&ewk->wu);
                return;
            }
        }

        ewk->wu.disp_flag = 0;
        ewk->wu.routine_no[0] = 2;
        return;

    default:
    case 2:
        Release_Effect(&ewk->wu);
        return;
    }
}

s32 effect_K9_init(State* wk, u8 data) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(5)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->my_master = wk;
    ewk->wu.active_flag = 1;
    ewk->wu.id = 209;
    ewk->wu.type = data;
    ewk->wu.work_id = 16;
    ewk->wu.my_sprite_sheet = 6;
    ewk->wu.my_family = 8;
    ewk->wu.position_x = 192;
    ewk->wu.position_y = 112 - g_state.base_y_pos;
    ewk->wu.dir_old = wk->current_char_type;
    ewk->wu.dir_step = wk->char_index;
    *ewk->wu.char_table = _plef_char_table;
    return 0;
}
