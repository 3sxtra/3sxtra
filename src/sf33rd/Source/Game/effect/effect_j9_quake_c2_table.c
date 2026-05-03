/**
 * @file effj9.c
 * Effect: Quake Effect (C2 Table)
 */

#include "sf33rd/Source/Game/effect/effect_j9_quake_c2_table.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_00_judge_system.h"
#include "sf33rd/Source/Game/effect/effect_c2_quake_bs2_data.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charid.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"

static void effJ9_trans(State* wk);
static s16 get_c2_quake(State* c2wk);

const s16 c2quake_table[19] = { 0, 3, 3, 2, 2, 1, 1, 1, 0, 0, 0, -1, -1, -1, -2, -2, -3, -3, 0 };

void effect_J9_move(State_Other* ewk) {
    State* c2wk = (State*)ewk->my_master;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.charset_id = 17;
        set_char_base_data(&ewk->wu);
        ewk->wu.my_col_mode = 0x4400;
        ewk->wu.my_col_code = 0x21FF;
        ewk->wu.position_y = ewk->wu.xyz[1].disp.pos;
        ewk->wu.position_z = ewk->wu.my_priority = 68;
        effect_00_init(&ewk->wu);
        ewk->wu.next_x = 0;
        break;

    case 1:
        if (ewk->wu.dead_f == 1) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0]++;
            break;
        }

        switch (ewk->wu.routine_no[1]) {
        case 0:
            ewk->wu.routine_no[1]++;
            ewk->wu.disp_flag = 1;
            set_char_move_init(&ewk->wu, 0, 0x44);
            break;

        case 1:
            ewk->wu.next_x = get_c2_quake(c2wk);

            if (c2wk->char_index == 0x47) {
                ewk->wu.next_x = 0;
                ewk->wu.routine_no[1]++;
                set_char_move_init(&ewk->wu, 0, 0x45);
            }

            break;

        default:
            ewk->wu.xyz[0].disp.pos = c2wk->xyz[0].disp.pos;
            break;
        }

        player_hosei_data(ewk, c2wk->dir_timer, 0);
        effJ9_trans(&ewk->wu);
        break;

    case 2:
        ewk->wu.routine_no[0] = 3;
        break;

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

static void effJ9_trans(State* wk) {
    wk->position_x = wk->xyz[0].disp.pos + wk->next_x;
    sort_push_request(wk);
}

static s16 get_c2_quake(State* c2wk) {
    u16 c2cg;

    if ((c2cg = c2wk->cg_number) > 18) {
        return 0;
    }

    return c2quake_table[c2cg];
}

s32 effect_J9_init(State_Other* wk, u8 data) {
    State_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(3)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 199;
    ewk->wu.work_id = 16;
    ewk->wu.type = data;
    ewk->wu.my_mts = 14;
    ewk->my_master = wk;
    ewk->master_player = wk->master_player;
    ewk->master_id = wk->master_id;
    ewk->master_work_id = wk->master_work_id;
    ewk->wu.xyz[0].disp.pos = wk->wu.xyz[0].disp.pos;
    ewk->wu.xyz[1].disp.pos = wk->wu.xyz[1].disp.pos;
    return 0;
}
