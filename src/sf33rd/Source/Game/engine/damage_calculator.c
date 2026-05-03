/**
 * @file damage_calculator.c
 * Damage Calculation
 */

#include "sf33rd/Source/Game/engine/damage_calculator.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/damage_data.h"
#include "sf33rd/Source/Game/engine/state_user.h"

/**
 * @brief Core damage calculation shared by player-vs-player and effect-vs-player paths.
 *
 * @param att_wu    Attacker's State data (for pow lookup and work_id check).
 * @param att_plus  Attacker's attack multiplier (from the owning PlayerEntity).
 * @param ds        Defender player work (receives damage_vitality and applies def_plus).
 */
static void cal_damage_core(State* att_wu, s16 att_plus, PlayerEntity* ds) {
    s16 power = Power_Data[att_wu->att.pow];
    s16 yy = (g_state.Play_Type == 1) ? Pow_Control_Data_1[0][3] : Pow_Control_Data_1[0][g_state.Round_Level];

    ds->wu.damage_vitality = (power * yy) / 100;

    if (att_wu->work_id == 1) {
        ds->wu.damage_vitality = (ds->wu.damage_vitality * att_plus) / 8;
    }

    if (ds->wu.work_id == 1) {
        ds->wu.damage_vitality = (ds->wu.damage_vitality * ds->def_plus) / 8;
    }
}

/** @brief Calculates damage vitality for a player-vs-player attack. */
void calculate_damage_vitality(PlayerEntity* as, PlayerEntity* ds) {
    cal_damage_core(&as->wu, as->att_plus, ds);
}

/** @brief Calculates damage vitality for an effect-vs-player attack. */
void cal_damage_vitality_eff(State_Other* as, PlayerEntity* ds) {
    cal_damage_core(&as->wu, ((PlayerEntity*)as)->att_plus, ds);
}

/** @brief Awards additional score for specific damage types (KO, special finish). */
void additional_score_damage(State_Other* wk, u16 ix) {
    s16 id;

    if (wk->wu.work_id == 1) {
        id = wk->wu.id;
    } else {
        if (((State*)wk->my_master)->work_id != 1) {
            return;
        }

        id = wk->master_id;
    }

    g_state.Score[id][2] += Score_Data[ix];

    if (g_state.Score[id][2] >= 99999900) {
        g_state.Score[id][2] = 99999900;
    }

    if ((g_state.Mode_Type != MODE_VERSUS) && (g_state.Mode_Type != MODE_REPLAY)) {
        if (!g_state.plw[id].wu.pl_operator) {
            return;
        }
    }

    g_state.Score[id][g_state.Play_Type] += Score_Data[ix];

    if (g_state.Score[id][g_state.Play_Type] >= 99999900) {
        g_state.Score[id][g_state.Play_Type] = 99999900;
    }
}
