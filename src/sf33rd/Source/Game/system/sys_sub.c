/**
 * @file sys_sub.c
 * @brief System state and management hub â€ core utilities.
 *
 * Wipe/transition effects, background layer setup and movement,
 * input remapping, button-cut helpers, fade control, soft reset,
 * clear/init helpers, and miscellaneous system-level utilities.
 *
 * Domain-specific subsystems have been split into:
 *   sys_replay.c  â€ replay record/playback
 *   sys_ranking.c â€ ranking insertion + opponent candidate selection
 *   sys_options.c â€ game option save/load/defaults/compare
 *   sys_score.c   â€ score display, win records, digit rendering, copyright
 *
 * Part of the system module.
 * Originally from the PS2 sys_sub module.
 */

#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/country_region.h"
#include "port/menu_task.h"
#include "port/init_task.h"
#include "port/task_api.h"
#include "common.h"
#include "main.h"
#include "port/menu_screen.h"
#include "port/mods/modded_stage.h"
#include "sf33rd/AcrSDK/common/mlPAD.h"
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/Source/Game/com/com_datu.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/eff93.h"
#include "sf33rd/Source/Game/effect/effb8.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/menu/menu.h"
#include "sf33rd/Source/Game/rendering/mmtmcnt.h"
#include "sf33rd/Source/Game/screen/entry.h"
#include "sf33rd/Source/Game/select_timer.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/sys_sub2.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include <string.h>

/* RmlUi Phase 3 bypass */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include "port/mods/modded_stage.h"

#define CONVERT_DATA_COUNT 12

static bool training_hitbox_display_enabled;

// forward decls
s32 Setup_Target_PL();
static void Reset_Sub0();

const u16 Convert_Data[CONVERT_DATA_COUNT] = { 16, 32, 64, 256, 512, 1024, 272, 544, 1088, 112, 1792, 0 };

/** @brief Initialize screen-switch (wipe-out) state. */
void Switch_Screen_Init(s32 unused1) {
    WipeInit();
    Forbid_Break = 1;
    Exec_Wipe = 1;
    Gap_Timer = 4;
    Stop_SG = 1;
    Escape_SS = 1;
}

/** @brief Advance the screen-switch wipe-out effect; returns 1 when complete. */
s32 Switch_Screen(u8 Wipe_Type) {
    if (WipeOut(Wipe_Type) && --Gap_Timer <= 0) {
        Exec_Wipe = 0;
        Stop_Combo = 0;
        return 1;
    }

    return 0;
}

/** @brief Advance the screen-switch wipe-in (revival) effect; returns 1 when complete. */
s32 Switch_Screen_Revival(u8 Wipe_Type) {
    if (WipeIn(Wipe_Type) && --Gap_Timer <= 0) {
        Exec_Wipe = 0;
        Stop_Combo = 0;
        return 1;
    }

    return 0;
}

/** @brief Remap raw button bits through a specific Pad_Infor config. */
u16 Remap_Buttons(u16 sw, const _PAD_INFOR* pad_info) {
    u16 answer = sw & (SWK_DIRECTIONS | SWK_START);

    if ((sw & SWK_WEST) && pad_info->Shot[0] < CONVERT_DATA_COUNT) {
        answer |= Convert_Data[pad_info->Shot[0]];
    }

    if ((sw & SWK_NORTH) && pad_info->Shot[1] < CONVERT_DATA_COUNT) {
        answer |= Convert_Data[pad_info->Shot[1]];
    }

    if ((sw & SWK_RIGHT_SHOULDER) && pad_info->Shot[2] < CONVERT_DATA_COUNT) {
        answer |= Convert_Data[pad_info->Shot[2]];
    }

    if ((sw & SWK_LEFT_SHOULDER) && pad_info->Shot[3] < CONVERT_DATA_COUNT) {
        answer |= Convert_Data[pad_info->Shot[3]];
    }

    if ((sw & SWK_SOUTH) && pad_info->Shot[4] < CONVERT_DATA_COUNT) {
        answer |= Convert_Data[pad_info->Shot[4]];
    }

    if ((sw & SWK_EAST) && pad_info->Shot[5] < CONVERT_DATA_COUNT) {
        answer |= Convert_Data[pad_info->Shot[5]];
    }

    if ((sw & SWK_RIGHT_TRIGGER) && pad_info->Shot[6] < CONVERT_DATA_COUNT) {
        answer |= Convert_Data[pad_info->Shot[6]];
    }

    if ((sw & SWK_LEFT_TRIGGER) && pad_info->Shot[7] < CONVERT_DATA_COUNT) {
        answer |= Convert_Data[pad_info->Shot[7]];
    }

    return answer;
}

