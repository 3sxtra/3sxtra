/**
 * @file effi0.c
 * Effect: Pebble / Koishi Effect
 */

#include "sf33rd/Source/Game/effect/effect_i0_pebble_koishi.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"

// forward declarations

const s16 pebble_charset_ids[8];
const s16 pebble_particle_count[8];
const s16 pebble_area_correction[5];
const s16 pebble_spawn_area[8][16];
const s16 pebble_speed_x[5][8];
const s16 pebble_speed_y[5][8];

void effect_I0_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0] += 1;
        ewk->wu.disp_flag = 1;
        ewk->wu.my_col_mode = 0x4200;
        ewk->wu.my_col_code = 0x2020;
        set_char_move_init(&ewk->wu, 0, pebble_charset_ids[random_16() & 7]);
        /* fallthrough */
    case 1:
        if (ewk->wu.death_timer == 1) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0] += 1;
            return;
        }

        if ((g_state.execute_flag == 0) && (g_state.Game_pause == 0)) {
            switch (ewk->wu.routine_no[1]) {
            case 0:
                add_mvxy_speed(&ewk->wu);
                cal_mvxy_speed(&ewk->wu);
                if (ewk->wu.mvxy.a[1].sp <= 0) {
                    ewk->wu.routine_no[1] += 1;
                }
                char_move(&ewk->wu);
                break;

            case 1:
                add_mvxy_speed(&ewk->wu);
                cal_mvxy_speed(&ewk->wu);
                if (ewk->wu.xyz[1].disp.pos <= ewk->wu.next_y) {
                    ewk->wu.routine_no[1] += 1;
                    char_move_wca(&ewk->wu);
                } else {
                default:
                    char_move(&ewk->wu);
                    if (ewk->wu.cg_type == 0xFF) {
                        ewk->wu.disp_flag = 0;
                        ewk->wu.routine_no[0] += 1;
                    }
                }
                break;
            }
        }
        ewk->wu.position_x = ewk->wu.xyz[0].disp.pos;
        ewk->wu.position_y = ewk->wu.xyz[1].disp.pos;
        sort_push_request8(&ewk->wu);
        return;

    case 2:
        ewk->wu.routine_no[0] = 3;
        return;

    default:
        Release_Effect(&ewk->wu);
        return;
    }
}

s32 effect_I0_init(State* wk, s16 hsx, s16 hsy, s16 spx, s16 spy, s16 nxy) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(3)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 0xB4;
    ewk->wu.work_id = 0x10;
    ewk->wu.facing_flag = wk->facing_flag;
    ewk->wu.my_family = wk->my_family;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.next_y = nxy;
    ewk->wu.mvxy.a[0].sp = spx << 8;
    ewk->wu.mvxy.d[0].sp = 0;
    ewk->wu.mvxy.a[1].sp = spy << 8;
    ewk->wu.mvxy.d[1].sp = -0x8000U;

    if (ewk->wu.facing_flag) {
        ewk->wu.xyz[0].disp.pos = wk->position_x - hsx;
    } else {
        ewk->wu.xyz[0].disp.pos = wk->position_x + hsx;
    }

    ewk->wu.xyz[1].disp.pos = wk->position_y + hsy;
    ewk->wu.position_z = wk->position_z + 1;
    ewk->wu.char_table[0] = _plef_char_table;
    return 0;
}

s32 setup_pebble_extra(State* wk, u8 num) {
    s16* dix;
    s16 i;
    s16 hsx;
    s16 hsy;
    s16 spx;
    s16 spy;
    s16 nxy;

    dix = (s16*)pebble_spawn_area[random_16() & 7];

    for (i = 0; i < pebble_particle_count[num]; i++) {
        hsx = (pebble_area_correction[dix[i]] + (random_16() - 7));
        hsy = -(random_16() & 3);
        nxy = (hsy - (random_16() & 3));
        spx = pebble_speed_x[dix[i]][random_16() & 7];
        spy = pebble_speed_y[dix[i]][random_16() & 7];
        effect_I0_init(wk, hsx, hsy, spx, spy, nxy);
    }

    return 0;
}

const s16 pebble_charset_ids[8] = { 85, 86, 87, 85, 86, 87, 85, 86 };

const s16 pebble_particle_count[8] = { 6, 7, 8, 10, 12, 14, 15, 16 };

const s16 pebble_area_correction[5] = { 0, 20, -20, 40, -40 };

const s16 pebble_spawn_area[8][16] = {
    { 0, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 },
    { 3, 4, 3, 4, 1, 2, 1, 2, 0, 0, 1, 2, 3, 4, 3, 4 }, { 3, 4, 3, 0, 1, 2, 4, 3, 4, 3, 0, 1, 2, 4, 3, 4 },
    { 1, 2, 1, 2, 3, 4, 3, 4, 0, 0, 1, 2, 1, 2, 3, 4 }, { 0, 2, 4, 1, 3, 1, 3, 2, 4, 0, 1, 2, 3, 4, 1, 2 },
    { 2, 3, 1, 2, 0, 0, 4, 4, 0, 2, 3, 1, 4, 2, 4, 3 }, { 2, 4, 2, 4, 0, 1, 3, 0, 1, 3, 2, 2, 4, 0, 1, 3 }
};

const s16 pebble_speed_x[5][8] = { { 64, -64, 128, -128, 256, -256, 384, -384 },
                                   { -256, -384, -512, -640, -768, -896, -1024, -1280 },
                                   { 384, 512, 640, 768, 896, 1024, 1152, 1408 },
                                   { -768, -896, -1024, -1152, -1280, -1408, -1536, -1664 },
                                   { 896, 1024, 1152, 1280, 1408, 1536, 1664, 2048 } };

const s16 pebble_speed_y[5][8] = { { 768, 1024, 1152, 1280, 1408, 1536, 1792, 2048 },
                                   { 640, 704, 768, 832, 896, 1024, 1280, 1536 },
                                   { 576, 640, 704, 768, 832, 896, 1152, 1408 },
                                   { 512, 576, 640, 704, 768, 896, 1024, 1152 },
                                   { 512, 448, 512, 576, 640, 704, 896, 1024 } };
