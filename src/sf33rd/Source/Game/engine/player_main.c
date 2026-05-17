/**
 * @file plmain.c
 * Player Character's Core Gameplay Logic
 */

#include "sf33rd/Source/Game/engine/player_main.h"
#include "game_state.h"
#include "common.h"
#include "constants.h"
#include "sf33rd/Source/Game/animation/appear.h"
#include "sf33rd/Source/Game/com/ai_player_control.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/calculate_direction.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/cmd_main.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_pattern.h"
#include "sf33rd/Source/Game/engine/player_grab_controller.h"
#include "sf33rd/Source/Game/engine/player_grabbed_controller.h"
#include "sf33rd/Source/Game/engine/player_damage_controller.h"
#include "sf33rd/Source/Game/engine/player_normal_state.h"
#include "sf33rd/Source/Game/engine/player_state_dispatcher.h"
#include "sf33rd/Source/Game/engine/player_common_mechanics.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/super_gauge.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/metamorphosis_color.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/system/system_director.h"
#include "sf33rd/Source/Game/training/training_state.h"

static void plmv_1010(PlayerEntity* wk);
static void plmv_1020(PlayerEntity* wk, s16 step);
static void mpg_union(PlayerEntity* wk);
static void eag_union(PlayerEntity* wk);
static void sag_union(PlayerEntity* wk);
static void addSAAttribute(u8* move_type, u16* koa);
static void check_omop_vital(PlayerEntity* wk);
static s16 select_hit_stop(s16 ms, s16 sb);

/** @brief Top-level per-frame player move update — processes input, state, and gauge. */
void Player_move(PlayerEntity* wk, u16 lv_data) {
    s16 i;

#if CPS3
    if (DAT_02016b6c == -1) {
        wk->cp->input_held = processed_lvbt(FUN_06092294(wk->wu.id));
    } else {
        if (wk->wu.pl_operator) {
            wk->cp->input_held = lv_data;
        } else {
            wk->cp->input_held = processed_lvbt(cpu_algorithm(wk));
        }

        if (wk->metamor_over) {
            wk->cp->input_held = 0;
        }
    }
#else
    if (g_lua_dummy_active && wk->wu.id == g_lua_dummy_player_id) {
        // Lua dummy: use g_state.Lever_Buff written by joypad.set() in emu.registerbefore()
        wk->cp->input_held = processed_lvbt(g_state.Lever_Buff[wk->wu.id]);
    } else if (wk->wu.pl_operator) {
        wk->cp->input_held = lv_data;
    } else {
        wk->cp->input_held = processed_lvbt(cpu_algorithm(wk));
    }

    wk->cp->input_held = check_illegal_lever_data(wk->cp->input_held);

    if (wk->metamor_over) {
        wk->cp->input_held = 0;
    }

    if (wk->resurrection_resv) {
        wk->cp->input_held = 0;
    }
#endif

    if (wk->death_timerlag) {
        wk->cp->input_held = 0;
    }

    if (wk->wakeup_key_flag) {
        wk->cp->input_held = 0;
    }

    if ((wk->death_timerlag + wk->wakeup_key_flag) == 0) {
        wk->cannot_turn_flag = 0;
    }

    for (i = 0; i < 8; i++) {
        wk->wu.old_routine_no[i] = wk->wu.routine_no[i];
    }

    for (i = 0; i < 3; i++) {
        wk->wu.old_pos[i] = wk->wu.xyz[i].disp.pos;
    }

    get_recent_movement_delta(wk);
    wk->old_gdflag = wk->guard_flag;
    wk->wu.renew_attack = 0;
    wk->wu.vital_old = wk->wu.vital_new;

    if (wk->sa_stop_flag != 1) {
        move_check(wk);
    } else {
        key_thru(wk);
    }

    wk->wu.script_register_bank[10] = wk->cp->lever_grace_period;
    wk->wu.script_register_bank[11] += wk->cp->lever_grace_period;
    wk->wu.script_register_bank[11] &= 0x7FFF;
    wk->wu.script_register_bank[12] = wk->cp->input_pressed;
    wk->wu.script_register_bank[13] = wk->cp->input_current;
    plmain_lv_00[wk->wu.routine_no[0]](wk);
}

#if !CPS3
/** @brief Sanitizes raw lever data by blocking illegal simultaneous directions. */
u16 check_illegal_lever_data(u16 data) {
    u16 lever = data & 0xF;

    data = (data & ~0xF) | Correct_Lv_Data[lever];
    return data;
}
#endif

