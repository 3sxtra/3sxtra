/**
 * @file win_pl.c
 * @brief Winning-character post-round animations.
 *
 * Per-character victory poses and special animations dispatched by
 * `win_player()`, including judge verdicts, bonus-game results, and
 * meta-character (Gill) win sequences.
 *
 * Part of the animation module.
 */

#include "sf33rd/Source/Game/animation/win_pl.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_30_object_etc3_character.h"
#include "sf33rd/Source/Game/effect/effect_31_object_etc3_character.h"
#include "sf33rd/Source/Game/effect/effect_32_object_etc3_character.h"
#include "sf33rd/Source/Game/effect/effect_82_visual_generic.h"
#include "sf33rd/Source/Game/effect/effect_83_visual_generic.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effect_l3_wait_timer_data_table.h"
#include "sf33rd/Source/Game/effect/effect_l6_visual_generic.h"
#include "sf33rd/Source/Game/effect/effect_m2_character_table_animal.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"

static void Win_00000(PLW* wk);
static void Win_01000(PLW* wk);
static void jijii_nebukuro(PLW* wk);
static void jijii_jump(PLW* wk);
static void jijii_full(PLW* wk);
static void Win_02000(PLW* wk);
static void Win_03000(PLW* wk);
static void Win_04000(PLW* wk);
static void Normal_normal_Winner(PLW* wk);
static void Judge_normal_winner(PLW* wk);
static void Win_05000(PLW* wk);
static void Win_06000(PLW* wk);
static void Win_07000(PLW* wk);
static void Win_08000(PLW* wk);
static void Win_09000(PLW* wk);
static void Win_10000(PLW* wk);
static void q_keeping_action(PLW* wk);
static void q_leave_after_action(PLW* wk);
static void Win_11000(PLW* wk);
static void twelve_win_away(PLW* wk);
static void twelve_win_backjump(PLW* wk);
static void Win_12000(PLW* wk);
static void Win_13000(PLW* wk);
static void Win_14000(PLW* wk);
static void urien_dash(PLW* wk);
static void Win_15000(PLW* wk);
static s16 win_select(PLW* /* unused */, s16 num);
static void bonus_game_win_pause(PLW* wk);
static void meta_win_pause(PLW* wk);

s16 a_rno;

/* === Named Constants === */
#define CHARACTER_COUNT 20 /**< Number of playable characters */

const s16 winner_type_tbl[CHARACTER_COUNT] = { 6, 0, 0, 6, 2, 7, 9, 3, 4, 1, 12, 0, 5, 14, 8, 13, 6, 10, 11, 15 };

/** @brief Top-level winner dispatch — select type-specific win handler. */
void win_player(PLW* wk) {
    if (g_state.My_char[wk->wu.id] != wk->player_number) {
        meta_win_pause(wk);
        return;
    }

    if (g_state.Bonus_Game_Flag) {
        bonus_game_win_pause(wk);
        return;
    }

    if (g_state.pcon_rno[0] == 2 && g_state.pcon_rno[1] == 3) {
        Judge_normal_winner(wk);
        return;
    }

    if (wk->player_number < 0 || wk->player_number >= CHARACTER_COUNT) {
        return;
    }

    switch (winner_type_tbl[wk->player_number]) {
    case 0:
        Win_00000(wk);
        break;
    case 1:
        Win_01000(wk);
        break;
    case 2:
        Win_02000(wk);
        break;
    case 3:
        Win_03000(wk);
        break;
    case 4:
        Win_04000(wk);
        break;
    case 5:
        Win_05000(wk);
        break;
    case 6:
        Win_06000(wk);
        break;
    case 7:
        Win_07000(wk);
        break;
    case 8:
        Win_08000(wk);
        break;
    case 9:
        Win_09000(wk);
        break;
    case 10:
        Win_10000(wk);
        break;
    case 11:
        Win_11000(wk);
        break;
    case 12:
        Win_12000(wk);
        break;
    case 13:
        Win_13000(wk);
        break;
    case 14:
        Win_14000(wk);
        break;
    case 15:
        Win_15000(wk);
        break;
    default:
        break;
    }
}

/** @brief Win type 0 — standard win pose (delegates to Normal_normal_Winner). */
static void Win_00000(PLW* wk) {
    Normal_normal_Winner(wk);
}

const s16 win_10000_tbl[2][8] = { { 32, 33, 34, 32, 36, 37, 38, 33 }, { 35, 39, 34, 35, 36, 37, 38, 39 } };

