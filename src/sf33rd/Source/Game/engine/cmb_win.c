/**
 * @file cmb_win.c
 * Manages in-game combo detection, scoring, and HUD display
 */

#include "sf33rd/Source/Game/engine/cmb_win.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/ui/sc_data.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

#include <SDL3/SDL.h>

#include <string.h>

// bss

// sbss

const u8 cmb_pos_tbl[2][21] = { { 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27 },
                                { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 } };

const u8 cmb_window_move_tbl[7] = { 12, 16, 20, 16, 12, 10, 12 };

const u8 cmb_window_time_tbl[7] = { 28, 40, 60, 60, 50, 50, 24 };

const u16 combo_score_tbl[12][2] = { { 300, 200 },   { 500, 400 },   { 1000, 600 },  { 1000, 600 },
                                     { 2000, 800 },  { 3000, 600 },  { 4000, 800 },  { 5000, 1000 },
                                     { 6000, 2000 }, { 8000, 2000 }, { 9000, 3000 }, { 10000, 4000 } };

/** @brief Initializes the combo window controller for both players. */
void combo_cont_init() {
    u8 i;

    for (i = 0; i < 2; i++) {
        old_cmb_flag[i] = 0;
        cmb_stock[i] = 0;
        rever_attack[i] = 0;
        paring_attack[i] = 0;
        bonus_pts[i] = 0;
        sarts_finish_flag[i] = 0;
        cmb_calc_now[i] = 0;
        cst_read[i] = 0;
        cst_write[i] = 0;
        SDL_zero(plw[i].combo_type);
        SDL_zero(plw[i].remake_power);
        SDL_zeroa(calc_hit[i]);
        SDL_zeroa(score_calc[i]);
    }

    first_attack = 0;
    hit_num = 0;
    sa_kind = 0;
    cmb_all_stock[0] = 0;
    last_hit_time = 0;
    SDL_zero(cmst_buff);
}

/** @brief Per-frame combo window update — checks and displays combos for both sides. */
void combo_cont_main() {
    s8 i;

    if (Stop_Combo) {
        if (Demo_Flag) {
            return;
        }
        combo_cont_init();

        if (Demo_Flag) {
            Stop_Combo = 0;
        }

        return;
    }

    if (Demo_Flag != 0) {
        if (Game_pause == 0) {
            if (Game_timer & 1) {
                for (i = 0; i < 2; i++) {
                    combo_control(i);
                    combo_window_trans(i);
                }
            } else {
                for (i = 1; i > -1; i--) {
                    combo_control(i);
                    combo_window_trans(i);
                }
            }
        } else {
            for (i = 0; i < 2; i++) {
                combo_window_trans(i);
            }
        }

        cmb_all_stock[0] = cmb_stock[0] + cmb_stock[1];
    }
}

/** @brief Controls a single player's combo detection and scoring state machine. */
void combo_control(s8 PL) {
    s16 cmb_flag;

    cmb_flag = check_combo_end(PL + 0);

    if (cmb_flag) {
        cmb_calc_now[PL] = 1;
    } else {
        cmb_calc_now[PL] = 0;
    }

    if (reversal_check(PL) == 0) {
        if (rever_attack[PL]) {
            reversal_continue_check(PL);
        }

        if (!paring_check(PL) && plw[PL].combo_type.total) {
            if (first_attack == 0) {
                first_attack = plw[PL].wu.id + 1;
                combo_window_push(PL, 4);
                return;
            }

            if (pcon_dp_flag && last_hit_time == 0) {
                super_arts_last_check(PL);
            }

            if (cmb_flag != 0) {
                return;
            } else if (plw[PL].combo_type.total == 1) {
                training_disp_data_set(PL, 1);
                super_arts_finish_check(PL);
                combo_hensuu_clear(PL);
                first_attack = 3;
                return;
            }

            check_and_set_combo(PL);
        }
    }
}