/** @brief Player move phase 0 — initial setup and work initialization. */
static void player_mv_0000(PlayerEntity* wk) {
    s16 i;

    for (i = 0; i < 8; i++) {
        wk->old_pos_data[i] = 0;
    }

    setup_vitality(&wk->wu, (wk->player_number));
    set_player_shadow(wk);
    wk->bullet_hit_count = wk->bullet_hit_count_timer = 0;
    wk->auto_guard = 0;
    wk->wu.hit_stop = wk->wu.damage_hit_stop = 0;
    wk->wu.hit_quake = wk->wu.damage_screen_shake = 0;
    wk->throw_invuln_flag = 0;
    wk->slide_timer = 0;
    wk->invuln_flag = false;
    wk->is_throwing = wk->is_being_thrown = false;
    clear_kizetsu_point(wk);

#if CPS3
    DAT_20281a8[wk->wu.id] = 0; // TODO: figure out what this does
#endif

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

#if !CPS3
    wk->resurrection_resv = 0;
#endif

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

#if CPS3
    if (wk->player_number == CHAR_ELENA) {
        FUN_06107d24(wk); // No-op function
    }
#endif

    wk->wu.routine_no[6] = 0;
    wk->wu.script_register_bank[0] = 0;

#if !CPS3
    wk->emergency_vital_timer = 40;

    if (wk->player_number == 18) {
        metamor_color_restore(wk->wu.id);
    }

    switch (wk->special_move_disabled_flag2 & (DIP2_SA_GAUGE_ROUND_RESET_DISABLED | DIP2_SA_GAUGE_MAX_START_DISABLED)) {
    case DIP2_SA_GAUGE_MAX_START_DISABLED:
        clear_super_arts_point(wk);
        spgauge_cont_init();
        break;

    case DIP2_SA_GAUGE_ROUND_RESET_DISABLED:
        if (g_state.Round_num != 0) {
            break;
        }

        /* fallthrough */

    case 0:
        demo_set_sa_full(wk->sa);
        spgauge_cont_demo_init();
        break;
    }
#endif

    about_gauge_process(wk);
}

/** @brief Player move phase 1 — appearance / entrance animation. */
static void player_mv_1000(PlayerEntity* wk) {
    switch (g_state.appear_type) {
    case APPEAR_TYPE_NON_ANIMATED:
        plmv_1010(wk);

        if (g_state.Combo_Demo_Flag == 0) {
            plmv_1020(wk, 88);
        } else {
            set_super_arts_status(wk->wu.id);
            demo_set_sa_full(wk->sa);
        }

        g_state.Appear_end++;
        break;

    case APPEAR_TYPE_VICTORY:
        plmv_1010(wk);
        plmv_1020(wk, 128);
        break;

    case APPEAR_TYPE_ANIMATED:
    case APPEAR_TYPE_TRANSITIONAL:
        wk->wu.routine_no[0] = 2;
        wk->wu.routine_no[1] = 0;
        wk->wu.routine_no[2] = 0;
        wk->wu.routine_no[3] = 0;

        if (g_state.Combo_Demo_Flag == 0) {
            wk->wu.disp_flag = 1;
        }

        appear_data_init_set(wk);
        break;
    }

    Player_normal(wk);

#if !CPS3
    about_gauge_process(wk);
#endif
}

/** @brief Sub-phase of entrance: sets initial routine numbers and display flags. */
static void plmv_1010(PlayerEntity* wk) {
    wk->wu.routine_no[0] = 3;
    wk->wu.routine_no[1] = 0;
    wk->wu.routine_no[2] = 1;
    wk->wu.routine_no[3] = 0;

    if (g_state.Combo_Demo_Flag == 0) {
        wk->wu.disp_flag = 1;
    }
}

/** @brief Sub-phase of entrance: positions the player at the spawn offset. */
static void plmv_1020(PlayerEntity* wk, s16 step) {
    if (wk->wu.id) {
        wk->wu.facing_flag = 0;
        wk->wu.xyz[0].disp.pos = step + get_center_position();
        wk->wu.xyz[1].disp.pos = 0;
    } else {
        wk->wu.facing_flag = 1;
        wk->wu.xyz[0].disp.pos = get_center_position() - step;
        wk->wu.xyz[1].disp.pos = 0;
    }

#if !CPS3
    about_gauge_process(wk);
#endif
}

/** @brief Player move phase 2 — intro/cinematic wait state. */
static void player_mv_2000(PlayerEntity* wk) {
    if (wk->wu.routine_no[2] == 1) {
        wk->wu.routine_no[0] = 3;

        if (g_state.Combo_Demo_Flag == 0) {
            wk->wu.disp_flag = 1;
        }

        wk->wu.cg_type = 0;
    }

    Player_normal(wk);

#if !CPS3
    about_gauge_process(wk);
#endif
}