/** @brief Win type 1 — Oro’s win (sleeping bag, jump, full-power variants). */
static void Win_01000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;
        work = win_select(wk, 7);

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            if (g_state.Round_Result & 0x800) {
                wk->wu.script_register_bank[0] = 0;
                set_char_move_init(&wk->wu, 9, 42);
                g_state.win_rno[0] = 3;
                break;
            }

            if (g_state.bg_w.stage == 9) {
                set_char_move_init(&wk->wu, 9, 41);
                g_state.win_rno[0] = 1;
                break;
            }

            set_char_move_init(&wk->wu, 9, win_10000_tbl[1][work]);

            if (work == 4) {
                g_state.win_rno[0] = 2;
            }

            break;
        }

        set_char_move_init(&wk->wu, 9, win_10000_tbl[0][work]);

        if (work == 4) {
            g_state.win_rno[0] = 2;
        }

        break;

    case 1:
    case 9:
        switch (g_state.win_rno[0]) {
        case 0:
            char_move(&wk->wu);
            break;

        case 1:
            jijii_nebukuro(wk);
            break;

        case 2:
            jijii_jump(wk);
            break;

        case 3:
            jijii_full(wk);
            break;
        }

        break;
    }
}

/** @brief Oro sleeping-bag win sub-sequence. */
static void jijii_nebukuro(PLW* wk) {
    g_state.bg_app_stop = 1;

    switch (g_state.win_rno[1]) {
    case 0:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 1) {
            g_state.win_rno[1]++;
            char_move_z(&wk->wu);
            wk->wu.mvxy.a[1].sp = 0xF0000;
            wk->wu.mvxy.d[1].sp = -0x600;
        }

        break;

    case 1:
        if (wk->wu.cg_type != 2) {
            char_move(&wk->wu);
        }

        add_y_sub((State_Other*)wk);

        if (wk->wu.xyz[1].disp.pos > 256) {
            g_state.win_rno[1]++;
            g_state.win_sp_flag = 2;
            set_char_move_init(&wk->wu, 9, 40);
            wk->wu.xyz[1].disp.pos = 200;
        }

        break;

    case 2:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Oro jump-away win sub-sequence. */
static void jijii_jump(PLW* wk) {
    s16 id_w;

    g_state.bg_app_stop = 1;
    id_w = wk->wu.id ^ 1;
    wk->wu.position_z = g_state.plw[id_w].wu.position_z - 1;
    wk->wu.my_priority = wk->wu.position_z;

    switch (g_state.win_rno[1]) {
    case 0:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 9) {
            g_state.win_rno[1]++;

            if (wk->wu.rl_flag) {
                wk->wu.mvxy.a[0].sp = 0x60000;
                wk->wu.mvxy.d[0].sp = 0x1000;
            } else {
                wk->wu.mvxy.a[0].sp = -0x60000;
                wk->wu.mvxy.d[0].sp = -0x1000;
            }

            wk->wu.mvxy.a[1].sp = 0xA0000;
            wk->wu.mvxy.d[1].sp = -0x600;
        }

        break;

    case 1:
        if (wk->wu.cg_type != 99) {
            char_move(&wk->wu);
        }

        add_x_sub((State_Other*)wk);
        add_y_sub((State_Other*)wk);

        if (wk->wu.rl_flag) {
            if (wk->wu.xyz[0].disp.pos > g_state.bg_w.bgw[1].xy[0].disp.pos + 320) {
                g_state.win_rno[1]++;
                effect_work_kill(3, 13);
            }

            break;
        }

        if (wk->wu.xyz[0].disp.pos < g_state.bg_w.bgw[1].xy[0].disp.pos - 320) {
            g_state.win_rno[1]++;
            effect_work_kill(3, 13);
        }

        break;

    case 2:
        g_state.win_rno[1]++;
        set_char_move_init2(&wk->wu, 9, 36, 7, 0);
        g_state.win_free[wk->wu.id] = 48;
        break;

    case 3:
        g_state.win_free[wk->wu.id]--;

        if (g_state.win_free[wk->wu.id] > 0) {
            break;
        }

        g_state.win_rno[1]++;

        if (wk->wu.rl_flag) {
            wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].xy[0].disp.pos - 328;
            wk->wu.mvxy.a[0].sp = 0x18000;
        } else {
            wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].xy[0].disp.pos + 328;
            wk->wu.mvxy.a[0].sp = -0x18000;
        }

        wk->wu.mvxy.d[0].sp = 0;
        wk->wu.xyz[1].cal = 0;
        /* fallthrough */

    case 4:
        add_x_sub((State_Other*)wk);
        char_move(&wk->wu);

        break;
    }
}

/** @brief Oro full-power win sub-sequence. */
static void jijii_full(PLW* wk) {
    g_state.bg_app_stop = 1;

    switch (g_state.win_rno[1]) {
    case 0:
        char_move(&wk->wu);
        if (wk->wu.script_register_bank[0] == 1) {
            g_state.win_rno[1]++;
            break;
        }

        break;

    case 1:
        char_move(&wk->wu);
        wk->wu.xyz[1].cal += 0x10000;

        if (wk->wu.xyz[1].disp.pos >= 42) {
            g_state.win_rno[1]++;
            wk->wu.script_register_bank[0] = 2;
            set_char_move_init(&wk->wu, 9, 43);
            break;
        }

        break;

    case 2:
        char_move(&wk->wu);
        break;
    }
}

const s16 win_2000_tbl[18] = { 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1 };

