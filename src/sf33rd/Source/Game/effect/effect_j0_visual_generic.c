/**
 * @file effj0.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_j0_visual_generic.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"

void effect_J0_move(State_Other* ewk) {
    State_Other* mwk = (State_Other*)ewk->my_master;
    State_Other* cwk = (State_Other*)ewk->wu.target_adrs;
    State* sub_w = (State*)cwk->wu.target_adrs;
    ImageBuff* image_buff = (ImageBuff*)sub_w + 9;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.anim_hitbox_index = 0;
        ewk->wu.anim_hurtbox_index = 0;
        sort_push_request(&ewk->wu);
        break;

    case 1:
        if (ewk->wu.death_timer == 1) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0] = 2;
            break;
        }

        if (mwk->wu.routine_no[0] >= 2 || mwk->wu.routine_no[1] >= 2) {
            ewk->wu.routine_no[0] = 2;
            ewk->wu.disp_flag = 0;
            break;
        }

        if (!g_state.execute_flag && !g_state.Game_pause && mwk->wu.hit_stop <= 0) {
            if (--ewk->wu.dir_timer == 0) {
                ewk->wu.routine_no[0] = 2;
                break;
            }

            ewk->wu.position_x = image_buff[ewk->wu.dir_step].pos_x;
            ewk->wu.position_y = image_buff[ewk->wu.dir_step].pos_y;
        }

        ewk->wu.old_cgnum = ewk->wu.cg_number = mwk->wu.cg_number;
        ewk->wu.facing_flag = mwk->wu.facing_flag;
        ewk->wu.cg_flip = mwk->wu.cg_flip;
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

s32 effect_J0_init(State_Other* ek, State_Other* mk, s16 data) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(3)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.disp_flag = 2;
    ewk->wu.id = 190;
    ewk->wu.old_cgnum = ek->wu.cg_number = mk->wu.cg_number;
    ewk->wu.dir_step = data;
    ewk->wu.position_x = ewk->wu.old_pos[0] = mk->wu.position_x;
    ewk->wu.position_y = ewk->wu.old_pos[1] = mk->wu.position_y;
    ewk->wu.facing_flag = mk->wu.facing_flag;
    ewk->wu.cg_flip = mk->wu.cg_flip;
    ewk->wu.blink_timing = mk->wu.blink_timing;
    ewk->wu.work_id = 16;
    ewk->wu.my_family = mk->wu.my_family;
    ewk->wu.graphic_rom_type = mk->wu.graphic_rom_type;
    ewk->wu.my_col_mode = mk->wu.my_col_mode;
    ewk->wu.my_col_code = mk->wu.my_col_code;
    ewk->wu.extra_col = mk->wu.current_colcd;
    ewk->my_master = mk;
    ewk->wu.target_adrs = ek;
    ewk->master_id = mk->wu.id;
    ewk->master_player = mk->master_player;
    ewk->wu.dir_timer = ek->wu.dir_timer;
    return 0;
}