/** @brief Player move phase 3 — idle/standby before fight start. */
static void player_mv_3000(PlayerEntity* wk) {
    if (g_state.gouki_app) {
        gouki_appear(wk);
    } else {
        Player_normal(wk);
    }

#if !CPS3
    about_gauge_process(wk);
#endif
}

/** @brief Player move phase 4 — active gameplay state, processes all combat. */
static void player_mv_4000(PlayerEntity* wk) {
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

        if (g_state.Timer_Freeze == 0 && wk->wu.hit_stop == 0 && wk->slide_timer > 0) {
            wk->slide_timer -= 2;
        }

        if (wk->slide_timer < 0) {
            wk->invuln_flag = true;
        } else {
            wk->invuln_flag = false;
        }

#if !CPS3
        check_omop_vital(wk);
#endif
    }

    if (g_state.Timer_Freeze == 0) {
        look_after_timers(wk);
    }

    about_gauge_process(wk);
}

/** @brief Checks and processes hit-stop freeze frames for a player. */
s16 check_hit_stop(PlayerEntity* wk) {
    s16 num;
    State* emwk = (State*)wk->wu.target_adrs;

    num = 0;

    if ((wk->wu.damage_hit_stop != 0) && (wk->wu.hit_stop != 0)) {
        if (wk->wu.routine_no[3]) {
            wk->wu.hit_stop = select_hit_stop(wk->wu.hit_stop, wk->wu.damage_hit_stop);
            wk->wu.damage_hit_stop = 0;
        } else {
            wk->wu.damage_hit_stop = select_hit_stop(wk->wu.damage_hit_stop, wk->wu.hit_stop);
            wk->wu.hit_stop = 0;
            return 0;
        }
    }

    if (wk->wu.hit_stop) {
        num = 1;

        if (wk->wu.hit_stop > 0) {
            wk->wu.hit_stop--;

            if (wk->sa_stop_flag == 2) {
                if (wk->just_sa_stop_timer == g_state.Game_timer) {
                    wk->wu.hit_stop++;
                }

                if (wk->wu.hit_stop <= wk->super_art_stop_index) {
                    wk->super_art_stop_lever_dir = wk->cp->input_held;
                    wk->sa_stop_flag = 1;
                }
            }
        } else {
            wk->wu.hit_stop++;
            char_move(&wk->wu);
        }

        if ((wk->wu.routine_no[3] == 0) && ((wk->wu.routine_no[1] == 1) || (wk->wu.routine_no[1] == 3)) &&
            (emwk->routine_no[1] != 1) && (emwk->routine_no[1] != 3)) {
            num = 0;
        }

        if ((wk->wu.hit_stop == 0) && (wk->high_jump_ok != 0)) {
            char_move_cmd_hit_stop(wk);
        }
    }

    if (wk->sa_stop_flag) {
        g_state.Timer_Freeze = 1;
    }

    return num;
}

/** @brief Selects the effective hit-stop duration from master vs. sub values. */
static s16 select_hit_stop(s16 ms, s16 sb) {
    s16 maf = 1;

    if (sb < 0) {
        sb = -sb;
    }

    if (ms < 0) {
        ms = -ms;
        maf = -1;
    }

    if (ms < sb) {
        ms = sb;
    }

    return ms * maf;
}

/** @brief Decrements and manages miscellaneous per-player timers each frame. */
void look_after_timers(PlayerEntity* wk) {
    if (wk->throw_invuln_flag) {
        wk->throw_invuln_flag--;
    }

    if (wk->catch_break_ok_timer) {
        wk->catch_break_ok_timer--;
    }

    if (wk->ukemi_cooldown_ok) {
        wk->recovery_roll_ok_timer--;

        if (wk->recovery_roll_ok_timer <= 0) {
            wk->recovery_roll_ok_timer = 0;
            wk->ukemi_cooldown_ok = 0;
            wk->recovery_roll_success = 0;
        } else if (check_ukemi_flag(wk)) {
            wk->recovery_roll_ok_timer = 0;
            wk->ukemi_cooldown_ok = 0;
            wk->recovery_roll_success = 1;
        }
    }

    if (wk->bullet_hit_count) {
        if (--wk->bullet_hit_count_timer <= 0) {
            wk->bullet_hit_count = 0;
        }
    }

    if (wk->py->now.quantity.h && (wk->wu.hit_stop == 0)) {
        wk->py->now.timer -= (wk->py->recover * stun_gauge_r_omake[omop_stun_gauge_rcv[wk->wu.id]]) / 32;

        if (wk->py->now.quantity.h <= 0) {
            wk->py->now.timer = 0;
        }
    }

#if DEBUG
    if (Debug_w[DEBUG_1SHOT_SA]) {
        const s16 sa_ixs[] = { wk->sa->normal_sa_graphic_ix, wk->sa->ex_sa_graphic_ix, wk->sa->ex_sa2_graphic_ix,
                               wk->sa->normal_sa_anim_ix,    wk->sa->ex_sa_anim_ix,    wk->sa->ex_sa2_anim_ix };
        s16 si;

        for (si = 0; si < 6; si++) {
            if (sa_ixs[si] != 0) {
                wk->cp->move_state_flags[sa_ixs[si]] = 9;
            }
        }
    }
#endif
}