/** @brief Win type 2 — win with stage-dependent variant. */
static void Win_02000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;

        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        work = win_select(wk, 3);

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            if (win_2000_tbl[g_state.bg_w.bg_index]) {
                set_char_move_init(&wk->wu, 9, work + 36);
            } else if (work & 1) {
                set_char_move_init(&wk->wu, 9, 32);
            } else {
                set_char_move_init(&wk->wu, 9, 38);
            }
        } else {
            set_char_move_init(&wk->wu, 9, 32);
        }

        if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
            set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
        }

        break;

    default:
        Normal_normal_Winner(wk);
        break;
    }
}

const s16 Win_3000_tbl[16] = { 42, 34, 33, 42, 32, 42, 32, 35, 42, 34, 33, 42, 32, 42, 32, 35 };

const s8 Win_3001_tbl[16] = { 36, 40, 41, 40, 41, 38, 40, 39, 36, 40, 41, 39, 41, 37, 39, 40 };

/** @brief Win type 3 — win with vanish and stage-specific poses. */
static void Win_03000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        work = win_select(wk, 15);

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            if (g_state.bg_w.stage == 7) {
                set_char_move_init(&wk->wu, 9, 43);
                break;
            }

            set_char_move_init(&wk->wu, 9, Win_3001_tbl[work]);

            if (Win_3001_tbl[work] == 41) {
                g_state.win_rno[0] = 1;
            }

            break;
        }

        set_char_move_init(&wk->wu, 9, Win_3000_tbl[work]);
        break;

    default:
        if (g_state.win_rno[0]) {
            char_move(&wk->wu);

            if (wk->wu.cg_type == 0xFF) {
                wk->wu.disp_flag = 0;
                g_state.win_rno[0] = 0;
            }

            break;
        }

        char_move(&wk->wu);
        break;
    }

    if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
        set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
    }
}

/** @brief Win type 4 — win with continued-animation variant. */
static void Win_04000(PLW* wk) {
    s16 work;
    s16 work2;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;

        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        work = win_select(wk, 3);

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            set_char_move_init(&wk->wu, 9, work + 36);
            break;
        }

        switch (work) {
        case 1:
        case 3:
            if (wk->wu.current_char_type == 0 && wk->wu.char_index == 0) {
                work2 = wk->wu.graphic_index / wk->wu.char_graphic_data_type;
                work2 += 2;
                set_char_move_init2(&wk->wu, 9, work + 32, work2, 0);
            } else {
                set_char_move_init(&wk->wu, 9, work + 32);
            }

            break;

        default:
            set_char_move_init(&wk->wu, 9, work + 32);
            break;
        }

        break;

    default:
    case 1:
        char_move(&wk->wu);
        break;
    }

    if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
        set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
    }
}

/** @brief Standard normal-round winner animation (random pose pick). */
static void Normal_normal_Winner(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        work = win_select(wk, 7);
        set_char_move_init(&wk->wu, 9, work + 32);
        break;

    case 1:
    case 9:
        char_move(&wk->wu);
        break;
    }

    if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
        set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
    }
}

/** @brief Judge-round winner animation (random verdict pose). */
static void Judge_normal_winner(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;
        work = win_select(wk, 3);
        set_char_move_init(&wk->wu, 9, work + 52);
        break;

    case 1:
    case 9:
        char_move(&wk->wu);
        break;
    }

    if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
        set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
    }
}

/** @brief Win type 5 — Dudley leap-away / final-round special. */
static void Win_05000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type]) {
            set_char_move_init(&wk->wu, 9, 36);

            if (wk->wu.rl_flag) {
                wk->wu.mvxy.a[0].sp = 0x20000;
            } else {
                wk->wu.mvxy.a[0].sp = -0x20000;
            }

            wk->wu.mvxy.d[0].sp = 0;
            wk->wu.mvxy.a[1].sp = 0x80000;
            wk->wu.mvxy.d[1].sp = -0x6000;
            g_state.win_rno[0] = 0;
            break;
        }

        work = win_select(wk, 3);
        set_char_move_init(&wk->wu, 9, work + 32);
        g_state.win_rno[0] = 1;
        break;

    default:
        if (g_state.win_rno[0]) {
            Normal_normal_Winner(wk);
            break;
        }

        switch (g_state.win_rno[1]) {
        case 0:
            char_move(&wk->wu);
            add_x_sub((State_Other*)wk);
            add_y_sub((State_Other*)wk);

            if (wk->wu.xyz[1].disp.pos < 0) {
                g_state.win_rno[1]++;
                wk->wu.xyz[1].cal = 0;
                char_move_z(&wk->wu);
            }

            break;

        case 1:
            char_move(&wk->wu);
            break;
        }
    }

    if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
        set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
    }
}

/** @brief Win type 6 — generic win with round-dependent pose selection. */
static void Win_06000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            work = win_select(wk, 3);
            set_char_move_init(&wk->wu, 9, work + 36);
        } else {
            work = win_select(wk, 3);
            set_char_move_init(&wk->wu, 9, work + 32);
        }

        if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
            set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
        }

        break;

    default:
        Normal_normal_Winner(wk);
        break;
    }
}

