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

#include "sf33rd/Source/Game/system/sys_replay.h"
#include "common.h"
#include "main.h"
#include "port/menu_task.h"
#include "port/task_api.h"
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effb8.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/menu/menu.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/work_sys.h"

#define REPLAY_KEY_BUFF_SIZE 7198

// forward decls
static void Setup_Replay_Header(void);
static void Get_Replay_Header(void);
static void Get_Replay(s16 PL_id);
static void Setup_Replay_Buff(s16 PL_id, u16 sw_buff);
static void Replay(s16 PL_id);

/** @brief Initialize replay recording or playback based on Play_Mode. */
void Check_Replay() {
    s16 ix;

    if (!Demo_Flag) {
        return;
    }

    switch (Play_Mode) {
    case 1:
        Replay_Status[0] = 1;
        Replay_Status[1] = 1;

        if (plw[0].wu.pl_operator == 0) {
            Replay_Status[0] = 0;
            CP_No[0][0] = 0;
        }

        if (plw[1].wu.pl_operator == 0) {
            Replay_Status[1] = 0;
            CP_No[1][0] = 0;
        }

        Condense_Buff[0] = 0xFFFF;
        Condense_Buff[1] = 0xFFFF;
        memset(&Replay_w, 0, sizeof(Replay_w));

        if (Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_PARRY_TRAINING || Mode_Type == MODE_TRIALS) {
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
        Lag_Timer = 1;
        Bg_Kakikae_Set();
        break;

    case 3:
        Replay_Status[0] = 3;
        Replay_Status[1] = 3;
        CP_No[0][0] = 0;
        CP_No[1][0] = 0;
        Vital_Handicap[Present_Mode][0] = Rep_Game_Infor[10].Vital_Handicap[0];
        Vital_Handicap[Present_Mode][1] = Rep_Game_Infor[10].Vital_Handicap[1];
        Get_Replay_Header();
        Lag_Ptr = Replay_w.lag;
        Lag_Timer = (s8)*Lag_Ptr;
        Lag_Ptr += 1;
        Bg_Kakikae_Set();
        break;

    default:
        return;
    }

    Record_Timer = 0;
    Demo_Timer[0] = 0;
    Demo_Timer[1] = 0;
    Demo_Ptr[0] = Replay_w.io_unit.key_buff[0];
    Demo_Ptr[1] = Replay_w.io_unit.key_buff[1];
}

/** @brief Save current game state (stage, characters, RNG seeds) into the replay header. */
static void Setup_Replay_Header() {
    s16 ix;

    Rep_Game_Infor[10].stage = bg_w.stage;
    Rep_Game_Infor[10].Direction_Working = Direction_Working[Present_Mode];
    Rep_Game_Infor[10].Vital_Handicap[0] = Vital_Handicap[Present_Mode][0];
    Rep_Game_Infor[10].Vital_Handicap[1] = Vital_Handicap[Present_Mode][1];

    for (ix = 0; ix < 2; ix++) {
        Rep_Game_Infor[10].player_infor[ix].my_char = My_char[ix];
        Rep_Game_Infor[10].player_infor[ix].sa = Super_Arts[ix];
        Rep_Game_Infor[10].player_infor[ix].color = Player_Color[ix];
        Rep_Game_Infor[10].player_infor[ix].player_type = plw[ix].wu.pl_operator;
        Rep_Game_Infor[10].Vital_Handicap[ix] = Vital_Handicap[Present_Mode][ix];
    }

    Rep_Game_Infor[10].Random_ix16 = Random_ix16;
    Rep_Game_Infor[10].Random_ix32 = Random_ix32;
    Rep_Game_Infor[10].Random_ix16_ex = Random_ix16_ex;
    Rep_Game_Infor[10].Random_ix32_ex = Random_ix32_ex;
    Rep_Game_Infor[10].players_timer = players_timer;
    Random_ix16_com = Random_ix16;
    Random_ix32_com = Random_ix32;
    Random_ix16_ex_com = Random_ix16_ex;
    Random_ix32_ex_com = Random_ix32_ex;
    Random_ix16_bg = Random_ix16;
    Rep_Game_Infor[10].old_mes_no2 = old_mes_no2;
    Rep_Game_Infor[10].old_mes_no3 = old_mes_no3;
    Rep_Game_Infor[10].old_mes_no_pl = old_mes_no_pl;
    Rep_Game_Infor[10].mes_already = mes_already;
    Replay_w.champion = Champion;
    Replay_w.full_data = 0;
}

/** @brief Restore game state (RNG seeds, timers, messages) from the replay header for playback. */
static void Get_Replay_Header() {
    Random_ix16 = Rep_Game_Infor[10].Random_ix16;
    Random_ix32 = Rep_Game_Infor[10].Random_ix32;
    Random_ix16_ex = Rep_Game_Infor[10].Random_ix16_ex;
    Random_ix32_ex = Rep_Game_Infor[10].Random_ix32_ex;
    players_timer = Rep_Game_Infor[10].players_timer;
    old_mes_no2 = Rep_Game_Infor[10].old_mes_no2;
    old_mes_no3 = Rep_Game_Infor[10].old_mes_no3;
    old_mes_no_pl = Rep_Game_Infor[10].old_mes_no_pl;
    mes_already = Rep_Game_Infor[10].mes_already;
    Random_ix16_com = Random_ix16;
    Random_ix32_com = Random_ix32;
    Random_ix16_ex_com = Random_ix16_ex;
    Random_ix32_ex_com = Random_ix32_ex;
    Random_ix16_bg = Random_ix16;
    Champion = Replay_w.champion;
    New_Challenger = Champion ^ 1;
    Control_Time = Replay_w.Control_Time_Buff;
    CurrentSave()->Difficulty = Replay_w.Difficulty;
}

/** @brief Dispatch replay action for a player based on their replay status (record, playback, idle, full). */
void Check_Replay_Status(s16 PL_id, u8 Status) {
    if (Demo_Flag == 0) {
        return;
    }

    switch (Status) {
    case 1:
        Get_Replay(PL_id);

        if ((Game_pause != 0x81) && Debug_w[DEBUG_DISP_REC_STATUS]) {
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

    if (Game_pause == 0x81) {
        return;
    }

    if (PL_id) {
        sw_buff = p2sw_0;
    } else {
        sw_buff = p1sw_0;
    }

    if (sw_buff == Condense_Buff[PL_id]) {
        if (Demo_Timer[PL_id] >= 16) {
            Setup_Replay_Buff(PL_id, sw_buff);
        } else {
            Demo_Timer[PL_id]++;
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

    if (Condense_Buff[PL_id] == 0xFFFF) {
        Demo_Timer[PL_id] = 1;
        Condense_Buff[PL_id] = sw_buff;
        return;
    }

    timer = Demo_Timer[PL_id] - 1;
    timer <<= 12;
    buff = Condense_Buff[PL_id] & 0xFFF;
    buff |= timer;
    *Demo_Ptr[PL_id] = buff;
    Demo_Ptr[PL_id]++;

    if (&Replay_w.io_unit.key_buff[PL_id][REPLAY_KEY_BUFF_SIZE - 1] < Demo_Ptr[PL_id]) {
        Replay_Status[PL_id] = 99;
        Replay_w.full_data |= PL_id + 1;
        Rec_Time[PL_id] = Record_Timer;
        return;
    }

    Demo_Timer[PL_id] = 1;
    Condense_Buff[PL_id] = sw_buff;
}

/** @brief Play back recorded input for the given player from the replay buffer. */
static void Replay(s16 PL_id) {
    u16 sw;
    u16 buff;

    if (&Replay_w.io_unit.key_buff[PL_id][REPLAY_KEY_BUFF_SIZE] < Demo_Ptr[PL_id]) {
        Replay_Status[0] = 2;
        Replay_Status[1] = 2;

        if (Mode_Type == MODE_REPLAY) {
            cpExitTask(TASK_PAUSE);
            cpReadyTask(TASK_MENU, Menu_Task);
            MenuTask_SetPhase(MTP_RESET);
        }

        Demo_Time_Stop = 1;
        return;
    }

    if (Game_pause == 0x81) {
        return;
    }

    if (Demo_Timer[PL_id] == 0) {
        sw = *Demo_Ptr[PL_id];
        Demo_Ptr[PL_id]++;
        buff = sw;
        sw &= 0xFFF;
        Condense_Buff[PL_id] = sw;
        buff &= 0xF000;
        buff >>= 12;
        Demo_Timer[PL_id] = buff + 1;
    }

    if (plw[PL_id].wu.pl_operator == 0) {
        if (PL_id) {
            p2sw_0 = 0;
        } else {
            p1sw_0 = 0;
        }
    } else if (PL_id) {
        p2sw_0 = Condense_Buff[PL_id];
    } else {
        p1sw_0 = Condense_Buff[PL_id];
    }

    Demo_Timer[PL_id]--;
}
