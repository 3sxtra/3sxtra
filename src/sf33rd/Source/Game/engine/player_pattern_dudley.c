/**
 * @file plpat_dudley.c
 * Dudley Attacks
 */

#include "sf33rd/Source/Game/engine/player_pattern_dudley.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_pattern.h"
#include "sf33rd/Source/Game/engine/player_patternuni.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"

#define EXATT_TABLE_SIZE 18

void (*const pl04_exatt_table[18])(PlayerEntity*);

/** @brief Dudley: extra attack dispatcher. */
void pl_dudley_extra_attack(PlayerEntity* wk) {
    s16 idx = wk->wu.routine_no[2] - 16;
    if (idx >= 0 && idx < EXATT_TABLE_SIZE)
        pl04_exatt_table[idx](wk);
}

/** @brief Dudley: special action (tokushu koudou). */
static void Att_PL04_TOKUSHUKOUDOU(PlayerEntity* wk) {
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
        case 40:
            wk->wu.cg_type = 0;
            add_sp_arts_gauge_tokushu(wk);
            break;

        case 20:
            wk->wu.cg_type = 0;
            wk->strike_scaling += 8;
            break;

        case 64:
            grade_add_personal_action(wk->wu.id);
            break;
        }

        if (wk->strike_scaling > 8) {
            wk->strike_scaling = 8;
        }

        break;
    }
}

void (*const pl04_exatt_table[18])(PlayerEntity*) = { Att_SENPUUKYAKU,     Att_SENPUUKYAKU,    Att_HADOUKEN,
                                             Att_SHOURYUUREPPA,   Att_HADOUKEN2,      Att_CHOUCHUURENGEKI,
                                             Att_CHOUCHUURENGEKI, Att_SLIDE_and_JUMP, Att_SLIDE_and_JUMP,
                                             Att_HADOUKEN,        Att_NM_OKIAGARI,    Att_DUMMY,
                                             Att_HOMING_JUMP,     Att_DUMMY,          Att_PL04_TOKUSHUKOUDOU,
                                             Att_DUMMY,           Att_METAMOR_WAIT,   Att_METAMOR_REBIRTH };