/** @brief Win type 7 — Hugo’s win with optional Poison/Hugo effects. */
static void Win_07000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            work = win_select(wk, 7);

            if (work < 4) {
                if (g_state.plw[0].player_number == 5 && g_state.plw[1].player_number == 5) {
                    g_state.win_rno[0] = 0;
                    set_char_move_init(&wk->wu, 9, work + 32);
                    break;
                }

                effect_82_init(&wk->wu);
                g_state.win_rno[0] = 1;
                set_char_move_init(&wk->wu, 9, 60);
                wk->wu.script_register_bank[1] = 0;
                break;
            }

            if (g_state.plw[0].player_number == 5 && g_state.plw[1].player_number == 5) {
                g_state.win_rno[0] = 0;
                set_char_move_init(&wk->wu, 9, work + 32);
                break;
            }

            effect_83_init(&wk->wu);
            g_state.win_rno[0] = 2;
            set_char_move_init(&wk->wu, 9, 60);
            wk->wu.script_register_bank[1] = 0;
            break;
        }

        g_state.win_rno[0] = 0;
        work = win_select(wk, 7);
        set_char_move_init(&wk->wu, 9, work + 32);
        break;

    default:
        switch (g_state.win_rno[0]) {
        case 0:
            char_move(&wk->wu);
            break;

        default:
            if (g_state.win_rno[1] == 0) {
                if (wk->wu.script_register_bank[1]) {
                    g_state.win_rno[1]++;

                    if (g_state.win_rno[0] == 1) {
                        set_char_move_init(&wk->wu, 9, 32);
                    } else {
                        set_char_move_init(&wk->wu, 9, 37);
                    }

                    break;
                }

                char_move(&wk->wu);
                break;
            }

            char_move(&wk->wu);
        }
    }

    if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
        set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
    }
}

/** @brief Win type 8 — Ibuki’s win with super-finish variant. */
static void Win_08000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;

        if (g_state.Round_Result & 0x800) {
            set_char_move_init(&wk->wu, 9, 40);
        } else if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
                   g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            work = win_select(wk, 3);
            set_char_move_init(&wk->wu, 9, work + 36);
        } else {
            work = win_select(wk, 3);
            set_char_move_init(&wk->wu, 9, work + 32);
        }

        if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
            set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
        }

        break;

    default:
        Normal_normal_Winner(wk);
        break;
    }
}

/** @brief Win type 9 — Necro’s win with poison and L6 effects. */
static void Win_09000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;
        work = win_select(wk, 7);

        if (work == 7) {
            set_char_move_init(&wk->wu, 9, 32);
        } else {
            set_char_move_init(&wk->wu, 9, (work) + 32);
        }

        if (g_state.Round_num < (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) &&
            g_state.PL_Wins[wk->wu.id] < CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            break;
        }

        if (g_state.poison_flag[wk->wu.id]) {
            break;
        }

        switch (work) {
        case 0:
            effect_L6_init(&wk->wu, 0);
            break;

        case 3:
            effect_30_init(&wk->wu);
            break;

        case 4:
            effect_31_init(&wk->wu);
            break;

        case 5:
            effect_32_init(&wk->wu);
            break;

        case 7:
            wk->wu.script_register_bank[0] = 0;
            effect_L6_init(&wk->wu, 1);
            set_char_move_init(&wk->wu, 0, 0);
            g_state.win_rno[0] = 1;
            break;
        }

        break;

    default:
        if (g_state.win_rno[0]) {
            switch (g_state.win_rno[1]) {
            case 0:
                char_move(&wk->wu);

                if (wk->wu.script_register_bank[0]) {
                    g_state.win_rno[1]++;
                    set_char_move_init(&wk->wu, 9, 39);
                }

                break;

            case 1:
                char_move(&wk->wu);
                break;
            }

            break;
        }

        Normal_normal_Winner(wk);
        break;
    }

    if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
        set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
    }
}

/** @brief Win type 10 — Q’s win with keeping/leave-after actions. */
static void Win_10000(PLW* wk) {
    s16 work;
    s16 work2;
    s16 id_w;

    g_state.bg_app_stop = 1;

    id_w = wk->wu.id ^ 1;
    wk->wu.position_z = wk->wu.next_z = g_state.plw[id_w].wu.position_z + 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;
        work = win_select(wk, 3);

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            work2 = wk->wu.xyz[0].disp.pos - g_state.plw[id_w].wu.xyz[0].disp.pos;

            if (work2 < 0) {
                work2 = -work2;
            }

            if (work2 > 224) {
                if (work & 1) {
                    g_state.win_rno[0] = 1;
                } else {
                    g_state.win_rno[0] = 2;
                }
            } else if (work > 1) {
                if (work & 1) {
                    if (g_state.plw[id_w].wu.char_index != 67) {
                        g_state.win_rno[0] = 1;
                    } else {
                        g_state.win_rno[0] = 3;
                    }
                } else if (g_state.plw[id_w].wu.char_index != 67) {
                    g_state.win_rno[0] = 2;
                } else {
                    g_state.win_rno[0] = 4;
                }
            } else if (work & 1) {
                g_state.win_rno[0] = 1;
            } else {
                g_state.win_rno[0] = 2;
            }
        } else {
            set_char_move_init(&wk->wu, 9, work + 32);
        }

        if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
            set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
        }

        break;

    default:
        switch (g_state.win_rno[0]) {
        case 0:
            Normal_normal_Winner(wk);
            break;

        case 1:
        case 3:
            q_keeping_action(wk);
            break;

        case 2:
        case 4:
            q_leave_after_action(wk);
            break;
        }
    }
}

