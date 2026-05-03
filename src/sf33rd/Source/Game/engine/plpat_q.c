/**
 * @file plpat_q.c
 * Q Attacks
 */

#include "sf33rd/Source/Game/engine/plpat_q.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/plpat.h"
#include "sf33rd/Source/Game/engine/plpatuni.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/workuser.h"

#define EXATT_TABLE_SIZE 18

void (*const pl18_exatt_table[18])(PLW*);

/** @brief Q: extra attack dispatcher. */
void pl_q_extra_attack(PLW* wk) {
    s16 idx = wk->wu.routine_no[2] - 16;
    if (idx >= 0 && idx < EXATT_TABLE_SIZE)
        pl18_exatt_table[idx](wk);
}

/** @brief Q: Ningen Bakudan (self-destruct human bomb SA). */
static void Att_PL18_NINGENBAKUDAN(PLW* wk) {
    wk->scr_pos_set_flag = 0;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        wk->wu.rl_flag = wk->wu.active_move;
        force_grounded_state(wk);
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 50) {
            wk->wu.routine_no[1] = 1;
            wk->wu.routine_no[2] = 91;
            wk->wu.routine_no[3] = 0;
            wk->wu.damage_vitality = 0;
            wk->sa->gauge.i = wk->sa->dtm * wk->sa->dtm_mul;

            if (g_state.Bonus_Game_Flag == 20) {
                wk->wu.dm_rl = (wk->wu.rl_flag + 1) & 1;
            } else {
                wk->wu.dm_rl = ((PLW*)wk->wu.target_adrs)->wu.rl_flag;
            }

            wk->wu.dm_attlv = wk->wu.att.level;
            wk->wu.dm_impact = wk->wu.att.impact;
            wk->wu.dm_dir = wk->wu.dir_atthit;
            wk->wu.damage_hit_stop = 1;
            wk->wu.damage_screen_shake = 1;

            if (wk->wu.damage_screen_shake < 0) {
                wk->wu.damage_screen_shake = -wk->wu.damage_screen_shake;
            }

            wk->wu.dm_weight = wk->wu.weight_level;
            wk->wu.damage_knockback_type = wk->wu.att.but_ix;
            wk->wu.damage_invuln = wk->wu.attack_invuln;
            wk->wu.dm_attribute = wk->wu.at_attribute;
            wk->wu.dm_ten_ix = wk->wu.at_ten_ix;
            wk->wu.damage_kind_of_arts = wk->wu.at_koa;
            wk->wu.hm_dm_side = wk->wu.att.dmg_mark;
            wk->wu.dm_work_id = wk->wu.work_id;
            wk->wu.dm_arts_point = 0;
            wk->wu.damage_attack_type = wk->wu.attack_type;
            wk->wu.dm_nodeathattack = wk->wu.no_death_attack;
            wk->wu.dm_jump_att_flag = wk->wu.jump_att_flag;
            wk->wu.dm_exdm_ix = wk->exdm_ix;
            wk->wu.dm_plnum = wk->player_number;
            wk->wu.frame_link_hit_flag = 1;
        }

        break;
    }
}

/** @brief Q: special action (tokushu koudou). */
static void Att_PL18_TOKUSHUKOUDOU(PLW* wk) {
    wk->scr_pos_set_flag = 0;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        wk->wu.rl_flag = wk->wu.active_move;
        force_grounded_state(wk);
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 40) {
            wk->wu.cg_type = 0;
            add_sp_arts_gauge_tokushu(wk);
            wk->guts_scaling += 4;

            if (wk->guts_scaling > 12) {
                wk->guts_scaling = 12;
            }
        }

        if (wk->wu.cg_type == 64) {
            grade_add_personal_action(wk->wu.id);
            wk->wu.routine_no[3]++;

            if (wk->tk_success <= 0) {
                wk->tk_success++;
                wk->py->recover = (wk->py->recover * 120) / 100;
            }
        }

        break;

    default:
        char_move(&wk->wu);
        break;
    }
}

void (*const pl18_exatt_table[18])(PLW*) = { Att_SLIDE_and_JUMP,
                                             Att_SLIDE_and_JUMP,
                                             Att_HADOUKEN2,
                                             Att_HADOUKEN2,
                                             Att_SLIDE_and_JUMP,
                                             Att_HADOUKEN2,
                                             Att_HADOUKEN,
                                             Att_PL18_NINGENBAKUDAN,
                                             Att_HADOUKEN,
                                             Att_DUMMY,
                                             Att_DUMMY,
                                             Att_DUMMY,
                                             Att_DUMMY,
                                             Att_DUMMY,
                                             Att_PL18_TOKUSHUKOUDOU,
                                             Att_DUMMY,
                                             Att_METAMOR_WAIT,
                                             Att_METAMOR_REBIRTH };
