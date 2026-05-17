/**
 * @file plpat.c
 * Player Attack Controller
 */

#include "sf33rd/Source/Game/engine/player_pattern.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_g6_data_g6.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_pattern_gill.h"
#include "sf33rd/Source/Game/engine/player_pattern_alex.h"
#include "sf33rd/Source/Game/engine/player_pattern_ryu.h"
#include "sf33rd/Source/Game/engine/player_pattern_yun.h"
#include "sf33rd/Source/Game/engine/player_pattern_dudley.h"
#include "sf33rd/Source/Game/engine/player_pattern_necro.h"
#include "sf33rd/Source/Game/engine/player_pattern_hugo.h"
#include "sf33rd/Source/Game/engine/player_pattern_ibuki.h"
#include "sf33rd/Source/Game/engine/player_pattern_elena.h"
#include "sf33rd/Source/Game/engine/player_pattern_oro.h"
#include "sf33rd/Source/Game/engine/player_pattern_yang.h"
#include "sf33rd/Source/Game/engine/player_pattern_ken.h"
#include "sf33rd/Source/Game/engine/player_pattern_sean.h"
#include "sf33rd/Source/Game/engine/player_pattern_urien.h"
#include "sf33rd/Source/Game/engine/player_pattern_akuma.h"
#include "sf33rd/Source/Game/engine/player_pattern_chun_li.h"
#include "sf33rd/Source/Game/engine/player_pattern_makoto.h"
#include "sf33rd/Source/Game/engine/player_pattern_q.h"
#include "sf33rd/Source/Game/engine/player_pattern_twelve.h"
#include "sf33rd/Source/Game/engine/player_pattern_remy.h"
#include "sf33rd/Source/Game/engine/player_normal_state.h"
#include "sf33rd/Source/Game/engine/player_state_dispatcher.h"
#include "sf33rd/Source/Game/engine/player_common_mechanics.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/io/rumble.h"

static s16 ja_nmj_rno_change(State* wk);
static void Attack_07000(PlayerEntity* wk);
static void attack_ground_init(PlayerEntity* wk);
static void attack_special_init(PlayerEntity* wk);
static void get_cancel_timer(PlayerEntity* wk);
static void check_jump_attack_dummy_rtn(PlayerEntity* wk);
static u8 get_cjdR(PlayerEntity*);

void (*const plpat_lv_00[16])(PlayerEntity* wk);
void (*const plxx_extra_attack_table[])();

const u8* cancel_whiff_table[20];
const u8* cancel_hit_table[20];
const u8* cancel_block_table[20];
const u8* cancel_defense_table[20];

/** @brief Main player attack dispatcher — routes to attack level sub-handlers. */
void Player_attack(PlayerEntity* wk) {
    wk->wu.next_z = wk->wu.my_priority;
    wk->running_flag = 0;
    wk->py->flag = 0;
    wk->guard_flag = 3;
    wk->guard_active = 0;
    wk->is_throwing = false;
    wk->is_being_thrown = false;
    wk->scr_pos_set_flag = 1;
    wk->damage_pushbox_flag = 0;
    wk->recovery_roll_success = 0;
    wk->slide_timer = 0;
    wk->slide_index_counter = 0;
    wk->sa_stop_flag = 0;
    wk->recovery_roll_success = 0;
    wk->recovery_roll_ok_timer = 0;
    wk->ukemi_cooldown_ok = 0;
    wk->inescapable_flag = 0;
    wk->catch_break_reserve = 0;
    wk->wu.swallow_no_effect = 0;
    check_em_tk_power_off(wk, (PlayerEntity*)wk->wu.target_adrs);

    if (wk->wu.routine_no[3] == 0) {
        wk->caution_flag = 1;
        wk->dm_vital_backup = 0;
        wk->dm_vital_use = 0;
        wk->total_att_hit_ok = 0;
        wk->high_jump_ok = 0;

        if (wk->wu.routine_no[2] < 16) {
            clear_chainex_check(wk->wu.id);
        }
    } else {
        pp_pulpara_remake_at(wk);
    }

    jumping_guard_type_check(wk);

    if (wk->wu.routine_no[2] > 15) {
        plxx_extra_attack_table[wk->player_number](wk);
    } else {
        wk->sa->super_effect_can_activate = 0;
        wk->sa->super_effect_meter = 0;
        plpat_lv_00[wk->wu.routine_no[2]](wk);
    }

    wk->wu.next_z = ((PlayerEntity*)wk->wu.target_adrs)->wu.my_priority - 3;

    if (wk->wu.cg_prio) {
        if (wk->wu.cg_prio == 1) {
            wk->wu.next_z += 4;
        } else {
            wk->wu.next_z -= 4;
        }
    }
}