/** @brief Processes gauge-related logic (stun recovery, SA charge, MP, EX). */
void about_gauge_process(PlayerEntity* wk) {
    eag_union(wk);
    sag_union(wk);
    mpg_union(wk);

#if !CPS3
    add_sp_arts_gauge_maxbit(wk);
#endif
}

/** @brief Updates the MP gauge (general meter) for a player. */
static void mpg_union(PlayerEntity* wk) {
    switch (wk->sa->meter_routine_no) {
    case 0:
        if (wk->sa->stock == wk->sa->stock_max) {
            wk->sa->meter_routine_no = 1;
            wk->sa->meter_points = 1;
        }

        wk->sa->super_effect_meter = 0;
        break;

    case 1:
        if (wk->sa->stock < wk->sa->stock_max) {
            wk->sa->meter_routine_no = 0;
            wk->sa->meter_points = 0;
        } else if (wk->sa->meter_points == -1) {
            wk->sa->meter_routine_no = 2;
            wk->sa->super_effect_meter = 1;
        }

        break;

    case 2:
        switch (wk->sa->super_effect_meter) {
        case -1:
            if (!g_state.pcon_dp_flag) {
                wk->sa->stock = 0;
                wk->sa->gauge.i = 0;
            }

#if !CPS3
            sag_bug_fix(wk->wu.id);
#endif

            wk->sa->super_effect_meter = 0;
            wk->sa->meter_routine_no = 0;
            wk->sa->meter_points = 0;

#if !CPS3
            g_state.sag_inc_timer[(wk->wu.id)] = 20;
#endif
            break;

        case 1:
            if (wk->wu.routine_no[1] == 4) {
                break;
            }

            /* fallthrough */

        default:
            wk->sa->super_effect_meter = 0;
            wk->sa->meter_routine_no = 0;
            wk->sa->meter_points = 0;
            break;
        }

        break;

    default:
        wk->sa->meter_routine_no = 0;
        wk->sa->meter_points = 0;
        wk->sa->stock = 0;
        wk->sa->gauge.i = 0;
        wk->sa->super_effect_meter = 0;
        break;
    }
}

/** @brief Updates the EX gauge for a player. */
static void eag_union(PlayerEntity* wk) {
    switch (wk->sa->ex_routine_no) {
    case 0:
        if (wk->player_number == 14 || wk->player_number == 0) {
            if (wk->sa->stock != 0) {
                wk->sa->ex_routine_no = 1;
                wk->sa->ex_mode = 1;
            }

            break;
        }

        if ((wk->sa->stock != 0) || (wk->sa->gauge.s.h >= use_ex_gauge[omop_use_ex_gauge_ix[wk->wu.id]])) {
            wk->sa->ex_routine_no = 1;
            wk->sa->ex_mode = 1;
            break;
        }

        break;

    case 1:
        if (wk->player_number == 14 || wk->player_number == 0) {
            if (wk->sa->stock == 0) {
                wk->sa->ex_routine_no = 0;
                wk->sa->ex_mode = 0;
                break;
            }
        } else if ((wk->sa->stock == 0) && (wk->sa->gauge.s.h < use_ex_gauge[omop_use_ex_gauge_ix[wk->wu.id]])) {
            wk->sa->ex_routine_no = 0;
            wk->sa->ex_mode = 0;
            break;
        }

        if (wk->sa->ex_mode == -1) {
            wk->sa->ex_routine_no = 2;
            g_state.sa_gauge_flash[wk->wu.id] |= 2;
        }

        break;

    case 2:
        if (!g_state.pcon_dp_flag) {
            if (wk->sa->gauge_type == 1 && wk->sa->stock == wk->sa->stock_max) {
                wk->sa->gauge.i = 0;
            }

            if (wk->sa->gauge.s.h >= use_ex_gauge[omop_use_ex_gauge_ix[wk->wu.id]]) {
                wk->sa->gauge.s.h -= use_ex_gauge[omop_use_ex_gauge_ix[wk->wu.id]];
            } else {
                wk->sa->stock--;
                wk->sa->gauge.s.h += wk->sa->gauge_length - use_ex_gauge[omop_use_ex_gauge_ix[wk->wu.id]];
            }
        }

        sag_bug_fix(wk->wu.id);
        wk->sa->ex_routine_no = 0;
        wk->sa->ex_mode = 0;
        g_state.sag_inc_timer[wk->wu.id] = 20;
        break;

    default:
        wk->sa->ex_routine_no = 0;
        wk->sa->ex_mode = 0;
        wk->sa->stock = 0;
        wk->sa->gauge.i = 0;
        break;
    }
}