/** @brief Remap physical button presses through the player's configured button layout. */
u16 Convert_User_Setting(s16 PL_id) {
    if (Debug_w[DEBUG_YOSHIZUMI_EXP] == 16) {
        if (PL_id == 0) {
            return p1sw_0;
        }

        return p2sw_0;
    }

    u16 sw = (PL_id == 0) ? p1sw_0 : p2sw_0;
    return Remap_Buttons(sw, &CurrentSave()->Pad_Infor[PL_id]);
}

/** @brief Reset all per-player session data (score, wins, cursors, etc.) for the given player. */
void Clear_Personal_Data(s16 PL_id) {
    s16 xx;

    Lost_Round[PL_id] = 0;
    Super_Arts_Finish[PL_id] = 0;
    Perfect_Finish[PL_id] = 0;
    Cheap_Finish[PL_id] = 0;
    Completion_Bonus[PL_id][0] = 0;
    Completion_Bonus[PL_id][1] = 0;
    Stage_Continue[PL_id] = 0;
    Introduce_Boss[PL_id][0] = 0;
    Introduce_Boss[PL_id][1] = 0;
    Introduce_Break_Into[PL_id] = 0;
    Score[PL_id][0] = 0;
    Stock_Score[PL_id] = 0;
    Stage_Stock_Score[PL_id] = 0;
    Continue_Coin[PL_id] = 0;
    Win_Record[PL_id] = 0;
    VS_Win_Record[PL_id] = 0;
    Stock_Win_Record[PL_id] = 0;
    VS_Index[PL_id] = 0;
    Used_char[PL_id] = 0xFF;
    Arts_Y[PL_id] = 0;
    Continue_Count[PL_id] = 0;
    Continue_Coin2[PL_id] = 0;
    Sel_PL_Complete[PL_id] = 0;
    Sel_Arts_Complete[PL_id] = 0;
    Sel_EM_Complete[PL_id] = 0;
    Last_Player_id = -1;
    Last_Super_Arts[PL_id] = 0;
    Last_My_char[PL_id] = -1;
    Last_My_char2[PL_id] = -1;
    Last_Selected_EM[PL_id] = 1;
    Select_Start[PL_id] = 0;
    paring_ctr_vs[0][PL_id] = 0;
    Straight_Counter[PL_id] = 0;
    Straight_Flag[PL_id] = 0;
    SC_Personal_Time[PL_id] = 481;
    E_Number[PL_id][0] = 0;
    E_Number[PL_id][1] = 0;
    E_Number[PL_id][2] = 0;
    E_Number[PL_id][3] = 0;
    E_07_Flag[PL_id] = 0;
    Request_Break[PL_id] = 0;

    if (PL_id == 0) {
        Cursor_X[0] = permission_player[Present_Mode].cursor_infor[0].first_x;
        Cursor_Y[0] = permission_player[Present_Mode].cursor_infor[0].first_y;
    } else {
        Cursor_X[1] = permission_player[Present_Mode].cursor_infor[1].first_x;
        Cursor_Y[1] = permission_player[Present_Mode].cursor_infor[1].first_y;
    }

    for (xx = 0; xx < 10; xx++) {
        EM_History[PL_id][xx] = 0;
    }
}

