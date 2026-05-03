/**
 * @file effg5.c
 * Effect: Sweat / Ase Effect
 */

#include "sf33rd/Source/Game/effect/effect_g5_sweat_ase.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/calculate_direction.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"

const s16 sweat_particle_count[4] = { 0, 3, 5, 6 };

const s16 sweat_dir_modifier[4][6] = {
    { -2, 3, -1, 30, 1, 33 }, { 2, -3, 1, 33, -1, 30 }, { 1, -2, 2, 34, 0, 31 }, { -1, 2, -2, 31, 0, 34 }
};

const s16 sweat_speed_modifier[4][6] = { { 0x480, 0x500, 0x680, 0x400, 0x700, 0x500 },
                                    { 0x480, 0x500, 0x680, 0x400, 0x700, 0x500 },
                                    { 0x480, 0x500, 0x680, 0x400, 0x700, 0x500 },
                                    { 0x480, 0x500, 0x680, 0x400, 0x700, 0x500 } };

const s16 sweat_delta_modifier[4][6] = { { -88, -96, -112, -80, -128, -88 },
                                    { -88, -96, -112, -80, -128, -88 },
                                    { -88, -96, -112, -80, -128, -88 },
                                    { -88, -96, -112, -80, -128, -88 } };

void effect_G5_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        ewk->wu.my_col_mode = 0x4200;
        ewk->wu.my_col_code = 0x2020;
        add_pos_dir_064(&ewk->wu, ewk->wu.dir_old * 4);
        set_char_move_init(&ewk->wu, 0, (random_16() & 1) + 8);
        /* fallthrough */

    case 1:
        if (ewk->wu.death_timer == 1) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0]++;
            break;
        }

        if (g_state.Pause_Hit_Marks) {
            break;
        }

        if (g_state.execute_flag == 0 && g_state.Game_pause == 0) {
            ewk->wu.dir_old += ewk->wu.dir_step;

            if (ewk->wu.dir_old < 0) {
                ewk->wu.dir_old = 0;
            }

            add_pos_dir_064(&ewk->wu, ewk->wu.dir_old);
            char_move(&ewk->wu);

            if (ewk->wu.cg_type == 0xFF || ewk->wu.dir_old < 0) {
                ewk->wu.disp_flag = 0;
                ewk->wu.routine_no[0]++;
                break;
            }
        }

        ewk->wu.position_x = ewk->wu.xyz[0].disp.pos;
        ewk->wu.position_y = ewk->wu.xyz[1].disp.pos;
        sort_push_request8(&ewk->wu);
        break;

    case 2:
        ewk->wu.routine_no[0] = 3;
        break;

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

static s32 effect_G5_init(State* wk, s16 dr, s16 sp, s16 dl) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(3)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 165;
    ewk->wu.work_id = 16;
    ewk->wu.direction = dr;
    ewk->wu.dir_old = sp;
    ewk->wu.dir_step = dl;
    ewk->wu.my_family = wk->my_family;
    ewk->wu.xyz[0].disp.pos = wk->position_x;
    ewk->wu.xyz[1].disp.pos = wk->position_y;
    ewk->wu.position_z = wk->position_z + 1;
    *ewk->wu.char_table = _plef_char_table;
    return 0;
}

s32 setup_sweat_extra(State* wk, u8 num) {
    s16 i;
    s16 way;
    s16 rnd_00;
    s16 rnd_01;

    if (sweat_particle_count[num] == 0) {
        return 0;
    }

    way = wk->direction * 4;
    rnd_00 = random_16() & 3;
    rnd_01 = random_16() & 3;

    for (i = 0; i < sweat_particle_count[num]; i++) {
        effect_G5_init(
            wk, way + sweat_dir_modifier[rnd_00][i] & 0x3F, sweat_speed_modifier[rnd_01][i], sweat_delta_modifier[rnd_01][i]);
    }

    return 0;
}