/** @brief Checks for a new combo hit and updates the combo chain counter. */
void check_and_set_combo(s8 PL) {
    s8 PLS;

    if (PL == 0) {
        PLS = 1;
    } else {
        PLS = 0;
    }

    hit_num = plw[PL].combo_type.total;

    if (hit_num > 99) {
        hit_num = 99;
    }

    training_disp_data_set(PL, hit_num);

    if (first_attack == 1 || first_attack == 2) {
        first_attack_pts_check(PL);
    }

    if (rever_attack[PLS] == 1) {
        reversal_attack_pts_check(PL);
    }

    hit_combo_check(PL);
    combo_hensuu_clear(PL);
}

/** @brief Clears all per-combo tracking variables for a player. */
void combo_hensuu_clear(s8 PL) {
    SDL_zero(plw[PL].combo_type);
    combo_rp_clear_check(PL);
    SDL_zeroa(calc_hit[PL]);
    SDL_zeroa(score_calc[PL]);
    bonus_pts[PL] = 0;
    plw[PL].combo_type.total = 0;
    hit_num = 0;
    tr_data[PL].total_damage = 0;
}

/** @brief Checks if the combo display timer has expired and clears. */
void combo_rp_clear_check(s8 PL) {
    if (plw[PL].wu.routine_no[1] != 1 || plw[PL].wu.routine_no[2] != 17 || plw[PL].wu.routine_no[3] == 0 ||
        plw[PL].wu.routine_no[3] == 3) {
        SDL_zero(plw[PL].remake_power);
    }
}

/** @brief Checks if a Super Art finish bonus should be awarded. */
void super_arts_finish_check(s8 PL) {
    if (arts_finish_check2(PL) != 0) {
        if ((plw[PL].combo_type.new_dm & 0x3F) < 48) {
            sa_kind = 2;
        } else {
            sa_kind = 3;
        }

        combo_window_push(PL, 3);
    }
}

/** @brief Checks if the last hit of a Super Art was the finishing blow. */
void super_arts_last_check(s8 PL) {
    if ((plw[PL].combo_type.new_dm & 0x3F) >= 0x20) {
        sarts_finish_flag[PL] = 1;
    } else {
        sarts_finish_flag[PL] = 0;
    }

    last_hit_time = 1;
}

/** @brief Awards first-attack bonus points if applicable. */
void first_attack_pts_check(s8 PL) {
    if (first_attack - 1 == plw[PL].wu.id) {
        first_attack = 3;
        bonus_pts[PL] += 2;
    }
}

/** @brief Checks if the current attack qualifies as a reversal. */
s32 reversal_check(s8 PL) {
    s8 PLS;

    if (rever_attack[PL]) {
        return 0;
    }

    if (plw[PL].wu.routine_no[1] == 4 && plw[PL].wu.old_rno[1] == 1 && !pcon_dp_flag &&
        plw[PL].wu.routine_no[2] >= 0x10) {
        rever_attack[PL] = 1;

        if (PL == 0) {
            PLS = (1);
        } else {
            PLS = 0;
        }

        combo_window_push(PLS, 5);
        grade_add_reversal(PL);
        return 1;
    }

    return 0;
}

/** @brief Continues tracking a reversal bonus across multiple hits. */
void reversal_continue_check(s8 PL) {
    if (plw[PL].wu.routine_no[1] != 4) {
        rever_attack[PL] = 0;
    } else {
        return;
    }
}

/** @brief Awards reversal attack bonus points. */
void reversal_attack_pts_check(s8 PL) {
    bonus_pts[PL]++;
}

/** @brief Checks if the current action qualifies as a parry for the combo window. */
s32 paring_check(s8 PL) {
    s8 PLS;

    if (paring_bonus_r[PL]) {
        paring_bonus_r[PL] = 0;
        paring_attack[PL] = 1;

        if (PL == 0) {
            PLS = 1;
        } else {
            PLS = 0;
        }

        combo_window_push(PLS, 6);
        return 1;
    }

    return 0;
}