/** @brief Common case-0 preamble for ground-based attack init. */
static void attack_ground_init(PlayerEntity* wk) {
    wk->wu.routine_no[3]++;
    force_grounded_state(wk);
    wk->wu.facing_flag = wk->wu.active_move;
    setup_lvdir_after_autodir(wk);
    get_cancel_timer(wk);
    set_char_move_init(&wk->wu, 4, wk->as->char_ix);
}

/** @brief Common case-0 preamble for special/follow-up attacks (ground init + mvxy). */
static void attack_special_init(PlayerEntity* wk) {
    attack_ground_init(wk);
    setup_mvxy_data(&wk->wu, wk->as->data_ix);
}

/** @brief Attack level 0: Ground normal attack start. */
static void Attack_00000(PlayerEntity* wk) {
    wk->scr_pos_set_flag = 0;

    switch (wk->wu.routine_no[3]) {
    case 0:
        attack_ground_init(wk);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 20) {
            wk->wu.routine_no[2] = 4;
            wk->wu.cg_type = 0;
            wk->scr_pos_set_flag = 1;
        }

        break;
    }
}

/** @brief Attack level 1: Normal attack follow-up (cancel chain). */
static void Attack_01000(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        attack_special_init(wk);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 1) {
            add_mvxy_speed(&wk->wu);
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
            break;
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);
        break;

    case 3:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Attack level 2: Special move attack execution. */
static void Attack_02000(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        attack_special_init(wk);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 1) {
            add_mvxy_speed(&wk->wu);
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
            break;
        }

        break;

    case 2:
        cal_mvxy_speed(&wk->wu);
        add_mvxy_speed(&wk->wu);
        /* fallthrough */

    case 3:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 1) {
            reset_mvxy_data(&wk->wu);
            wk->wu.cg_type = 0;
        }

        break;
    }
}

/** @brief Attack level 3: Super Art activation and execution. */
static void Attack_03000(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        get_cancel_timer(wk);
        if ((g_state.Bonus_Game_Flag == 20 && wk->bs2_on_car) || (wk->wu.xyz[1].disp.pos <= 0)) {
            force_grounded_state(wk);
            wk->wu.facing_flag = wk->wu.active_move;
            setup_lvdir_after_autodir(wk);
            Normal_18000_init_unit(wk, wk->wu.pat_status);
        }

        set_char_move_init(&wk->wu, 4, wk->as->char_ix);
        break;

    case 1:
        if ((wk->wu.mvxy.a[1].sp > 0) && (wk->wu.xyz[1].disp.pos < 16)) {
            add_mvxy_speed(&wk->wu);
            cal_mvxy_speed(&wk->wu);
            break;
        }

        wk->wu.routine_no[3]++;

    case 2:
        jumping_union_process(&wk->wu, 3);

        if (wk->wu.routine_no[3] != 3) {
            check_jump_attack_dummy_rtn(wk);

            if (wk->wu.cg_type == 0x40) {
                if (!(wk->spmv_ng_flag & 0x100000) && ja_nmj_rno_change(&wk->wu)) {
                    wk->wu.routine_no[1] = 0;
                    wk->wu.routine_no[3] = 1;
                }

                wk->wu.cg_type = 0;
                break;
            }
        }

        break;

    case 3:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Handles routine number changes for jump-attack timing. */
static s16 ja_nmj_rno_change(State* wk) {
    s16 rnum = 0;

    switch (wk->pat_status) {
    case 20:
        wk->routine_no[2] = 21;
        rnum = 1;
        break;

    case 14:
        wk->routine_no[2] = 18;
        rnum = 1;
        break;

    case 26:
        wk->routine_no[2] = 24;
        rnum = 1;
        break;

    case 22:
        wk->routine_no[2] = 22;
        rnum = 1;
        break;

    case 16:
        wk->routine_no[2] = 19;
        rnum = 1;
        break;

    case 28:
        wk->routine_no[2] = 25;
        rnum = 1;
        break;

    case 24:
        wk->routine_no[2] = 23;
        rnum = 1;
        break;

    case 18:
        wk->routine_no[2] = 20;
        rnum = 1;
        break;

    case 30:
        wk->routine_no[2] = 26;
        rnum = 1;
        break;
    }

    return rnum;
}

/** @brief Checks and handles dummy RTN for jump normal/special transitions. */
static void check_jump_attack_dummy_rtn(PlayerEntity* wk) {
    if (wk->wu.xyz[1].disp.pos <= 0) {
        wk->jump_attack_routine = 0;
        return;
    }

    switch (wk->jump_attack_routine) {
    case 0:
        if ((wk->wu.cg_ja.attack_box_index != 0) || (wk->wu.cg_ja.catch_box_index != 0)) {
            wk->jump_attack_routine = 1;
        }

        break;

    case 1:
        if (((wk->wu.cg_ja.attack_box_index == 0) && (wk->wu.cg_ja.catch_box_index == 0)) || !wk->wu.att_hit_ok) {
            wk->jump_attack_timer = get_cjdR(wk);
            wk->jump_attack_routine = 2;
        }

        break;

    case 2:
        if (((wk->wu.cg_ja.attack_box_index != 0) || (wk->wu.cg_ja.catch_box_index != 0)) && wk->wu.att_hit_ok) {
            wk->jump_attack_routine = 1;
            break;
        }

        if (!--wk->jump_attack_timer) {
            wk->jump_attack_routine = 3;
        }

        break;

    default:
        if ((wk->wu.cg_ja.attack_box_index != 0) || (wk->wu.cg_ja.catch_box_index != 0)) {
            if (wk->wu.att_hit_ok) {
                wk->jump_attack_routine = 1;
                break;
            }
        } else if (wk->wu.cg_type == 0) {
            wk->wu.cg_type = 64;
        }

        break;
    }
}

/** @brief Gets the cancel-jump-dash routing data for the current state. */
static u8 get_cjdR(PlayerEntity* wk) {
    s16 w_ix = (wk->wu.attack_type & 6);
    w_ix += ((wk->wu.hf.hit.player & 0xA2) != 0);

    if (wk->wu.att_hit_ok || (wk->wu.hf.hit.player == 0)) {
        goto case0;
    }

    if (wk->wu.hf.hit.player & 3) {
        goto case1;
    }

    if (wk->wu.hf.hit.player & 0xC0) {
        goto case2;
    }

    if (wk->wu.hf.hit.player & 0x30) {
        goto case3;
    }

case0:
    return cancel_whiff_table[wk->player_number][w_ix];

case1:
    return cancel_hit_table[wk->player_number][w_ix];

case2:
    return cancel_block_table[wk->player_number][w_ix];

case3:
    return cancel_defense_table[wk->player_number][w_ix];
}

/** @brief Attack level 4: EX special move execution. */
static void Attack_04000(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        attack_ground_init(wk);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 20) {
            wk->wu.routine_no[2] = 1;
            wk->wu.cg_type = 0;
            wk->scr_pos_set_flag = 0;
        }

        break;
    }
}

