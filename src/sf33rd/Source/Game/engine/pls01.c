/**
 * @file pls01.c
 * Player Utility and Common Mechanics Library
 */

#include "sf33rd/Source/Game/engine/pls01.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/caldir.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/sysdir.h"

#define CLAMP_MIN_ZERO(val)                                                                                            \
    do {                                                                                                               \
        if ((val) < 0)                                                                                                 \
            (val) = 0;                                                                                                 \
    } while (0)

/** @brief Sets routine numbers, clearing rno[1] and rno[3] to 0. */
static inline void set_routine(PLW* wk, u8 rno2) {
    wk->wu.routine_no[1] = 0;
    wk->wu.routine_no[2] = rno2;
    wk->wu.routine_no[3] = 0;
}

const u8 about_rno[6] = { 0, 1, 2, 1, 2, 0 };

const s16 sel_hd_fg_hos[20][2] = { { 0, 92 }, { 24, 76 }, { 8, 76 },   { 20, 64 }, { 0, 84 }, { 4, 80 }, { 8, 88 },
                                   { 4, 68 }, { 0, 72 },  { -16, 64 }, { 20, 64 }, { 8, 76 }, { 8, 76 }, { 0, 92 },
                                   { 8, 76 }, { 0, 76 },  { 14, 58 },  { 0, 104 }, { 4, 80 }, { 4, 87 } };

const s16 dir32_rl_conv[32] = { 0,  31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17,
                                16, 15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1 };

const s16 dir32_sel_tbl[2][32] = {
    { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 }
};

const s16 chcgp_hos[20] = { 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1 };

/** @brief Checks if Super Art stop is currently active. */
s32 sa_stop_check() {
    if (g_state.plw[0].sa_stop_flag != 0) {
        return 1;
    }

    if (g_state.plw[1].sa_stop_flag != 0) {
        return 1;
    }

    return 0;
}

/** @brief Deactivates the player's own throw power-up flag. */
void check_my_tk_power_off(PLW* wk, PLW* /* unused */) {
    if (wk->wu.old_routine_no[1] == 1) {
        if (wk->wu.old_routine_no[2] < 8 && wk->wu.old_routine_no[2] > 3) {
            return;
        }

        wk->strike_scaling = 0;
        wk->throw_scaling = 0;
        wk->stun_scaling = 0;
        return;
    }

    if (wk->wu.old_routine_no[1] == 3 && wk->wu.routine_no[1] == 0 && wk->wu.routine_no[2] < 51) {
        if (wk->wu.routine_no[2] > 46) {
            // do nothing
        }
    }
}

/** @brief Deactivates the enemy's throw power-up flag. */
void check_em_tk_power_off(PLW* wk, PLW* tk) {
    if (about_rno[wk->wu.old_routine_no[1]] != 1) {
        return;
    }

    tk->strike_scaling -= wk->received_strike_scaling;
    tk->throw_scaling -= wk->received_throw_scaling;
    tk->stun_scaling -= wk->received_stun_scaling;
    wk->received_strike_scaling = wk->received_throw_scaling = wk->received_stun_scaling = 0;

    CLAMP_MIN_ZERO(tk->strike_scaling);
    CLAMP_MIN_ZERO(tk->throw_scaling);
    CLAMP_MIN_ZERO(tk->stun_scaling);
}

/** @brief Returns the ukemi (tech-roll) flag for the player. */
s16 check_ukemi_flag(PLW* wk) {
    return wk->cp->move_state_flags[7];
}

/** @brief Returns the left/right facing flag. */
s32 check_rl_flag(WORK* wk) {
    return wk->rl_flag == wk->active_move;
}

/** @brief Sets the left/right facing for the current move. */
void set_rl_move(PLW* wk) {
    WORK* em;
    s16 result;

    while (1) {
        if (g_state.Bonus_Game_Flag == 20) {
            if (wk->wu.pl_operator != 0) {
                if (wk->wu.xyz[0].disp.pos < g_state.bonus_stage2_offset[0] ||
                    wk->wu.xyz[0].disp.pos > g_state.bonus_stage2_offset[1]) {
                    break;
                }

                if (((result = wk->cp->input_held & 0xF) != 0) && !(result & 3)) {
                    wk->wu.active_move = (result & 8) != 0;
                    return;
                }
            }

            wk->wu.active_move = wk->wu.rl_flag;
            return;
        }

        break;
    }

    em = (WORK*)wk->wu.target_adrs;
    result = wk->wu.xyz[0].disp.pos - em->xyz[0].disp.pos;

    if (result) {
        if (result > 0) {
            wk->wu.active_move = 0;
            return;
        }

        wk->wu.active_move = 1;
        return;
    }

    wk->wu.active_move = (em->active_move + 1) & 1;
}

