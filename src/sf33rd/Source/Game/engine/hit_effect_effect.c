/**
 * @file hitefef.c
 * Hit Effect vs Effect
 */

#include "sf33rd/Source/Game/engine/hit_effect_effect.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_02_hit_marks_sparks.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/damage_calculator.h"

/** @brief Resolves effect-vs-effect hit collision (projectile clash). */
void effect_at_vs_effect_dm(s16 ix2, s16 ix) {
    State_Other* as = (State_Other*)q_hit_push[ix2];
    State_Other* ds = (State_Other*)q_hit_push[ix];

    ds->wu.damage_facing = as->wu.facing_flag;
    as->wu.damage_facing = ds->wu.facing_flag;
    cal_hit_mark_pos(&as->wu, &ds->wu, ix2, ix);

    if (ds->wu.att.dipsw & 2) {
        if (as->wu.att.dipsw & 2) {
            ds->wu.damage_vitality = 128;
            as->wu.damage_vitality = 128;
        } else {
            ds->wu.damage_vitality = 128;
            as->wu.damage_vitality = 128;

            if (as->wu.shell_vs_refrect == 0) {
                as->damage_reflect = 1;
                as->refrected = 1;
                as->wu.att_hit_ok = 1;
            }
        }
    } else if (ds->wu.id == 13) {
        switch (as->wu.work_id) {
        case 4:
            ds->wu.damage_vitality = as->wu.vital_new;
            as->wu.damage_vitality = ds->wu.vital_new;

            if (ds->wu.damage_vitality > ds->wu.vital_new) {
                ds->wu.damage_vitality = ds->wu.vital_new;
            }

            if (as->wu.damage_vitality > as->wu.vital_new) {
                as->wu.damage_vitality = as->wu.vital_new;
            }

            break;

        default:
            break;
        }
    } else if (ds->wu.id == 122 || ds->wu.id == 123) {
        calculate_damage_vitality((PlayerEntity*)as, (PlayerEntity*)ds);
        as->wu.damage_vitality = 256;
    } else {
        ds->wu.damage_vitality = 256;
    }

    if (ds->wu.xyz[1].disp.pos > 0) {
        as->wu.hf.hit.effect = 2;
    } else {
        as->wu.hf.hit.effect = 1;
    }

    ds->wu.routine_no[1] = 1;
    ds->wu.routine_no[2] = 0;
    as->wu.hit_stop = ds->wu.hit_stop = 6;
    ds->wu.damage_direction = as->wu.dir_atthit;
    ds->wu.damage_attack_type = as->wu.attack_type;

    if (ds->wu.id == 122 || ds->wu.id == 123) {
        effect_02_init(&as->wu, 2, 1, ds->wu.damage_facing);
    }

    hit_pattern_extdat_check(&as->wu);
}