/** @brief Decrement SA stock, respecting g_state.pcon_dp_flag and ex4th_exec. */
static void sag_decrement_store(PlayerEntity* wk) {
    if (!g_state.pcon_dp_flag) {
        if (wk->sa->ex4th_exec) {
            wk->sa->stock = 0;
        } else {
            wk->sa->stock--;
        }
    }
}

/** @brief Updates the Super Art gauge charge and stock for a player. */
#if CPS3
void sag_union_0(PlayerEntity* wk) {
    switch (wk->sa->super_art_routine_no) {
    case 0:
        if (wk->sa->stock != 0) {
            wk->sa->super_art_routine_no = 1;
            wk->sa->can_activate = 1;
            wk->sa->super_art_id += 1;
        }

        wk->sa->super_effect_can_activate = 0;
        break;

    case 1:
        if (wk->sa->stock == 0) {
            wk->sa->super_art_routine_no = 0;
            wk->sa->can_activate = 0;
        } else if (wk->sa->can_activate == -1) {
            wk->sa->super_art_routine_no = 2;
            wk->sa->super_effect_can_activate = 1;
        }

        break;

    case 2:
        if (wk->sa->super_effect_can_activate == -1) {
            if (!g_state.pcon_dp_flag) {
                wk->sa->stock -= 1;
            }

            wk->sa->super_effect_can_activate = 0;
            wk->sa->super_art_routine_no = 0;
            wk->sa->can_activate = 0;
        } else if ((wk->sa->super_effect_can_activate != 1) || (wk->wu.routine_no[1] != 4)) {
            wk->sa->super_effect_can_activate = 0;
            wk->sa->super_art_routine_no = 0;
            wk->sa->can_activate = 0;
        }

        break;

    default:
        wk->sa->super_art_routine_no = 0;
        wk->sa->can_activate = 0;
        wk->sa->stock = 0;
        wk->sa->super_effect_can_activate = 0;
        break;
    }
}

void sag_union_1(PlayerEntity* wk) {
    switch (wk->sa->super_art_routine_no) {
    case 0:
        if (wk->sa->stock != 0) {
            wk->sa->super_art_routine_no = 1;
            wk->sa->can_activate = 1;
            wk->sa->super_art_id += 1;
        }

        wk->sa->super_effect_can_activate = 0;
        break;

    case 1:
        if (wk->sa->stock == 0) {
            wk->sa->super_art_routine_no = 0;
            wk->sa->can_activate = 0;
        } else if (wk->sa->can_activate == -1) {
            wk->sa->super_art_routine_no = 2;
            wk->sa->super_effect_can_activate = 1;
        }

        break;

    case 2:
        if (wk->sa->super_effect_can_activate == -1) {
            if (!g_state.pcon_dp_flag) {
                wk->sa->stock -= -1;
            }

            wk->sa->gauge.s.h = wk->sa->gauge_length;
            wk->sa->gauge.s.l = -1;
            wk->sa->super_art_routine_no = 3;
            wk->sa->super_effect_can_activate = 0;
        } else if ((wk->sa->super_effect_can_activate != 1) || (wk->wu.routine_no[1] != 4)) {
            wk->sa->super_effect_can_activate = 0;
            wk->sa->super_art_routine_no = 0;
            wk->sa->can_activate = 0;
            wk->sa->damage_time_multiplier = 1;
        }

        break;

    case 3:
        if (g_state.Timer_Freeze) {
            break;
        }

        wk->sa->super_art_routine_no = 4;
        /* fallthrough */

    case 4:
        if ((wk->sa_stop_flag != 1) && (((PlayerEntity*)wk->wu.target_adrs)->sa_stop_flag != 1)) {
            wk->sa->gauge.i -= wk->sa->damage_time * wk->sa->damage_time_multiplier;
        }

        if (wk->sa->gauge.s.h < 1) {
            wk->sa->gauge.i = 0;
            wk->sa->can_activate = 0;
            wk->sa->super_art_routine_no = 0;
            wk->sa->damage_time_multiplier = 1;
        } else {
            if (g_state.My_char[wk->wu.id] == CHAR_YUN) {
                wk->wu.attack_type |= 0x20;
                wk->wu.attack_art_type = 0x80;
            }

            if (g_state.My_char[wk->wu.id] == CHAR_YANG) {
                wk->wu.attack_type |= 0x20;
                wk->wu.attack_art_type = 0x80;
            }

            if (g_state.My_char[wk->wu.id] == CHAR_MAKOTO) {
                wk->wu.attack_type |= 0x20;
                wk->wu.attack_art_type = 0x80;
            }

            if (g_state.My_char[wk->wu.id] == CHAR_TWELVE) {
                wk->wu.attack_type |= 0x20;
                wk->wu.attack_art_type = 0x80;
            }

            if ((g_state.My_char[wk->wu.id] == CHAR_ORO) && (wk->sa->kind_of_arts == 2)) {
                wk->wu.att.dipsw |= 0x10;
            }
        }

        break;

    default:
        wk->sa->super_art_routine_no = 0;
        wk->sa->can_activate = 0;
        wk->sa->stock = 0;
        wk->sa->super_effect_can_activate = 0;
        wk->sa->damage_time_multiplier = 1;
        break;
    }
}