/** @brief Checks if the player is on top of the bonus-stage car. */
s16 check_rl_on_car(PLW* wk) {
    s16 rnum;

    if (g_state.Bonus_Game_Flag != 20) {
        return 0;
    }

    if (wk->wu.pl_operator == 0) {
        return 0;
    }

    if (g_state.bs2_floor[2] == 0) {
        return 0;
    }

    rnum = 0;
    wk->bs2_area_car = 0;
    wk->bs2_over_car = 0;

    if (wk->wu.xyz[0].disp.pos >= g_state.bs2_floor[0] && !(wk->wu.xyz[0].disp.pos > g_state.bs2_floor[1])) {
        wk->bs2_area_car = 1;
    }

    if (wk->wu.xyz[0].disp.pos >= g_state.bonus_stage2_offset[0] &&
        !(wk->wu.xyz[0].disp.pos > g_state.bonus_stage2_offset[1])) {
        rnum = 1;
    }

    if (wk->wu.xyz[1].disp.pos + (wk->wu.cg_jphos) >= g_state.bs2_floor[2]) {
        wk->bs2_over_car = 1;
    }

    return rnum;
}

/** @brief Returns latest bonus-stage car area check result. */
s32 latest_bs2_area_car(PLW* wk) {
    wk->bs2_area_car2 = 0;
    wk->bs2_over_car2 = 0;

    if (g_state.pcon_dp_flag) {
        return 1;
    }

    if (wk->wu.xyz[0].disp.pos >= g_state.bs2_floor[0] && !(wk->wu.xyz[0].disp.pos > g_state.bs2_floor[1])) {
        wk->bs2_area_car2 = 1;
    }

    if (!(wk->wu.xyz[1].disp.pos + wk->wu.cg_jphos <= g_state.bs2_floor[2])) {
        wk->bs2_over_car2 = 1;
    }

    if (wk->bs2_over_car2) {
        return 1;
    }

    if (wk->bs2_area_car2 == 0) {
        return 1;
    }

    if (wk->wu.mvxy.a[1].sp >= 2) {
        return 1;
    }

    return 0;
}

/** @brief Returns whether the player is standing on the car in bonus stage 2. */
s8 latest_bs2_on_car(PLW* wk) {
    if (wk->bs2_on_car && (wk->wu.xyz[1].disp.pos > (g_state.bs2_floor[2] + 2))) {
        wk->bs2_on_car = 0;
    }

    return wk->bs2_on_car;
}

/** @brief Checks if the player can perform an air jump (double-jump). */
s32 check_air_jump(PLW* wk) {
    if (wk->spmv_ng_flag & DIP_AIR_JUMP_DISABLED) {
        return 0;
    }

    if (wk->extra_jump) {
        return 0;
    }
    if (wk->air_jump_ok_time) {
        return 0;
    }

    if (wk->wu.pat_status < 20 || wk->wu.pat_status > 30) {
        return 0;
    }

    if (wk->wu.position_y < 48) {
        return 0;
    }

    if (!(wk->cp->input_current & 1)) {
        return 0;
    }

    set_routine(wk, 53);
    wk->jump_direction = 0;
    grade_add_command_move(wk->wu.id);
    return 1;
}

/** @brief Checks if the player can perform a wall-kick (triangle jump). */
s32 check_sankaku_tobi(PLW* wk) {
    if (wk->spmv_ng_flag & DIP_WALL_JUMP_DISABLED) {
        return 0;
    }

    if (wk->extra_jump) {
        return 0;
    }

    if ((wk->wu.pat_status != 20) && (wk->wu.pat_status != 24) && (wk->wu.pat_status != 26) &&
        (wk->wu.pat_status != 30)) {
        return 0;
    }

    if (wk->corner_stuck_timer == 8 || wk->corner_stuck_timer == 0) {
        return 0;
    }

    if (!(wk->close_proximity_flag & wk->cp->input_held >> 2)) {
        return 0;
    }

    set_routine(wk, 52);
    wk->jump_direction = 0;
    grade_add_command_move(wk->wu.id);
    return 1;
}

