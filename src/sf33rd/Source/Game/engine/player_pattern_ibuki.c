/**
 * @file plpat_ibuki.c
 * Ibuki Attacks
 */

#include "sf33rd/Source/Game/engine/player_pattern_ibuki.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_pattern.h"
#include "sf33rd/Source/Game/engine/player_patternuni.h"
#include "sf33rd/Source/Game/engine/player_common_mechanics.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"

#define EXATT_TABLE_SIZE 18

void (*const pl07_exatt_table[18])(PlayerEntity*);

/** @brief Ibuki: extra attack dispatcher. */
void pl_ibuki_extra_attack(PlayerEntity* wk) {
    s16 idx = wk->wu.routine_no[2] - 16;
    if (idx >= 0 && idx < EXATT_TABLE_SIZE)
        pl07_exatt_table[idx](wk);
}

/** @brief Ibuki: Super Art 2 (Yoroitoshi). */
static void Att_PL07_SA2(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        force_grounded_state(wk);
        wk->wu.facing_flag = wk->wu.active_move;
        reset_mvxy_data(&wk->wu);
        wk->wu.mvxy.index = wk->as->r_no;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        break;

    default:
        char_move(&wk->wu);
        cal_mvxy_speed(&wk->wu);
        add_mvxy_speed(&wk->wu);

        switch (wk->wu.cg_type) {
        case 20:
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.cg_type = 0;
            break;

        case 30:
            wk->wu.mvxy.index = wk->as->data_ix;
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.cg_type = 0;
            break;

        case 88:
            reset_mvxy_data(&wk->wu);
            wk->wu.cg_type = 0;
            break;
        }

        break;
    }
}

/** @brief Ibuki: attack 1 (Kasumi Gake). */
static void Att_PL07_AT1(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        force_grounded_state(wk);
        wk->wu.facing_flag = wk->wu.active_move;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        reset_mvxy_data(&wk->wu);
        wk->wu.mvxy.index = wk->as->r_no;
        break;

    case 1:
        char_move(&wk->wu);
        cal_mvxy_speed(&wk->wu);
        add_mvxy_speed(&wk->wu);

        switch (wk->wu.cg_type) {
        case 20:
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.index++;
            wk->wu.cg_type = 0;
            break;

        case 21:
            reset_mvxy_data(&wk->wu);
            wk->wu.cg_type = 0;
            break;

        case 30:
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
            break;
        }

        break;
    case 2:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 1) {
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.routine_no[3] = 3;
            wk->wu.cg_type = 0;
        }

        break;

    case 3:
        jumping_union_process(&wk->wu, 4);
        break;

    case 4:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Ibuki: attack 2 (Hien). */
static void Att_PL07_AT2(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        wk->wu.facing_flag = wk->wu.active_move;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        reset_mvxy_data(&wk->wu);
        break;

    case 1:
        char_move(&wk->wu);
        add_mvxy_speed(&wk->wu);
        cal_mvxy_speed(&wk->wu);

        if (wk->wu.cg_type == 20) {
            setup_mvxy_data(&wk->wu, wk->as->r_no);
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);

        if (wk->wu.cg_type == 30) {
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.cg_type = 0;
        }

        break;

    case 3:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Ibuki: attack 3 (Tsumuji). */
static void Att_PL07_AT3(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 20) {
            wk->wu.routine_no[3]++;
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

/** @brief Ibuki: Super Art 3 (Yami Shigure). */
static void Att_PL07_SA3(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        wk->wu.facing_flag = wk->wu.active_move;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        /* fallthrough */

    case 1:
        jumping_union_process(&wk->wu, 2);

        if (wk->wu.routine_no[3] != 2) {
            if (wk->wu.cg_type == 20) {
                setup_mvxy_data(&wk->wu, wk->as->data_ix);
                wk->wu.cg_type = 0;
            }

            if (wk->wu.cg_type == 30) {
                setup_mvxy_data(&wk->wu, wk->as->r_no);
                wk->wu.cg_type = 0;
            }
        }

        break;

    case 2:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Ibuki: special action (tokushu koudou). */
static void Att_PL07_TOKUSHUKOUDOU(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        force_grounded_state(wk);
        wk->wu.facing_flag = wk->wu.active_move;
        setup_mvxy_data(&wk->wu, wk->as->data_ix);
        wk->wu.mvxy.index++;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 20) {
            wk->wu.cg_type = 0;
            wk->wu.routine_no[3]++;
            add_mvxy_speed(&wk->wu);
            cal_mvxy_speed(&wk->wu);
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);

        if (wk->wu.cg_type == 40) {
            wk->wu.cg_type = 0;
            add_sp_arts_gauge_tokushu(wk);
        }

        if (wk->wu.cg_type == 20) {
            wk->wu.cg_type = 0;
            wk->wu.mvxy.index++;
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
        }

        break;

    case 3:
        char_move(&wk->wu);
        break;

    case 4:
        jumping_union_process(&wk->wu, 3);

        if (wk->wu.cg_type == 30) {
            wk->wu.cg_type = 0;
            wk->strike_scaling += 14;
            wk->throw_scaling += 14;
        }

        if (wk->strike_scaling > 28) {
            wk->strike_scaling = 28;
        }

        if (wk->throw_scaling > 28) {
            wk->throw_scaling = 28;
        }

        if (wk->wu.cg_type == 64) {
            grade_add_personal_action(wk->wu.id);
        }

        break;
    }
}

void (*const pl07_exatt_table[18])(PlayerEntity*) = {
    Att_SHOURYUUKEN, Att_PL07_AT1, Att_CHOUCHUURENGEKI,    Att_SLIDE_and_JUMP, Att_CHOUCHUURENGEKI, Att_PL07_SA2,
    Att_PL07_AT2,    Att_PL07_AT3, Att_PL07_SA3,           Att_SLIDE_and_JUMP, Att_HOMING_JUMP,     Att_DUMMY,
    Att_DUMMY,       Att_DUMMY,    Att_PL07_TOKUSHUKOUDOU, Att_DUMMY,          Att_METAMOR_WAIT,    Att_METAMOR_REBIRTH
};
