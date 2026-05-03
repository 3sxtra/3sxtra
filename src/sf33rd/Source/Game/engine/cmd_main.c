/**
 * @file cmd_main.c
 * Command Input Parser
 */

#include "sf33rd/Source/Game/engine/cmd_main.h"
#include "game_state.h"
#include "arcade/arcade_balance.h"
#include "arcade/arcade_cmd_data.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/cmd_constants.h"
#include "sf33rd/Source/Game/engine/cmd_data.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_common_mechanics.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/system/sysdir.h"

#include "port/I_System.h"

#define CHK_MOVE_COUNT 28

void (*chk_move_jp[28])() = { check_init, check_0,  check_1,  check_2,  check_3,  check_4,  check_5,
                              check_6,    check_7,  check_7,  check_9,  check_10, check_11, check_12,
                              check_13,   check_14, check_15, check_16, check_16, check_18, check_19,
                              check_20,   check_21, check_22, check_23, check_24, check_25, check_26 };

/** @brief Main move/command check dispatcher — scans for special move inputs. */
void move_check(PLW* pl) {
    cmd_pl = pl;
    cmd_id = cmd_pl->wu.id;
    chk_pl = &g_state.t_pl_lvr[cmd_id];
    sw_pick_up();
    cmd_move();
}

/** @brief Passes lever input through without processing (used for disabled states). */
void key_thru(PLW* pl) {
    cmd_pl = pl;
    cmd_id = cmd_pl->wu.id;
    chk_pl = &g_state.t_pl_lvr[cmd_id];
    sw_pick_up();
}

/** @brief Initializes command sequence data tables from the move definition set. */
void cmd_data_set(PLW* /* unused */, s16 i) {
    u8* ptr3;
    u16* ptr4;

    g_state.wcp[cmd_id].reset[i] = *cmd_tbl_ptr++;
    g_state.move_work[cmd_id][i].w_dead = *cmd_tbl_ptr++;
    g_state.move_work[cmd_id][i].w_dead2 = *cmd_tbl_ptr++;

    ptr3 = &g_state.wcp[cmd_id].move_state_timers[i][0];
    *ptr3++ = (s8)*cmd_tbl_ptr++;
    *ptr3++ = (s8)*cmd_tbl_ptr++;
    *ptr3++ = (s8)*cmd_tbl_ptr++;
    *ptr3++ = (s8)*cmd_tbl_ptr++;

    g_state.wcp[cmd_id].btix[i] = *cmd_tbl_ptr++;

    ptr4 = &g_state.wcp[cmd_id].exdt[i][0];
    *ptr4++ = *cmd_tbl_ptr++;
    *ptr4++ = *cmd_tbl_ptr++;
    *ptr4++ = *cmd_tbl_ptr++;
    *ptr4++ = *cmd_tbl_ptr++;

    switch (i) {
    case 3:
    case 4:
    case 5:
        g_state.wcp[cmd_id].reset[i] += blok_b_omake[omop_b_block_ix[cmd_id]];
        make_red_blocking_time(cmd_id, i, g_state.wcp[cmd_id].reset[i]);
        break;

    case 6:
    case 12:
        g_state.wcp[cmd_id].reset[i] += blok_b_omake[omop_b_block_ix[cmd_id]];
        break;
    }
}

/** @brief Initializes the command input state machine for a player. */
void cmd_init(PLW* pl) {
    cmd_id = pl->wu.id;
    pl->cp = &g_state.wcp[cmd_id];

    I_ZeroArray(g_state.move_work[cmd_id]);

    // ⚡ Bolt: bulk memset replaces nested per-element zeroing loops
    memset(g_state.wcp[cmd_id].move_state_flags, 0, sizeof(g_state.wcp[cmd_id].move_state_flags));
    memset(g_state.wcp[cmd_id].move_state_timers, 0, sizeof(g_state.wcp[cmd_id].move_state_timers));

    move_compel_all_init(pl);
}

static const void* get_commands(s16 char_num) {
    if (ArcadeBalance_IsEnabled()) {
        return ArcadeCommandData_Get(char_num);
    } else if (g_state.cmd_sel[cmd_id]) {
        return pl_CMD[char_num];
    } else {
        return pl_cmd[char_num];
    }
}

/** @brief Advances all active command checks by one frame. */
void cmd_move() {
    s16 j;
    intptr_t* adrs;

    cmd_id = cmd_pl->wu.id;
    adrs = get_commands(cmd_pl->player_number);

    for (j = 0; j < 56; j++) {
        if (g_state.wcp[cmd_id].move_state_flags[j] != -1) {
            move_type[cmd_id] = j;
            cmd_tbl_ptr = (s16*)adrs[j];
            move_ptr = &g_state.move_work[cmd_id][j];
            if (move_ptr->w_type < CHK_MOVE_COUNT)
                chk_move_jp[move_ptr->w_type]();
        }
    }

    for (j = 0; j < 56; j++) {
        if ((g_state.wcp[cmd_id].move_state_flags[j] != -1) && (g_state.wcp[cmd_id].move_state_flags[j] != 0)) {
            move_ptr = &g_state.move_work[cmd_id][j];
            Command_Ok_Move(j);
        }
    }
}

/** @brief Initializes a command check for the current motion. */
void check_init() {
    cmd_tbl_ptr += 12;
    move_ptr->w_type = *cmd_tbl_ptr++;
    move_ptr->w_int = *cmd_tbl_ptr++;
    move_ptr->free1 = *cmd_tbl_ptr;
    move_ptr->free2 = *cmd_tbl_ptr++;
    move_ptr->w_lvr = *cmd_tbl_ptr++;
    move_ptr->w_ptr = cmd_tbl_ptr;
    move_ptr->uni0.tame.flag = 0;
    move_ptr->uni0.tame.shot_flag = 0;
    move_ptr->uni0.tame.shot_flag2 = 0;
    move_ptr->shot_ok = 0;
    move_ptr->free3 = 0;
    if (move_ptr->w_type < CHK_MOVE_COUNT)
        chk_move_jp[move_ptr->w_type]();
}

/** @brief Advances to the next step of the current command sequence. */
void check_next() {
    s16* next_ptr = move_ptr->w_ptr;

    move_ptr->w_type = *next_ptr++;
    move_ptr->w_int = *next_ptr++;
    move_ptr->free1 = *next_ptr;
    move_ptr->free2 = *next_ptr++;
    move_ptr->w_lvr = *next_ptr++;
    move_ptr->w_ptr = next_ptr;

    if (move_ptr->w_type != 10) {
        if (move_ptr->w_type < CHK_MOVE_COUNT)
            chk_move_jp[move_ptr->w_type]();
    }
}

/**
 * @brief Common lever-direction dispatch: exact (0x8000) / neutral / bitmask.
 *
 * Tests the current lever value against move_ptr->w_lvr using the standard
 * three-branch pattern.  On match, advances to command_ok() or check_next().
 *
 * @param sw_lever  Masked lever value to test (caller chooses the source).
 * @param guard     Extra bitmask guard for the else-branch (0xF to skip).
 */
static void lvr_match_or_next(u16 sw_lever, u16 guard) {
    if (move_ptr->w_lvr & CMD_FLIP_X) {
        sw_work = move_ptr->w_lvr & CMD_LEVER_MASK;

        if (sw_lever == sw_work) {
            if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                command_ok();
                return;
            }

            check_next();
        }
    } else if (move_ptr->w_lvr == 0) {
        if (sw_lever == 0) {
            if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                command_ok();
                return;
            }

            check_next();
        }
    } else if (guard && (sw_lever & move_ptr->w_lvr)) {
        if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
            command_ok();
            return;
        }

        check_next();
    }
}

