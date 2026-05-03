/**
 * @file hitefpl.c
 * Hit Effect vs Player
 */

#include "sf33rd/Source/Game/engine/hitefpl.h"
#include "bin2obj/gauge.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/pow_pow.h"
#include "sf33rd/Source/Game/system/sysdir.h"

static void setup_dm_rl_pldm(WORK* as, WORK* ds);

/** @brief Resolves effect-vs-player hit collision and applies damage. */
void effect_at_vs_player_dm(s16 ix2, s16 ix) {
    WORK_Other* as = (WORK_Other*)q_hit_push[ix2];
    PLW* ds = (PLW*)q_hit_push[ix];
    PLW* ms;
    s8 gddir;

    ds->dm_point = hs[ix].dm_body;
    gddir = get_guard_direction(&as->wu, &ds->wu);
    setup_latest_stick_dir(ds, gddir);
    setup_dm_rl_pldm(&as->wu, &ds->wu);
    cal_hit_mark_pos(&as->wu, &ds->wu, ix2, ix);
    cal_damage_vitality_eff(as, ds);
    ds->wu.damage_stun_value = _add_piyo_gauge[as->master_player][as->wu.att.stun_effect];
    ds->wu.damage_stun_value = ds->wu.damage_stun_value * stun_gauge_omake[omop_stun_gauge_add[(ds->wu.id + 1) & 1]] / 32;

    if ((ds->wu.pat_status == 32 || ds->wu.pat_status == 3) || ds->wu.pat_status == 25) {
        ds->wu.damage_vitality = (ds->wu.damage_vitality * 125) / 100;
    } else if (ds->wu.pat_status == 7 || ds->wu.pat_status == 23 || ds->wu.pat_status == 35) {
        ds->wu.damage_vitality = (ds->wu.damage_vitality * 150) / 100;
    } else if (ds->wu.pat_status == 1 || ds->wu.pat_status == 21 || ds->wu.pat_status == 37) {
        ds->wu.damage_vitality *= 2;
    }

    ms = (PLW*)as->my_master;

    if (ms->wu.work_id == 1) {
        if (as->wu.olc_work_ix[3] == 2) {
            ds->wu.damage_vitality = ds->wu.damage_vitality * (as->wu.olc_work_ix[1] + 32) / 32;
        }

        if (as->wu.olc_work_ix[3] == 4) {
            ds->wu.damage_vitality = ds->wu.damage_vitality * (as->wu.olc_work_ix[0] + 32) / 32;
        }

        ds->received_strike_scaling = as->wu.olc_work_ix[0];
        ds->received_throw_scaling = as->wu.olc_work_ix[1];
        ds->wu.damage_stun_value = ds->wu.damage_stun_value * (as->wu.olc_work_ix[2] + 32) / 32;
        ds->received_stun_scaling = as->wu.olc_work_ix[2];
    }

    as->wu.at_ten_ix = remake_score_index(ds->wu.damage_vitality);
    cal_combo_waribiki((PLW*)as, ds);
    cal_dm_vital_gauge_adjust(ds);
    cal_combo_waribiki2(ds);
    as->wu.damage_vitality = 256;
    ds->parry_flag = 0;
    plef_at_vs_player_damage_union((PLW*)as, ds, gddir);
}

/** @brief Sets the damage direction based on relative position of attacker. */
static void setup_dm_rl_pldm(WORK* as, WORK* ds) {
    s16 pw;

    ds->dm_rl = as->rl_flag;

    if (ds->xyz[1].disp.pos <= 0) {
        return;
    }

    if (!(as->att.dipsw & 2)) {
        return;
    }

    pw = ds->xyz[0].disp.pos - as->xyz[0].disp.pos;

    if (pw) {
        if (pw > 0) {
            ds->dm_rl = 1;
        } else {
            ds->dm_rl = 0;
        }
    } else {
        ds->dm_rl = as->rl_flag;
    }
}
