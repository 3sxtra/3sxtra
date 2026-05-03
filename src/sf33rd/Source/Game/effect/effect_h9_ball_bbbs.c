/**
 * @file effh9.c
 * Effect: Ball / BBBS Effect
 */

#include "sf33rd/Source/Game/effect/effect_h9_ball_bbbs.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/stage/bg.h"

const SpriteConnection bbbs_ball[4][3] = {
    { { 153, 0, 0, 32464 }, { 142, 0, 0, 32464 }, { 172, 8, 0, 32488 } },
    { { -141, 0, 0, 32464 }, { -152, 0, 0, 32464 }, { -174, 8, 0, 32488 } },
    { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
    { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
};

static void effH9_trans(State* ewk);
static void nokori_ball_effH9(EffectMultiSprite* ewk, s16 num);

void effect_H9_move(EffectMultiSprite* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        switch (ewk->wu.routine_no[1]) {
        case 0:
            ewk->wu.routine_no[1]++;
            ewk->wu.disp_flag = 1;
            ewk->wu.old_cgnum = 0;
            ewk->wu.position_z = ewk->wu.my_priority = 9;
            ewk->wu.direction = 0;
            ewk->wu.dir_timer = 0;
            nokori_ball_effH9(ewk, ewk->wu.direction);
            break;

        case 1:
            if (g_state.Game_pause || g_state.execute_flag) {
                break;
            }

            if (--ewk->wu.dir_timer > 0) {
                break;
            }

            ewk->wu.dir_timer = 3;
            ewk->wu.direction++;
            nokori_ball_effH9(ewk, ewk->wu.direction);

            if (ewk->wu.direction >= g_state.Bonus_Game_Work) {
                ewk->wu.routine_no[0] = 1;
                ewk->wu.routine_no[1] = 0;
            }

            break;
        }

        effH9_trans(&ewk->wu);
        break;

    case 1:
        if (ewk->wu.death_timer == 1) {
            ewk->wu.disp_flag = 0;
            ewk->wu.type = 0;
            ewk->wu.routine_no[0] = 2;
            break;
        }

        nokori_ball_effH9(ewk, g_state.Bonus_Game_Work);
        effH9_trans(&ewk->wu);
        break;

    case 2:
        ewk->wu.routine_no[0] = 3;
        break;

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

static void effH9_trans(State* ewk) {
    ewk->position_x = g_state.bg_w.bgw[2].wxy[0].disp.pos;
    ewk->position_y = g_state.bg_w.bgw[2].wxy[1].disp.pos;
    sort_push_request3(ewk);
}

static void nokori_ball_effH9(EffectMultiSprite* ewk, s16 num) {
    ewk->conn[0].chr = (num % 10) + 32464;
    ewk->conn[1].chr = (num / 10) + 32464;
}

s32 effect_H9_init(PlayerEntity* wk) {
    EffectMultiSprite* ewk;
    s16 ix;
    s16 i;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (EffectMultiSprite*)frw[ix];
    ewk->wu.active_flag = 1;
    ewk->wu.id = 179;
    ewk->wu.work_id = 16;
    ewk->wu.my_sprite_sheet = 14;
    ewk->wu.my_family = 3;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.type = wk->wu.facing_flag;
    ewk->wu.my_col_mode = 0x4200;
    ewk->wu.my_col_code = 73;
    ewk->num_of_conn = 3;

    if (wk->wu.facing_flag) {
        ix = 1;
    } else {
        ix = 0;
    }

    for (i = 0; i < 3; i++) {
        ewk->conn[i] = bbbs_ball[ix][i];
    }

    return 0;
}
