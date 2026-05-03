/**
 * @file plpat_alex.c
 * Alex Attacks
 */

#include "sf33rd/Source/Game/engine/player_pattern_alex.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/calculate_direction.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_pattern.h"
#include "sf33rd/Source/Game/engine/player_patternuni.h"
#include "sf33rd/Source/Game/engine/player_common_mechanics.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"

#define EXATT_TABLE_SIZE 18

void (*const pl01_exatt_table[18])(PlayerEntity*);

const s16 pl01_ddt_dat[20][2] = { { 46, 11 }, { 32, 12 }, { 28, 13 }, { 16, 14 }, { 36, 15 }, { 16, 16 }, { 60, 17 },
                                  { 20, 18 }, { 24, 19 }, { 12, 20 }, { 16, 14 }, { 28, 13 }, { 28, 13 }, { 46, 11 },
                                  { 28, 13 }, { 24, 21 }, { 18, 22 }, { 52, 23 }, { 18, 16 }, { 38, 24 } };

/** @brief Alex: extra attack dispatcher. */
void pl_alex_extra_attack(PlayerEntity* wk) {
    s16 idx = wk->wu.routine_no[2] - 16;
    if (idx >= 0 && idx < EXATT_TABLE_SIZE)
        pl01_exatt_table[idx](wk);
}

/** @brief Alex: DDT (pro-wrestling throw) attack. */
static void Att_PL01_DDT(PlayerEntity* wk) {
    PlayerEntity* twk = (PlayerEntity*)wk->wu.target_adrs;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        force_grounded_state(wk);
        wk->wu.facing_flag = wk->wu.active_move;
        reset_mvxy_data(&wk->wu);
        wk->wu.mvxy.index = wk->as->data_ix;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 20) {
            wk->wu.cg_type = 0;
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.mvxy.index++;
            wk->wu.routine_no[3] = 2;
            cal_initial_speed_y(&wk->wu, wk->as->r_no, pl01_ddt_dat[twk->player_number][0]);
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);

        if (wk->wu.routine_no[3] != 3 && wk->wu.cg_ja.catch_box_index) {
            wk->wu.cg_ja.catch_box_index = pl01_ddt_dat[twk->player_number][1];
            wk->wu.catch_box = wk->wu.cg_ja.catch_box_index + wk->wu.catch_adrs;
        }

        break;

    case 3:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Alex: special action (tokushu koudou). */
static void Att_PL01_TOKUSHUKOUDOU(PlayerEntity* wk) {
    wk->scr_pos_set_flag = 0;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        wk->wu.facing_flag = wk->wu.active_move;
        force_grounded_state(wk);
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 40) {
            wk->wu.cg_type = 0;
            add_sp_arts_gauge_tokushu(wk);
        }

        switch (wk->wu.cg_type) {
        case 20:
            wk->wu.cg_type = 0;
            wk->strike_scaling += 3;
            wk->throw_scaling += 2;
            break;

        case 30:
            wk->wu.cg_type = 0;
            wk->strike_scaling += 2;
            wk->throw_scaling += 2;
            break;

        case 64:
            grade_add_personal_action(wk->wu.id);
            break;
        }

        if (wk->strike_scaling > 12) {
            wk->strike_scaling = 12;
        }

        if (wk->throw_scaling > 16) {
            wk->throw_scaling = 16;
        }

        break;
    }
}

void (*const pl01_exatt_table[18])(PlayerEntity*) = { Att_CHOUCHUURENGEKI, Att_SHOURYUUKEN,     Att_HADOUKEN,
                                             Att_HADOUKEN2,       Att_CHOUCHUURENGEKI, Att_SENPUUKYAKU2,
                                             Att_SENPUUKYAKU,     Att_NM_OKIAGARI,     Att_PL01_DDT,
                                             Att_HOMING_JUMP,     Att_SLIDE_and_JUMP,  Att_DUMMY,
                                             Att_DUMMY,           Att_DUMMY,           Att_PL01_TOKUSHUKOUDOU,
                                             Att_DUMMY,           Att_METAMOR_WAIT,    Att_METAMOR_REBIRTH };