void sag_union_3(PlayerEntity* wk) {
    switch (wk->sa->super_art_routine_no) {
    case 0:
        if (wk->sa->stock != 0) {
            wk->sa->super_art_routine_no = 1;
            wk->sa->can_activate = 1;
        }

        wk->sa->super_effect_can_activate = 0;
        break;

    case 1:
        if (wk->sa->stock == 0) {
            wk->sa->super_art_routine_no = 0;
            wk->sa->can_activate = 0;
        } else if (wk->sa->can_activate == -1) {
            wk->sa->super_art_routine_no = 2;
            wk->sa->super_effect_can_activate = 1;
        }

        break;

    case 2:
        if (wk->sa->super_effect_can_activate == -1) {
            wk->sa->stock = wk->sa->stock + -1;
            wk->sa->gauge.i = 0;
            wk->sa->super_effect_can_activate = 0;
            wk->sa->super_art_routine_no = 3;
        } else if (wk->sa->super_effect_can_activate != 1) {
            wk->sa->super_effect_can_activate = 0;
            wk->sa->super_art_routine_no = 0;
            wk->sa->can_activate = 0;
        }

        break;

    case 3:
        // Do nothing
        break;

    default:
        wk->sa->super_art_routine_no = 0;
        wk->sa->can_activate = 0;
        wk->sa->stock = 0;
        wk->sa->super_effect_can_activate = 0;
        break;
    }
}
#else
void sag_union_ps2(PlayerEntity* wk) {
    switch (wk->sa->super_art_routine_no) {
    case 0:
        if (wk->sa->stock) {
            wk->sa->super_art_routine_no = 1;
            wk->sa->can_activate = 1;
            wk->sa->super_art_id++;
        }

        wk->sa->super_effect_can_activate = 0;
        break;

    case 1:
        if (wk->sa->stock == 0) {
            wk->sa->super_art_routine_no = 0;
            wk->sa->can_activate = 0;
            break;
        }

        if (wk->sa->can_activate == -1) {
            wk->sa->super_art_routine_no = 2;
            wk->sa->super_art_routine_no_2 = 0;
            wk->sa->super_effect_can_activate = 1;

            if (wk->sa->gauge_type_2 == 0) {
                wk->sa->backup_gauge_high = 0;
                break;
            }
        }

        break;

    case 2:
        switch (wk->sa->gauge_type_2) {
        case 0:
            switch (wk->sa->super_effect_can_activate) {
            case -1:
                sag_decrement_store(wk);
                sag_bug_fix(wk->wu.id);
                wk->sa->super_effect_can_activate = 0;
                wk->sa->super_art_routine_no = 0;
                wk->sa->can_activate = 0;
                g_state.sag_inc_timer[wk->wu.id] = 20;
                break;

            case 1:
                if (wk->wu.routine_no[1] != 4) {
                default:
                    wk->sa->super_effect_can_activate = 0;
                    wk->sa->super_art_routine_no = 0;
                    wk->sa->can_activate = 0;
                }
            }

            break;

        case 1:
            switch (wk->sa->super_art_routine_no_2) {
            case 0:
                switch (wk->sa->super_effect_can_activate) {
                case -1:
                    sag_decrement_store(wk);
                    sag_bug_fix(wk->wu.id);

                    if (wk->sa->meter_points == 1) {
                        wk->sa->backup_gauge_high = 0;
                    } else {
                        wk->sa->backup_gauge_high = wk->sa->gauge.s.h;
                    }

                    wk->sa->gauge.s.h = wk->sa->gauge_length;
                    wk->sa->gauge.s.l = -1;
                    wk->sa->super_art_routine_no_2 = 1;
                    wk->sa->super_effect_can_activate = 0;
                    break;

                case 1:
                    if (wk->wu.routine_no[1] != 4) {
                    default:
                        wk->sa->super_effect_can_activate = 0;
                        wk->sa->super_art_routine_no = 0;
                        wk->sa->can_activate = 0;
                        wk->sa->damage_time_multiplier = 1;
                    }
                }

                break;

            case 1:
                if (g_state.Timer_Freeze != 0) {
                    break;
                }

                wk->sa->super_art_routine_no_2 = 2;
                /* fallthrough */

            case 2:
                if ((wk->sa_stop_flag != 1) && (((PlayerEntity*)wk->wu.target_adrs)->sa_stop_flag != 1)) {
                    wk->sa->gauge.i -= wk->sa->damage_time * wk->sa->damage_time_multiplier;
                }

                if (wk->sa->gauge.s.h <= 0 || g_state.Suicide[6] != 0) {
                    wk->sa->gauge.i = 0;
                    wk->sa->can_activate = 0;
                    wk->sa->super_art_routine_no = 0;
                    wk->sa->damage_time_multiplier = 1;
                    wk->sa->gauge.s.h = wk->sa->backup_gauge_high;
                    g_state.sag_inc_timer[wk->wu.id] = 20;
                    break;
                }

                if (g_state.My_char[wk->wu.id] == 3) {
                    addSAAttribute(&wk->wu.attack_type, &wk->wu.attack_art_type);
                }

                if (g_state.My_char[wk->wu.id] == 10 || g_state.My_char[wk->wu.id] == 16 ||
                    g_state.My_char[wk->wu.id] == 18) {
                    wk->wu.attack_type |= 32;
                    wk->wu.attack_art_type = 128;
                }

                if ((g_state.My_char[wk->wu.id] == 9) && (wk->sa->kind_of_arts == 2)) {
                    wk->wu.att.dipsw |= 0x10;
                }

                break;
            }

            break;

        case 3:
            switch (wk->sa->super_art_routine_no_2) {
            case 0:
                switch (wk->sa->super_effect_can_activate) {
                case -1:
                    sag_bug_fix(wk->wu.id);
                    wk->sa->stock--;
                    wk->sa->super_effect_can_activate = 0;
                    wk->sa->super_art_routine_no_2 = 1;
                    break;

                case 1:
                    break;

                default:
                    wk->sa->super_effect_can_activate = 0;
                    wk->sa->super_art_routine_no = 0;
                    wk->sa->can_activate = 0;
                }

                break;

            default:
                break;
            }

            break;

        default:
            wk->sa->super_art_routine_no = 0;
            wk->sa->can_activate = 0;
            wk->sa->stock = 0;
            wk->sa->super_effect_can_activate = 0;
            break;
        }

        break;
    }
}
#endif

