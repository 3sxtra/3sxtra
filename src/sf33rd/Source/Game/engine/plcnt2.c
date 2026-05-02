/**
 * @file plcnt2.c
 * Player Controller for Bonus Stages
 */

#include "sf33rd/Source/Game/engine/plcnt2.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/cmd_main.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/manage.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/plmain2.h"
#include "sf33rd/Source/Game/engine/plpdm.h"
#include "sf33rd/Source/Game/engine/pls01.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/sys_sub.h"

#include "port/I_System.h"

static void setup_bs_scrrrl_bs();
static void setup_bs_scrrrl_bs2();
static void move_P1_move_P2_bonus(s16* field_work);
static void move_P2_move_P1_bonus(s16* field_work);

const s16 bsmr_range_table[3][2][2] = { { { 192, 192 }, { 192, 192 } },
                                        { { 64, 192 }, { 224, -136 } },
                                        { { -112, 224 }, { 216, 40 } } };

static void plcnt_b_move();
static void plcnt_b_die();

void (*const player_bonus_process[3])() = { plcnt_b_init, plcnt_b_move, plcnt_b_die };

/** @brief Main player controller for the basketball bonus stage. */
s32 Player_control_bonus() {
    if (((g_state.pcon_rno[0] + g_state.pcon_rno[1]) == 0) || (!g_state.Game_pause && !g_state.EXE_flag)) {
        g_state.players_timer++;
        g_state.players_timer &= 0x7FFF;
        player_bonus_process[g_state.pcon_rno[0]]();
        check_body_touch();
        check_damage_hosei_bonus();
        set_quake(&g_state.plw[0]);
        set_quake(&g_state.plw[1]);

        if (!g_state.plw[0].zuru_flag && !g_state.plw[0].zettai_muteki_flag) {
            hit_push_request(&g_state.plw[0].wu);
        }

        if (!g_state.plw[1].zuru_flag && !g_state.plw[1].zettai_muteki_flag) {
            hit_push_request(&g_state.plw[1].wu);
        }

        add_next_position(g_state.plw);
        add_next_position(&g_state.plw[1]);
        check_cg_zoom();
    }

    if (g_state.Game_pause != 0x81) {
        store_player_after_image_data();
    }

    if (g_state.pcon_rno[0] == 2 && g_state.pcon_rno[1] == 0 && g_state.pcon_rno[2] == 2) {
        return 1;
    }

    return 0;
}

/** @brief Initializes player work for the bonus stage. */
void plcnt_b_init() {
    switch (g_state.pcon_rno[1]) {
    case 0:
        g_state.pcon_rno[1] = 2;
        I_ZeroArray(g_state.plw);
        setup_base_and_other_data();
        g_state.pcon_dp_flag = false;
        g_state.round_slow_flag = false;
        g_state.dead_voice_flag = false;
        g_state.another_bg[0] = g_state.another_bg[1] = 0;
        g_state.plw[0].scr_pos_set_flag = g_state.plw[1].scr_pos_set_flag = 1;
        clear_super_arts_point(&g_state.plw[0]);
        clear_super_arts_point(&g_state.plw[1]);

        if (g_state.Bonus_Game_Flag == 21) {
            setup_bs_scrrrl_bs();
        }

        break;

    case 1:
        if (g_state.plw[0].wu.routine_no[0] != 3) {
            break;
        }

        if (g_state.plw[1].wu.routine_no[0] != 3) {
            break;
        }

        if (!g_state.Allow_a_battle_f) {
            break;
        }

        g_state.pcon_rno[0] = 1;
        g_state.pcon_rno[1] = 0;
        g_state.plw[0].wu.routine_no[0] = 4;
        g_state.plw[1].wu.routine_no[0] = 4;
        ca_check_flag = 1;

        break;

    case 2:
        g_state.pcon_rno[1] = 3;

        if (g_state.plw[0].wu.pl_operator) {
            g_state.parry_ctr_vs[0][0] = g_state.parry_ctr_ori[0];
        } else {
            g_state.parry_ctr_vs[0][0] = 0;
        }

        if (g_state.plw[1].wu.pl_operator) {
            g_state.parry_ctr_vs[0][1] = g_state.parry_ctr_ori[1];
        } else {
            g_state.parry_ctr_vs[0][1] = 0;
        }

        break;

    case 3:
        g_state.pcon_rno[1] = 1;
        pli_0002();
        break;
    }

    move_player_work_bonus();
}

