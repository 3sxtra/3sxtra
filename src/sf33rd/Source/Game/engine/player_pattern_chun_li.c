/**
 * @file plpat_chun_li.c
 * Chun-Li Attacks
 */

#include "sf33rd/Source/Game/engine/player_pattern_chun_li.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_pattern.h"
#include "sf33rd/Source/Game/engine/player_patternuni.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"

#define EXATT_TABLE_SIZE 18

void (*const pl16_exatt_table[18])(PlayerEntity*);

/** @brief Chun-Li: extra attack dispatcher. */
void pl_chun_li_extra_attack(PlayerEntity* wk) {
    s16 idx = wk->wu.routine_no[2] - 16;
    if (idx >= 0 && idx < EXATT_TABLE_SIZE)
        pl16_exatt_table[idx](wk);
}

/** @brief Chun-Li: special action (tokushu koudou). */
static void Att_PL16_TOKUSHUKOUDOU(PlayerEntity* wk) {
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

        switch (wk->wu.cg_type) {
        case 30:
            wk->wu.cg_type = 0;

            if (wk->guts_scaling == 0) {
                wk->guts_scaling = 6;
            }

            break;

        case 40:
            wk->wu.cg_type = 0;
            add_sp_arts_gauge_tokushu(wk);
            break;

        case 50:
            wk->wu.cg_type = 0;

            if (wk->strike_scaling < 10) {
                wk->strike_scaling = 10;
            }

            break;

        case 64:
            grade_add_personal_action(wk->wu.id);
            wk->wu.routine_no[3]++;

            if (wk->target_combo_success < 3) {
                wk->target_combo_success++;
                wk->py->recover = (wk->py->recover * 110) / 100;
            }

            break;
        }

        break;

    default:
        char_move(&wk->wu);
        break;
    }
}

void (*const pl16_exatt_table[18])(PlayerEntity*) = { Att_SENPUUKYAKU,    Att_HADOUKEN2,      Att_HADOUKEN,
                                                      Att_HADOUKEN,       Att_SLIDE_and_JUMP, Att_SLIDE_and_JUMP,
                                                      Att_SLIDE_and_JUMP, Att_DUMMY,          Att_DUMMY,
                                                      Att_DUMMY,          Att_DUMMY,          Att_DUMMY,
                                                      Att_DUMMY,          Att_DUMMY,          Att_PL16_TOKUSHUKOUDOU,
                                                      Att_DUMMY,          Att_METAMOR_WAIT,   Att_METAMOR_REBIRTH };
