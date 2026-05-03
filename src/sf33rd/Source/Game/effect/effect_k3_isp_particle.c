/**
 * @file effk3.c
 * Effect: ISP / Particle Effect
 */

#include "sf33rd/Source/Game/effect/effect_k3_isp_particle.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"

static void set_init_posspeed_effK3(State* wk);

const s16 numof_effK3[4] = { 2, 4, 6, 6 };

const s16 effK3_isp_table[4][4][2] = { { { -512, 768 }, { -1024, 512 }, { 512, 768 }, { 1024, 512 } },
                                       { { -1024, 1280 }, { -1536, 896 }, { 1024, 1280 }, { 1536, 896 } },
                                       { { -1536, 1792 }, { -2048, 1280 }, { 1536, 1792 }, { 2048, 1280 } },
                                       { { -2048, 2304 }, { -2560, 1664 }, { 2048, 2304 }, { 2560, 1664 } } };

const s16 effK3_isp_x_offset[4][8] = { { 0, 128, 256, 384, 0, -128, -256, -384 },
                                      { 0, 128, 256, 384, -128, -384, -512, -640 },
                                      { 0, 256, 384, 512, -256, -512, -768, -1024 },
                                      { 0, 256, 512, 768, -256, -512, -768, -1024 } };

const s16 effK3_isp_y_offset[4][8] = { { 0, 128, 256, 384, 512, 0, -128, -256 },
                                      { 0, 128, 256, 384, 512, 0, -128, -256 },
                                      { 0, 128, 128, 256, 256, 384, 384, -256 },
                                      { 0, 256, 512, 0, -256, -512, 384, -384 } };

const s16 effK3_life_time[4] = { 24, 20, 16, 12 };

void effect_K3_move(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.disp_flag = 1;
        ewk->wu.blink_timing = ewk->master_id;
        set_init_posspeed_effK3(&ewk->wu);
        ewk->wu.position_z = 24;
        set_char_move_init(&ewk->wu, 0, (random_16() & 1) + 0x71);
        /* fallthrough */

    case 1:
        if (ewk->wu.death_timer == 1 || g_state.Suicide[0] != 0) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0] = 2;
            break;
        }

        if (g_state.execute_flag == 0 && g_state.Game_pause == 0) {
            char_move(&ewk->wu);
            add_mvxy_speed(&ewk->wu);
            cal_mvxy_speed(&ewk->wu);

            if (ewk->wu.shadow_y) {
                ewk->wu.shadow_y--;
            } else {
                ewk->wu.disp_flag = 2;
            }

            if (--ewk->wu.shadow_prio < 0) {
                ewk->wu.disp_flag = 0;
                ewk->wu.routine_no[0] = 2;
            }
        }

        ewk->wu.position_x = ewk->wu.xyz[0].disp.pos;
        ewk->wu.position_y = ewk->wu.xyz[1].disp.pos;
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

static void set_init_posspeed_effK3(State* wk) {
    s16 data[4];
    s16 ix;
    s16 flag;

    wk->xyz[0].disp.pos = 520;
    wk->xyz[1].disp.pos = 96;
    flag = (random_32() * 2) - 32;
    wk->xyz[0].disp.pos += flag;
    wk->xyz[1].disp.pos += random_16();

    if (flag < 0) {
        flag = 2;
    } else {
        flag = 0;
    }

    ix = random_16() & 1;
    data[0] = effK3_isp_table[wk->damage_attack_level][flag + ix][0];
    data[2] = effK3_isp_table[wk->damage_attack_level][flag + ix][1];
    data[1] = 0;
    data[3] = -96;
    ix = random_16() & 7;
    data[0] += effK3_isp_x_offset[wk->damage_attack_level][ix];
    ix = random_16() & 7;
    data[2] += effK3_isp_y_offset[wk->damage_attack_level][ix];
    setup_move_data_easy(wk, &data[0], 1, 0);
    wk->shadow_prio = effK3_life_time[wk->damage_attack_level] + (random_16() & 3);
    wk->shadow_y = wk->shadow_prio / 2;
}

static s32 effect_K3_init(State_Other* wk) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(1)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 203;
    ewk->wu.work_id = 16;
    ewk->wu.my_sprite_sheet = 5;
    ewk->wu.my_family = wk->wu.my_family;
    ewk->wu.graphic_rom_type = wk->wu.graphic_rom_type;
    ewk->wu.my_col_mode = wk->wu.my_col_mode;
    ewk->wu.my_col_code = wk->wu.my_col_code;
    ewk->my_master = wk;
    ewk->wu.damage_direction = wk->wu.damage_direction;
    ewk->wu.damage_attack_level = wk->wu.damage_attack_level;
    ewk->master_player = wk->master_player;
    ewk->master_id = wk->master_id;
    ewk->master_work_id = wk->master_work_id;
    *ewk->wu.char_table = _bonus_char_table;
    return 0;
}

s32 setup_effK3(State* wk) {
    s16 i;

    if (wk->type != 3) {
        return 0;
    }

    if (wk->vital_old < 3 || wk->vital_old > 4) {
        return 0;
    }

    for (i = 0; i < numof_effK3[wk->damage_attack_level]; i++) {
        effect_K3_init((State_Other*)wk);
    }

    return 1;
}