const s16 q_em_distance_tbl[20][2] = { { -96, -16 }, { -104, 0 },  { -90, -16 }, { -100, -8 }, { -100, 0 },
                                       { -106, 0 },  { 12, -117 }, { -84, -21 }, { -112, 0 },  { -106, 4 },
                                       { -100, 0 },  { -90, -16 }, { -90, -16 }, { -96, -16 }, { -90, -16 },
                                       { -90, -16 }, { 0, -96 },   { -2, -112 }, { -112, 4 },  { -96, -6 } };

/** @brief Check Q’s distance from the enemy for win animation. */
static s16 q_em_distance_chk(PLW* wk) {
    s16 work;
    s16 id_w = wk->wu.id ^ 1;
    s16 rl_w = wk->wu.rl_flag ^ g_state.plw[id_w].wu.rl_flag;

    if (wk->wu.rl_flag) {
        work = wk->wu.xyz[0].disp.pos - g_state.plw[id_w].wu.xyz[0].disp.pos;

        if (work >= q_em_distance_tbl[g_state.plw[id_w].player_number][rl_w]) {
            return 1;
        }
    } else {
        work = g_state.plw[id_w].wu.xyz[0].disp.pos - wk->wu.xyz[0].disp.pos;

        if (work >= q_em_distance_tbl[g_state.plw[id_w].player_number][rl_w]) {
            return 1;
        }
    }

    return 0;
}

/** @brief Determine Q’s facing direction relative to the enemy. */
static s32 q_em_dir(PLW* wk) {
    s16 work;
    s16 pos_w;
    s16 id_w = wk->wu.id ^ 1;

    work = wk->wu.xyz[0].disp.pos - g_state.plw[id_w].wu.xyz[0].disp.pos;

    if (work < 0) {
        wk->wu.direction = 1;
    } else {
        wk->wu.direction = 0;
    }

    pos_w = wk->wu.xyz[0].disp.pos;

    if (q_em_distance_chk(wk)) {
        if (g_state.win_rno[0] == 3) {
            g_state.win_rno[0] = 1;
            set_char_move_init(&wk->wu, 9, 36);
        } else if (g_state.win_rno[0] == 4) {
            g_state.win_rno[0] = 2;
            set_char_move_init(&wk->wu, 9, 36);
        }

        wk->wu.direction = wk->wu.rl_flag;
        g_state.win_rno[1] = 4;
        wk->wu.xyz[0].disp.pos = pos_w;
        return 0;
    }

    wk->wu.xyz[0].disp.pos = pos_w;
    return 1;
}

/** @brief Q’s keeping-action win sub-sequence (stance hold). */
static void q_keeping_action(PLW* wk) {
    switch (g_state.win_rno[1]) {
    case 0:
        if (!q_em_dir(wk)) {
            break;
        }

        if (wk->wu.direction == wk->wu.rl_flag) {
            g_state.win_rno[1] = 2;
            break;
        }

        g_state.win_rno[1] = 1;
        set_char_move_init(&wk->wu, 9, 40);
        wk->wu.rl_flag ^= 1;
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            g_state.win_rno[1]++;
            break;
        }

        break;

    case 2:
        g_state.win_rno[1]++;
        set_char_move_init(&wk->wu, 9, 41);
        wk->wu.mvxy.d[0].sp = 0;

        if (wk->wu.rl_flag) {
            wk->wu.mvxy.a[0].sp = 0x1C000;
            break;
        }

        wk->wu.mvxy.a[0].sp = -0x1C000;
        break;

    case 3:
        char_move(&wk->wu);
        add_x_sub((State_Other*)wk);

        if (!q_em_distance_chk(wk)) {
            break;
        }

        g_state.win_rno[1]++;

        if (g_state.win_rno[0] == 1) {
            set_char_move_init(&wk->wu, 9, 36);
            break;
        }

        set_char_move_init(&wk->wu, 9, 37);
        break;

    case 4:
        char_move(&wk->wu);
        break;
    }
}