/** @brief Attack level 5: Command throw execution. */
static void Attack_05000(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.facing_flag = wk->wu.active_move;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);

        if (wk->wu.xyz[1].disp.pos > 0) {
            wk->wu.routine_no[3] = 2;
            char_move_wca(&wk->wu);
        } else {
            force_grounded_state(wk);
            wk->wu.routine_no[3] = 1;
        }

        wk->cancel_timer = 0;
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 1) {
            add_mvxy_speed(&wk->wu);
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
            effect_G6_init(&wk->wu, wk->wu.weight_level);
            break;
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);
        break;

    case 3:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Attack level 6: Placeholder/unused attack level. */
static void Attack_06000(PlayerEntity* wk) {
    wk->scr_pos_set_flag = 0;
    Attack_07000(wk);
}

/** @brief Attack level 7: Air throw execution. */
static void Attack_07000(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        force_grounded_state(wk);
        get_cancel_timer(wk);
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);

        if (wk->wu.cg_type == 20) {
            wk->wu.cg_type = 0;
            wk->wu.facing_flag = wk->wu.active_move;
            break;
        }

        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 20) {
            wk->wu.cg_type = 0;
            wk->wu.facing_flag = wk->wu.active_move;
        }

        break;
    }
}

/** @brief Attack level 8: Target combo execution. */
static void Attack_08000(PlayerEntity* wk) {
    s16 ixx;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;

        if (wk->wu.xyz[1].disp.pos <= 0) {
            wk->wu.facing_flag = wk->wu.active_move;
            wk->wu.xyz[1].disp.pos = 0;

            ixx = ((wk->wu.pat_status - 20) / 2 & 3) + 9;

            if (ixx > 11) {
                ixx = 10;
            }

            setup_mvxy_data(&wk->wu, ixx);
        }

        get_cancel_timer(wk);
        set_char_move_init(&wk->wu, 4, (s16)((wk->as->char_ix)));
        wk->wu.mvxy.index = wk->as->data_ix;
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 1) {
            wk->wu.routine_no[3]++;
        }

        if (wk->wu.cg_type == 20) {
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.cg_type = 0;
            wk->wu.mvxy.index++;
            break;
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);

        if ((wk->wu.routine_no[3] != 3) && (wk->wu.cg_type == 20)) {
            add_to_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.cg_type = 0;
            wk->wu.mvxy.index++;
            break;
        }

        break;

    case 3:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Attack level 9: Chain combo execution. */