static void sag_union(PlayerEntity* wk) {
#if CPS3
    sag_union_jump_table[wk->sa->gauge_type](wk);
#else
    sag_union_ps2(wk);
#endif
}

#if !CPS3
/** @brief Adds SA attribute flags to the current attack's kind-of-move. */
static void addSAAttribute(u8* move_type, u16* koa) {
    switch (*move_type & 0x78) {
    case 0:
    case 8:
        *move_type = 0x20;
        *koa = 0x80;
        break;

    case 16:
    case 24:
        *move_type = 0x28;
        *koa = 0x100;
        break;
    }
}
#endif

/** @brief Force-fills the SA gauge to max during demo playback. */
void demo_set_sa_full(SuperArtGauge* sa) {
    sa->super_art_routine_no = 1;
    sa->can_activate = 1;
    sa->stock = sa->stock_max;
    sa->super_art_id++;

#if CPS3
    if (sa->gauge_type == 1) {
        sa->gauge.s.h = sa->gauge_length;
        sa->damage_time_multiplier = 1;
    }
#else
    sa->gauge.s.h = 0;
    sa->gauge.s.l = 0;
    sa->damage_time_multiplier = 1;
#endif
}

/** @brief Records recent movement amount for gameplay calculations. */
void get_recent_movement_delta(PlayerEntity* wk) {
    s16 i;

    for (i = 0; i < 7; i++) {
        wk->old_pos_data[i] = wk->old_pos_data[i + 1];
    }

    wk->old_pos_data[i] = wk->wu.xyz[0].disp.pos;
    wk->move_distance = wk->old_pos_data[7] - wk->old_pos_data[0];
    wk->move_power = cal_move_quantity2(wk->old_pos_data[0], 0, wk->old_pos_data[7], 0);
    wk->move_power >>= 3;
}

