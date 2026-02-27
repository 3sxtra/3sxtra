/**
 * @file pow_pow.c
 * Damage Calculation
 */

#include "sf33rd/Source/Game/engine/pow_pow.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/pow_data.h"
#include "sf33rd/Source/Game/engine/workuser.h"

/**
 * @brief Core damage calculation shared by player-vs-player and effect-vs-player paths.
 *
 * @param att_wu    Attacker's WORK data (for pow lookup and work_id check).
 * @param att_plus  Attacker's attack multiplier (from the owning PLW).
 * @param ds        Defender player work (receives dm_vital and applies def_plus).
 */
static void cal_damage_core(WORK* att_wu, s16 att_plus, PLW* ds) {
    s16 power = Power_Data[att_wu->att.pow];
    s16 yy = (Play_Type == 1) ? Pow_Control_Data_1[0][3] : Pow_Control_Data_1[0][Round_Level];

    ds->wu.dm_vital = (power * yy) / 100;

    if (att_wu->work_id == 1) {
        ds->wu.dm_vital = (ds->wu.dm_vital * att_plus) / 8;
    }

    if (ds->wu.work_id == 1) {
        ds->wu.dm_vital = (ds->wu.dm_vital * ds->def_plus) / 8;
    }
}

/** @brief Calculates damage vitality for a player-vs-player attack. */
void cal_damage_vitality(PLW* as, PLW* ds) {
    cal_damage_core(&as->wu, as->att_plus, ds);
}

/** @brief Calculates damage vitality for an effect-vs-player attack. */
void cal_damage_vitality_eff(WORK_Other* as, PLW* ds) {
    cal_damage_core(&as->wu, ((PLW*)as)->att_plus, ds);
}

/** @brief Awards additional score for specific damage types (KO, special finish). */
void Additinal_Score_DM(WORK_Other* wk, u16 ix) {
    s16 id;

    if (wk->wu.work_id == 1) {
        id = wk->wu.id;
    } else {
        if (((WORK*)wk->my_master)->work_id != 1) {
            return;
        }

        id = wk->master_id;
    }

    Score[id][2] += Score_Data[ix];

    if (Score[id][2] >= 99999900) {
        Score[id][2] = 99999900;
    }

    if ((Mode_Type != MODE_VERSUS) && (Mode_Type != MODE_REPLAY)) {
        if (!plw[id].wu.pl_operator) {
            return;
        }
    }

    Score[id][Play_Type] += Score_Data[ix];

    if (Score[id][Play_Type] >= 99999900) {
        Score[id][Play_Type] = 99999900;
    }
}
