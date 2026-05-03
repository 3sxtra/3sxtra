/**
 * @file plmain2.c
 * Player Character's Core Gameplay Logic for Bonus Stages
 */

#include "sf33rd/Source/Game/engine/player_main_2.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/animation/appear.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/bonus_basketball_ai.h"
#include "sf33rd/Source/Game/engine/bonus_basketball_ai_2.h"
#include "sf33rd/Source/Game/engine/cmd_main.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_main.h"
#include "sf33rd/Source/Game/engine/player_normal_state.h"
#include "sf33rd/Source/Game/engine/player_state_dispatcher.h"
#include "sf33rd/Source/Game/engine/player_common_mechanics.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/metamorphosis_color.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"

static void player_mvbs_0000(PlayerEntity* wk);
static void player_mvbs_1000(PlayerEntity* wk);
static void plmv_b_1010(PlayerEntity* wk);
static void plmv_b_1020(PlayerEntity* wk, s16 step);
static void player_mvbs_2000(PlayerEntity* wk);
static void player_mvbs_3000(PlayerEntity* wk);
static void player_mvbs_4000(PlayerEntity* wk);

void (*const plmain_b_lv_00[5])(PlayerEntity* wk);

/** @brief Top-level per-frame player move update for bonus stages. */
void Player_move_bonus(PlayerEntity* wk, u16 lv_data) {
    s16 i;

    if (wk->wu.pl_operator) {
        if (wk->metamor_over) {
            wk->cp->input_held = 0;
        } else {
            wk->cp->input_held = lv_data;
        }
    } else {
        if (g_state.Bonus_Game_Flag == 21) {
            bbbs_com_execute(wk);
        } else {
            bonus_basketball_ai_execute2(wk);
        }

        wk->cp->input_held = 0;
    }

    if (wk->death_timerlag) {
        wk->cp->input_held = 0;
    }

    if (wk->wakeup_key_flag) {
        wk->cp->input_held = 0;
    }

    wk->cp->input_held = check_illegal_lever_data(wk->cp->input_held);

    if ((wk->death_timerlag + wk->wakeup_key_flag) == 0) {
        wk->cannot_turn_flag = 0;
    }

    for (i = 0; i < 8; i++) {
        wk->wu.old_routine_no[(i)] = wk->wu.routine_no[(i)];
    }

    for (i = 0; i < 3; i++) {
        wk->wu.old_pos[(i)] = wk->wu.xyz[(i)].disp.pos;
    }

    get_recent_movement_delta(wk);
    wk->old_gdflag = wk->guard_flag;
    wk->wu.renew_attack = 0;
    wk->wu.vital_old = wk->wu.vital_new;

    if (wk->sa_stop_flag != 1) {
        move_check(wk);
    }

    wk->wu.script_register_bank[10] = wk->cp->lever_grace_period;
    wk->wu.script_register_bank[11] += wk->cp->lever_grace_period;
    wk->wu.script_register_bank[11] &= 0x7FFF;
    wk->wu.script_register_bank[12] = wk->cp->input_pressed;
    wk->wu.script_register_bank[13] = wk->cp->input_current;
    plmain_b_lv_00[wk->wu.routine_no[0]](wk);
}

/** @brief Bonus stage move phase 0 — initial work setup. */
static void player_mvbs_0000(PlayerEntity* wk) {
    s16 i;

    for (i = 0; i < 8; i++) {
        wk->old_pos_data[i] = 0;
    }

    setup_vitality(&wk->wu, wk->player_number);
    set_player_shadow(wk);
    wk->bullet_hit_count = wk->bullet_hit_count_timer = 0;
    wk->auto_guard = 1;
    wk->wu.hit_stop = wk->wu.damage_hit_stop = 0;
    wk->wu.hit_quake = wk->wu.damage_screen_shake = 0;
    wk->throw_invuln_flag = 0;
    wk->slide_timer = 0;
    wk->invuln_flag = false;
    wk->is_throwing = wk->is_being_thrown = false;
    clear_kizetsu_point(wk);
    wk->recovery_roll_ok_timer = 0;
    wk->ukemi_cooldown_ok = 0;
    wk->recovery_roll_success = 0;
    clear_my_shell_ix(&wk->wu);
    wk->sa->meter_points = 0;
    wk->sa->can_activate = 0;
    wk->sa->ex_mode = 0;
    wk->sa->meter_routine_no = 0;
    wk->sa->meter_routine_no_2 = 0;
    wk->sa->super_art_routine_no = 0;
    wk->sa->super_art_routine_no_2 = 0;
    wk->sa->ex_routine_no = 0;
    wk->metamorphose = 0;
    wk->metamor_over = 0;
    wk->sa_healing = 0;
    demo_set_sa_full(wk->sa);
    wk->damage_pushbox_flag = 0;
    wk->chip_death_flag = 0;
    wk->wu.floor = 0;
    wk->bs2_area_car = 0;
    wk->bs2_over_car = 0;
    wk->bs2_on_car = 0;
    wk->wu.extra_col = wk->wu.extra_col_2 = 0;
    wk->sa_stop_flag = 0;
    clear_tk_flags(wk);
    wk->wu.routine_no[0] = 1;
    wk->wu.routine_no[6] = 0;
    wk->wu.script_register_bank[0] = 0;
    about_gauge_process(wk);

    if (wk->player_number == 18) {
        metamor_color_restore(wk->wu.id);
    }
}

