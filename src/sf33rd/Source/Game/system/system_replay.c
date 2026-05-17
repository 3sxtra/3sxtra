/**
 * @file sys_replay.c
 * @brief Replay record/playback subsystem.
 *
 * Handles initializing replay recording or playback, saving/restoring
 * game state to/from the replay header, per-frame input recording with
 * run-length encoding, and input playback from the replay buffer.
 *
 * Split from sys_sub.c for organizational clarity.
 */

#include "sf33rd/Source/Game/system/system_replay.h"
#include "game_state.h"
#include <string.h>
#include "common.h"
#include "main.h"
#include "port/menu_task.h"
#include "port/task_api.h"
#include "port/debug_print.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect_b8_quake.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/menu/menu.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"

#define REPLAY_KEY_BUFF_SIZE 7198

// forward decls
static void Setup_Replay_Header(void);
static void Get_Replay_Header(void);
static void Get_Replay(s16 PL_id);
static void Setup_Replay_Buff(s16 PL_id, u16 sw_buff);
static void Replay(s16 PL_id);

/** @brief Initialize replay recording or playback based on g_state.Play_Mode. */
void Check_Replay() {
    s16 ix;

    if (!g_state.Demo_Flag) {
        return;
    }

    switch (g_state.Play_Mode) {
    case 1:
        g_state.Replay_Status[0] = 1;
        g_state.Replay_Status[1] = 1;

        if (g_state.plw[0].wu.pl_operator == 0) {
            g_state.Replay_Status[0] = 0;
            g_state.CP_No[0][0] = 0;
        }

        if (g_state.plw[1].wu.pl_operator == 0) {
            g_state.Replay_Status[1] = 0;
            g_state.CP_No[1][0] = 0;
        }

        g_state.Condense_Buff[0] = 0xFFFF;
        g_state.Condense_Buff[1] = 0xFFFF;
        memset(&Replay_w, 0, sizeof(Replay_w));

        if (g_state.Mode_Type == MODE_NORMAL_TRAINING || g_state.Mode_Type == MODE_PARRY_TRAINING ||
            g_state.Mode_Type == MODE_TRIALS) {
            for (ix = 0; ix < 0x1C1E; ix++) {
                Replay_w.io_unit.key_buff[0][ix] = 0xF000;
                Replay_w.io_unit.key_buff[1][ix] = 0xF000;
            }
        }

        Setup_Replay_Header();

        for (ix = 0; ix < 14; ix++) {
            Replay_w.lag[ix] = 1;
        }

        Lag_Ptr = Replay_w.lag;
        g_state.Lag_Timer = 1;
        Bg_Kakikae_Set();
        break;

    case 3:
        g_state.Replay_Status[0] = 3;
        g_state.Replay_Status[1] = 3;
        g_state.CP_No[0][0] = 0;
        g_state.CP_No[1][0] = 0;
        g_state.Vital_Handicap[g_state.Present_Mode][0] = Rep_Game_Infor[10].Vital_Handicap[0];
        g_state.Vital_Handicap[g_state.Present_Mode][1] = Rep_Game_Infor[10].Vital_Handicap[1];
        Get_Replay_Header();
        Lag_Ptr = Replay_w.lag;
        g_state.Lag_Timer = (s8)*Lag_Ptr;
        Lag_Ptr += 1;
        Bg_Kakikae_Set();
        break;

    default:
        return;
    }

    Record_Timer = 0;
    g_state.Demo_Timer[0] = 0;
    g_state.Demo_Timer[1] = 0;
    Demo_Ptr[0] = Replay_w.io_unit.key_buff[0];
    Demo_Ptr[1] = Replay_w.io_unit.key_buff[1];
}