/** @brief Per-frame bonus stage player movement and state update. */
static void plcnt_b_move() {
    if (g_state.No_Death) {
        g_state.plw[0].wu.dm_vital = g_state.plw[1].wu.dm_vital = 0;
    }

    if (g_state.Break_Into) {
        g_state.plw[0].wu.dm_vital = g_state.plw[1].wu.dm_vital = 0;
    }

    move_player_work_bonus();

    if (g_state.mutual_trade_flag) {
        subtract_dm_vital_aiuchi(&g_state.plw[0]);
        subtract_dm_vital_aiuchi(&g_state.plw[1]);

        if ((g_state.plw[0].dead_flag != 0) && (g_state.plw[1].dead_flag != 0)) {
            g_state.plw[0].wu.hit_stop = g_state.plw[1].wu.hit_stop = 2;
            g_state.plw[0].wu.dm_stop = g_state.plw[1].wu.dm_stop = 0;
            g_state.plw[0].wu.hit_quake = g_state.plw[1].wu.hit_quake = 4;
            g_state.plw[0].wu.dm_quake = g_state.plw[1].wu.dm_quake = 0;
        } else if ((g_state.plw[0].dead_flag != 0) || (g_state.plw[1].dead_flag != 0)) {
            g_state.plw[0].wu.hit_stop = g_state.plw[1].wu.hit_stop = 4;
            g_state.plw[0].wu.dm_stop = g_state.plw[1].wu.dm_stop = 0;
            g_state.plw[0].wu.hit_quake = g_state.plw[1].wu.hit_quake = 8;
            g_state.plw[0].wu.dm_quake = g_state.plw[1].wu.dm_quake = 0;
        }
    }

    if (g_state.Bonus_Stage_RNO[0] == 2) {
        g_state.pcon_rno[0] = 2;
    }
}

/** @brief Handles bonus stage KO/completion finalization. */
static void plcnt_b_die() {
    g_state.plw[0].wu.dm_vital = g_state.plw[1].wu.dm_vital = 0;

    switch (g_state.pcon_rno[2]) {
    case 0:
        g_state.plw[0].wkey_flag = g_state.plw[1].wkey_flag = 1;
        g_state.plw[0].image_setup_flag = g_state.plw[1].image_setup_flag = 0;
        g_state.pcon_rno[2]++;
        /* fallthrough */

    case 1:
        if (footwork_check_bns(0) && footwork_check_bns(1)) {
            g_state.pcon_rno[2]++;
        }

        break;

    case 2:
        complete_victory_pause();
        g_state.plw[0].wu.routine_no[2] = 40;
        g_state.plw[1].wu.routine_no[2] = 40;
        g_state.plw[0].wu.routine_no[1] = g_state.plw[1].wu.routine_no[1] = 0;
        g_state.plw[0].wu.routine_no[3] = g_state.plw[1].wu.routine_no[3] = 0;
        g_state.plw[0].wu.cg_type = g_state.plw[1].wu.cg_type = 0;
        g_state.pcon_rno[2]++;
        break;

    case 3:
        if ((g_state.plw[0].wu.routine_no[3] == 9) && (g_state.plw[1].wu.routine_no[3] == 9)) {
            g_state.pcon_rno[2]++;
        }

        break;
    }

    move_player_work_bonus();
}

/** @brief Returns whether a player is performing footwork in bonus stage context. */
s16 footwork_check_bns(s8 ix) {
    s16 rnum = 0;

    if ((g_state.Bonus_Game_Flag == 20) && g_state.plw[ix].wu.pl_operator == 0) {
        return 1;
    }

    if ((g_state.plw[ix].wu.routine_no[1] == 0) && (g_state.plw[ix].wu.routine_no[2] == 1)) {
        rnum = 1;
    }

    return rnum;
}

/** @brief Sets up scroll boundaries for the basketball bonus stage. */
static void setup_bs_scrrrl_bs() {
    s16 scrc = 512;

    switch (g_state.plw[0].wu.pl_operator + (g_state.plw[1].wu.pl_operator * 2)) {
    case 1:
        g_state.bs_scrrrl[0][0] = scrc + bsmr_range_table[1][0][0];
        g_state.bs_scrrrl[0][1] = scrc - bsmr_range_table[1][0][1];
        g_state.bs_scrrrl[1][0] = scrc + bsmr_range_table[1][1][0];
        g_state.bs_scrrrl[1][1] = scrc - bsmr_range_table[1][1][1];
        break;

    case 2:
        g_state.bs_scrrrl[0][0] = scrc + bsmr_range_table[2][0][0];
        g_state.bs_scrrrl[0][1] = scrc - bsmr_range_table[2][0][1];
        g_state.bs_scrrrl[1][0] = scrc + bsmr_range_table[2][1][0];
        g_state.bs_scrrrl[1][1] = scrc - bsmr_range_table[2][1][1];
        break;

    default:
        g_state.bs_scrrrl[0][0] = scrc + bsmr_range_table[0][0][0];
        g_state.bs_scrrrl[0][1] = scrc - bsmr_range_table[0][0][1];
        g_state.bs_scrrrl[1][0] = scrc + bsmr_range_table[0][1][0];
        g_state.bs_scrrrl[1][1] = scrc - bsmr_range_table[0][1][1];
        break;
    }
}