/** @brief Check if a continue-count button press should be accepted within the given limit. */
s16 Check_Count_Cut(s16 PL_id, s16 Limit) {
    s16 xx;

    Continue_Cut[PL_id] = 0;

    if (Continue_Count[PL_id] >= (Limit)) {
        return 0;
    }

    if (PL_id) {
        xx = p2sw_0 & ~p2sw_1;
    } else {
        xx = p1sw_0 & ~p1sw_1;
    }

    return xx & 0xFF0;
}

/** @brief Display a personal counter value on the HUD for the given player. */
void Disp_Personal_Count(s16 PL_id, s8 counter) {
    SSPutDec(DE_X[PL_id] + 14, 0, 9, counter, 0);
}

/** @brief Determine play type (0 = single player, 1 = two players) from operator status. */
void Setup_Play_Type() {
    if (Operator_Status[0] & 0x7F && Operator_Status[1] & 0x7F) {
        Play_Type = 1;
    } else {
        Play_Type = 0;
    }
}

/** @brief Clear all flash-number slots for both players. */
void Clear_Flash_No() {
    F_No0[0] = F_No1[0] = F_No2[0] = F_No3[0] = 0;
    F_No0[1] = F_No1[1] = F_No2[1] = F_No3[1] = 0;
}

void Set_Training_Hitbox_Display(bool enabled) {
    training_hitbox_display_enabled = enabled;
}

bool Is_Training_Hitbox_Display_Enabled() {
    return training_hitbox_display_enabled;
}

bool Cut_Cut_Cut() {
    if (Is_Training_Mode(Mode_Type)) {
        return true;
    }

    if (Demo_Flag == 0) {
        return false;
    }

    if (plw[0].wu.pl_operator && (p1sw_0 & SWK_ATTACKS)) {
        return true;
    }

    if (plw[1].wu.pl_operator && (p2sw_0 & SWK_ATTACKS)) {
        return true;
    }

    return false;
}

/** @brief Countdown timer with early-exit on button press; returns 1 when expired or cut. */
s32 Button_Cut_EX(s16* pTimer, s16 limit) {
    s16 PL_id = Setup_Target_PL();
    u16 xx;

    if (PL_id) {
        xx = p2sw_0;
    } else {
        xx = p1sw_0;
    }

    --*pTimer;

    if (*pTimer == 0) {
        return 1;
    }

    if ((xx & SWK_ATTACKS) && limit >= *pTimer) {
        return 1;
    }

    return 0;
}

/** @brief Determine the target player index (0 or 1) based on play type and operator state. */
s32 Setup_Target_PL() {
    if (Play_Type == 1) {
        return Winner_id;
    }

    if (Round_Operator[0]) {
        return 0;
    }

    if (plw[0].wu.pl_operator) {
        return 0;
    }

    return 1;
}

/** @brief Set up the final grade data for the losing player at game over. */
void Setup_Final_Grade() {
    if (Break_Com[Player_id][0] == 0) {
        Final_Result_id = LOSER;
        WGJ_Target = LOSER;
        WGJ_Win = Win_Record[LOSER];
        WGJ_Score = Continue_Coin[LOSER] + Score[LOSER][0];
    }
}

/** @brief Zero out all win-type, flash-win-type, and sync-win-type arrays for both players. */
void Clear_Win_Type() {
    s16 i;

    for (i = 0; i < 4; i++) {
        win_type[0][i] = 0;
        win_type[1][i] = 0;
        flash_win_type[0][i] = 0;
        flash_win_type[1][i] = 0;
        sync_win_type[0][i] = 0;
        sync_win_type[1][i] = 0;
    }
}

/** @brief Reset all ranking display request slots and rank-in slots for the given player. */
void Clear_Disp_Ranking(s16 PL_id) {
    s16 ix;

    for (ix = 0; ix <= 3; ix++) {
        Request_Disp_Rank[PL_id][ix] = -1;
        Rank_In[PL_id][ix] = -1;
    }
}