/** @brief Save current game state (stage, characters, RNG seeds) into the replay header. */
static void Setup_Replay_Header() {
    s16 ix;

    Rep_Game_Infor[10].stage = g_state.bg_w.stage;
    Rep_Game_Infor[10].Direction_Working = g_state.Direction_Working[g_state.Present_Mode];
    Rep_Game_Infor[10].Vital_Handicap[0] = g_state.Vital_Handicap[g_state.Present_Mode][0];
    Rep_Game_Infor[10].Vital_Handicap[1] = g_state.Vital_Handicap[g_state.Present_Mode][1];

    for (ix = 0; ix < 2; ix++) {
        Rep_Game_Infor[10].player_infor[ix].my_char = g_state.My_char[ix];
        Rep_Game_Infor[10].player_infor[ix].sa = g_state.Super_Arts[ix];
        Rep_Game_Infor[10].player_infor[ix].color = g_state.Player_Color[ix];
        Rep_Game_Infor[10].player_infor[ix].player_type = g_state.plw[ix].wu.pl_operator;
        Rep_Game_Infor[10].Vital_Handicap[ix] = g_state.Vital_Handicap[g_state.Present_Mode][ix];
    }

    Rep_Game_Infor[10].Random_ix16 = g_state.Random_ix16;
    Rep_Game_Infor[10].Random_ix32 = g_state.Random_ix32;
    Rep_Game_Infor[10].Random_ix16_ex = g_state.Random_ix16_ex;
    Rep_Game_Infor[10].Random_ix32_ex = g_state.Random_ix32_ex;
    Rep_Game_Infor[10].players_timer = g_state.players_timer;
    g_state.Random_ix16_com = g_state.Random_ix16;
    g_state.Random_ix32_com = g_state.Random_ix32;
    g_state.Random_ix16_ex_com = g_state.Random_ix16_ex;
    g_state.Random_ix32_ex_com = g_state.Random_ix32_ex;
    g_state.Random_ix16_bg = g_state.Random_ix16;
    Rep_Game_Infor[10].old_mes_no2 = g_state.old_mes_no2;
    Rep_Game_Infor[10].old_mes_no3 = g_state.old_mes_no3;
    Rep_Game_Infor[10].old_mes_no_pl = g_state.old_mes_no_pl;
    Rep_Game_Infor[10].mes_already = g_state.mes_already;
    Replay_w.champion = g_state.Champion;
    Replay_w.full_data = 0;
}

/** @brief Restore game state (RNG seeds, timers, messages) from the replay header for playback. */
static void Get_Replay_Header() {
    g_state.Random_ix16 = Rep_Game_Infor[10].Random_ix16;
    g_state.Random_ix32 = Rep_Game_Infor[10].Random_ix32;
    g_state.Random_ix16_ex = Rep_Game_Infor[10].Random_ix16_ex;
    g_state.Random_ix32_ex = Rep_Game_Infor[10].Random_ix32_ex;
    g_state.players_timer = Rep_Game_Infor[10].players_timer;
    g_state.old_mes_no2 = Rep_Game_Infor[10].old_mes_no2;
    g_state.old_mes_no3 = Rep_Game_Infor[10].old_mes_no3;
    g_state.old_mes_no_pl = Rep_Game_Infor[10].old_mes_no_pl;
    g_state.mes_already = Rep_Game_Infor[10].mes_already;
    g_state.Random_ix16_com = g_state.Random_ix16;
    g_state.Random_ix32_com = g_state.Random_ix32;
    g_state.Random_ix16_ex_com = g_state.Random_ix16_ex;
    g_state.Random_ix32_ex_com = g_state.Random_ix32_ex;
    g_state.Random_ix16_bg = g_state.Random_ix16;
    g_state.Champion = Replay_w.champion;
    g_state.New_Challenger = g_state.Champion ^ 1;
    g_state.Control_Time = Replay_w.Control_Time_Buff;
    CurrentSave()->Difficulty = Replay_w.Difficulty;
}

/** @brief Dispatch replay action for a player based on their replay status (record, playback, idle, full). */
void Check_Replay_Status(s16 PL_id, u8 Status) {
    if (g_state.Demo_Flag == 0) {
        return;
    }

    switch (Status) {
    case 1:
        Get_Replay(PL_id);

        if ((g_state.Game_pause != 0x81) && Debug_w[DEBUG_DISP_REC_STATUS]) {
            flPrintColor(0xFFFFFFFF);
            flPrintL(16, 8, "HUMAN REC!");
            break;
        }

        break;

    case 3:
        Replay(PL_id);
        break;

    case 2:
        if (PL_id) {
            p2sw_0 = 0;
            break;
        }

        p1sw_0 = 0;
        break;

    case 99:
        flPrintColor(0xFFFFFF00);
        flPrintL(12, 20, "[REPLAY AREA FULL!!]");
        Disp_Rec_Time(PL_id, Rec_Time[PL_id]);
        break;
    }
}