/** @brief Check type 0: Directional input (single joystick direction match). */
void check_0() {
    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->w_type = 0;
    }

    if (dead_lvr_check() == 0)
        lvr_match_or_next(chk_pl->sw_lever & CMD_LEVER_MASK, chk_pl->now_lvbt & CMD_LEVER_MASK);
}

/** @brief Check type 1: Directional input with held requirement. */
void check_1() {
    if (dead_lvr_check() == 0) {
        sw_work = move_ptr->w_lvr & CMD_LEVER_MASK;
        if (move_ptr->w_lvr & CMD_FLIP_X) {
            if (sw_work == chk_pl->sw_lever) {
                move_ptr->free2--;

                if (!move_ptr->uni0.tame.flag && move_ptr->free2 < 0) {
                    move_ptr->uni0.tame.flag = 1;
                }
            } else {
                if (move_ptr->uni0.tame.flag) {
                    move_ptr->uni0.tame.flag = 0;

                    if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                        command_ok();
                    } else {
                        check_next();
                    }
                    return;
                }

                move_ptr->free2 = move_ptr->free1;
                move_ptr->w_int--;

                if (move_ptr->w_int < 0) {
                    move_ptr->w_type = 0;
                }
            }
        } else {
            if (sw_work & chk_pl->sw_lever) {
                if (!move_ptr->uni0.tame.flag) {
                    move_ptr->free1--;

                    if (move_ptr->free1 < 0) {
                        move_ptr->uni0.tame.flag = 1;
                    }
                }
            } else {
                if (move_ptr->uni0.tame.flag) {
                    move_ptr->uni0.tame.flag = 0;

                    if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                        command_ok();
                    } else {
                        check_next();
                    }
                    return;
                }

                move_ptr->free2 = move_ptr->free1;
                move_ptr->w_int--;

                if (move_ptr->w_int < 0) {
                    move_ptr->w_type = 0;
                }
            }
        }
    }
}

/** @brief Check type 2: Button press check (punch/kick). */
void check_2() {
    sw_work = chk_pl->input_pressed & move_ptr->w_lvr;

    if (move_ptr->w_lvr == sw_work) {
        if (!move_ptr->uni0.tame.flag) {
            move_ptr->free2--;

            if (move_ptr->free2 < 0) {
                move_ptr->uni0.tame.flag = 1;
            }
        }
    } else {
        if (move_ptr->uni0.tame.flag && sw_work == 0) {
            move_ptr->uni0.tame.flag = 0;

            if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                command_ok();
            } else {
                check_next();
            }

            return;
        }

        move_ptr->free2 = move_ptr->free1;
        move_ptr->w_int--;

        if (move_ptr->w_int < 0) {
            move_ptr->w_type = 0;
        }
    }
}

/** @brief Check type 3: Button press with extra conditions (SA gauge, etc.). */
void check_3() {
    s16 i;
    s16 w_flag;
    s16* shot_cnt_adrs;

    sw_work = chk_pl->input_pressed & CMD_BTN_ATTACKS;
    move_ptr->uni0.tame.shot_flag2 = move_ptr->uni0.tame.shot_flag;
    move_ptr->uni0.tame.shot_flag = 0;
    shot_cnt_adrs = &chk_pl->s1_cnt;
    w_flag = CMD_BTN_LP;

    for (i = 0; i < 6; i++) {
        if (*shot_cnt_adrs >= move_ptr->w_int) {
            move_ptr->uni0.tame.shot_flag |= w_flag;
        }

        shot_cnt_adrs++;

        if ((chk_pl->shot_down & w_flag) && (move_ptr->uni0.tame.shot_flag2 & w_flag)) {
            move_ptr->shot_ok++;
        }

        w_flag <<= 1;
    }

    if (move_ptr->shot_ok) {
        move_ptr->free2--;

        if (move_ptr->free2 < 0) {
            move_ptr->shot_ok = 0;
            move_ptr->free2 = move_ptr->free1;
        }
    }

    if (move_ptr->shot_ok >= move_ptr->w_lvr) {
        move_ptr->shot_ok = 0;
        move_ptr->free2 = move_ptr->free1;

        if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
            command_ok();
            return;
        }

        check_next();
    }
}

/** @brief Check type 4: Charge-motion direction check (hold-back-then-forward). */
void check_4() {
    if (move_ptr->w_lvr == CMD_BTN_LP) {
        if (chk_pl->input_current & CMD_BTN_LP) {
            move_ptr->uni0.tame.flag++;
        }

        if (chk_pl->input_current & CMD_BTN_MP) {
            move_ptr->uni0.tame.shot_flag++;
        }

        if (chk_pl->input_current & CMD_BTN_HP) {
            move_ptr->uni0.tame.shot_flag2++;
        }
    } else {
        if (chk_pl->input_current & CMD_BTN_LK) {
            move_ptr->uni0.tame.flag++;
        }

        if (chk_pl->input_current & CMD_BTN_MK) {
            move_ptr->uni0.tame.shot_flag++;
        }

        if (chk_pl->input_current & CMD_BTN_HK) {
            move_ptr->uni0.tame.shot_flag2++;
        }
    }

    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->uni0.tame.flag = 0;
        move_ptr->uni0.tame.shot_flag = 0;
        move_ptr->uni0.tame.shot_flag2 = 0;
        move_ptr->w_int = move_ptr->free1;
    }

    if (g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]]) {
        if (move_ptr->w_int > 0 && move_ptr->uni0.tame.shot_flag2) {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
            move_ptr->uni0.tame.shot_flag2 = 0;
            move_ptr->w_int = 9;
            return;
        }
    } else if (move_ptr->uni0.tame.shot_flag2 >= 5) {
        g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
        move_ptr->uni0.tame.shot_flag2 = 0;
        move_ptr->w_int = 9;
        chk_pl->move_id = move_type[cmd_id];
        return;
    }

    if (g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]]) {
        if (move_ptr->w_int > 0 && move_ptr->uni0.tame.shot_flag) {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
            move_ptr->uni0.tame.shot_flag = 0;
            move_ptr->w_int = 12;
            return;
        }
    } else if (move_ptr->uni0.tame.shot_flag >= 5) {
        g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
        move_ptr->uni0.tame.shot_flag = 0;
        move_ptr->w_int = 12;
        chk_pl->move_id = move_type[cmd_id];
        return;
    }

    if (g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]]) {
        if (move_ptr->w_int > 0 && move_ptr->uni0.tame.flag) {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
            move_ptr->uni0.tame.flag = 0;
            move_ptr->w_int = 15;
        }
    } else if (move_ptr->uni0.tame.flag >= 5) {
        g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
        move_ptr->uni0.tame.flag = 0;
        move_ptr->w_int = 15;
        chk_pl->move_id = move_type[cmd_id];
    }
}

/** @brief Check type 5: Multi-button simultaneous press check. */
void check_5() {
    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->w_type = 0;
    }

    if (dead_lvr_check() == 0 && move_ptr->w_lvr == chk_pl->input_current) {
        if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
            command_ok();
            return;
        }

        check_next();
    }
}

