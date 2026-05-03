/**
 * @file plpat_yun.c
 * Yun Attacks
 */

#include "sf33rd/Source/Game/engine/player_pattern_yun.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_pattern.h"
#include "sf33rd/Source/Game/engine/player_patternuni.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"

#define EXATT_TABLE_SIZE 18

void (*const pl03_exatt_table[18])(PlayerEntity*);

/** @brief Yun: extra attack dispatcher. */
void pl_yun_extra_attack(PlayerEntity* wk) {
    s16 idx = wk->wu.routine_no[2] - 16;
    if (idx >= 0 && idx < EXATT_TABLE_SIZE)
        pl03_exatt_table[idx](wk);
}

/** @brief Yun: special action (tokushu koudou). */
static void Att_PL03_TOKUSHUKOUDOU(PlayerEntity* wk) {
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
            wk->strike_scaling += 2;
            wk->throw_scaling += 2;
            break;

        case 30:
            wk->wu.cg_type = 0;
            wk->strike_scaling += 2;
            wk->throw_scaling++;
            break;

        case 64:
            grade_add_personal_action(wk->wu.id);
            break;
        }

        if (wk->strike_scaling > 16) {
            wk->strike_scaling = 16;
        }

        if (wk->throw_scaling > 8) {
            wk->throw_scaling = 8;
        }

        break;
    }
}

void (*const pl03_exatt_table[18])(PlayerEntity*) = {
    Att_HADOUKEN, Att_SHOURYUUKEN, Att_SENPUUKYAKU,        Att_HADOUKEN, Att_SLIDE_and_JUMP, Att_SLIDE_and_JUMP,
    Att_HADOUKEN, Att_DUMMY,       Att_SLIDE_and_JUMP,     Att_DUMMY,    Att_DUMMY,          Att_DUMMY,
    Att_DUMMY,    Att_DUMMY,       Att_PL03_TOKUSHUKOUDOU, Att_DUMMY,    Att_METAMOR_WAIT,   Att_METAMOR_REBIRTH
};