/** @brief Processes the combo hit counter and determines combo display. */
void hit_combo_check(s8 PL) {
    s32* sa_ptr = (s32*)plw[PL].combo_type.kind_of[4][0];
    s8 lpx;

    for (lpx = 0; lpx < 20; lpx++) {
        if (!(*sa_ptr++ == 0)) {
            if (arts_finish_check(PL)) {
                if (lpx < 8) {
                    bonus_pts[PL] += 2;
                    sa_kind = 2;
                } else {
                    bonus_pts[PL] += 3;
                    sa_kind = 3;
                }

                combo_window_push(PL, 2);
                return;
            }

            combo_window_push(PL, 1);
            return;
        }
    }

    combo_window_push(PL, 0);
}

/** @brief Returns 1 if the current hit was an arts (special) finish blow. */
s32 arts_finish_check(s8 PL) {
    if (Conclusion_Flag && Conclusion_Type == 0 && Loser_id == PL && sarts_finish_flag[PL]) {
        return 1;
    }

    return 0;
}

/** @brief Returns 1 if arts_finish_check2 detects an arts finish (variant). */
s32 arts_finish_check2(u8 PL) {
    if (Conclusion_Flag && Conclusion_Type == 0 && Loser_id == PL && (plw[PL].combo_type.new_dm & 0x3F) >= 32) {
        return 1;
    }

    return 0;
}

/** @brief Calculates the total score for the current combo (base + bonuses). */
u32 SCORE_CALCULATION(s8 PL) {
    s16* c_ptr;
    s16* s_ptr;
    s16* k_ptr;
    s8 lpx;
    s8 lpy;
    s16 hit;
    s16 h;
    u32 score;
    s8 last;

    k_ptr = plw[PL].combo_type.kind_of[0][0];
    c_ptr = &calc_hit[PL][1];
    s_ptr = score_calc[PL];

    for (lpx = 0; lpx < 4; lpx++) {
        *s_ptr++ = k_ptr[0] + k_ptr[1];
        k_ptr += 2;
    }

    s_ptr = &score_calc[PL][4];

    for (lpy = 0; lpy < 8; lpy++) {
        *s_ptr++ = *c_ptr++;
    }

    hit = 0;
    score = 0;

    for (lpy = 0; lpy < 12; lpy++) {
        if (score_calc[PL][lpy]) {
            last = lpy;
            h = score_calc[PL][lpy];
            hit += h;
            score += *combo_score_tbl[lpy];

            if (h - 1) {
                score += (hit - 1) * combo_score_tbl[lpy][1];
            }
        }
    }

    if (bonus_pts[PL]) {
        score += bonus_pts[PL] * combo_score_tbl[last][1];
    }

    return score;
}

/** @brief Adds points to the player's running score with overflow protection. */
void SCORE_PLUS(s8 PL, u32 PTS) {
    Score[PL][2] += PTS;
    if (Score[PL][2] >= 99999900) {
        Score[PL][2] = 99999900;
    }

    Score[PL][Play_Type] += PTS;
    if (Score[PL][Play_Type] >= 99999900) {
        Score[PL][Play_Type] = 99999900;
    }
}