/** @brief Check type 6: Button release check. */
void check_6() {
    s16 i;
    u16 lvr_work;

    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        cmd_tbl_ptr += 12;
        move_ptr->w_type = *cmd_tbl_ptr++;
        move_ptr->w_int = *cmd_tbl_ptr++;
        move_ptr->free2 = *cmd_tbl_ptr++;
        move_ptr->w_lvr = *cmd_tbl_ptr++;
        move_ptr->w_ptr = cmd_tbl_ptr;
        move_ptr->uni0.tame.flag = 0;
        move_ptr->uni0.tame.shot_flag = 0;
        move_ptr->uni0.tame.shot_flag2 = 0;
        move_ptr->free1 = 14;
        move_ptr->shot_ok = 0;
    } else {
        move_ptr->free1--;

        if (move_ptr->free1 <= 0) {
            move_ptr->free1 = 14;
            move_ptr->shot_ok = 0;
        }
    }

    lvr_work = 1 & 0xFFFF;

    for (i = 0; i < 4; i++) {
        if (chk_pl->sw_lever == lvr_work) {
            move_ptr->shot_ok |= (lvr_work);
            move_ptr->free1 = 14;
        }

        lvr_work *= 2;
    }

    if (move_ptr->shot_ok == 15) {
        if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
            command_ok();
            return;
        }

        move_ptr->shot_ok = 0;
        check_next();
    }
}

/** @brief Check type 7: Negative edge button release check. */
void check_7() {
    s16 i;
    s16 w_flag;
    s16* shot_cnt_adrs;

    move_ptr->w_int--;

    if (move_ptr->w_type == 8) {
        sw_work = chk_pl->input_pressed & CMD_BTN_PUNCHES;
        shot_cnt_adrs = &chk_pl->s1_cnt;
        w_flag = CMD_BTN_LP;
    } else {
        sw_work = chk_pl->input_pressed & CMD_BTN_KICKS_ALT;
        shot_cnt_adrs = &chk_pl->s4_cnt;
        w_flag = CMD_BTN_LK;
    }

    move_ptr->uni0.tame.shot_flag2 = move_ptr->uni0.tame.shot_flag;
    move_ptr->uni0.tame.shot_flag = 0;

    for (i = 0; i < 3; i++) {
        if (*shot_cnt_adrs & move_ptr->w_lvr) {
            move_ptr->uni0.tame.shot_flag |= w_flag;
        }

        shot_cnt_adrs++;

        if (chk_pl->shot_down & w_flag && move_ptr->uni0.tame.shot_flag2 & w_flag) {
            move_ptr->shot_ok += 1;
        }

        w_flag *= 2;
    }

    if (move_ptr->shot_ok) {
        move_ptr->free2--;

        if (move_ptr->free2 < 0) {
            move_ptr->shot_ok = 0;
            move_ptr->free2 = move_ptr->free1;
            move_ptr->uni0.tame.shot_flag = 0;
        }
    }

    if (move_ptr->shot_ok >= move_ptr->w_lvr) {
        move_ptr->shot_ok = 0;
        move_ptr->free2 = move_ptr->free1;

        if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
            command_ok();
            return;
        }

        check_next();
    }
}

/** @brief Check type 9: Direction-hold check with charge time. */
void check_9() {
    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->w_type = 0;
    }

    if (move_ptr->w_lvr & CMD_FLIP_X) {
        sw_work = move_ptr->w_lvr & CMD_LEVER_MASK;

        if (move_ptr->w_lvr == 0) {
            if (chk_pl->new_lvbt == 0) {
                if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                    command_ok();
                    return;
                }

                check_next();
            }
        } else if ((chk_pl->old_lvbt & CMD_LEVER_MASK) != (chk_pl->new_lvbt & CMD_LEVER_MASK)) {
            if (chk_pl->sw_lever == sw_work) {
                if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                    command_ok();
                    return;
                }

                check_next();
                return;
            }

            move_ptr->w_type = 0;
        }
    } else if (move_ptr->w_lvr == 0) {
        if (chk_pl->new_lvbt == 0) {
            if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                command_ok();
                return;
            }

            check_next();
            return;
        }

        if ((chk_pl->old_lvbt & CMD_LEVER_MASK) != (chk_pl->new_lvbt & CMD_LEVER_MASK)) {
            move_ptr->w_type = 0;
        }
    } else if ((chk_pl->old_lvbt & CMD_LEVER_MASK) != (chk_pl->new_lvbt & CMD_LEVER_MASK)) {
        if (chk_pl->sw_lever & move_ptr->w_lvr) {
            if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                command_ok();
                return;
            }

            check_next();
            return;
        }

        move_ptr->w_type = 0;
    }
}

/** @brief Resets the parry miss input lock timer. */
void paring_miss_init() {
    move_ptr->free3 = 0;
    move_ptr->w_type = 0;
    move_ptr->uni0.tame.flag = 0;
    g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
}