/** @brief Sets up scroll boundaries for the car-crush bonus stage. */
static void setup_bs_scrrrl_bs2() {
    s16 scrc = get_center_position();

    g_state.bs_scrrrl[0][0] = scrc + 192;
    g_state.bs_scrrrl[0][1] = scrc - 192;
    g_state.bs_scrrrl[1][0] = g_state.bs_scrrrl[0][0];
    g_state.bs_scrrrl[1][1] = g_state.bs_scrrrl[0][1];
}

/** @brief Processes player work updates for bonus stage (movement, scroll). */
void move_player_work_bonus() {
    g_state.positional_relation = check_work_position(&g_state.plw->wu, &g_state.plw[1].wu);
    set_rl_waza(&g_state.plw[0]);
    set_rl_waza(&g_state.plw[1]);
    g_state.Timer_Freeze = 0;

    if (g_state.Bonus_Game_Flag == 20) {
        setup_bs_scrrrl_bs2();
    }

    if (g_state.plw->wu.pl_operator) {
        move_P1_move_P2_bonus(*g_state.bs_scrrrl);
        return;
    }

    move_P2_move_P1_bonus(*g_state.bs_scrrrl);
}

/** @brief Updates P1 first then P2 for bonus stage frame ordering. */
static void move_P1_move_P2_bonus(s16* field_work) {
    Player_move_bonus(&g_state.plw[0], processed_lvbt(Convert_User_Setting(0)));

    if (set_field_hosei_flag(&g_state.plw[0], field_work[0], 1) != 0) {
        set_field_hosei_flag(&g_state.plw[0], field_work[1], 0);
    }

    Player_move_bonus(&g_state.plw[1], processed_lvbt(Convert_User_Setting(1)));

    if (set_field_hosei_flag(&g_state.plw[1], field_work[2], 1) != 0) {
        set_field_hosei_flag(&g_state.plw[1], field_work[3], 0);
    }

    if (g_state.Bonus_Game_Flag == 20) {
        g_state.plw[1].wu.disp_flag = 0;
    }
}

/** @brief Updates P2 first then P1 for bonus stage frame ordering. */
static void move_P2_move_P1_bonus(s16* field_work) {
    Player_move_bonus(&g_state.plw[1], processed_lvbt(Convert_User_Setting(1)));

    if (set_field_hosei_flag(&g_state.plw[1], field_work[2], 1) != 0) {
        set_field_hosei_flag(&g_state.plw[1], field_work[3], 0);
    }

    Player_move_bonus(&g_state.plw[0], processed_lvbt(Convert_User_Setting(0)));

    if (set_field_hosei_flag(&g_state.plw[0], field_work[0], 1) != 0) {
        set_field_hosei_flag(&g_state.plw[0], field_work[1], 0);
    }

    if (g_state.Bonus_Game_Flag == 20) {
        g_state.plw[0].wu.disp_flag = 0;
    }
}

/** @brief Applies damage correction for bonus stage interactions. */
void check_damage_hosei_bonus() {
    g_state.plw[0].muriyari_ugoku = g_state.plw[0].hosei_amari;
    g_state.plw[1].muriyari_ugoku = g_state.plw[1].hosei_amari;

    switch ((g_state.plw[0].hosei_amari != 0) + ((g_state.plw[1].hosei_amari != 0) * 2)) {
    case 1:
        if ((!g_state.plw[0].tsukami_f || g_state.plw[0].kind_of_catch != 1) &&
            (g_state.plw[0].tsukamare_f | g_state.plw[0].dm_hos_flag) == 0) {
            break;
        }

    one:
        g_state.plw[1].wu.xyz[0].disp.pos += g_state.plw[0].hosei_amari;
        g_state.plw[1].muriyari_ugoku += g_state.plw[0].hosei_amari;
        break;

    case 2:
        if ((!g_state.plw[1].tsukami_f || g_state.plw[1].kind_of_catch != 1) &&
            (g_state.plw[1].tsukamare_f | g_state.plw[1].dm_hos_flag) == 0) {
            break;
        }

    two:
        g_state.plw[0].wu.xyz[0].disp.pos += g_state.plw[1].hosei_amari;
        g_state.plw[0].muriyari_ugoku += g_state.plw[1].hosei_amari;
        break;

    case 3:
        if (g_state.plw[0].hos_fi_flag == g_state.plw[1].hos_fi_flag) {
            if (g_state.plw[0].tsukamare_f) {
                goto one;
            }

            if (g_state.plw[1].tsukamare_f) {
                goto two;
            }
        }

        break;
    }

    g_state.plw[0].hosei_amari = g_state.plw[1].hosei_amari = 0;
}