/** @brief Manages the extra-jump timer and clears the flag when expired. */
void check_extra_jump_timer(PLW* wk) {
    if (wk->air_jump_ok_time) {
        wk->air_jump_ok_time--;
    }

    if (wk->wu.xyz[1].disp.pos > 48 && wk->close_proximity_flag) {
        if (wk->wu.routine_no[1] == 1) {
            wk->corner_stuck_timer = 0;
        }

        wk->corner_stuck_timer++;

        if (wk->corner_stuck_timer > 8) {
            wk->corner_stuck_timer = 8;
        }

        return;
    }

    wk->corner_stuck_timer = 0;
}

/** @brief Rebuilds movement X/Y speeds after a wall-kick. */
void remake_sankaku_tobi_mvxy(WORK* wk, u8 kabe) {
    if (kabe == 1) {
        wk->rl_flag = 0;
    }

    if (kabe == 2) {
        wk->rl_flag = 1;
    }

    if (kabe == 0) {
        if (wk->position_x > get_center_position()) {
            wk->rl_flag = 0;
        } else {
            wk->rl_flag = 1;
        }
    }

    if (wk->mvxy.a[0].sp < 0) {
        wk->mvxy.a[0].sp = -wk->mvxy.a[0].sp;
        wk->mvxy.d[0].sp = -wk->mvxy.d[0].sp;
    }

    if (wk->mvxy.a[1].real.h <= 0) {
        wk->mvxy.a[1].real.h = 4;
        wk->mvxy.a[0].real.h = (wk->mvxy.a[0].real.h * 5 / 4);

    } else {
        wk->mvxy.a[1].real.h = ((wk->mvxy.a[1].real.h << 2) / 3);
        wk->mvxy.a[0].real.h = (wk->mvxy.a[0].real.h * 5 / 4);
        wk->mvxy.a[1].real.h += 2;
    }

    if (wk->mvxy.a[1].real.h < 4) {
        wk->mvxy.a[1].real.h = 4;
    }

    wk->mvxy.d[1].sp = -0x8800;
}

/** @brief Checks if forward or backward dash input was detected. */
s16 check_F_R_dash(PLW* wk) {
    s16 num;
    s16 rnum;

    if (g_state.Bonus_Game_Flag != 20 || !wk->bs2_on_car) {
        if (wk->wu.xyz[1].disp.pos > 0) {
            return 0;
        }
    }

    num = (wk->cp->move_state_flags[0] != 0) + (wk->cp->move_state_flags[1] != 0) * 2;
    rnum = 0;

    while (1) {
        switch (num) {
        case 1:
            if (!(wk->spmv_ng_flag & DIP_FORWARD_DASH_DISABLED)) {
                set_routine(wk, 5);
                rnum = 1;
            }

            break;

        case 2:
            if (!(wk->spmv_ng_flag & 8)) {
                set_routine(wk, 6);
                rnum = 1;
            }

            break;

        case 3:
            if (wk->cp->lever_dir < 2) {
                num = 1;
                continue;
            } else {
                num = 2;
                continue;
            }

            break;
        }

        break;
    }

    if (rnum) {
        grade_add_command_move(wk->wu.id);
    }

    return rnum;
}

/** @brief Checks if the player has jump-ready input (up direction). */
s32 check_jump_ready(PLW* wk) {
    if (!(wk->cp->input_pressed & 1)) {
        return 0;
    }

    if (!(wk->spmv_ng_flag & DIP_HIGH_JUMP_DISABLED) && wk->cp->move_state_flags[2] != 0) {
        set_routine(wk, 17);
        grade_add_command_move(wk->wu.id);
    } else {
        if (wk->spmv_ng_flag & DIP_JUMP_DISABLED) {
            return 0;
        }

        set_routine(wk, 16);
    }

    wk->jump_direction = 0;
    return 1;
}

/** @brief Checks if high-jump only (up+button) input was entered. */
s32 check_hijump_only(PLW* wk) {
    if (wk->spmv_ng_flag & DIP_HIGH_JUMP_DISABLED) {
        return 0;
    }

    if (!(wk->cp->input_pressed & 1)) {
        return 0;
    }

    if (wk->cp->move_state_flags[2] == 0) {
        return 0;
    }

    if (wk->wu.xyz[1].disp.pos > 0) {
        return 0;
    }

    set_routine(wk, 17);
    wk->jump_direction = 0;
    grade_add_command_move(wk->wu.id);
    return 1;
}

