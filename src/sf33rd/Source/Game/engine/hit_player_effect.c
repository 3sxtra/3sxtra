/**
 * @file hitplef.c
 * Hit Player vs Effect
 */

#include "sf33rd/Source/Game/engine/hit_player_effect.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_02_hit_marks_sparks.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/damage_calculator.h"
#include "sf33rd/Source/Game/io/rumble.h"

/** @brief Resolves player-vs-effect hit collision (attacking a projectile). */
void player_at_vs_effect_dm(s16 ix2, s16 ix) {
    PlayerEntity* as = (PlayerEntity*)q_hit_push[ix2];
    State_Other* ds = (State_Other*)q_hit_push[ix];

    pp_pulpara_hit(&as->wu);
    ds->wu.damage_facing = as->wu.facing_flag;
    cal_hit_mark_pos(&as->wu, &ds->wu, ix2, ix);

    if (ds->wu.id == 122 || ds->wu.id == 123) {
        calculate_damage_vitality(as, (PlayerEntity*)ds);
    } else {
        ds->wu.damage_vitality = 256;
    }

    if (ds->wu.work_id == 2 && (ds->wu.id == 122 || ds->wu.id == 123)) {
        if (ds->wu.xyz[1].disp.pos <= 0) {
            as->wu.hf.hit.player = 2;
        } else {
            as->wu.hf.hit.player = 1;
        }
    } else if (ds->wu.xyz[1].disp.pos <= 0) {
        as->wu.hf.hit.player = 32;
    } else {
        as->wu.hf.hit.player = 16;
    }

    ds->wu.routine_no[1] = 1;
    ds->wu.routine_no[2] = 0;

    if (ds->wu.work_id != 2 || ds->wu.id != 0x87) {
        if (ds->wu.att.dipsw & 2) {
            effect_02_init(&as->wu, 2, 2, ds->wu.damage_facing);
        } else if (ds->wu.id != 13) {
            effect_02_init(&as->wu, 2, 1, ds->wu.damage_facing);
        } else if (ds->wu.charset_id == 2) {
            effect_02_init(&as->wu, 2, 2, ds->wu.damage_facing);
        }
    }

    dm_status_copy(&as->wu, &ds->wu);

    if (ds->wu.work_id == 2 && ds->wu.id != 122 && ds->wu.id != 123) {
        as->wu.att_hit_ok = 1;
        as->wu.hit_stop /= 2;
        ds->wu.damage_hit_stop /= 2;
    }

    hit_pattern_extdat_check(&as->wu);
}