/** @brief Q’s leave-after-action win sub-sequence (walk away). */
static void q_leave_after_action(PLW* wk) {
    s16 work;

    switch (g_state.win_rno[1]) {
    case 0:
        if (q_em_dir(wk) == 0) {
            break;
        }

        if (wk->wu.direction == wk->wu.rl_flag) {
            g_state.win_rno[1] = 2;
        } else {
            g_state.win_rno[1] = 1;
            set_char_move_init(&wk->wu, 9, 40);
            wk->wu.rl_flag ^= 1;
        }

        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            g_state.win_rno[1]++;
        }

        break;

    case 2:
        g_state.win_rno[1]++;
        set_char_move_init(&wk->wu, 9, 41);
        wk->wu.mvxy.d[0].sp = 0;

        if (wk->wu.rl_flag) {
            wk->wu.mvxy.a[0].sp = 0x1C000;
        } else {
            wk->wu.mvxy.a[0].sp = -0x1C000;
        }

        break;

    case 3:
        char_move(&wk->wu);
        add_x_sub((State_Other*)wk);

        if (q_em_distance_chk(wk)) {
            g_state.win_rno[1]++;

            if (g_state.win_rno[0] == 2) {
                set_char_move_init(&wk->wu, 9, 36);
            } else {
                set_char_move_init(&wk->wu, 9, 39);
            }
        }

        break;

    case 4:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            g_state.win_rno[1]++;
            set_char_move_init(&wk->wu, 9, 41);
            wk->wu.mvxy.d[0].sp = 0;

            if (wk->wu.rl_flag) {
                wk->wu.mvxy.a[0].sp = 0x1C000;
            } else {
                wk->wu.mvxy.a[0].sp = -0x1C000;
            }
        }

        break;

    case 5:
        char_move(&wk->wu);
        add_x_sub((State_Other*)wk);

        if (wk->wu.rl_flag) {
            work = g_state.bg_w.bgw[1].wxy[0].disp.pos + g_state.bg_w.pos_offset;
            work += 64;

            if (work < wk->wu.xyz[0].disp.pos) {
                g_state.win_rno[1]++;
            }

            break;
        }

        work = g_state.bg_w.bgw[1].wxy[0].disp.pos - g_state.bg_w.pos_offset;
        work -= 64;

        if (work > wk->wu.xyz[0].disp.pos) {
            g_state.win_rno[1]++;
        }

        break;
    }
}

/** @brief Win type 11 — Twelve’s win (walk-away or backjump variants). */
static void Win_11000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;
        work = win_select(wk, 3);

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            if (g_state.Perfect_Flag) {
                g_state.win_rno[0] = 1;
                set_char_move_init(&wk->wu, 9, 38);
                effect_L3_init(wk);
            } else {
                set_char_move_init(&wk->wu, 9, work + 36);
                switch (work) {
                case 0:
                    g_state.win_rno[0] = 2;
                    break;

                case 1:
                    break;

                default:
                    effect_L3_init(wk);
                    g_state.win_rno[0] = 1;
                    break;
                }
            }
        } else {
            g_state.win_rno[0] = 0;
            set_char_move_init(&wk->wu, 9, work + 32);
        }

        if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
            set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
        }

        break;

    default:
        switch (g_state.win_rno[0]) {
        case 0:
            Normal_normal_Winner(wk);
            break;

        case 1:
            twelve_win_away(wk);
            break;

        case 2:
            twelve_win_backjump(wk);
            break;
        }
    }
}

/** @brief Twelve walk-away win sub-sequence. */
static void twelve_win_away(PLW* wk) {
    switch (g_state.win_rno[1]) {
    case 0:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 1) {
            g_state.win_rno[1]++;
            wk->wu.mvxy.a[0].sp = 0;
            wk->wu.mvxy.d[0].sp = 0;
            wk->wu.mvxy.a[1].sp = 0x78000;
            wk->wu.mvxy.d[1].sp = -0x6000;
        }

        break;

    case 1:
        add_y_sub((State_Other*)wk);
        char_move(&wk->wu);

        if (wk->wu.cg_type != 2) {
            break;
        }

        g_state.win_rno[1]++;
        wk->wu.mvxy.d[0].sp = 0;

        if (wk->wu.rl_flag) {
            wk->wu.mvxy.a[0].sp = 0x80000;
        } else {
            wk->wu.mvxy.a[0].sp = -0x80000;
        }

        wk->wu.mvxy.a[1].sp = -0x8000;
        wk->wu.mvxy.d[1].sp = 0x4000;
        break;

    case 2:
        add_x_sub((State_Other*)wk);
        add_y_sub((State_Other*)wk);

        if (!range_x_check3((State_Other*)wk, 208)) {
            g_state.win_rno[1]++;
        }

        break;

    case 3:
        break;
    }
}