/** @brief Checks if the player should bend/crouch from standing. */
s32 check_bend_myself(PLW* wk) {
    if (!(wk->cp->input_pressed & 2)) {
        return 0;
    }

    set_routine(wk, 8);
    return 1;
}

/** @brief Checks if forward or backward walk input is held. */
s16 check_F_R_walk(PLW* wk) {
    s16 rnum = 0;

    switch (wk->cp->lever_dir) {
    case 1:
        set_routine(wk, 3);
        rnum = 1;
        break;

    case 2:
        set_routine(wk, 4);
        rnum = 1;
        break;
    }

    return rnum;
}

/** @brief Checks if the player has turned to face backwards. */
s32 check_turn_to_back(PLW* wk) {
    if (wk->cannot_turn_flag) {
        return 0;
    }

    if (g_state.Bonus_Game_Flag == 20) {
        if (check_rl_flag(&wk->wu)) {
            return 0;
        }
    } else if (check_hurimuki(&wk->wu)) {
        return 0;
    }

    if (wk->cp->input_held & 2) {
        set_routine(wk, 10);
    } else {
        set_routine(wk, 2);
    }

    wk->wu.cg_type = 0;
    wk->cannot_turn_flag = 1;
    return 1;
}

/** @brief Checks if the player needs to turn around (hurimuki). */
s32 check_hurimuki(WORK* wk) {
    WORK* em = (WORK*)wk->target_adrs;
    s16 result = wk->xyz[0].disp.pos - em->old_pos[0];

    if (result) {
        if (result > 0) {
            return wk->rl_flag == 0;
        }
        return wk->rl_flag;
    }

    return 1;
}

/** @brief Returns the walking lever direction relative to the current facing. */
s16 check_walking_lv_dir(PLW* wk) {
    s16 rnum = 0;

    switch (wk->cp->lever_dir) {
    case 1:
        if (wk->wu.routine_no[2] != 3) {
            rnum = 1;
        }

        break;

    case 2:
        if (wk->wu.routine_no[2] != 4) {
            rnum = 1;
        }

        break;

    default:
        rnum = 1;
        break;
    }

    if (rnum) {
        if (wk->wu.pat_status < 32) {
            set_routine(wk, 1);
        } else {
            set_routine(wk, 9);
        }
    }

    return rnum;
}

/** @brief Checks if the player should stand up from crouching. */
s32 check_stand_up(PLW* wk) {
    if (wk->cp->input_pressed & 2) {
        return 0;
    }

    set_routine(wk, 7);
    return 1;
}

/** @brief Checks if the player is holding a defensive lever direction. */
s32 check_defense_lever(PLW* wk) {
#if !CPS3
    if (wk->spmv_ng_flag & DIP_GUARD_DISABLED) {
        return 0;
    }
#endif

    if (!check_em_catt(wk)) {
        return 0;
    }

    if (wk->cp->input_pressed & 2) {
        set_routine(wk, 29);
    } else if (check_attbox_dir(wk)) {
        set_routine(wk, 28);
    } else {
        set_routine(wk, 27);
    }
    return 1;
}

/** @brief Checks if the enemy is attempting a catch/grab. */
s32 check_em_catt(PLW* wk) {
    PLW* em = (PLW*)wk->wu.target_adrs;
    s16 xd;
    s8 rlf;

    if (em->caution_flag == 0) {
        return 0;
    }

    if ((rlf = (wk->wu.rl_flag + em->wu.rl_flag) & 1) == 0) {
        return 0;
    }

    if (wk->cp->lever_dir != 2 || wk->cp->input_pressed & 1) {
        return 0;
    }

    xd = wk->wu.xyz[0].disp.pos - em->wu.xyz[0].disp.pos;

    if (xd < 0) {
        xd = -xd;
    }

    if (xd > guard_distance[omop_guard_distance_ix[wk->wu.id]]) {
        return 0;
    }

    return 1;
}

