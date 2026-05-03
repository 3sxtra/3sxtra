/**
 * @file plpat_gill.c
 * Gill Attacks
 */

#include "sf33rd/Source/Game/engine/player_pattern_gill.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effi3.h"
#include "sf33rd/Source/Game/engine/calculate_direction.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_pattern.h"
#include "sf33rd/Source/Game/engine/player_patternuni.h"
#include "sf33rd/Source/Game/engine/player_damage_controller.h"
#include "sf33rd/Source/Game/engine/player_common_mechanics.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"

static s16 get_life_add_point(u8 num, s16 ori_add);

#define EXATT_TABLE_SIZE 18

void (*const pl00_exatt_table[18])(PLW*);

const s16 mnd_em_tall[21][2] = { { 28, 56 }, { 24, 44 }, { 24, 40 }, { 20, 32 }, { 24, 48 }, { 24, 40 }, { 28, 60 },
                                 { 16, 44 }, { 32, 32 }, { 28, 24 }, { 20, 32 }, { 24, 40 }, { 24, 40 }, { 28, 56 },
                                 { 24, 40 }, { 24, 40 }, { 24, 40 }, { 24, 40 }, { 24, 40 }, { 24, 40 }, { 24, 40 } };

const s16 glap_table[5] = { 1, 2, 3, 4, 0 };

/** @brief Gill: extra attack dispatcher. */
void pl_gill_extra_attack(PLW* wk) {
    s16 idx = wk->wu.routine_no[2] - 16;
    if (idx >= 0 && idx < EXATT_TABLE_SIZE)
        pl00_exatt_table[idx](wk);
}

/** @brief Gill: Moonsault Knee Drop attack. */
static void Att_MOONSALT_KNEE_DROP(PLW* wk) {
    PLW* twk;
    s16 ex;
    s16 ey;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        wk->wu.rl_flag = wk->wu.active_move;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        setup_mvxy_data(&wk->wu, wk->as->data_ix);
        twk = (PLW*)wk->wu.target_adrs;

        if (wk->wu.rl_flag) {
            ex = twk->wu.position_x - mnd_em_tall[twk->player_number][0];
        } else {
            ex = twk->wu.position_x + mnd_em_tall[twk->player_number][0];
        }

        ey = mnd_em_tall[twk->player_number][1];
        wk->wu.mvxy.a[0].sp = 0;
        cal_delta_speed(&wk->wu, wk->as->r_no, ex, ey, 2, 2);

        if (wk->wu.rl_flag == 0) {
            wk->wu.mvxy.a[0].sp = -wk->wu.mvxy.a[0].sp;
            wk->wu.mvxy.d[0].sp = -wk->wu.mvxy.d[0].sp;
        }

        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 20) {
            wk->wu.routine_no[3]++;
            wk->wu.cg_type = 0;
            add_mvxy_speed(&wk->wu);
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);
        break;

    case 3:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Gill: Resurrection Super Art (revive with health regen). */
static void Att_RESURRECTION(PLW* wk) {
    wk->scr_pos_set_flag = 0;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        wk->wu.direction = 0;
        reset_mvxy_data(&wk->wu);
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        g_state.round_slow_flag = false;
        wk->resurrection_resv = 0;
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type) {
            wk->wu.direction = get_life_add_point(wk->wu.cg_type, wk->wu.direction);
            wk->wu.cg_type = 0;
        }

        wk->wu.vital_new += wk->wu.direction;

        if (wk->wu.vital_new >= wk->wu.vitality) {
            wk->wu.vital_new = wk->wu.vitality;
            wk->wu.mvxy.d[1].sp = -0x8000;
            wk->wu.direction = 0;
            wk->wu.routine_no[3]++;

            if (wk->wu.vital_new < 0) {
                wk->wu.vital_new = 0;
            }

            char_move_cmja(&wk->wu);
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);
        break;

    default:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Returns the vitality add point scaled by resurrection count. */
static s16 get_life_add_point(u8 num, s16 ori_add) {
    s16 add_pts = ori_add;
    u16 ix = num - 20;

    if (ix < 9) {
        add_pts = glap_table[ix / 2];
    }

    return add_pts;
}

/** @brief Gill: special action (tokushu koudou). */
static void Att_PL00_TOKUSHUKOUDOU(PLW* wk) {
    wk->scr_pos_set_flag = 0;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        wk->wu.rl_flag = wk->wu.active_move;
        force_grounded_state(wk);
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 40) {
            wk->wu.cg_type = 0;
            add_sp_arts_gauge_tokushu(wk);
        }

        if (wk->wu.cg_type == 64) {
            wk->wu.routine_no[3]++;
            wk->strike_scaling += 16;
            wk->throw_scaling += 8;
            wk->stun_scaling += 2;

            if (wk->strike_scaling > 16) {
                wk->strike_scaling = 16;
            }

            if (wk->throw_scaling > 8) {
                wk->throw_scaling = 8;
            }

            if (wk->stun_scaling > 2) {
                wk->stun_scaling = 2;
            }

            grade_add_personal_action(wk->wu.id);
        }

        break;

    default:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Gill: Jyouka (purification) Super Art. */