/** @brief Decompress 16-bit LZ-style packed data from source to destination buffer. */
void Meltw(u16* s, u16* d, s32 file_ptr) {
    s32 flag;
    s32 i;
    u32 s_cnt;
    u32 s_len;
    u16* s_ptr;

    while (1) {
        flag = *s++ * 0x10000;
        file_ptr--;
        i = 16;

        do {
            if (flag >= 0) {
                *d++ = *s++;
                file_ptr--;
            } else {
                s_len = *s++;
                s_cnt = s_len >> 11;

                if (s_cnt != 0) {
                    s_len = s_len & 0x7FF;
                    file_ptr--;
                } else {
                    s_cnt = *s++;
                    file_ptr -= 2;
                }

                if (s_len == 0 && s_cnt == 0) {
                    return;
                }

                if (s_len == 0) {
                    do {
                        *d++ = 0;
                    } while (--s_cnt);
                } else {
                    s_ptr = d - s_len;

                    do {
                        *d++ = *s_ptr++;
                    } while (--s_cnt);
                }
            }

            flag <<= 1;
        } while (--i);
    }
}

/** @brief Assign COM_id and Player_id based on which side has an active operator. */
void Setup_ID() {
    if (Operator_Status[0] == 0) {
        COM_id = 0;
        Player_id = 1;
    } else {
        COM_id = 1;
        Player_id = 0;
    }
}

/** @brief Restore task conditions from the keep_condition backup array. */
void cpRevivalTask() {
    struct _TASK* task_ptr;
    s16 ix;

    for (task_ptr = task, ix = 0; ix < 11; task_ptr++, ix++) {
        task_ptr->condition = keep_condition[ix];
    }
}

/** @brief Check whether the menu task is active or in the correct training sub-state. */
s32 Check_Menu_Task() {
    if (Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_PARRY_TRAINING || Mode_Type == MODE_TRIALS) {
        if (MenuTask_GetPhase() == MTP_IN_GAME && MenuTask_GetSubPhase() == MTSP_IN_GAME_ACTIVE) {
            return 1;
        }

        return 0;
    }

    if (MenuTask_IsActive()) {
        return 1;
    }

    return 0;
}

/** @brief Calculate the time limit for gameplay based on difficulty and country settings. */
void Setup_Limit_Time() {
    s16 limit;

    limit = Level_18_Data[CurrentSave()->Difficulty][16];
    limit += 20;
    if (Country == COUNTRY_JAPAN) {
        Limit_Time = 1241;
    } else {
        Limit_Time = 1061;
    }

    if (limit > Limit_Time) {
        Limit_Time = limit;
    }
}

/** @brief Compute the training-mode control time based on Limit_Time and difficulty. */
void Setup_Training_Difficulty() {
    s16 unit_time;
    s16 min_time;

    unit_time = Limit_Time - 481;
    unit_time = unit_time / 5;
    min_time = 481 - (unit_time * 2);

    if (min_time < 0) {
        min_time = 0;
    }

    Control_Time = min_time + (unit_time * CurrentSave()->Difficulty);
}

/** @brief Initialize a background layer at the given position and mark it as active. */
void Setup_BG(s16 BG_INDEX, s16 X, s16 Y) {
    Unsubstantial_BG[BG_INDEX] = 1;
    bg_w.bgw[BG_INDEX].xy[0].disp.pos = X;
    bg_w.bgw[BG_INDEX].xy[1].disp.pos = Y;
    bg_w.bgw[BG_INDEX].wxy[0].disp.pos = X;
    bg_w.bgw[BG_INDEX].wxy[1].disp.pos = Y;
    bg_w.bgw[BG_INDEX].xy[0].disp.low = 0;
    bg_w.bgw[BG_INDEX].xy[1].disp.low = 0;
    bg_w.bgw[BG_INDEX].position_x = X;
    bg_w.bgw[BG_INDEX].position_y = Y;
    Bg_Family_Set_Ex(BG_INDEX);
}