/** @brief Twelve backjump win sub-sequence. */
static void twelve_win_backjump(PLW* wk) {
    switch (g_state.win_rno[1]) {
    case 0:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 1) {
            g_state.win_rno[1]++;
            wk->wu.mvxy.a[0].sp = 0x30000;
            wk->wu.mvxy.d[0].sp = 0;
            wk->wu.mvxy.a[1].sp = 0x78000;
            wk->wu.mvxy.d[1].sp = -0x5000;

            if (wk->wu.rl_flag) {
                wk->wu.mvxy.a[0].sp = -wk->wu.mvxy.a[0].sp;
            }
        }

        if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
            set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
        }

        break;

    case 1:
        add_y_sub((State_Other*)wk);
        add_x_sub((State_Other*)wk);
        char_move(&wk->wu);

        if (wk->wu.cg_type == 2) {
            g_state.win_rno[1]++;
            char_move_z(&wk->wu);
            wk->wu.xyz[1].cal = 0;
        }

        if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
            set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
        }

        break;

    case 2:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 9) {
            g_state.win_rno[1]++;
        }

        break;

    case 3:
        char_move(&wk->wu);
        wk->wu.xyz[1].cal += 0x20000;

        if (wk->wu.xyz[1].disp.pos > 256) {
            g_state.win_rno[1]++;
        }

        break;

    case 4:
        break;
    }
}

/** @brief Win type 12 — Makoto’s win with taunt. */
static void Win_12000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;
        work = win_select(wk, 7);
        set_char_move_init(&wk->wu, 9, work + 32);

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            effect_M2_init(&wk->wu, 1);
        }

        if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
            set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
        }

        break;

    default:
        Normal_normal_Winner(wk);
        break;
    }
}

/** @brief Win type 13 — Remy’s win animation. */
static void Win_13000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            if (wk->wu.id) {
                if (p2sw_0 & 1) {
                    set_char_move_init(&wk->wu, 9, 40);
                    return;
                }
            } else if (p1sw_0 & 1) {
                set_char_move_init(&wk->wu, 9, 40);
                return;
            }

            work = win_select(wk, 3);
            set_char_move_init(&wk->wu, 9, work + 36);
        } else {
            work = win_select(wk, 3);
            set_char_move_init(&wk->wu, 9, work + 32);
        }

        if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
            set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
        }

        break;

    default:
        Normal_normal_Winner(wk);
        break;
    }
}

/** @brief Win type 14 — Urien’s win with dash and M2 effect. */
static void Win_14000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            work = win_select(wk, 3);

            if (!(work & 1)) {
                g_state.win_rno[0] = 1;
            } else {
                set_char_move_init(&wk->wu, 9, work + 36);
            }
        } else {
            work = win_select(wk, 3);
            set_char_move_init(&wk->wu, 9, work + 32);
        }

        break;

    default:
        if (g_state.win_rno[0]) {
            urien_dash(wk);
        } else {
            Normal_normal_Winner(wk);
        }

        break;
    }

    if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
        set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
    }
}

/** @brief Check if Urien should dash toward the enemy for his win. */
static s32 urien_dash_chk(PLW* wk) {
    s16 id_w = wk->wu.id ^ 1;
    s16 pos_w = wk->wu.xyz[0].disp.pos - g_state.plw[id_w].wu.xyz[0].disp.pos;

    if (pos_w < 0) {
        pos_w = -pos_w;

        if (!wk->wu.active_move) {
            wk->wu.active_move = 1;
        }
    } else if (wk->wu.active_move) {
        wk->wu.active_move = 0;
    }

    if (pos_w < 88) {
        return 1;
    }

    return 0;
}

/** @brief Urien dash-toward-enemy win sub-sequence. */
static void urien_dash(PLW* wk) {
    switch (g_state.win_rno[1]) {
    case 0:
        g_state.win_rno[1]++;

        if (urien_dash_chk(wk)) {
            g_state.win_rno[1] = 5;
        } else {
            wk->wu.rl_flag = wk->wu.active_move;
            set_char_move_init(&wk->wu, 0, 4);
            setup_mvxy_data(&wk->wu, 2);
        }

        /* fallthrough */

    case 1:
        if (wk->wu.cg_type == 1) {
            g_state.win_rno[1]++;
            add_mvxy_speed(&wk->wu);
        } else {
            char_move(&wk->wu);
        }

        break;

    case 2:
        add_mvxy_speed(&wk->wu);
        cal_mvxy_speed(&wk->wu);
        char_move(&wk->wu);

        if (wk->wu.xyz[1].disp.pos + wk->wu.cg_jphos >= 1) {
            break;
        }

        g_state.win_rno[1]++;
        wk->wu.position_y = 0;
        wk->wu.xyz[1].cal = 0;
        wk->wu.mvxy.a[1].sp = 0;
        char_move_cmja(&wk->wu);
        break;

    case 3:
        char_move(&wk->wu);

        if (wk->wu.cg_type != 64) {
            break;
        }

        if (urien_dash_chk(wk)) {
            g_state.win_rno[1]++;
        } else {
            g_state.win_rno[1] = 0;
        }

        break;

    case 4:
        char_move(&wk->wu);

        if (wk->wu.cg_type != 0xFF) {
            break;
        }

        g_state.win_rno[1]++;
        /* fallthrough */

    case 5:
        g_state.win_rno[1]++;
        set_char_move_init(&wk->wu, 9, 36);
        break;

    case 6:
        char_move(&wk->wu);
        break;
    }
}