/** @brief Record the current frame's input for the given player into the replay buffer. */
static void Get_Replay(s16 PL_id) {
    u16 sw_buff;

    if (g_state.Game_pause == 0x81) {
        return;
    }

    if (PL_id) {
        sw_buff = p2sw_0;
    } else {
        sw_buff = p1sw_0;
    }

    if (sw_buff == g_state.Condense_Buff[PL_id]) {
        if (g_state.Demo_Timer[PL_id] >= 16) {
            Setup_Replay_Buff(PL_id, sw_buff);
        } else {
            g_state.Demo_Timer[PL_id]++;
        }
    } else {
        Setup_Replay_Buff(PL_id, sw_buff);
    }

    if (PL_id == 0) {
        Disp_Rec_Time(PL_id, Record_Timer);
    }
}

/** @brief Flush the previous run-length-encoded input and start a new run for the given switches. */
static void Setup_Replay_Buff(s16 PL_id, u16 sw_buff) {
    u16 buff;
    u16 timer;

    if (g_state.Condense_Buff[PL_id] == 0xFFFF) {
        g_state.Demo_Timer[PL_id] = 1;
        g_state.Condense_Buff[PL_id] = sw_buff;
        return;
    }

    timer = g_state.Demo_Timer[PL_id] - 1;
    timer <<= 12;
    buff = g_state.Condense_Buff[PL_id] & 0xFFF;
    buff |= timer;
    *Demo_Ptr[PL_id] = buff;
    Demo_Ptr[PL_id]++;

    if (&Replay_w.io_unit.key_buff[PL_id][REPLAY_KEY_BUFF_SIZE - 1] < Demo_Ptr[PL_id]) {
        g_state.Replay_Status[PL_id] = 99;
        Replay_w.full_data |= PL_id + 1;
        Rec_Time[PL_id] = Record_Timer;
        return;
    }

    g_state.Demo_Timer[PL_id] = 1;
    g_state.Condense_Buff[PL_id] = sw_buff;
}

/** @brief Play back recorded input for the given player from the replay buffer. */
static void Replay(s16 PL_id) {
    u16 sw;
    u16 buff;

    if (&Replay_w.io_unit.key_buff[PL_id][REPLAY_KEY_BUFF_SIZE] < Demo_Ptr[PL_id]) {
        g_state.Replay_Status[0] = 2;
        g_state.Replay_Status[1] = 2;

        if (g_state.Mode_Type == MODE_REPLAY) {
            cpExitTask(TASK_PAUSE);
            cpReadyTask(TASK_MENU, Menu_Task);
            MenuTask_SetPhase(MTP_RESET);
        }

        g_state.Demo_Time_Stop = 1;
        return;
    }

    if (g_state.Game_pause == 0x81) {
        return;
    }

    if (g_state.Demo_Timer[PL_id] == 0) {
        sw = *Demo_Ptr[PL_id];
        Demo_Ptr[PL_id]++;
        buff = sw;
        sw &= 0xFFF;
        g_state.Condense_Buff[PL_id] = sw;
        buff &= 0xF000;
        buff >>= 12;
        g_state.Demo_Timer[PL_id] = buff + 1;
    }

    if (g_state.plw[PL_id].wu.pl_operator == 0) {
        if (PL_id) {
            p2sw_0 = 0;
        } else {
            p1sw_0 = 0;
        }
    } else if (PL_id) {
        p2sw_0 = g_state.Condense_Buff[PL_id];
    } else {
        p1sw_0 = g_state.Condense_Buff[PL_id];
    }

    g_state.Demo_Timer[PL_id]--;
}