/** @brief Initialize a virtual (non-substantiated) background layer at the given position. */
void Setup_Virtual_BG(s16 BG_INDEX, s16 X, s16 Y) {
    bg_w.bgw[BG_INDEX].xy[0].disp.pos = X;
    bg_w.bgw[BG_INDEX].xy[1].disp.pos = Y;
    bg_w.bgw[BG_INDEX].wxy[0].disp.pos = X;
    bg_w.bgw[BG_INDEX].wxy[1].disp.pos = Y;
    bg_w.bgw[BG_INDEX].xy[0].disp.low = 0;
    bg_w.bgw[BG_INDEX].xy[1].disp.low = 0;
    bg_w.bgw[BG_INDEX].position_x = X;
    bg_w.bgw[BG_INDEX].position_y = Y;
    Bg_Family_Set_Ex(BG_INDEX);
}

/** @brief Update positions for all active (unsubstantial) background layers. */
void BG_move() {
    s16 ix;

    for (ix = 0; ix < 4; ix++) {
        if (Unsubstantial_BG[ix]) {
            bg_pos_hosei_sub2(ix);
            Bg_Family_Set_appoint(ix);
        }
    }
}

/** @brief Extended background move â€ recalculate scroll for a single layer. */
void BG_move_Ex(u8 ix) {
    scr_calc(ix);
}

/** @brief Run per-frame basic processing: save old BG position, then move 6 effect work slots. */
void Basic_Sub() {
    bg_w.bgw[0].old_pos_x = bg_w.bgw[0].xy[0].disp.pos;
    move_effect_work(0);
    move_effect_work(1);
    move_effect_work(2);
    move_effect_work(3);
    move_effect_work(4);
    move_effect_work(5);
}

/** @brief Run extended basic processing: move 6 effect work slots (without saving old BG position). */
void Basic_Sub_Ex() {
    move_effect_work(0);
    move_effect_work(1);
    move_effect_work(2);
    move_effect_work(3);
    move_effect_work(4);
    move_effect_work(5);
}

/** @brief Check if both players' load-request queues have completed; returns 1 if both ready. */
s32 Check_PL_Load() {
    if (!Check_LDREQ_Queue_Player(0) || !Check_LDREQ_Queue_Player(1)) {
        return 0;
    }

    return 1;
}

/** @brief Main background drawing dispatcher: handles scroll transfer, family movement, and ending movement. */
void BG_Draw_System() {
    u8 i;
    u16 mask = 1 & 0xFFFF;
    u16 s2;
    u16 s3;

    if (bg_disp_off == 0) {
        if (ModdedStage_IsActiveForCurrentStage() || ModdedStage_IsRenderingDisabled()) {
            /* HD modded stage active or rendering disabled: suppress tile rendering
             * but keep scroll calculations alive so parallax positions are correct.
             * The actual HD layers are rendered from SDLApp_EndFrame(). */
            for (i = 0; i < 4; i++, s2 = mask *= 2) {
                if (Screen_Switch_Buffer & mask) {
                    scr_calc(i);
                }
            }
        } else {
            for (i = 0; i < 4; i++, s2 = mask *= 2) {
                if (Screen_Switch_Buffer & mask) {
                    scr_trans(i);
                }
            }
        }
    } else {
        for (i = 0; i < 4; i++, s3 = mask *= 2) {
            if (Screen_Switch_Buffer & mask) {
                scr_calc(i);
            }
        }
    }

    if (Play_Game == 0) {
        for (i = 0; i < 4; i++) {
            if (Unsubstantial_BG[i]) {
                scr_calc(i);
            }
        }
    } else if (Play_Game == 1) {
        Family_Move();
    } else {
        Ending_Family_Move();
    }
}

/** @brief Read and return the next demo input data word for the given player, handling run-length timing. */
u16 Check_Demo_Data(s16 PL_id) {
    u16 ans;

    if (Demo_Timer[PL_id] == 0) {
        ans = *Demo_Ptr[PL_id];
        Demo_Ptr[PL_id]++;
    } else {
        Demo_Timer[PL_id]--;
        return 0;
    }

    if (ans & 0x8000) {
        Demo_Timer[PL_id] = ans & 0x7FFF;
        return 0;
    }

    return ans;
}

