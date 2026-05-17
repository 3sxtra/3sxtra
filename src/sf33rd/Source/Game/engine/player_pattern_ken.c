/**
 * @file plpat_ken.c
 * Ken Attacks
 */

#include "sf33rd/Source/Game/engine/player_pattern_ken.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_pattern.h"
#include "sf33rd/Source/Game/engine/player_patternuni.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"

#define EXATT_TABLE_SIZE 18

void (*const pl11_exatt_table[18])(PlayerEntity*);

/** @brief Ken: extra attack dispatcher. */
void pl_ken_extra_attack(PlayerEntity* wk) {
    s16 idx = wk->wu.routine_no[2] - 16;
    if (idx >= 0 && idx < EXATT_TABLE_SIZE)
        pl11_exatt_table[idx](wk);
}

/** @brief Ken: special action (tokushu koudou). */
static void Att_PL11_TOKUSHUKOUDOU(PlayerEntity* wk) {
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
            wk->strike_scaling += 10;

            if (wk->strike_scaling > 10) {
                wk->strike_scaling = 10;
            }

            break;

        case 64:
            grade_add_personal_action(wk->wu.id);
            break;
        }

        break;
    }
}

void (*const pl11_exatt_table[18])(PlayerEntity*) = { Att_HADOUKEN,
                                                      Att_SHOURYUUKEN,
                                                      Att_SENPUUKYAKU,
                                                      Att_SHOURYUUREPPA,
                                                      Att_SHOURYUUREPPA,
                                                      Att_SLIDE_and_JUMP,
                                                      Att_KUUCHUUNICHIRINSHOU,
                                                      Att_CHOUCHUURENGEKI,
                                                      Att_DUMMY,
                                                      Att_DUMMY,
                                                      Att_DUMMY,
                                                      Att_DUMMY,
                                                      Att_DUMMY,
                                                      Att_DUMMY,
                                                      Att_PL11_TOKUSHUKOUDOU,
                                                      Att_DUMMY,
                                                      Att_METAMOR_WAIT,
                                                      Att_METAMOR_REBIRTH };
