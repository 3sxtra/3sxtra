/**
 * @file plpat_akuma.c
 * Akuma/Gouki Attacks
 */

#include "sf33rd/Source/Game/engine/plpat_akuma.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/plpat.h"
#include "sf33rd/Source/Game/engine/plpatuni.h"
#include "sf33rd/Source/Game/engine/pls01.h"
#include "sf33rd/Source/Game/engine/pls02.h"

s8 stop_count[2];

const s16 pl14_HYAKKI_dat[20] = { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 7, 6, 6, 4, 6, 14, 15, 16, 17, 18 };

#define EXATT_TABLE_SIZE 18

void (*const pl14_exatt_table[18])(PLW*);

/** @brief Akuma: extra attack dispatcher. */
void pl_akuma_extra_attack(PLW* wk) {
    s16 idx = wk->wu.routine_no[2] - 16;
    if (idx >= 0 && idx < EXATT_TABLE_SIZE)
        pl14_exatt_table[idx](wk);
}

/** @brief Akuma: attack 1 (Hyakki Goushou dive). */
static void Att_PL14_AT1(PLW* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        force_grounded_state(wk);
        wk->wu.rl_flag = wk->wu.active_move;
        wk->rl_save = wk->wu.rl_flag;
        reset_mvxy_data(&wk->wu);
        wk->wu.mvxy.index = wk->as->r_no;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        stop_count[wk->wu.id] = 0;
        break;

    case 1:
        char_move(&wk->wu);
        cal_mvxy_speed(&wk->wu);

        if (wk->rl_save) {
            wk->wu.xyz[0].cal += wk->wu.mvxy.a[0].sp;
        } else {
            wk->wu.xyz[0].cal -= wk->wu.mvxy.a[0].sp;
        }

        wk->wu.xyz[1].cal += wk->wu.mvxy.a[1].sp;

        switch (wk->wu.cg_type) {
        case 10:
            wk->wu.routine_no[3]++;
            /* fallthrough */

        case 20:
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.index++;
            wk->wu.cg_type = 0;
            break;

        case 21:
            reset_mvxy_data(&wk->wu);
            wk->wu.cg_type = 0;
            break;
        }

        break;

    default:
        char_move(&wk->wu);
        cal_mvxy_speed(&wk->wu);

        if (wk->rl_save) {
            wk->wu.xyz[0].cal += wk->wu.mvxy.a[0].sp;
        } else {
            wk->wu.xyz[0].cal -= wk->wu.mvxy.a[0].sp;
        }

        wk->wu.xyz[1].cal += wk->wu.mvxy.a[1].sp;
        wk->wu.rl_flag = wk->wu.active_move;

        if ((wk->wu.mvxy.a[0].sp != 0) && wk->old_pos_data[0] == wk->old_pos_data[1]) {
            char_move_z(&wk->wu);
        }

        switch (wk->wu.cg_type) {
        case 20:
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.index++;
            wk->wu.cg_type = 0;
            break;

        case 21:
            reset_mvxy_data(&wk->wu);
            wk->wu.cg_type = 0;
            char_move_z(&wk->wu);
            break;
        }

        break;
    }
}