/** @brief Check type 10: Parry (blocking) input detection. */
void check_10() {
    switch (move_ptr->shot_ok) {
    case 0:
        if (chk_pl->sw_lever == 0) {
            move_ptr->shot_ok++;
        }
        break;

    case 1:
        if ((cmd_pl->wu.xyz[1].disp.pos > 0 || (move_type[cmd_id] != 5 && move_type[cmd_id] != 6)) &&
            chk_pl->now_lvbt & CMD_LEVER_MASK) {
            if (chk_pl->sw_lever == move_ptr->w_lvr) {
                move_ptr->shot_ok++;
                g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
                move_ptr->free3 = g_state.wcp[cmd_id].reset[move_type[cmd_id]] + 10;
                move_ptr->w_int = 6;

                switch (move_type[cmd_id]) {
                case 3:
                    if (g_state.wcp[cmd_id].move_state_flags[3] > g_state.wcp[cmd_id].move_state_flags[4]) {
                        g_state.wcp[cmd_id].move_state_flags[4] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[3] > g_state.wcp[cmd_id].move_state_flags[5]) {
                        g_state.wcp[cmd_id].move_state_flags[5] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[3] > g_state.wcp[cmd_id].move_state_flags[6]) {
                        g_state.wcp[cmd_id].move_state_flags[6] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[3] > g_state.wcp[cmd_id].move_state_flags[12]) {
                        g_state.wcp[cmd_id].move_state_flags[12] = 0;
                    }

                    break;

                case 4:
                    if (g_state.wcp[cmd_id].move_state_flags[4] > g_state.wcp[cmd_id].move_state_flags[3]) {
                        g_state.wcp[cmd_id].move_state_flags[3] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[4] > g_state.wcp[cmd_id].move_state_flags[5]) {
                        g_state.wcp[cmd_id].move_state_flags[5] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[4] > g_state.wcp[cmd_id].move_state_flags[6]) {
                        g_state.wcp[cmd_id].move_state_flags[6] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[4] > g_state.wcp[cmd_id].move_state_flags[12]) {
                        g_state.wcp[cmd_id].move_state_flags[12] = 0;
                    }

                    break;

                case 5:
                    if (g_state.wcp[cmd_id].move_state_flags[5] > g_state.wcp[cmd_id].move_state_flags[3]) {
                        g_state.wcp[cmd_id].move_state_flags[3] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[5] > g_state.wcp[cmd_id].move_state_flags[4]) {
                        g_state.wcp[cmd_id].move_state_flags[4] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[5] > g_state.wcp[cmd_id].move_state_flags[6]) {
                        g_state.wcp[cmd_id].move_state_flags[6] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[5] > g_state.wcp[cmd_id].move_state_flags[12]) {
                        g_state.wcp[cmd_id].move_state_flags[12] = 0;
                    }

                    if (g_state.move_work[cmd_id][6].free3 > 0) {
                        g_state.wcp[cmd_id].move_state_flags[5] = 0;
                    }

                    break;

                case 6:
                    if (g_state.wcp[cmd_id].move_state_flags[6] > g_state.wcp[cmd_id].move_state_flags[3]) {
                        g_state.wcp[cmd_id].move_state_flags[3] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[6] > g_state.wcp[cmd_id].move_state_flags[4]) {
                        g_state.wcp[cmd_id].move_state_flags[4] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[6] > g_state.wcp[cmd_id].move_state_flags[5]) {
                        g_state.wcp[cmd_id].move_state_flags[5] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[6] > g_state.wcp[cmd_id].move_state_flags[12]) {
                        g_state.wcp[cmd_id].move_state_flags[12] = 0;
                    }

                    if (g_state.move_work[cmd_id][5].free3 > 0) {
                        g_state.wcp[cmd_id].move_state_flags[6] = 0;
                    }

                    break;

                case 12:
                    if (g_state.wcp[cmd_id].move_state_flags[12] > g_state.wcp[cmd_id].move_state_flags[3]) {
                        g_state.wcp[cmd_id].move_state_flags[3] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[12] > g_state.wcp[cmd_id].move_state_flags[4]) {
                        g_state.wcp[cmd_id].move_state_flags[4] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[12] > g_state.wcp[cmd_id].move_state_flags[5]) {
                        g_state.wcp[cmd_id].move_state_flags[5] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[12] > g_state.wcp[cmd_id].move_state_flags[6]) {
                        g_state.wcp[cmd_id].move_state_flags[6] = 0;
                    }

                    break;
                }
            } else {
                move_ptr->shot_ok = 0;
                break;
            }
        }

        break;

    case 2:
        move_ptr->w_int--;
        move_ptr->free3--;

        if (move_ptr->w_int > 0) {
            if (chk_pl->sw_lever == 0) {
                move_ptr->shot_ok++;
                break;
            }

            if (chk_pl->sw_lever & CMD_LEVER_DOWN) {
                g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
                move_ptr->shot_ok++;
                break;
            }

            if (chk_pl->sw_lever != move_ptr->w_lvr) {
                g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
                move_ptr->shot_ok++;
                break;
            }
        } else {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
            move_ptr->shot_ok++;
        }

        break;

    case 3:
        move_ptr->free3--;

        if (move_ptr->free3 < 0) {
            move_ptr->w_type = 0;
            break;
        }

        if ((chk_pl->input_current & CMD_LEVER_DOWN) || !(chk_pl->input_current != move_ptr->w_lvr)) {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
            break;
        }

        if (chk_pl->input_current & CMD_LEVER_MASK) {
            move_ptr->shot_ok++;
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
        }

        break;

    case 4:
        move_ptr->free3--;

        if (move_ptr->free3 < 0) {
            move_ptr->w_type = 0;
        }

        break;
    }
}

/** @brief Check type 11: Throw tech (ukemi) input detection. */
void check_11() {
    if (dead_lvr_check()) {
        paring_miss_init();
        return;
    }

    switch (move_ptr->uni0.tame.flag) {
    case 0:
        if (chk_pl->sw_lever & CMD_LEVER_DOWN) {
            move_ptr->uni0.tame.flag = 1;
            break;
        }

        move_ptr->uni0.tame.flag = 0;
        break;

    case 1:
        if (chk_pl->sw_lever == 2) {
            check_next();
            break;
        }

        if (!(chk_pl->sw_lever & CMD_LEVER_DOWN)) {
            move_ptr->uni0.tame.flag = 0;
        }

        break;
    }
}

/** @brief Check type 12: Super Art (SA) motion + button input. */
void check_12() {
    switch (move_ptr->shot_ok) {
    case 0:
        if (chk_pl->sw_lever == 0) {
            move_ptr->shot_ok++;
        }
        break;

    case 1:
        if (cmd_pl->wu.xyz[1].disp.pos > 0 && (chk_pl->now_lvbt & CMD_LEVER_MASK) != 0) {
            if (chk_pl->sw_lever == move_ptr->w_lvr) {
                move_ptr->shot_ok++;
                g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
                move_ptr->free3 = g_state.wcp[cmd_id].reset[move_type[cmd_id]] + 10;
                move_ptr->w_int = 6;

                switch (move_type[cmd_id]) {
                case 3:
                    if (g_state.wcp[cmd_id].move_state_flags[3] > g_state.wcp[cmd_id].move_state_flags[4]) {
                        g_state.wcp[cmd_id].move_state_flags[4] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[3] > g_state.wcp[cmd_id].move_state_flags[5]) {
                        g_state.wcp[cmd_id].move_state_flags[5] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[3] > g_state.wcp[cmd_id].move_state_flags[6]) {
                        g_state.wcp[cmd_id].move_state_flags[6] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[3] > g_state.wcp[cmd_id].move_state_flags[12]) {
                        g_state.wcp[cmd_id].move_state_flags[12] = 0;
                    }

                    break;

                case 4:
                    if (g_state.wcp[cmd_id].move_state_flags[4] > g_state.wcp[cmd_id].move_state_flags[3]) {
                        g_state.wcp[cmd_id].move_state_flags[3] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[4] > g_state.wcp[cmd_id].move_state_flags[5]) {
                        g_state.wcp[cmd_id].move_state_flags[5] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[4] > g_state.wcp[cmd_id].move_state_flags[6]) {
                        g_state.wcp[cmd_id].move_state_flags[6] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[4] > g_state.wcp[cmd_id].move_state_flags[12]) {
                        g_state.wcp[cmd_id].move_state_flags[12] = 0;
                    }

                    break;

                case 5:
                    if (g_state.wcp[cmd_id].move_state_flags[5] > g_state.wcp[cmd_id].move_state_flags[3]) {
                        g_state.wcp[cmd_id].move_state_flags[3] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[5] > g_state.wcp[cmd_id].move_state_flags[4]) {
                        g_state.wcp[cmd_id].move_state_flags[4] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[5] > g_state.wcp[cmd_id].move_state_flags[6]) {
                        g_state.wcp[cmd_id].move_state_flags[6] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[5] > g_state.wcp[cmd_id].move_state_flags[12]) {
                        g_state.wcp[cmd_id].move_state_flags[12] = 0;
                    }

                    break;

                case 6:
                    if (g_state.wcp[cmd_id].move_state_flags[6] > g_state.wcp[cmd_id].move_state_flags[3]) {
                        g_state.wcp[cmd_id].move_state_flags[3] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[6] > g_state.wcp[cmd_id].move_state_flags[4]) {
                        g_state.wcp[cmd_id].move_state_flags[4] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[6] > g_state.wcp[cmd_id].move_state_flags[5]) {
                        g_state.wcp[cmd_id].move_state_flags[5] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[6] > g_state.wcp[cmd_id].move_state_flags[12]) {
                        g_state.wcp[cmd_id].move_state_flags[12] = 0;
                    }

                    break;

                case 12:
                    if (g_state.wcp[cmd_id].move_state_flags[12] > g_state.wcp[cmd_id].move_state_flags[3]) {
                        g_state.wcp[cmd_id].move_state_flags[3] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[12] > g_state.wcp[cmd_id].move_state_flags[4]) {
                        g_state.wcp[cmd_id].move_state_flags[4] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[12] > g_state.wcp[cmd_id].move_state_flags[5]) {
                        g_state.wcp[cmd_id].move_state_flags[5] = 0;
                    }

                    if (g_state.wcp[cmd_id].move_state_flags[12] > g_state.wcp[cmd_id].move_state_flags[6]) {
                        g_state.wcp[cmd_id].move_state_flags[6] = 0;
                    }

                    break;
                }
            } else {
                move_ptr->shot_ok = 0;
                break;
            }
        }

        break;

    case 2:
        move_ptr->w_int--;
        move_ptr->free3--;

        if (move_ptr->w_int > 0) {
            if (chk_pl->sw_lever == 0) {
                move_ptr->shot_ok++;
                break;
            }

            if (chk_pl->sw_lever & CMD_LEVER_DOWN) {
                g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
                move_ptr->shot_ok++;
                break;
            }

            if (chk_pl->sw_lever != move_ptr->w_lvr) {
                g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
                move_ptr->shot_ok++;
                break;
            }
        } else {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
            move_ptr->shot_ok++;
        }

        break;

    case 3:
        move_ptr->free3--;

        if (move_ptr->free3 < 0) {
            move_ptr->w_type = 0;
            break;
        }

        if ((chk_pl->input_current & CMD_LEVER_DOWN) || !(chk_pl->input_current != move_ptr->w_lvr)) {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
            break;
        }

        if (chk_pl->input_current & CMD_LEVER_MASK) {
            move_ptr->shot_ok++;
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
        }

        break;

    case 4:
        move_ptr->free3--;

        if (move_ptr->free3 < 0) {
            move_ptr->w_type = 0;
        }

        break;
    }
}