static void Att_JYOUKA(PLW* wk) {
    s16 x1;
    s16 y1;

    wk->wu.swallow_no_effect = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        if (g_state.Bonus_Game_Flag == 20) {
            wk->wu.routine_no[3] = 10;
            wk->wu.rl_flag = wk->wu.active_move;
            set_char_move_init(&wk->wu, 5, wk->as->char_ix);
            x1 = g_state.bg_w.bgw[1].wxy[0].disp.pos;
            y1 = 80;
            cal_all_speed_data(&wk->wu, 20, x1, y1, 1, 2);

            if (wk->wu.rl_flag == 0) {
                wk->wu.mvxy.a[0].sp = -wk->wu.mvxy.a[0].sp;
                wk->wu.mvxy.d[0].sp = -wk->wu.mvxy.d[0].sp;
            }

            effect_I3_init(&wk->wu, 3);
            wk->sfwing_pos = 88;
            Bg_Y_Sitei(1, wk->sfwing_pos);
            break;
        }

        wk->wu.routine_no[3]++;
        wk->wu.rl_flag = wk->wu.active_move;
        set_char_move_init(&wk->wu, 5, (wk->as->char_ix));
        x1 = g_state.bg_w.bgw[1].wxy[0].disp.pos;
        y1 = 40;
        cal_all_speed_data(&wk->wu, 20, x1, y1, 1, 1);

        if (wk->wu.rl_flag == 0) {
            wk->wu.mvxy.a[0].sp = -wk->wu.mvxy.a[0].sp;
            wk->wu.mvxy.d[0].sp = -wk->wu.mvxy.d[0].sp;
        }

        effect_I3_init(&wk->wu, 3);

        break;

    case 1:
    case 10:
        if ((setup_kuzureochi(wk) == 0) && (char_move(&wk->wu), wk->wu.cg_type == 20)) {
            wk->wu.routine_no[3]++;
            wk->wu.cg_type = 0;
        }

        break;

    case 2:
        if (wk->resurrection_resv) {
            set_char_move_init2(&wk->wu, 5, 60, 8, 1);
            reset_mvxy_data(&wk->wu);
            wk->wu.routine_no[3] = 3;
            break;
        }
        char_move(&wk->wu);
        add_mvxy_speed(&wk->wu);
        cal_mvxy_speed(&wk->wu);

        if (wk->wu.cg_type == 30) {
            wk->wu.routine_no[3]++;
            wk->wu.cg_type = 0;
            reset_mvxy_data(&wk->wu);
        }

        break;

    case 3:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 20) {
            wk->wu.routine_no[3]++;
            wk->wu.cg_type = 0;
            wk->wu.mvxy.d[1].sp = 0xFFFFA000;
            add_mvxy_speed(&wk->wu);
        }

        break;

    case 4:
        jumping_union_process(&wk->wu, 5);
        break;

    case 5:
        char_move(&wk->wu);
        break;

    case 11:
        wk->sfwing_pos += 2;
        char_move(&wk->wu);
        add_mvxy_speed(&wk->wu);
        cal_mvxy_speed(&wk->wu);

        if (wk->wu.cg_type == 30) {
            wk->wu.routine_no[3]++;
            wk->wu.cg_type = 0;
            reset_mvxy_data(&wk->wu);
        }

        Bg_Y_Sitei(1, wk->sfwing_pos);
        break;

    case 12:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 20) {
            wk->wu.routine_no[3] = 4;
            wk->wu.cg_type = 0;
            wk->wu.mvxy.d[1].sp = 0xFFFFA000;
            add_mvxy_speed(&wk->wu);
        }

        break;
    }
}

void (*const pl00_exatt_table[18])(PLW*) = { Att_HADOUKEN,
                                             Att_MOONSALT_KNEE_DROP,
                                             Att_SLIDE_and_JUMP,
                                             Att_SENPUUKYAKU,
                                             Att_HADOUKEN,
                                             Att_RESURRECTION,
                                             Att_JYOUKA,
                                             Att_DUMMY,
                                             Att_DUMMY,
                                             Att_DUMMY,
                                             Att_DUMMY,
                                             Att_DUMMY,
                                             Att_DUMMY,
                                             Att_DUMMY,
                                             Att_PL00_TOKUSHUKOUDOU,
                                             Att_DUMMY,
                                             Att_METAMOR_WAIT,
                                             Att_METAMOR_REBIRTH };
