/**
 * @file plpat_elena.c
 * Elena Attacks
 */

#include "sf33rd/Source/Game/engine/player_pattern_elena.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_pattern.h"
#include "sf33rd/Source/Game/engine/player_patternuni.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"

const u8 pl08_hcs_tbl[8] = { 0, 0, 0, 1, 0, 1, 1, 1 };

#define EXATT_TABLE_SIZE 18

void (*const pl08_exatt_table[18])(PlayerEntity*);

/** @brief Elena: extra attack dispatcher. */
void pl_elena_extra_attack(PlayerEntity* wk) {
    s16 idx = wk->wu.routine_no[2] - 16;
    if (idx >= 0 && idx < EXATT_TABLE_SIZE)
        pl08_exatt_table[idx](wk);
}

/** @brief Elena: Healing Super Art (regenerates vitality). */
static void Att_PL08_HEALING(PlayerEntity* wk) {
    u16 cpsw;

    wk->scr_pos_set_flag = 0;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        force_grounded_state(wk);
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.script_register_bank[0]) {
            cpsw = (wk->cp->input_current & 0x770);
            cpsw >>= 4;

            if (pl08_hcs_tbl[cpsw & 7] || pl08_hcs_tbl[(cpsw >> 4) & 7]) {
                wk->wu.script_register_bank[0] = 0;
                char_move_cmms(&wk->wu);
            }
        }

        if (!g_state.pcon_dp_flag) {
            switch (wk->wu.cg_type) {
            case 24:
                wk->wu.vital_new += 3;
                break;

            case 22:
                wk->wu.vital_new += 2;
                break;

            case 20:
                wk->wu.vital_new += 1;
                break;
            }
        }

        if (wk->wu.vital_new > wk->wu.vitality) {
            wk->wu.vital_new = wk->wu.vitality;
            wk->sa_healing = 1;
        }

        break;
    }
}

/** @brief Elena: special action (tokushu koudou). */
static void Att_PL08_TOKUSHUKOUDOU(PlayerEntity* wk) {
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

        if (wk->wu.cg_type == 64) {
            wk->wu.routine_no[3]++;
            wk->stun_scaling += 6;

            if (wk->stun_scaling > 24) {
                wk->stun_scaling = 24;
            }

            grade_add_personal_action(wk->wu.id);
        }

        break;

    default:
        char_move(&wk->wu);
        break;
    }
}

void (*const pl08_exatt_table[18])(PlayerEntity*) = { Att_SHOURYUUKEN,   Att_SENPUUKYAKU,    Att_SENPUUKYAKU,
                                             Att_SHOURYUUREPPA, Att_SHOURYUUREPPA,  Att_PL08_HEALING,
                                             Att_DUMMY,         Att_SLIDE_and_JUMP, Att_HADOUKEN,
                                             Att_DUMMY,         Att_DUMMY,          Att_DUMMY,
                                             Att_DUMMY,         Att_DUMMY,          Att_PL08_TOKUSHUKOUDOU,
                                             Att_DUMMY,         Att_METAMOR_WAIT,   Att_METAMOR_REBIRTH };
