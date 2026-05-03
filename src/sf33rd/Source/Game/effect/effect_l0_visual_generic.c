/**
 * @file effl0.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/effect_l0_visual_generic.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/state_user.h"

void effect_L0_move(State_Other* ewk) {
    PLW* mwk = (PLW*)ewk->my_master;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        mwk->wu.disp_flag = 0;
        /* fallthrough */

    case 1:
        if (ewk->wu.dead_f == 0 && g_state.Suicide[0] == 0) {
            if (g_state.Game_pause || g_state.EXE_flag) {
                break;
            }

            if (mwk->sa_stop_flag != 1) {
                ewk->wu.dir_timer--;
            }

            if (ewk->wu.dead_f == 0 && ewk->wu.dir_timer > 0 && mwk->wu.routine_no[1] != 1 &&
                mwk->wu.routine_no[1] != 2 && mwk->wu.routine_no[1] != 3 &&
                (mwk->wu.current_char_type != 5 ||
                 (!(mwk->wu.attack_type & 0x20) && mwk->wu.char_index != 0x40 && mwk->wu.char_index != 1))) {

                if (ewk->wu.dir_timer >= 30) {
                    break;
                }

                mwk->wu.my_bright_type = 1;

                if (ewk->wu.dir_timer < 10) {
                    mwk->wu.disp_flag = 1;
                    mwk->wu.my_bright_level = ewk->wu.dir_timer;
                } else {
                    mwk->wu.disp_flag = 2;
                    mwk->wu.my_bright_level = 13;
                    mwk->wu.my_col_mode = 0x4400;
                }

                break;
            }

            mwk->wu.disp_flag = 1;
            mwk->wu.my_bright_type = 0;
            mwk->wu.my_bright_level = 0;
            mwk->wu.my_clear_level = 0;
            mwk->wu.my_col_mode = 0x4200;
        }

        ewk->wu.routine_no[0]++;
        break;

    default:
    case 2:
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_L0_init(State* wk, s16 data) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(3)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->my_master = wk;
    ewk->wu.be_flag = 1;
    ewk->wu.id = 210;
    ewk->wu.dir_timer = data;
    ewk->wu.work_id = 16;
    ewk->wu.my_col_mode = wk->my_col_mode;
    ewk->wu.my_col_code = wk->my_col_code;
    ewk->wu.current_char_type = wk->current_char_type;
    ewk->wu.char_index = wk->char_index;
    effect_L0_move(ewk);
    return 0;
}