/** @brief Check type 13: Air parry input detection. */
void check_13() {
    u16 sw_w;

    if (move_ptr->free3 > 0) {
        move_ptr->free3--;

        if (move_ptr->free3 <= 0) {
            move_ptr->w_type = 0;
        }
    }

    if ((chk_pl->old_lvbt & CMD_LEVER_MASK) != (chk_pl->new_lvbt & CMD_LEVER_MASK) && (chk_pl->sw_lever) == 2) {
        g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] =
            16 - ukemi_time_tbl[g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]]];
        move_ptr->free3 = 16;
        chk_pl->move_id = move_type[cmd_id];
    }

    sw_w = (chk_pl->input_current | chk_pl->old_now) & CMD_BTN_PUNCHES;

    if (sw_w == CMD_BTN_PUNCHES) {
        g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] =
            16 - ukemi_time_tbl[g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]]];
        move_ptr->free3 = 16;
        chk_pl->move_id = move_type[cmd_id];
    }
}

/** @brief Check type 14: EX special move input (two same-type buttons). */
void check_14() {
    move_ptr->w_int--;

    if (move_ptr->w_lvr == CMD_BTN_LP) {
        if (chk_pl->input_current & CMD_BTN_PUNCHES) {
            move_ptr->uni0.tame.flag++;
        }
    } else if (chk_pl->input_current & CMD_BTN_KICKS) {
        move_ptr->uni0.tame.flag += 1;
    }

    if (g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]]) {
        if (move_ptr->w_int <= 0) {
            if (move_ptr->uni0.tame.flag) {
                g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
                move_ptr->uni0.tame.flag = 0;

                if (move_type[cmd_id] & 1) {
                    move_ptr->w_int = 10;
                } else {
                    move_ptr->w_int = 6;
                }
                return;
            }

            move_ptr->uni0.tame.flag = 0;
            move_ptr->w_int = move_ptr->free1;
        }
    } else if (move_type[cmd_id] & 1) {
        if (move_ptr->uni0.tame.flag >= 3) {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
            move_ptr->uni0.tame.flag = 0;
            move_ptr->w_int = 10;
            chk_pl->move_id = move_type[cmd_id];
            return;
        }

        if (move_ptr->w_int < 0) {
            move_ptr->uni0.tame.flag = 0;
            move_ptr->w_int = move_ptr->free1;
        }
    } else {
        if (move_ptr->uni0.tame.flag >= 3) {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
            move_ptr->uni0.tame.flag = 0;
            move_ptr->w_int = 6;
            chk_pl->move_id = move_type[cmd_id];
            return;
        }

        if (move_ptr->w_int < 0) {
            move_ptr->uni0.tame.flag = 0;
            move_ptr->w_int = move_ptr->free1;
        }
    }
}

/** @brief Check type 15: Kara-cancel input detection. */
void check_15() {
    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->w_type = 0;
        return;
    }

    if (!dead_lvr_check()) {
        if (move_ptr->w_lvr & CMD_FLIP_X) {
            sw_work = move_ptr->w_lvr & CMD_LEVER_MASK;

            if (chk_pl->sw_lever == sw_work) {
                move_ptr->shot_ok++;

                if (move_ptr->shot_ok >= move_ptr->free1) {
                    if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                        command_ok();
                        return;
                    }

                    check_next();
                }
            }
        } else if (move_ptr->w_lvr == 0) {
            if (chk_pl->sw_lever == 0) {
                move_ptr->shot_ok += 1;

                if (move_ptr->shot_ok >= move_ptr->free1) {
                    if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                        command_ok();
                        return;
                    }

                    check_next();
                }
            }
        } else if (((chk_pl->old_lvbt & CMD_LEVER_MASK) != (chk_pl->new_lvbt & CMD_LEVER_MASK)) &&
                   (chk_pl->sw_lever & move_ptr->w_lvr) &&
                   (move_ptr->shot_ok += 1, move_ptr->shot_ok < move_ptr->free1 == 0)) {
            if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                command_ok();
                return;
            }

            check_next();
        }
    }
}

/** @brief Check type 16: Personal action (taunt) input detection. */
void check_16() {
    s16 i;
    u16 w_flag;

    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->w_type = 0;
        move_ptr->shot_ok = 0;
        return;
    }

    if (move_ptr->w_type == 17) {
        sw_work = chk_pl->input_current & CMD_BTN_PUNCHES;
        w_flag = CMD_BTN_LP;
    } else {
        sw_work = chk_pl->input_current & CMD_BTN_KICKS;
        w_flag = CMD_BTN_LK;
    }

    move_ptr->uni0.tame.shot_flag2 = move_ptr->uni0.tame.shot_flag;
    move_ptr->uni0.tame.shot_flag = 0;

    for (i = 0; i < 3; i++) {
        if (sw_work & w_flag) {
            move_ptr->shot_ok++;
        }

        w_flag *= 2;
    }

    if (move_ptr->shot_ok >= move_ptr->w_lvr) {
        move_ptr->shot_ok = 0;

        if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
            command_ok();
            return;
        }

        check_next();
    }
}

