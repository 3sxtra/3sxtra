/**
 * @file eff84.c
 * Effect: Time Data / Slow Effect
 */

#include "sf33rd/Source/Game/effect/effect_84_time_data_slow.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_56_color_bonus.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/stage/bg.h"

const u8 Time_Data[5] = { 80, 90, 50, 50, 50 };

void effect_84_move(State_Other* ewk) {
    if (g_state.Suicide[0]) {
        Release_Effect(&ewk->wu);
        return;
    }

    switch (ewk->wu.routine_no[0]) {
    case 0:
        if (g_state.request_message) {
            ewk->wu.routine_no[0]++;
            ewk->wu.dir_timer = Time_Data[g_state.request_message];
        }

        break;

    case 1:
        switch (ewk->wu.routine_no[1]) {
        case 0:
            switch (g_state.message_index) {
            case 0:
                g_state.Game_pause = 1;
                ewk->wu.routine_no[1]++;
                effect_56_init(0, 0);
                break;

            case 1:
                g_state.Game_pause = 1;
                ewk->wu.routine_no[1]++;
                effect_56_init(1, 0);
                break;

            case 2:
                g_state.Game_pause = 1;
                ewk->wu.routine_no[1]++;

                if (g_state.Bonus_Game_Flag == 20 && g_state.bg_w.stage == 20) {
                    ewk->wu.dir_timer = 90;
                }

                effect_56_init(2, 0);
                break;

            case 3:
                ewk->wu.routine_no[1]++;
                effect_56_init(3, 3);
                break;

            case 4:
            default:
                ewk->wu.routine_no[1]++;
                effect_56_init(4, 2);
                break;
            }

            break;

        case 1:
            if ((ewk->wu.dir_timer -= 1) != 0) {
                break;
            }

            switch (g_state.message_index) {
            case 4:
                g_state.Message_Suicide[2] = 1;
                break;

            case 3:
                g_state.Message_Suicide[3] = 1;
                break;

            default:
                g_state.Message_Suicide[0] = 1;
                break;
            }

            dead_voice_request();
            g_state.request_message = 0;
            g_state.Game_pause = 0;
            ewk->wu.routine_no[0] = ewk->wu.routine_no[1] = 0;
            break;
        }

        break;

    case 2:
        break;
    }
}

s32 effect_84_init() {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 84;
    return 0;
}