/** @brief Level-B system clear: close backgrounds, reset effects, and stop the select timer. */
void System_all_clear_Level_B() {
    Bg_Close();
    effect_work_init();
    SelectTimer_Finish();
}

/** @brief Decrement C_Timer, skipping to zero if a player presses a button; returns remaining time. */
s16 Cut_Cut_C_Timer() {
    C_Timer--;

    if (!Cut_Cut_Cut()) {
        return C_Timer;
    }

    return C_Timer = 0;
}

/** @brief Set rendering priority order slot 56 to priority 7 for one frame. */
void Switch_Priority_76() {
    Order[56] = 7;
    Order_Timer[56] = 1;
}

/** @brief Return xx (early exit value) if a player presses attack during a demo; otherwise return 1. */
s32 Cut_Cut_Sub(s16 xx) {
    if (Demo_Flag == 0) {
        return 1;
    }

    if (plw[0].wu.pl_operator && (p1sw_0 & SWK_ATTACKS)) {
        return xx;
    }

    if (plw[1].wu.pl_operator && (p2sw_0 & SWK_ATTACKS)) {
        return xx;
    }

    return 1;
}

/** @brief Check if the losing player pressed an attack button to skip an animation. */
bool Cut_Cut_Loser() {
    if (Round_Operator[0] && (p1sw_0 & SWK_ATTACKS)) {
        return true;
    }

    if (Round_Operator[1] && (p2sw_0 & SWK_ATTACKS)) {
        return true;
    }

    return false;
}

/** @brief Legacy infinite-loop VSync wait stub (no longer functional). */
/** @brief Legacy infinite-loop VSync wait stub.
 *  On PS3, sleeps to prevent invisible CPU-burn death loops. */
void njWaitVSync_with_N() {
#ifdef PLATFORM_PS3
    /* PS3: njWaitVSync_with_N is used in fatal-error while(1) loops throughout
     * the engine (texcash, sound3rd, etc). As a no-op, these become silent
     * CPU-burning hangs. Sleep + print + break after a few iterations. */
    static int vwait_count = 0;
    if (++vwait_count <= 3) {
        extern int printf(const char*, ...);
        printf("[PS3] njWaitVSync_with_N: fatal error loop detected (call #%d)\n", vwait_count);
    }
    /* Sleep 16ms (one frame) to avoid CPU burn */
    {
        extern int sys_timer_usleep(unsigned long long);
        sys_timer_usleep(16000);
    }
#endif
}

/** @brief Perform the soft-reset sequence: fade out, stop audio, purge textures, reinitialize tasks. */
void Soft_Reset_Sub() {
    FadeOut(1, 0xFF, 8);
    sound_all_off();
    SsBgmHalfVolume(0);

    if (Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_PARRY_TRAINING) {
        Set_Training_Hitbox_Display(false);
    }

    /* Hide all RmlUi overlays so they don't persist across the reset. */
    if (use_rmlui) {
        rmlui_wrapper_hide_all_game_documents();
        rmlui_wrapper_hide_all_documents();
    }

    MenuScreen_ExitToLegacy(MenuTask_GetTaskPtr());

    if (!Task_IsActive(TASK_GAME)) {
        cpReadyTask(TASK_GAME, Game_Task);
    }

    if (!Task_IsActive(TASK_DEBUG)) {
        cpReadyTask(TASK_DEBUG, Debug_Task);
    }

    Next_Title_Sub();
    Bg_TexInit();
    Purge_mmtm_area(6);
    Make_texcash_of_list(6);
    pulpul_stop();
    init_pulpul_work();
    pp_operator_check_flag(1);
    Init_Load_Request_Queue_1st();
    cpExitTask(TASK_MENU);
    MenuScreen_ExitToLegacy(MenuTask_GetTaskPtr());
    cpExitTask(TASK_SAVER);
    cpExitTask(TASK_PAUSE);
    Reset_Sub0();
    InitTask_SetPhase(ITP_RUNNING);
    InitTask_ResetSubPhases();
    vm_w.Request = 0;
    vm_w.Access = 0;
}