/** @brief Pushes combo data into the combo window HUD display queue. */
void combo_window_push(s8 PL, s8 KIND) {
    u32 score;
    s8 PLS;
    u32 Pts_Buff;
    s32 xx;
    s16 i;

    if (KIND < 3) {
        score = SCORE_CALCULATION(PL);
        grade_max_combo_check(PL ^ 1, hit_num);
    }

    if (PL == 0) {
        PLS = 1;
    } else {
        PLS = 0;
    }

    if (cmb_stock[PL] == 5) {
        switch (KIND) {
        case 2:
            if (sa_kind == 2) {
                score += 20000;
            } else {
                score += 30000;
            }
            break;

        case 3:
            if (sa_kind == 2) {
                score = 20000;
            } else {
                score = 30000;
            }
            break;

        case 4:
            score = 1500;
            grade_get_first_attack(PLS);
            break;

        case 6:
            score = paring_counter[PLS] * 100;
            break;
        }

        if (score >= 1000000) {
            score = 999900;
        }

        SCORE_PLUS(PLS, score);

        if (Mode_Type == MODE_VERSUS) {
            Score_Sub();
        } else if (plw[PLS].wu.pl_operator) {
            Score_Sub();
        }

        return;
    } else {
        CMST_BUFF* b = &cmst_buff[PL][cst_write[PL]];

        cmb_stock[PL]++;
        b->routine_num = 0;
        b->hit_hi = (u8)hit_num / 10;
        b->hit_low = (u8)hit_num - (b->hit_hi * 10);
        b->kind = KIND;

        if (Mode_Type == MODE_VERSUS) {
            b->pts_flag = 1;
        } else if (Mode_Type == 5) {
            b->pts_flag = 1;
        } else if (plw[PLS].wu.pl_operator) {
            b->pts_flag = 1;
        } else {
            b->pts_flag = 0;
        }

        switch (KIND) {
        case 0:
        case 1:
            break;

        case 2:
            if (sa_kind == 2) {
                score += 20000;
            } else {
                score += 30000;
            }
            break;

        case 3:
            if (sa_kind == 2) {
                score = 20000;
            } else {
                score = 30000;
            }
            break;

        case 4:
            score = 1500;
            grade_get_first_attack(PLS);
            break;

        case 5:
            score = 0;
            b->pts_flag = 0;
            break;

        case 6:
            score = paring_counter[PLS] * 100;
            break;
        }

        if (score >= 1000000) {
            score = 999900;
        }

        Pts_Buff = b->pts = score;
        i = 3;
        xx = 100000;

        b->first_digit = -1;

        while (i >= 0) {
            b->pts_digit[i] = Pts_Buff / xx & 0xFF;
            Pts_Buff -= (b->pts_digit[i] * xx);

            if (b->first_digit < 0 && b->pts_digit[i]) {
                b->first_digit = i;
            }

            i--;
            xx /= 10;
        }

        b->move[1] = b->first_digit + 6;

        if (cst_write[PL] == 4) {
            cst_write[PL] = 0;
        } else {
            cst_write[PL]++;
        }
    }
}

/** @brief Renders the combo message sprites for the current entry. */
static void render_combo_message(s8 PL, const CMST_BUFF* b) {
    combo_message_set(PL, b->kind,
                      cmb_pos_tbl[PL][b->x_posnum[0] - 1],
                      b->x_posnum[0], b->hit_hi, b->hit_low);
}

/** @brief Renders the combo points sprites at final position for the current entry. */
static void render_combo_pts(s8 PL, const CMST_BUFF* b) {
    combo_pts_set(PL,
                  cmb_pos_tbl[PL][b->x_posnum[1] - 1],
                  (b->move[1] - 1),
                  (s8*)&b->pts_digit[0], b->first_digit);
}