/** @brief Check type 18: Quick stand (recovery) input detection. */
void check_18() {
    u16 sw_lever;

    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->w_type = 0;
        return;
    }

    sw_lever = chk_pl->sw_lever & CMD_LEVER_MASK;

    if (!dead_lvr_check()) {
        if (move_ptr->w_lvr & CMD_FLIP_X) {
            if ((chk_pl->old_lvbt & CMD_LEVER_MASK) != (chk_pl->new_lvbt & CMD_LEVER_MASK)) {
                sw_work = move_ptr->w_lvr & CMD_LEVER_MASK;

                if (sw_lever == sw_work) {
                    move_ptr->w_int = move_ptr->free1;
                    g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
                }
            }
        } else if (move_ptr->w_lvr == 0) {
            if (chk_pl->sw_lever == 0) {
                move_ptr->w_int = move_ptr->free1;
                g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
            }
        } else if ((chk_pl->old_lvbt & CMD_LEVER_MASK) != (chk_pl->new_lvbt & CMD_LEVER_MASK) &&
                   (sw_lever & move_ptr->w_lvr)) {
            move_ptr->w_int = move_ptr->free1;
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
        }
    }
}

/** @brief Check type 19: Grab escape (tech throw) input detection. */
void check_19() {
    u16 sw_lever;

    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->w_type = 0;
    }

    sw_lever = chk_pl->sw_lever & CMD_LEVER_MASK;

    if (!dead_lvr_check()) {
        if (move_ptr->w_lvr & CMD_FLIP_X) {
            if (chk_pl->now_lvbt & CMD_LEVER_MASK) {
                sw_work = move_ptr->w_lvr & CMD_LEVER_MASK;
                if (sw_lever == sw_work) {
                    g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
                    check_next();
                }
            }
        } else if (move_ptr->w_lvr == 0) {
            if (chk_pl->sw_lever == 0) {
                g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
                check_next();
            }
        } else if ((chk_pl->now_lvbt & CMD_LEVER_MASK) != 0 && (sw_lever & move_ptr->w_lvr)) {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];
            check_next();
        }
    }
}

/** @brief Check type 20: Unused/no-op check. */
void check_20() {}

/** @brief Check type 21: High jump input (down-up). */
void check_21() {
    u16 sw_lever;

    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->w_type = 0;
    }

    sw_lever = chk_pl->sw_lever & CMD_LEVER_MASK;

    if (!dead_lvr_check()) {
        if (move_ptr->w_lvr & CMD_FLIP_X) {
            sw_work = move_ptr->w_lvr & CMD_LEVER_MASK;
            if (sw_work == 0) {
                if (sw_lever == 0) {
                    if (((*move_ptr->w_ptr)) == CHK_MOVE_COUNT) {
                        command_ok();
                        return;
                    }
                    check_next();
                }
            } else if (chk_pl->now_lvbt & CMD_LEVER_MASK) {
                if (sw_lever == sw_work) {
                    if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                        command_ok();
                        return;
                    }
                    check_next();
                }
            }
        } else if (move_ptr->w_lvr == 0) {
            if (sw_lever == 0) {
                if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                    command_ok();
                    return;
                }

                check_next();
            }
        } else if ((chk_pl->now_lvbt & CMD_LEVER_MASK) && (sw_lever & move_ptr->w_lvr)) {
            if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                command_ok();
                return;
            }

            check_next();
        }
    }
}

/** @brief Check type 22: Dash input (forward-forward or back-back). */
void check_22() {
    s16 i;

    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->w_int = move_ptr->free2;
        cmd_tbl_ptr += 12;
        move_ptr->w_type = *cmd_tbl_ptr++;
        move_ptr->w_int = *cmd_tbl_ptr++;
        move_ptr->free1 = *cmd_tbl_ptr;
        move_ptr->free2 = *cmd_tbl_ptr++;
        move_ptr->w_lvr = *cmd_tbl_ptr++;
        move_ptr->w_ptr = cmd_tbl_ptr;
        move_ptr->uni0.tame.flag = 0;
        move_ptr->uni0.tame.shot_flag = 0;
        move_ptr->uni0.tame.shot_flag2 = 0;
        move_ptr->shot_ok = 0;
        move_ptr->free3 = 0;
    }

    for (i = 0; i < 8; i++) {
        if (chk_pl->sw_lever == chk22_tbl[i]) {
            move_ptr->free3 |= 1 << i;
        }
    }

    if (move_ptr->free3 == 0xFF) {
        if (((*move_ptr->w_ptr)) == CHK_MOVE_COUNT) {
            command_ok();
            return;
        }

        move_ptr->free3 = 0;
        check_next();
    }
}

/** @brief Check type 23: Target combo input detection. */
void check_23() {
    switch (move_ptr->shot_ok) {
    case 0:
        if (chk_pl->sw_lever == 0) {
            move_ptr->shot_ok++;
            break;
        }

        break;

    case 1:
        if ((chk_pl->old_lvbt & CMD_LEVER_MASK) != (chk_pl->new_lvbt & CMD_LEVER_MASK) &&
            chk_pl->sw_lever == move_ptr->w_lvr) {
            move_ptr->shot_ok++;
            g_state.wcp[cmd_id].move_state_flags[(move_type[cmd_id])] = g_state.wcp[cmd_id].reset[(move_type[cmd_id])];
            move_ptr->free3 = (s16)(((((g_state.wcp[cmd_id].reset[(move_type[cmd_id])])) + 3)));
            move_ptr->w_int = 6;
        }

        break;

    case 2:
        move_ptr->w_int -= 1;
        move_ptr->free3 -= 1;

        if (((move_ptr->w_int)) > 0) {
            if (chk_pl->sw_lever == 0) {
                move_ptr->shot_ok++;
                break;
            }

            if (chk_pl->sw_lever & CMD_LEVER_DOWN) {
                g_state.wcp[cmd_id].move_state_flags[(move_type[cmd_id])] = 0;
                move_ptr->shot_ok++;
                break;
            }

            if (chk_pl->sw_lever != ((move_ptr->w_lvr))) {
                g_state.wcp[cmd_id].move_state_flags[(move_type[cmd_id])] = 0;
                move_ptr->w_type = 0;
                break;
            }
        } else {
            g_state.wcp[cmd_id].move_state_flags[(move_type[cmd_id])] = 0;
            move_ptr->shot_ok++;
        }

        break;

    case 3:
        move_ptr->free3--;

        if (move_ptr->free3 < 0) {
            move_ptr->w_type = 0;
            break;
        }

        if ((chk_pl->input_current & CMD_LEVER_DOWN) || !(chk_pl->input_current != move_ptr->w_lvr)) {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
            break;
        }

        if (chk_pl->input_current & CMD_LEVER_MASK) {
            g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = 0;
            move_ptr->w_type = 0;
        }

        break;
    }
}

/** @brief Check type 24: Chain combo input detection. */
void check_24() {
    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->w_type = 0;
    }

    if (!dead_lvr_check())
        lvr_match_or_next(chk_pl->now_lvbt & CMD_LEVER_MASK, CMD_LEVER_MASK);
}

/** @brief Check type 25: Leap attack input detection (close + direction + button). */
void check_25() {
    move_ptr->w_int--;

    if (move_ptr->w_int < 0) {
        move_ptr->w_type = 0;
    }

    if (!dead_lvr_check())
        lvr_match_or_next(chk_pl->sw_lever & CMD_LEVER_MASK, CMD_LEVER_MASK);
}

/** @brief Check type 26: Special grab/throw input detection. */
void check_26() {
    u16 sw_lever = chk_pl->input_current & CMD_LEVER_MASK;
    u16 sw_now_lvr = chk_pl->sw_lever & CMD_LEVER_MASK;

    if (!dead_lvr_check()) {
        sw_work = move_ptr->w_lvr & CMD_LEVER_MASK;

        if (sw_lever != sw_work) {
            if (sw_now_lvr != sw_work && move_ptr->uni0.tame.flag) {
                if (*move_ptr->w_ptr == CHK_MOVE_COUNT) {
                    command_ok();
                    return;
                }

                check_next();
            }
        } else {
            move_ptr->uni0.tame.flag = 1;
        }
    }
}