/** @brief Akuma: attack 2 (Ashura Senku teleport). */
static void Att_PL14_AT2(PLW* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        wk->wu.mvxy.index = wk->as->r_no;
        break;

    case 1:
        char_move(&wk->wu);

        switch (wk->wu.cg_type) {
        case 10:
            wk->wu.routine_no[3]++;
            wk->wu.cg_type = 0;
            break;

        case 20:
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.index++;
            wk->wu.cg_type = 0;
            break;

        case 30:
            add_to_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.index++;
            wk->wu.cg_type = 0;
            break;
        }
        break;

    case 2:
        jumping_union_process(&wk->wu, 3);

        if (wk->wu.cg_type == 40) {
            wk->wu.routine_no[3] = 1;
            wk->wu.cg_type = 0;
        }

        break;

    case 3:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Akuma: attack 3 (Shun Goku Satsu / Raging Demon). */
static void Att_PL14_AT3(PLW* wk) {
    PLW* twk = (PLW*)wk->wu.target_adrs;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        force_grounded_state(wk);
        wk->wu.rl_flag = wk->wu.active_move;
        reset_mvxy_data(&wk->wu);
        wk->wu.mvxy.index = wk->as->r_no;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 20) {
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.index++;
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
        }

        if (wk->wu.cg_type == 30) {
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.a[1].sp = wk->wu.mvxy.d[1].sp = wk->wu.mvxy.physics_curve_type[1] = 0;
            wk->wu.mvxy.index++;
            wk->wu.routine_no[3] = 3;
            wk->wu.cg_type = 0;
        }

        if (wk->wu.routine_no[3] != 1) {
            add_mvxy_speed(&wk->wu);
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 1);

        if (wk->wu.cg_type == 1) {
            wk->wu.cg_type = 0;
            wk->wu.routine_no[3] = 4;
        }

        if ((wk->wu.routine_no[3] != 1) && (wk->wu.cg_type == 20)) {
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.index++;
            wk->wu.cg_type = 0;
        }

        if ((wk->wu.routine_no[3] != 1) && wk->wu.cg_ja.catch_box_index) {
            wk->wu.cg_ja.catch_box_index = pl14_HYAKKI_dat[twk->player_number];
            wk->wu.catch_box = wk->wu.catch_adrs + wk->wu.cg_ja.catch_box_index;
        }

        break;

    case 3:
        char_move(&wk->wu);
        cal_mvxy_speed(&wk->wu);
        add_mvxy_speed(&wk->wu);

        if (wk->wu.cg_type == 20) {
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.index++;
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
        }

        if (wk->wu.cg_type == 21) {
            reset_mvxy_data(&wk->wu);
            wk->wu.cg_type = 0;
            wk->wu.routine_no[3] = 1;
        }

        if (wk->wu.cg_type == 30) {
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.a[1].sp = wk->wu.mvxy.d[1].sp = wk->wu.mvxy.physics_curve_type[1] = 0;
            wk->wu.mvxy.index++;
            wk->wu.cg_type = 0;
        }

        break;

    case 4:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 1) {
            wk->wu.cg_type = 0;
            wk->wu.routine_no[3] = 2;
        }

        if ((wk->wu.routine_no[3] != 2) && (wk->wu.cg_type == 20)) {
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.index++;
            wk->wu.cg_type = 0;
        }

        if ((wk->wu.routine_no[3] != 2) && wk->wu.cg_ja.catch_box_index) {
            wk->wu.cg_ja.catch_box_index = pl14_HYAKKI_dat[(twk->player_number)];
            wk->wu.catch_box = wk->wu.catch_adrs + wk->wu.cg_ja.catch_box_index;
        }

        break;
    }
}

/** @brief Akuma: special action (tokushu koudou). */
static void Att_PL14_TOKUSHUKOUDOU(PLW* wk) {
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
            wk->wu.cg_type = 0;
            wk->strike_scaling += 14;
            wk->stun_scaling += 9;

            if (wk->strike_scaling > 14) {
                wk->strike_scaling = 14;
            }

            if (wk->stun_scaling > 9) {
                wk->stun_scaling = 9;
            }

            grade_add_personal_action(wk->wu.id);
        }

        break;
    }
}

void (*const pl14_exatt_table[18])(PLW*) = { Att_HADOUKEN,
                                             Att_SHOURYUUKEN,
                                             Att_SENPUUKYAKU,
                                             Att_KUUCHUUJINNCHUUWATARI,
                                             Att_SHOURYUUREPPA,
                                             Att_SLIDE_and_JUMP,
                                             Att_KUUCHUUNICHIRINSHOU,
                                             Att_PL14_AT1,
                                             Att_CHOUCHUURENGEKI,
                                             Att_PL14_AT2,
                                             Att_HADOUKEN,
                                             Att_PL14_AT3,
                                             Att_DUMMY,
                                             Att_DUMMY,
                                             Att_PL14_TOKUSHUKOUDOU,
                                             Att_DUMMY,
                                             Att_METAMOR_WAIT,
                                             Att_METAMOR_REBIRTH };