/** @brief Renders the combo window sprites — numbers, labels, and bonus text. */
void combo_window_trans(s8 PL) {
    s8 PLS;

    if (cmb_stock[PL] != 0) {
        CMST_BUFF* b = &cmst_buff[PL][cst_read[PL]];

        if (Mode_Type == MODE_NORMAL_TRAINING && Training_ID == PL) {
            cmb_stock[PL]--;
            return;
        }

        if (b->pts_flag) {
            switch (b->routine_num) {
            case 0:
                end_flag[PL] = 0;
                b->move[0] = cmb_window_move_tbl[(b->kind)];
                b->x_posnum[0] = 0;
                b->timer[0] = 8;
                b->x_posnum[1] = 0;
                b->routine_num++;
                break;

            case 1:
                if (b->x_posnum[0] != 0) {
                    render_combo_message(PL, b);
                }

                if (!(Game_pause & 0x80) && !(end_flag[PL] & 1)) {
                    if ((b->x_posnum[0]) < (b->move[0])) {
                        b->x_posnum[0]++;
                    } else {
                        end_flag[PL] |= 1;
                    }
                }

                if (!(end_flag[PL] & 2)) {
                    if (!(Game_pause & 0x80)) {
                        b->timer[0]--;
                    }

                    if (((b->timer[0])) < 0) {
                        if ((b->x_posnum[1]) < ((b->move[1]) + 2)) {
                            if ((b->x_posnum[1]) < (b->move[1])) {
                                if (b->x_posnum[1] != 0) {
                                    combo_pts_set(PL,
                                                  cmb_pos_tbl[PL][b->x_posnum[1] - 1],
                                                  b->x_posnum[1],
                                                  &b->pts_digit[0],
                                                  b->first_digit);
                                }
                            } else if (b->x_posnum[1] != 0) {
                                render_combo_pts(PL, b);
                            }

                            if (!(Game_pause & 0x80)) {
                                b->x_posnum[1]++;
                            }
                        } else {
                            end_flag[PL] |= 2;
                            render_combo_pts(PL, b);
                        }
                    }
                } else {
                    render_combo_pts(PL, b);
                }

                if (!(Game_pause & 0x80) && ((end_flag[PL] & 3) == 3)) {
                    b->routine_num++;
                    b->timer[1] = cmb_window_time_tbl[(b->kind)];

                    if (PL == 0) {
                        PLS = 1;
                    } else {
                        PLS = 0;
                    }

                    SCORE_PLUS(PLS, b->pts);

                    if (Mode_Type == MODE_VERSUS) {
                        Score_Sub();
                        return;
                    }

                    if (plw[PLS].wu.pl_operator) {
                        Score_Sub();
                        return;
                    }
                }

                break;

            case 2:
                if (!(Game_pause & 0x80)) {
                    b->timer[1]--;

                    if (b->timer[1]) {
                        render_combo_message(PL, b);
                        render_combo_pts(PL, b);
                        return;
                    }
                } else {
                    render_combo_message(PL, b);
                    render_combo_pts(PL, b);
                    return;
                }

                if (cst_read[PL] == 4) {
                    cst_read[PL] = 0;
                } else {
                    cst_read[PL]++;
                }

                cmb_stock[PL]--;
                break;
            }
        } else {
            switch ((b->routine_num)) {
            case 0:
                b->move[0] = cmb_window_move_tbl[(b->kind)];
                b->x_posnum[0] = 1;
                b->routine_num++;
                break;

            case 1:
                if (!(Game_pause & 0x80)) {
                    if ((b->x_posnum[0]) < (b->move[0])) {
                        b->x_posnum[0]++;
                    } else {
                        b->timer[1] = 36;
                        b->routine_num++;
                    }
                }

                render_combo_message(PL, b);
                break;

            case 2:
                if (!(Game_pause & 0x80)) {
                    b->timer[1]--;

                    if (b->timer[1]) {
                        render_combo_message(PL, b);
                        return;
                    }
                } else {
                    render_combo_message(PL, b);
                    return;
                }

                if (b->pts_flag) {
                    b->routine_num++;
                    return;
                }

                if (cst_read[PL] == 4) {
                    cst_read[PL] = 0;
                } else {
                    cst_read[PL]++;
                }

                cmb_stock[PL]--;
            }
        }
    }
}

/** @brief Sets up training-mode combo display data for a given player. */
void training_disp_data_set(s8 PL, s16 hnum) {
    if (tr_data[PL].max_hitcombo < (u8)hnum) {
        if (Disp_Attack_Data != 0) {
            if (Champion != PL) {
                SsRequest(0x9C);
            }

            tr_data[PL].new_max_flag = 0x14;
            tr_data[PL].frash_switch = 2;
        }

        tr_data[PL].frash_flag = 0;
        tr_data[PL].max_hitcombo = hnum;

        if (tr_data[PL].max_hitcombo > 99) {
            tr_data[PL].max_hitcombo = 99;
        }
    }
}