/** @brief Clears the attack number tracking in a State item. */
void clear_attack_num(State* wk) {
    s16 i;

    for (i = 0; i < 4; i++) {
        wk->received_attack[i] = 0;
    }

    wk->attack_num = 0;
}

/** @brief Clears throw/grab-related tracking flags for a player. */
void clear_tk_flags(PlayerEntity* wk) {
    wk->target_combo_success = 0;
    wk->strike_scaling = 0;
    wk->throw_scaling = 0;
    wk->stun_scaling = 0;
    wk->guts_scaling = 0;
    wk->att_plus = 8;
    wk->def_plus = 8;
}

void (*const plmain_lv_00[5])(PlayerEntity* wk) = {
    player_mv_0000, player_mv_1000, player_mv_2000, player_mv_3000, player_mv_4000
};

void (*const plmain_lv_02[5])(PlayerEntity* wk) = {
    Player_normal, Player_damage, Player_catch, Player_caught, Player_attack
};

#if CPS3
void (*const sag_union_jump_table[4])(PlayerEntity* wk) = { sag_union_0, sag_union_1, sag_union_0, sag_union_3 };
#else

const u8 plpnm_mvkind[59] = { 0, 3, 3, 3, 3, 1, 1, 3, 3, 3, 3, 0, 0, 0, 0, 0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3,
                              3, 2, 2, 2, 2, 2, 3, 3, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

const u8 plpdm_mvkind[32] = { 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 };

const u8 plpxx_kind[5] = { 0, 1, 0, 1, 0 };

/** @brief Applies operator-mode vitality adjustments based on settings. */
static void check_omop_vital(PlayerEntity* wk) {
    if (g_state.pcon_dp_flag) {
        return;
    }

    if (wk->death_timerlag) {
        return;
    }

    if (sa_stop_check()) {
        return;
    }

    if (wk->resurrection_resv) {
        wk->wu.vital_new = -1;
        return;
    }

    switch (omop_vital_ix[wk->wu.id]) {
    case 0:
        if (g_state.vital_dec_timer) {
            break;
        }

        if ((wk->wu.routine_no[1] == 0) && !(plpnm_mvkind[wk->wu.routine_no[2]] & 1)) {
            break;
        }

        if ((wk->wu.routine_no[1] == 1) && !(plpdm_mvkind[wk->wu.routine_no[2]] & 1)) {
            break;
        }

        if (wk->wu.routine_no[1] == 3) {
            break;
        }

        if (wk->player_number == 0) {
            if ((wk->wu.routine_no[1] == 4) && (wk->wu.routine_no[2] == 21)) {
                if (ca_check_flag == 0) {
                    ca_check_flag = 1;
                }

                break;
            }

            if ((wk->wu.routine_no[1] == 4) && (wk->wu.routine_no[2] == 22) && (wk->wu.pat_status == 23)) {
                break;
            }
        }

        wk->wu.vital_new--;

        if (wk->wu.vital_new < 0) {
            wk->wu.vital_new = -1;
            wk->wu.damage_kind_of_arts = 4;
            wk->death_timerlag = 1;
            wk->guard_flag = 3;
            ca_check_flag = 0;
            break;
        }

        break;

    case 2:
        if (g_state.vital_inc_timer) {
            break;
        }

        if (wk->wu.routine_no[1] != 0) {
            break;
        }

        if (!(plpnm_mvkind[wk->wu.routine_no[2]] & 2)) {
            break;
        }

        wk->wu.vital_new++;

        if (wk->wu.vital_new > 160) {
            wk->wu.vital_new = 160;
            break;
        }

        break;

    case 3:
        if (plpxx_kind[wk->wu.routine_no[1]]) {
            break;
        }

        if (plpxx_kind[wk->wu.old_routine_no[1]]) {
            wk->emergency_vital_timer = 40;
        }

        if (wk->emergency_vital_timer) {
            wk->emergency_vital_timer--;
            break;
        }

        /* fallthrough */

    case 4:
        wk->wu.vital_new++;

        if (wk->wu.vital_new > 160) {
            wk->wu.vital_new = 160;
        }

        break;
    }
}
#endif