/** @brief Clear pause/game state flags and reset mode to arcade defaults. */
static void Reset_Sub0() {
    Pause = 0;
    Game_pause = 0;
    Play_Game = 0;
    Forbid_Break = 0;
    Extra_Break = 0;

    // Don't clobber netplay mode during soft resets
    if (Mode_Type != MODE_NETWORK) {
        Mode_Type = MODE_ARCADE;
        Present_Mode = 1;
    } else {
        Present_Mode = MODE_NETWORK;
    }

    Play_Mode = 0;
    Replay_Status[0] = 0;
    Replay_Status[1] = 0;
}

/** @brief Initialize the clear-flash pulsing effect with the given intensity level. */
void Clear_Flash_Init(s16 level) {
    Synchro_No = 0;
    Flash_Synchro = 0;
    Synchro_Level = level;
}

/** @brief Advance the clear-flash oscillation and return the current flash intensity (0â€127). */
s16 Clear_Flash_Sub() {
    switch (Synchro_No) {
    case 0:
        Flash_Synchro -= Synchro_Level;

        if (Flash_Synchro <= 0) {
            Synchro_No = 1;
            Flash_Synchro = 1;
        }

        break;

    case 1:
        Flash_Synchro += Synchro_Level;

        if (Flash_Synchro > 127) {
            Synchro_No = 0;
            Flash_Synchro = 127;
        }

        break;
    }

    return Flash_Synchro;
}

/** @brief Reset all four random-number generator index counters to zero. */
void All_Clear_Random_ix() {
    Random_ix16 = 0;
    Random_ix32 = 0;
    Random_ix16_ex = 0;
    Random_ix32_ex = 0;
}

/** @brief Reset system_timer, Game_timer, and players_timer to zero. */
void All_Clear_Timer() {
    system_timer = 0;
    Game_timer = 0;
    players_timer = 0;
}

/** @brief Clear miscellaneous message tracking state (old_mes_no2/3, old_mes_no_pl, mes_already). */
void All_Clear_ETC() {
    old_mes_no2 = 0;
    old_mes_no3 = 0;
    old_mes_no_pl = 0;
    mes_already = 0;
}

/** @brief Initialize all RNG indices to zero for netplay synchronization. */
void Setup_Net_Random_ix() {
    u8 ix = 0;

    Random_ix16 = ix;
    Random_ix32 = ix;
    Random_ix16_ex = ix;
    Random_ix32_ex = ix;
}

/** @brief Request a fade transition with the given code; returns 1 if accepted, 0 if already fading. */
s32 Request_Fade(u16 fade_code) {
    if (Fade_Flag == 0) {
        Fade_Flag = 1;
        Fade_R_No0 = Fade_R_No1 = 0;
        Fade_Number = fade_code;
        Forbid_Break = 1;
        fade_cont_init();
        return 1;
    }

    return 0;
}

/** @brief Advance the fade and return 1 when the fade completes (special variant). */
s32 Check_Fade_Complete_SP() {
    fade_cont_main();
    return Fade_Flag ^ 1;
}

/** @brief Advance the fade and return 1 when the fade completes (normal variant). */
s32 Check_Fade_Complete() {
    if (Fade_Flag) {
        fade_cont_main();
        return 0;
    }

    Forbid_Break = 1;
    return 1;
}

/** @brief Clear all 8 Suicide slots and all 4 Menu_Suicide flags. */
void All_Clear_Suicide() {
    s16 ix;

    for (ix = 0; ix < 8; ix++) {
        Suicide[ix] = 0;
    }

    for (ix = 0; ix < 4; ix++) {
        Menu_Suicide[ix] = 0;
    }
}

/** @brief Placeholder flash-violent callback (always returns 1). */
s32 Flash_Violent(WORK_Other* unused1, s32 unused2) {
    return 1;
}
