/**
 * @file plcnt3.c
 * Player Controller for the "Crush the Car!" Bonus Stage
 */

#include "sf33rd/Source/Game/engine/plcnt3.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/engine/hitcheck.h"
#include "sf33rd/Source/Game/engine/manage.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/plcnt2.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"

static void plcnt_b2_move();
static void plcnt_b2_die();

void (*const player_bonus2_process[3])() = { plcnt_b_init, plcnt_b2_move, plcnt_b2_die };

/** @brief Main player controller for the car-crush bonus stage. */
s32 Player_control_bonus2() {
    if (((g_state.pcon_rno[0] + g_state.pcon_rno[1]) == 0) || (!g_state.Game_pause && !g_state.EXE_flag)) {
        g_state.players_timer++;
        g_state.players_timer &= 0x7FFF;
        player_bonus2_process[g_state.pcon_rno[0]]();

        if (check_be_car_object()) {
            check_body_touch2();
            check_damage_hosei_bonus();
        }

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

/** @brief Per-frame car-crush bonus stage movement update. */
static void plcnt_b2_move() {
    if (g_state.No_Death) {
        g_state.plw[0].wu.dm_vital = g_state.plw[1].wu.dm_vital = 0;
    }

    if (g_state.Break_Into) {
        g_state.plw[0].wu.dm_vital = g_state.plw[1].wu.dm_vital = 0;
    }

    move_player_work_bonus();

    if (g_state.Bonus_Stage_RNO[0] == 2) {
        g_state.Time_Stop = 1;
        g_state.pcon_rno[0] = 2;
        g_state.pcon_rno[1] = 0;
        g_state.pcon_rno[2] = 0;
    }

    if (g_state.Time_Over) {
        g_state.pcon_rno[0] = 2;
        g_state.pcon_rno[1] = 0;
        g_state.pcon_rno[2] = 0;
    }
}

/** @brief Handles car-crush bonus stage completion/finalization. */
static void plcnt_b2_die() {
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

        if (g_state.plw[0].wu.pl_operator) {
            g_state.plw[0].wu.routine_no[1] = 0;
            g_state.plw[0].wu.routine_no[2] = 40;
            g_state.plw[0].wu.routine_no[3] = 0;
        } else {
            g_state.plw[0].wu.routine_no[3] = 9;
        }

        if (g_state.plw[1].wu.pl_operator) {
            g_state.plw[1].wu.routine_no[1] = 0;
            g_state.plw[1].wu.routine_no[2] = 40;
            g_state.plw[1].wu.routine_no[3] = 0;
        } else {
            g_state.plw[1].wu.routine_no[3] = 9;
        }

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