static void Attack_09000(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        force_grounded_state(wk);
        get_cancel_timer(wk);
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        setup_mvxy_data(&wk->wu, wk->as->data_ix);

        if (wk->wu.cg_type == 20) {
            wk->wu.cg_type = 0;
            wk->wu.facing_flag = wk->wu.active_move;
            break;
        }

        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 20) {
            wk->wu.cg_type = 0;
            wk->wu.facing_flag = wk->wu.active_move;
        }

        if (wk->wu.cg_type == 1) {
            add_mvxy_speed(&wk->wu);
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
            break;
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);
        break;

    case 3:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Attack level 10: Leap attack (overhead) execution. */
static void Attack_10000(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        force_grounded_state(wk);
        wk->wu.facing_flag = wk->wu.active_move;
        set_char_move_init(&wk->wu, 5, wk->as->char_ix);
        setup_mvxy_data(&wk->wu, wk->as->data_ix);
        wk->cancel_timer = 0;
        wk->wu.dir_timer = 3;

        if (wk->wu.cg_type == 20) {
            wk->wu.routine_no[3] = 2;
        }

        grade_add_leap_attack(wk->wu.id);
        break;

    case 1:
        char_move(&wk->wu);
        if (wk->wu.cg_type != 20) {
            break;
        }

        wk->wu.routine_no[3]++;

    case 2:
        jumping_union_process(&wk->wu, 4);
        if ((wk->wu.routine_no[3] != 4) && wk->wu.hf.hit.player) {
            if ((wk->wu.hf.hit.player & 3) != 0) {
                wk->wu.mvxy.a[0].sp /= 4;
                wk->wu.routine_no[3] = 4;
                break;
            }

            if ((wk->wu.hf.hit.player & 0x30) != 0) {
                wk->wu.mvxy.a[0].sp /= 4;
                wk->wu.mvxy.a[1].sp = 0;
                wk->wu.routine_no[3] = 3;
                break;
            }

            if ((wk->wu.hf.hit.player & 0xC0) != 0) {
                wk->wu.mvxy.a[0].sp /= 2;
                wk->wu.mvxy.a[0].sp = -wk->wu.mvxy.a[0].sp;
                wk->wu.mvxy.a[1].sp = 0;
                wk->wu.routine_no[3] = 4;
            }
        }

        break;

    case 3:
        if (--wk->wu.dir_timer > 0) {
            break;
        }

        wk->wu.routine_no[3] = 4;
        /* fallthrough */

    case 4:
        jumping_union_process(&wk->wu, 5);
        break;

    case 5:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Attack level 14: Kara-cancel attack execution. */
static void Attack_14000(PlayerEntity* wk) {
    wk->scr_pos_set_flag = 0;
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        force_grounded_state(wk);
        wk->wu.facing_flag = wk->wu.active_move;
        wk->catch_break_ok_timer = 6;
        setup_lvdir_after_autodir(wk);
        set_char_move_init(&wk->wu, 4, wk->as->char_ix);
        break;

    case 1:
        char_move(&wk->wu);
        break;
    }

    if (wk->catch_break_ok_timer) {
        wk->catch_break_reserve = 1;
    }
}