const s16 Win_15000_tbl[8] = { 38, 37, 40, 39, 38, 40, 39, 36 };

/** @brief Win type 15 — Gill’s win animation. */
static void Win_15000(PLW* wk) {
    s16 work;

    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        g_state.win_rno[0] = g_state.win_rno[1] = 0;
        wk->wu.routine_no[3]++;

        if (g_state.Round_num >= (CurrentSave()->Battle_Number[g_state.Play_Type] * 2) ||
            g_state.PL_Wins[wk->wu.id] >= CurrentSave()->Battle_Number[g_state.Play_Type] + 1) {
            work = win_select(wk, 7);
            set_char_move_init(&wk->wu, 9, Win_15000_tbl[work]);
        } else {
            work = win_select(wk, 3);
            set_char_move_init(&wk->wu, 9, work + 32);
        }

        if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
            set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
        }

        break;

    default:
        Normal_normal_Winner(wk);
        break;
    }
}

/** @brief Select a random win-pose index masked to num+1 variants. */
static s16 win_select(PLW* /* unused */, s16 num) {
    s16 work = random_16();
    work &= num;
    return work;
}

/** @brief Bonus-game win-pause animation. */
static void bonus_game_win_pause(PLW* wk) {
    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        g_state.win_rno[0] = g_state.win_rno[1] = 0;

        if (g_state.Bonus_Game_Flag == 20) {
            if (wk->wu.pl_operator) {
                if (g_state.Time_Over) {
                    set_char_move_init(&wk->wu, 9, 67);
                } else {
                    set_char_move_init(&wk->wu, 9, 65);
                }

                break;
            }

            wk->wu.routine_no[3] = 99;
            break;
        }

        if (wk->wu.pl_operator) {
            if (g_state.Bonus_Game_result == 20 || g_state.Bonus_Game_ex_result == 20) {
                set_char_move_init(&wk->wu, 9, 65);
                break;
            }

            if (g_state.Bonus_Game_result > 10) {
                set_char_move_init(&wk->wu, 9, 66);
                break;
            }

            set_char_move_init(&wk->wu, 9, 67);
            break;
        }

        if (g_state.Bonus_Game_result == 20 || g_state.Bonus_Game_ex_result == 20) {
            g_state.win_rno[0] = 1;

            if (wk->wu.rl_flag) {
                wk->wu.mvxy.a[0].sp = 0x20000;
            } else {
                wk->wu.mvxy.a[0].sp = -0x20000;
            }

            wk->wu.mvxy.d[0].sp = 0;
            wk->wu.mvxy.a[1].sp = 0x80000;
            wk->wu.mvxy.d[1].sp = -0x6000;
            g_state.win_rno[0] = 0;
            set_char_move_init(&wk->wu, 9, 66);
            break;
        }

        set_char_move_init(&wk->wu, 9, 52);
        break;

    case 1:
    case 9:
        char_move(&wk->wu);
        break;
    }

    if (set_field_adjust_flag(&g_state.plw[1], g_state.bs_scrrrl[1][0], 1)) {
        set_field_adjust_flag(&g_state.plw[1], g_state.bs_scrrrl[1][1], 0);
    }

    if (set_field_adjust_flag(&g_state.plw[0], g_state.bs_scrrrl[0][0], 1)) {
        set_field_adjust_flag(&g_state.plw[0], g_state.bs_scrrrl[0][1], 0);
    }
}

const s16 meta_win_tbl[CHARACTER_COUNT] = { 33, 32, 32, 32, 32, 32, 33, 32, 32, 37,
                                            32, 32, 32, 32, 34, 32, 32, 32, 32, 32 };

/** @brief Meta-character (Gill) win-pause animation. */
static void meta_win_pause(PLW* wk) {
    g_state.bg_app_stop = 1;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        if (wk->player_number >= 0 && wk->player_number < CHARACTER_COUNT) {
            set_char_move_init(&wk->wu, 9, meta_win_tbl[wk->player_number]);
        }
        break;

    case 1:
    case 9:
        char_move(&wk->wu);
        break;
    }

    if (g_state.Bonus_Game_Flag) {
        if (set_field_adjust_flag(&g_state.plw[1], g_state.bs_scrrrl[1][0], 1)) {
            set_field_adjust_flag(&g_state.plw[1], g_state.bs_scrrrl[1][1], 0);
        }

        if (set_field_adjust_flag(&g_state.plw[0], g_state.bs_scrrrl[0][0], 1)) {
            set_field_adjust_flag(&g_state.plw[0], g_state.bs_scrrrl[0][1], 0);
        }
    } else {
        if (set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrr, 1)) {
            set_field_adjust_flag(&g_state.plw[wk->wu.id], g_state.scrl, 0);
        }
    }
}