/** @brief Bonus stage move phase 1 — appearance/entrance animation. */
static void player_mvbs_1000(PlayerEntity* wk) {
    switch (g_state.appear_type) {
    case APPEAR_TYPE_NON_ANIMATED:
        plmv_b_1010(wk);
        plmv_b_1020(wk, 96);
        g_state.Appear_end++;
        break;

    case APPEAR_TYPE_VICTORY:
        plmv_b_1010(wk);
        plmv_b_1020(wk, 128);
        break;

    case APPEAR_TYPE_ANIMATED:
    case APPEAR_TYPE_TRANSITIONAL:
        wk->wu.routine_no[0] = 2;

        if (g_state.Bonus_Game_Flag != 20 || wk->wu.pl_operator) {
            wk->wu.routine_no[1] = 0;
            wk->wu.routine_no[2] = 0;
            wk->wu.routine_no[3] = 0;
            wk->wu.disp_flag = 1;
        }

        appear_data_init_set(wk);
        break;
    }

    if ((wk->wu.pl_operator == 0) && (g_state.Bonus_Game_Flag == 20)) {
        wk->wu.routine_no[1] = 0;
        wk->wu.routine_no[2] = 51;
        wk->wu.routine_no[3] = 0;
        wk->wu.xyz[0].disp.pos = 468;
        wk->wu.xyz[1].disp.pos = 0;
    }

    Player_normal(wk);
}

/** @brief Sub-phase: sets initial routine numbers for bonus entrance. */
static void plmv_b_1010(PlayerEntity* wk) {
    wk->wu.routine_no[0] = 3;

    if (g_state.Bonus_Game_Flag != 20 || wk->wu.pl_operator) {
        wk->wu.routine_no[1] = 0;
        wk->wu.routine_no[2] = 1;
        wk->wu.routine_no[3] = 0;
        wk->wu.disp_flag = 1;
    }
}

/** @brief Sub-phase: positions the player at the bonus stage spawn offset. */
static void plmv_b_1020(PlayerEntity* wk, s16 step) {
    if (wk->wu.id) {
        wk->wu.facing_flag = 0;
        wk->wu.xyz[0].disp.pos = step + get_center_position();
        wk->wu.xyz[1].disp.pos = 0;
        return;
    }

    wk->wu.facing_flag = 1;
    wk->wu.xyz[0].disp.pos = get_center_position() - step;
    wk->wu.xyz[1].disp.pos = 0;
}

/** @brief Bonus stage move phase 2 — intro wait/transition. */
static void player_mvbs_2000(PlayerEntity* wk) {
    if (g_state.Bonus_Game_Flag != 20 || wk->wu.pl_operator) {
        if (wk->wu.routine_no[2] == 1) {
            wk->wu.routine_no[0] = 3;
            wk->wu.disp_flag = 1;
            wk->wu.cg_type = 0;
        }
    } else {
        wk->wu.routine_no[0] = 3;
        wk->wu.cg_type = 0;
    }

    Player_normal(wk);
}

/** @brief Bonus stage move phase 3 — standby/normal state. */
static void player_mvbs_3000(PlayerEntity* wk) {
    Player_normal(wk);
}

/** @brief Bonus stage move phase 4 — active gameplay with combat. */
static void player_mvbs_4000(PlayerEntity* wk) {
    wk->permitted_art_type = 0;
    check_extra_jump_timer(wk);

    if (wk->sa_stop_flag != 1) {
        check_lever_data(wk);
    }

    if (wk->is_being_thrown) {
        wk->wu.hit_stop = wk->wu.damage_hit_stop = 0;
    }

    if (!check_hit_stop(wk)) {
        plmain_lv_02[wk->wu.routine_no[1]](wk);

        if ((g_state.Timer_Freeze == 0) && (wk->wu.hit_stop == 0) && (wk->slide_timer > 0)) {
            wk->slide_timer -= 2;
        }

        if (wk->slide_timer < 0) {
            wk->invuln_flag = true;
        } else {
            wk->invuln_flag = false;
        }
    }

    if (g_state.Timer_Freeze == 0) {
        look_after_timers(wk);
    }

    about_gauge_process(wk);
}

void (*const plmain_b_lv_00[5])(PlayerEntity* wk) = {
    player_mvbs_0000, player_mvbs_1000, player_mvbs_2000, player_mvbs_3000, player_mvbs_4000
};