/** @brief Returns the attack box direction relative to the opponent. */
s16 check_attbox_dir(PLW* wk) {
    s16 target_pos_x;
    s16 target_pos_y;
    s16 emdir;
    s16* dttbl;

    get_target_att_position((WORK*)wk->wu.target_adrs, &target_pos_x, &target_pos_y);
    dttbl = (s16*)sel_hd_fg_hos[wk->player_number];

    if (wk->wu.rl_flag) {
        emdir = caldir_pos_032(
            wk->wu.xyz[0].disp.pos - dttbl[0], wk->wu.xyz[1].disp.pos + dttbl[1], target_pos_x, target_pos_y);
    } else {
        emdir = caldir_pos_032(
            wk->wu.xyz[0].disp.pos + dttbl[0], wk->wu.xyz[1].disp.pos + dttbl[1], target_pos_x, target_pos_y);
        emdir = dir32_rl_conv[emdir];
    }

    if ((wk->wu.now_koc == 0) && ((wk->wu.char_index) == 29)) {
        emdir = dir32_sel_tbl[1][emdir];
    } else {
        emdir = dir32_sel_tbl[0][emdir];
    }

    return emdir;
}

/** @brief Determines the type of defense (high, low, crouch). */
u16 check_defense_kind(PLW* wk) {
    u16 rnum = 0;

    switch (wk->wu.routine_no[2]) {
    case 27:
        if (wk->cp->input_pressed & 2) {
            rnum = 3;
        } else if (chcgp_hos[wk->player_number] && check_attbox_dir(wk)) {
            rnum = 2;
        }

        break;

    case 28:
        if (wk->cp->input_pressed & 2) {
            rnum = 3;
        } else if (chcgp_hos[wk->player_number] && (check_attbox_dir(wk) == 0)) {
            rnum = 1;
        }

        break;

    case 29:
        if (!(wk->cp->input_pressed & 2)) {
            if (check_attbox_dir(wk)) {
                rnum = 2;
            } else {
                rnum = 1;
            }
        }

        break;
    }

    if (rnum) {
        wk->wu.routine_no[2] = rnum + 26;
        set_char_move_init(&wk->wu, 0, rnum + 28);

        while (1) {
            if (wk->wu.cg_type == 1) {
                break;
            }

            char_move_z(&wk->wu);
        }
    }

    return rnum;
}

/** @brief Processes the unified jump arc including gravity and landing. */
void jumping_union_process(WORK* wk, s16 num) {
    add_mvxy_speed(wk);
    cal_mvxy_speed(wk);
    char_move(wk);

    if ((g_state.Bonus_Game_Flag == 20) && (wk->pl_operator != 0) && (latest_bs2_area_car((PLW*)wk) == 0)) {
        if (!(wk->xyz[1].disp.pos + wk->cg_jphos > g_state.bs2_floor[2])) {
            wk->position_y = wk->xyz[1].disp.pos = g_state.bs2_floor[2];
            wk->mvxy.a[1].sp = 0;
            wk->routine_no[3] = num;
            ((PLW*)wk)->bs2_on_car = 1;
            char_move_cmja(wk);
        }

        return;
    }

    if ((wk->xyz[1].disp.pos + wk->cg_jphos) <= 0) {
        wk->position_y = 0;
        wk->xyz[1].cal = 0;
        wk->mvxy.a[1].sp = 0;
        wk->routine_no[3] = num;
        char_move_cmja(wk);
    }
}

/** @brief Checks if the player is above the floor level. */
s32 check_floor(PLW* wk) {
    if (wk->bs2_on_car == 0) {
        return 0;
    }

    if (wk->bs2_area_car) {
        return 0;
    }

    return 1;
}

/** @brief Checks if the player's feet are below the ground (footwork check). */
s32 check_ashimoto(PLW* wk) {
    if (check_floor(wk) == 0) {
        return 0;
    }

    set_routine(wk, 54);
    wk->jump_direction = 0;
    return 1;
}

/** @brief Extended floor check with landing height threshold. */
s32 check_floor_2(PLW* wk) {
    if (check_floor(wk) == 0) {
        return 0;
    }

    WORK* efw = (WORK*)((WORK*)wk->wu.target_adrs)->my_effadrs;

    if (hit_check_x_only(&wk->wu, efw, &wk->wu.adjust_adrs->hos_box[4], &efw->pushbox->hos_box[0]) != 0) {
        return 0;
    }

    return 1;
}

/** @brief Extended footwork check with height threshold. */
s32 check_ashimoto_ex(PLW* wk) {
    if (check_floor_2(wk) == 0) {
        return 0;
    }

    set_routine(wk, 55);
    return 1;
}