/** @brief Marks the current command check as successful and records the move. */
void command_ok() {
    g_state.wcp[cmd_id].move_state_flags[move_type[cmd_id]] = g_state.wcp[cmd_id].reset[move_type[cmd_id]];

    if (move_ptr->w_type != 14) {
        move_ptr->w_type = 0;
        chk_pl->move_id = move_type[cmd_id];
    }
}

/** @brief Marks a specific move number as successfully detected. */
void Command_Ok_Move(s16 move_num) {
    if (dead_lvr_check()) {
        g_state.wcp[cmd_id].move_state_flags[move_num] = 0;
        return;
    }

    g_state.wcp[cmd_id].move_state_flags[move_num]--;
}

/** @brief Returns 1 if the lever is in a neutral (dead zone) position. */
s32 dead_lvr_check() {
    if ((!move_ptr->w_dead || move_ptr->w_dead != chk_pl->input_pressed) &&
        (!move_ptr->w_dead2 || move_ptr->w_dead2 != chk_pl->input_pressed)) {
        return 0;
    }

    move_ptr->w_type = 0;
    return 1;
}

/** @brief Processes raw joystick input into the player's lever buffer. */
void pl_lvr_set() {
    u16 sw_work;
    u16 work2;
    u16 sw_0;
    u16 sw_hana;
    u16 hana2;

    sw_0 = g_state.wcp[cmd_id].input_held;

    if (check_rl_on_car(cmd_pl)) {
        if (cmd_pl->wu.rl_flag) {
            sw_work = (sw_0 & CMD_LEVER_LR);
            if (sw_work) {
                sw_0 &= CMD_SW_LR_CLR;
                sw_work ^= CMD_LEVER_LR;
                sw_0 |= sw_work;
            }
        }
    } else if (cmd_pl->wu.active_move) {
        sw_work = sw_0 & CMD_LEVER_LR;

        if (sw_work) {
            sw_0 &= CMD_SW_LR_CLR;
            sw_work ^= CMD_LEVER_LR;
            sw_0 |= sw_work;
        }
    }

    g_state.wcp[cmd_id].old_now = chk_pl->input_current;
    chk_pl->old_now = chk_pl->input_current;
    chk_pl->old_lvbt = chk_pl->new_lvbt;

    sw_work = ~(chk_pl->old_lvbt) & (g_state.wcp[cmd_id].input_held);
    sw_hana = chk_pl->input_pressed & ~(sw_0);

    work2 = sw_work & CMD_SW_PUNCH_MASK;
    hana2 = sw_hana & CMD_SW_PUNCH_MASK;

    switch (work2) {
    case CMD_SW_3P_70:
    case CMD_SW_2P_30:
    case CMD_SW_2P_50:
    case CMD_SW_2P_60:
        g_state.wcp[cmd_id].input_held |= CMD_MULTI_PUNCH;
        sw_0 |= CMD_MULTI_PUNCH;
        break;

    default:
        switch (hana2) {
        case CMD_SW_3P_70:
        case CMD_SW_2P_30:
        case CMD_SW_2P_50:
        case CMD_SW_2P_60:
            g_state.wcp[cmd_id].input_held |= CMD_MULTI_PUNCH;
            sw_0 |= CMD_MULTI_PUNCH;
            break;

        default:
            g_state.wcp[cmd_id].input_held &= CMD_MULTI_PUNCH_CLR;
            sw_0 &= CMD_MULTI_PUNCH_CLR;
            break;
        }
    }

    work2 = sw_work & CMD_SW_KICK_MASK;
    hana2 = sw_hana & CMD_SW_KICK_MASK;

    switch (work2) {
    case CMD_SW_3K_700:
    case CMD_SW_2K_300:
    case CMD_SW_2K_500:
    case CMD_SW_2K_600:
        g_state.wcp[cmd_id].input_held |= CMD_MULTI_KICK;
        sw_0 |= CMD_MULTI_KICK;
        break;

    default:
        switch (hana2) {
        case CMD_SW_3K_700:
        case CMD_SW_2K_300:
        case CMD_SW_2K_500:
        case CMD_SW_2K_600:
            g_state.wcp[cmd_id].input_held |= CMD_MULTI_KICK;
            sw_0 |= CMD_MULTI_KICK;
            break;

        default:
            g_state.wcp[cmd_id].input_held &= CMD_MULTI_KICK_CLR;
            sw_0 &= CMD_MULTI_KICK_CLR;
            break;
        }
    }

    chk_pl->new_lvbt = g_state.wcp[cmd_id].input_held;
    chk_pl->input_old = chk_pl->input_pressed;
    chk_pl->input_pressed = sw_0;
    chk_pl->input_current = sw_0 & ~(chk_pl->input_old);
    chk_pl->now_lvbt = ~(chk_pl->old_lvbt) & (g_state.wcp[cmd_id].input_held);
    chk_pl->input_changed = (chk_pl->input_current) | (chk_pl->input_old & ~(sw_0));
    chk_pl->sw_lever = sw_0 & CMD_LEVER_MASK;
    chk_pl->shot_up = chk_pl->input_current & CMD_BTN_ATTACKS;
    chk_pl->shot_down = chk_pl->input_old & ~(sw_0)&CMD_BTN_ATTACKS;
    chk_pl->shot_ud = ((chk_pl->shot_up) | (chk_pl->shot_down));
    sw_work = ((chk_pl->input_current) | (g_state.wcp[cmd_id].old_now));

    if ((sw_work & CMD_SW_2BTN_LP_LK) == CMD_SW_2BTN_LP_LK) {
        g_state.wcp[cmd_id].ca14 = 1;
    } else {
        g_state.wcp[cmd_id].ca14 = 0;
    }

    if ((sw_work & CMD_SW_2BTN_MP_MK) == CMD_SW_2BTN_MP_MK) {
        g_state.wcp[cmd_id].ca25 = 1;
    } else {
        g_state.wcp[cmd_id].ca25 = 0;
    }
    if ((sw_work & CMD_SW_2BTN_HP_HK) == CMD_SW_2BTN_HP_HK) {
        g_state.wcp[cmd_id].ca36 = 1;
    } else {
        g_state.wcp[cmd_id].ca36 = 0;
    }

    g_state.wcp[cmd_id].lgp = lever_gacha_tbl[cmd_pl->cp->input_current & CMD_LEVER_MASK] * 4;
    g_state.wcp[cmd_id].lgp += lever_gacha_tbl[cmd_pl->cp->input_released & CMD_LEVER_MASK] * 2;
    g_state.wcp[cmd_id].lgp += lever_gacha_tbl[(cmd_pl->cp->input_current / 16) & 7] * 2;
    g_state.wcp[cmd_id].lgp += lever_gacha_tbl[(cmd_pl->cp->input_current / 256) & 7] * 1;
}

