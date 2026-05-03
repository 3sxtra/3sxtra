/**
 * @file effm3.c
 * Effect: Bahn / Path Movement Effect
 */

#include "sf33rd/Source/Game/effect/effect_m3_bahn_path_movement.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/stage/bg.h"

const s16 M3_bahn_data[5] = { 16, 10, 78, 0, -512 };

static void effM3_trans(State* ewk);

void effect_M3_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0] = 1;
        ewk->wu.disp_flag = 0;
        ewk->wu.my_sprite_sheet = 13;
        ewk->wu.my_col_mode = 0x4200;
        ewk->wu.my_col_code = 0x90;
        ewk->wu.my_family = 3;
        ewk->wu.position_z = 60 - (ewk->wu.type + 2);
        ewk->wu.damage_calc_multiplier = M3_bahn_data[0];
        ewk->wu.damage_calc_divider = M3_bahn_data[1];
        ewk->wu.dir_timer = M3_bahn_data[2];
        ewk->wu.old_cgnum = 0;
        break;

    case 1:
        if (ewk->wu.death_timer == 1 || g_state.Suicide[2] != 0) {
            ewk->wu.disp_flag = 0;
            ewk->wu.type = 0;
            ewk->wu.routine_no[0] = 2;
            break;
        }

        switch (ewk->wu.routine_no[1]) {
        case 0:
            if (!(g_state.Next_Step & 1)) {
                break;
            }

            ewk->wu.routine_no[1]++;
            ewk->wu.mvxy.a[0].real.h = 64;
            ewk->wu.mvxy.a[0].real.l = -1;
            ewk->wu.mvxy.d[0].real.h = -1;
            ewk->wu.mvxy.d[0].real.l = M3_bahn_data[4] * 16;
            ewk->wu.mvxy.physics_curve_type[0] = 1;
            ewk->wu.mirror_flag = 1;
            /* fallthrough */

        case 1:
            if (--ewk->wu.dir_timer >= 0) {
                break;
            }

            ewk->wu.routine_no[1]++;
            ewk->wu.disp_flag = 1;
            /* fallthrough */

        case 2:
            cal_mvxy_speed(&ewk->wu);
            ewk->wu.mvxy.d[0].sp = (ewk->wu.mvxy.d[0].sp * ewk->wu.damage_calc_multiplier) / ewk->wu.damage_calc_divider;

            if (!ewk->wu.mvxy.a[0].real.h) {
                ewk->wu.routine_no[1]++;

                if (ewk->wu.type == 0) {
                    g_state.Next_Step = 0;
                }
            }

            break;

        default:
            ewk->wu.disp_flag = 0;
            ewk->wu.type = 0;
            ewk->wu.routine_no[0] = 2;
            break;
        }

        ewk->wu.mirror_scale.size.x = ewk->wu.mirror_scale.size.y = ewk->wu.mvxy.a[0].real.h + 63;
        effM3_trans(&ewk->wu);
        break;

    case 2:
        ewk->wu.routine_no[0] = 3;
        break;

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

static void effM3_trans(State* ewk) {
    ewk->position_x = g_state.bg_w.bgw[ewk->my_family - 1].wxy[0].disp.pos;
    ewk->position_y = g_state.bg_w.bgw[ewk->my_family - 1].wxy[1].disp.pos;
    ewk->position_x += ewk->xyz[0].disp.pos;
    ewk->position_y += ewk->xyz[1].disp.pos;
    sort_push_request4(ewk);
}

s32 effect_M3_init(EffectMultiSprite* wk, s16 num) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 223;
    ewk->wu.work_id = 16;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.type = num;
    ewk->wu.xyz[0].disp.pos = wk->conn[num].nx;
    ewk->wu.xyz[1].disp.pos = wk->conn[num].ny + 40;
    ewk->wu.cg_number = wk->conn[num].chr;
    return 0;
}