/** @brief Attack level 15: Personal action (taunt) execution. */
static void Attack_15000(PlayerEntity* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;

        if (wk->wu.xyz[1].disp.pos <= 0) {
            wk->wu.facing_flag = wk->wu.active_move;
            setup_lvdir_after_autodir(wk);
            wk->wu.xyz[1].disp.pos = 0;
            Normal_18000_init_unit(wk, wk->wu.pat_status);
        }

        wk->catch_break_ok_timer = 6;
        set_char_move_init(&wk->wu, 4, wk->as->char_ix);

        if (wk->catch_break_ok_timer) {
            wk->catch_break_reserve = 1;
            break;
        }

        break;

    case 1:
        if (wk->catch_break_ok_timer) {
            wk->catch_break_reserve = 1;
        }

        if ((wk->wu.mvxy.a[1].sp > 0) && (wk->wu.xyz[1].disp.pos < 0)) {
            add_mvxy_speed(&wk->wu);
            cal_mvxy_speed(&wk->wu);
            break;
        }

        wk->wu.routine_no[3]++;

    case 2:
        jumping_union_process(&wk->wu, 3);

        if ((wk->wu.routine_no[3] != 3) && wk->catch_break_ok_timer) {
            wk->catch_break_reserve = 1;
            break;
        }

        break;

    case 3:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Retrieves the cancel timer value from the attack pattern data. */
static void get_cancel_timer(PlayerEntity* wk) {
    if (wk->tc_1st_flag) {
        wk->cancel_timer = 0;
        return;
    }

    if (wk->wu.xyz[1].disp.pos > 0) {
        wk->cancel_timer = 2;
        return;
    }

    wk->cancel_timer = 2;
}

/** @brief Forces a landing if the player is airborne as a safety check. */
void force_grounded_state(PlayerEntity* wk) {
    if ((g_state.Bonus_Game_Flag == 20) && wk->bs2_on_car) {
        wk->wu.xyz[1].disp.pos = g_state.bs2_floor[2];
        return;
    }

    wk->wu.xyz[1].disp.pos = 0;
}

void (*const plpat_lv_00[16])(PlayerEntity* wk) = { Attack_00000, Attack_01000, Attack_02000, Attack_03000,
                                                    Attack_04000, Attack_05000, Attack_06000, Attack_07000,
                                                    Attack_08000, Attack_09000, Attack_10000, Attack_00000,
                                                    Attack_00000, Attack_00000, Attack_14000, Attack_15000 };

void (*const plxx_extra_attack_table[])() = { pl_gill_extra_attack,    pl_alex_extra_attack,   pl_ryu_extra_attack,
                                              pl_yun_extra_attack,     pl_dudley_extra_attack, pl_necro_extra_attack,
                                              pl_hugo_extra_attack,    pl_ibuki_extra_attack,  pl_elena_extra_attack,
                                              pl_oro_extra_attack,     pl_yang_extra_attack,   pl_ken_extra_attack,
                                              pl_sean_extra_attack,    pl_urien_extra_attack,  pl_akuma_extra_attack,
                                              pl_chun_li_extra_attack, pl_makoto_extra_attack, pl_q_extra_attack,
                                              pl_twelve_extra_attack,  pl_remy_extra_attack };

const u8 cancel_whiff_type3[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };

const u8* cancel_whiff_table[20] = { cancel_whiff_type3, cancel_whiff_type3, cancel_whiff_type3, cancel_whiff_type3,
                                     cancel_whiff_type3, cancel_whiff_type3, cancel_whiff_type3, cancel_whiff_type3,
                                     cancel_whiff_type3, cancel_whiff_type3, cancel_whiff_type3, cancel_whiff_type3,
                                     cancel_whiff_type3, cancel_whiff_type3, cancel_whiff_type3, cancel_whiff_type3,
                                     cancel_whiff_type3, cancel_whiff_type3, cancel_whiff_type3, cancel_whiff_type3 };

const u8 cancel_hit_type3[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };

const u8* cancel_hit_table[20] = {
    cancel_hit_type3, cancel_hit_type3, cancel_hit_type3, cancel_hit_type3, cancel_hit_type3,
    cancel_hit_type3, cancel_hit_type3, cancel_hit_type3, cancel_hit_type3, cancel_hit_type3,
    cancel_hit_type3, cancel_hit_type3, cancel_hit_type3, cancel_hit_type3, cancel_hit_type3,
    cancel_hit_type3, cancel_hit_type3, cancel_hit_type3, cancel_hit_type3, cancel_hit_type3,
};

const u8 cjdr_blocking_type0[8] = { 16, 7, 18, 9, 20, 11, 20, 11 };
const u8 cjdr_blocking_type1[8] = { 17, 8, 19, 10, 21, 12, 21, 12 };
const u8 cjdr_blocking_type2[8] = { 18, 9, 20, 11, 22, 13, 22, 13 };

const u8* cancel_block_table[20] = {
    cjdr_blocking_type0, cjdr_blocking_type1, cjdr_blocking_type1, cjdr_blocking_type1, cjdr_blocking_type0,
    cjdr_blocking_type2, cjdr_blocking_type1, cjdr_blocking_type0, cjdr_blocking_type1, cjdr_blocking_type1,
    cjdr_blocking_type1, cjdr_blocking_type0, cjdr_blocking_type1, cjdr_blocking_type2, cjdr_blocking_type0,
    cjdr_blocking_type1, cjdr_blocking_type1, cjdr_blocking_type1, cjdr_blocking_type2, cjdr_blocking_type1
};

const u8 cjdr_defense_type3[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };

const u8* cancel_defense_table[20] = { cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3,
                                       cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3,
                                       cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3,
                                       cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3,
                                       cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3 };