/** @brief Picks up button presses and releases from the raw switch data. */
void sw_pick_up() {
    s16 i;
    s16* cnt_address1;

    pl_lvr_set();
    sw_work = 1;
    cnt_address1 = &chk_pl->up_cnt;

    for (i = 0; i < 10; i++) {
        if (chk_pl->input_pressed & sw_work) {
            *cnt_address1 += 1;
        } else {
            *cnt_address1 = 0;
        }

        cnt_address1++;
        sw_work *= 2;
    }

    for (i = 0; i < 4; i++) {
        if (chk_pl->input_pressed & lvr_chk_tbl[0][i]) {
            *cnt_address1 += 1;
        } else {
            *cnt_address1 = 0;
        }

        cnt_address1++;
    }

    g_state.wcp[cmd_id].input_pressed = chk_pl->input_pressed;
    g_state.wcp[cmd_id].input_old = chk_pl->input_old;
    g_state.wcp[cmd_id].input_changed = chk_pl->input_changed;
    g_state.wcp[cmd_id].input_current = chk_pl->input_current;
    g_state.wcp[cmd_id].input_released = chk_pl->shot_down;

    if ((i = g_state.wcp[cmd_id].input_held & CMD_LEVER_LR)) {
        if (cmd_pl->wu.rl_flag) {
            if (i & 8) {
                g_state.wcp[cmd_id].lever_dir = 1;
            } else {
                g_state.wcp[cmd_id].lever_dir = 2;
            }
        } else if (i & 4) {
            g_state.wcp[cmd_id].lever_dir = 1;
        } else {
            g_state.wcp[cmd_id].lever_dir = 2;
        }
    } else {
        g_state.wcp[cmd_id].lever_dir = 0;
    }

    if ((chk_pl->left_cnt != 0) && (chk_pl->left_cnt < 12)) {
        g_state.wcp[cmd_id].calf = 1;
    } else {
        g_state.wcp[cmd_id].calf = 0;
    }

    if ((chk_pl->right_cnt != 0) && (chk_pl->right_cnt < 12)) {
        g_state.wcp[cmd_id].calr = 1;
        return;
    }

    g_state.wcp[cmd_id].calr = 0;
}

/** @brief Clears all dash-detection flags for a player. */
void dash_flag_clear(s16 pl_id) {
    intptr_t* adrs;

    if (g_state.cmd_sel[pl_id]) {
        adrs = pl_CMD[g_state.plw[pl_id].player_number];
    } else {
        adrs = pl_cmd[g_state.plw[pl_id].player_number];
    }

    move_compel_init(pl_id, 0, adrs);
    move_compel_init(pl_id, 1, adrs);
}

/** @brief Clears all high-jump detection flags for a player. */
void hi_jump_flag_clear(s16 pl_id) {
    intptr_t* adrs;

    if (g_state.cmd_sel[pl_id]) {
        adrs = pl_CMD[g_state.plw[pl_id].player_number];
    } else {
        adrs = pl_cmd[g_state.plw[pl_id].player_number];
    }

    move_compel_init(pl_id, 2, adrs);
}

/** @brief Clears the detection flag for a single move by its index. */
void move_flag_clear_only_1(s16 pl_id, s16 wznum) {
    intptr_t* adrs;

    if (g_state.cmd_sel[pl_id]) {
        adrs = pl_CMD[g_state.plw[pl_id].player_number];
    } else {
        adrs = pl_cmd[g_state.plw[pl_id].player_number];
    }

    move_compel_init(pl_id, wznum, adrs);
}

/** @brief Force-sets a command detection state for AI/scripted input. */
void move_compel_init(s16 pl_id, s16 num, intptr_t* adrs) {
    MOVE_WORK* w_ptr;
    s16* ptr;

    ptr = (s16*)adrs[num];
    ptr += 12;
    w_ptr = &g_state.move_work[pl_id][num];
    w_ptr->w_type = *ptr++;
    w_ptr->w_int = *ptr++;
    w_ptr->free1 = *ptr;
    w_ptr->free2 = *ptr++;
    w_ptr->w_lvr = *ptr++;
    w_ptr->w_ptr = ptr;
    w_ptr->uni0.tame.flag = 0;
    w_ptr->uni0.tame.shot_flag = 0;
    w_ptr->uni0.tame.shot_flag2 = 0;
    w_ptr->shot_ok = 0;
    w_ptr->free3 = 0;
    g_state.wcp[pl_id].move_state_flags[num] = 0;
}

/** @brief Force-initializes all command detection slots for a player (AI). */
void move_compel_all_init(PLW* pl) {
    s16 i;
    intptr_t* adrs;

    if (g_state.cmd_sel[pl->wu.id]) {
        adrs = pl_CMD[pl->player_number];
    } else {
        adrs = pl_cmd[pl->player_number];
    }

    for (i = 0; i < pl_cmd_num[pl->player_number][0]; i++) {
        cmd_tbl_ptr = (s16*)adrs[i];
        cmd_data_set(pl, i);
    }

    for (i = pl_cmd_num[pl->player_number][0]; i < 20; i++) {
        g_state.wcp[cmd_id].move_state_flags[i] = -1;
    }

    for (i = 20; i < pl_cmd_num[pl->player_number][1]; i++) {
        cmd_tbl_ptr = (s16*)adrs[i];
        cmd_data_set(pl, i);
    }

    for (i = pl_cmd_num[pl->player_number][1]; i < 24; i++) {
        g_state.wcp[cmd_id].move_state_flags[i] = -1;
    }

    for (i = 24; i < pl_cmd_num[pl->player_number][2]; i++) {
        cmd_tbl_ptr = (s16*)adrs[i];
        cmd_data_set(pl, i);
    }

    for (i = pl_cmd_num[pl->player_number][2]; i < 28; i++) {
        g_state.wcp[cmd_id].move_state_flags[i] = -1;
    }

    for (i = 28; i < pl_cmd_num[pl->player_number][3]; i++) {
        cmd_tbl_ptr = (s16*)adrs[i];
        cmd_data_set(pl, i);
    }

    for (i = pl_cmd_num[pl->player_number][3]; i < 38; i++) {
        g_state.wcp[cmd_id].move_state_flags[i] = -1;
    }

    for (i = 38; i < pl_cmd_num[pl->player_number][4]; i++) {
        cmd_tbl_ptr = (s16*)adrs[i];
        cmd_data_set(pl, i);
    }

    for (i = pl_cmd_num[pl->player_number][4]; i < 42; i++) {
        g_state.wcp[cmd_id].move_state_flags[i] = -1;
    }

    for (i = 42; i < pl_cmd_num[pl->player_number][5]; i++) {
        cmd_tbl_ptr = (s16*)adrs[i];
        cmd_data_set(pl, i);
    }

    for (i = pl_cmd_num[pl->player_number][5]; i < 46; i++) {
        g_state.wcp[cmd_id].move_state_flags[i] = -1;
    }

    for (i = 46; i < pl_cmd_num[pl->player_number][6]; i++) {
        cmd_tbl_ptr = (s16*)adrs[i];
        cmd_data_set(pl, i);
    }

    for (i = pl_cmd_num[pl->player_number][6]; i < 56; i++) {
        g_state.wcp[cmd_id].move_state_flags[i] = -1;
    }
}

/** @brief Simplified version of move_compel_all_init for specific use cases. */
void move_compel_all_init2(PLW* pl) {
    s16 j;

    for (j = 0; j < 56; j++) {
        if (g_state.wcp[pl->wu.id].move_state_flags[j] != -1) {
            g_state.move_work[pl->wu.id][j].w_type = 0;
        }
    }
}

/** @brief Applies directional mirroring to a lever/button data word. */
u16 processed_lvbt(u16 lv_data) {
    return lv_data & 0xFFF;
}
