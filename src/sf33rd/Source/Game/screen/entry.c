/**
 * @file entry.c
 * Manages all the screens
 */

#include "sf33rd/Source/Game/fsm.h"
#include "game_state.h"
#include "sf33rd/Source/Game/screen/entry.h"
#include "common.h"
#include "constants.h"
#include "main.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect_a2_color_table.h"
#include "sf33rd/Source/Game/ending/end_data.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/opening/opening.h"
#include "sf33rd/Source/Game/screen/n_input.h"
#include "sf33rd/Source/Game/screen/ranking.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/system/reset.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/hud_subroutines.h"
#include "structs.h"

/* RmlUi Phase 3 bypass */
#include "port/sdl/rmlui/rmlui_attract_overlay.h"
#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include "port/sdl/rmlui/rmlui_copyright.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui/rmlui_title_screen.h"
#include "port/sdl/rmlui/rmlui_tournament_lobby.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

/* Macro: skip SSPutStr if entry text RmlUi is active, or if the RmlUi network/casual lobby is open */
#define ENTRY_TEXT_GATED                                                                                               \
    ((use_rmlui && rmlui_screen_entry_text) || rmlui_wrapper_is_game_document_visible("network_lobby") ||              \
     rmlui_wrapper_is_game_document_visible("ranked_matchmaking") || rmlui_casual_lobby_is_visible() ||                \
     rmlui_tournament_lobby_is_visible() || rmlui_wrapper_is_game_document_visible("leaderboard") ||                   \
     rmlui_wrapper_is_game_document_visible("replay_picker") ||                                                        \
     rmlui_wrapper_is_game_document_visible("network_replay_picker") ||                                                \
     rmlui_wrapper_is_game_document_visible("player_profile"))

u8 letter_stack[40];
u8 letter_counter;
u8* letter_ptr;

const u8 Coin_Message_Data[7][2] = { { 5, 30 }, { 2, 27 }, { 7, 32 }, { 17, 37 }, { 6, 31 }, { 5, 42 }, { 0, 0 } };

static void Entry_TitleBlink();
static void Entry_WaitStart();
static void Entry_MidGameEntry();
static void Entry_PreFightBreak();
static void Entry_MidRoundBreak();
static void Entry_PostContinueBreak();
static void Entry_PostFightBreak();
static void Entry_EndGameBreak();
static void Entry_FinalEnding();

static void Disp_00_0();
static void Entry_01_Sub(s16 PL_id);
static void Exit_Title_Entry();
static void Entry_Main_Sub(s16 PL_id, s16 Jump_Index);
static void entry_phase_1st(s16 jump_index);
static void entry_end_2nd(s16 jump_index);
static void Entry_PreFightBreak_Phase1();
static void Entry_PreFightBreak_Phase2();
static void Entry_MidRoundBreak_Phase1();
static void Entry_MidRoundBreak_Phase2();
void Correct_BI_Data();
static void Entry_PostContinueBreak_Phase1();
static void Entry_PostContinueBreak_Phase2();
static void Entry_PostFightBreak_Phase1();
static void Entry_PostFightBreak_Phase2();
static void Entry_EndGameBreak_Phase1();
static void Entry_EndGameBreak_Phase2();
static void Entry_FinalEnding_Phase1();
static void Entry_FinalEnding_Phase2();
static void Break_Into_Sub(s16 PL_id, s16 Jump_Index);
static void Entry_Common_Sub(s16 PL_id, s16 Jump_Index);
static void Entry_Continue_Sub(s16 PL_id);
static void In_Game_Sub(s16 PL_id);
static void In_Over_Sub(s16 PL_id);
static void Loser_Scene_Sub(s16 PL_id, s16 Jump_Index);
static void Name_In_Sub(s16 PL_id);
static void Name_In_Sub0(s16 PL_id, s16 xx);
static s32 Credit_Continue_1P();
static s32 Credit_Continue_2P();
static void Naming_Cut_1P();
static void Naming_Cut_2P();
static void Naming_Init(s16 PL_id);
s32 Ck_Break_Into_SP(u16 Sw_0, u16 Sw_1, s16 PL_id);
s32 Ck_Break_Into(u16 Sw_0, u16 Sw_1, s16 PL_id);
static s32 Credit_1P();
static s32 Credit_2P();
static s32 Loser_1P();
static s32 Loser_2P();
static s32 Flash_Start(s16 PL_id);
static s32 Flash_Please(s16 PL_id);
static void Setup_Next_Step(s16 PL_id);
static void Break_Into_02(s16 PL_id);
static void Break_Into_04(s16 PL_id);
static void Break_Into_05(s16 PL_id);
static void Break_Into_07(s16 PL_id);
static void Break_Into_08(s16 PL_id);
static void Break_Into_09(s16 PL_id);
static void Break_Into_10(s16 PL_id);
static void Continue_Score_Sub(s16 PL_id);
static void activate_new_operators(void);

#define ENTRY_JMP_COUNT 11

/** @brief Main entry-task callback — dispatch per-frame screen transitions for all entry phases. */
void Entry_Task(struct _TASK* /* unused */) {
    s16 ix;
    s16 ff;

    if (g_state.Pause || nowSoftReset()) {
        return;
    }

    ff = sysFF;

    for (ix = 0; ix < ff; ix++) {
        if (ix == (ff - 1)) {
            No_Trans = 0;
        } else {
            No_Trans = 1;
        }

        letter_counter = 0;
        letter_ptr = letter_stack;
        if (g_state.entry_phase[0] < ENTRY_JMP_COUNT) {
            switch (g_state.entry_phase[0]) {
            case ENTRY_TITLE_BLINK:
                Entry_TitleBlink();
                break;
            case ENTRY_WAIT_START:
                Entry_WaitStart();
                break;
            case ENTRY_MID_GAME_ENTRY:
                Entry_MidGameEntry();
                break;
            case ENTRY_PRE_FIGHT_BREAK:
                Entry_PreFightBreak();
                break;
            case ENTRY_MID_ROUND_BREAK:
                Entry_MidRoundBreak();
                break;
            case ENTRY_UNUSED_5:
                Entry_PreFightBreak();
                break;
            case ENTRY_POST_CONTINUE_BREAK:
                Entry_PostContinueBreak();
                break;
            case ENTRY_POST_FIGHT_BREAK:
                Entry_PostFightBreak();
                break;
            case ENTRY_END_GAME_BREAK:
                Entry_EndGameBreak();
                break;
            case ENTRY_UNUSED_9:
                Entry_PreFightBreak();
                break;
            case ENTRY_FINAL_ENDING:
                Entry_FinalEnding();
                break;
            }
        }
    }
}

/** @brief Entry phase 0 — idle/title attract screen; blink "PRESS START" messages. */
static void Entry_TitleBlink() {
    switch (g_state.entry_phase[1]) {
    case ENTRY_PL_INIT:
        break;

    case ENTRY_PL_CREDIT:
        g_state.entry_phase[1] += 1;
        g_state.entry_timer = 50;
        Disp_00_0();
        break;

    case ENTRY_PL_NAMING:
        if (--g_state.entry_timer == 0) {
            g_state.entry_phase[1] += 1;
            g_state.entry_timer = 30;
            break;
        }

        Disp_00_0();
        break;

    case ENTRY_PL_RANKING:
        if (!--g_state.entry_timer) {
            g_state.entry_phase[1] -= 1;
            g_state.entry_timer = 50;
            Disp_00_0();
            break;
        }

        break;
    }
}

/** @brief Display the "PRESS START" and per-player start prompts on title screen. */
static void Disp_00_0() {
    if (save_w[1].extra_option.contents[3][5] == 0) {
        return;
    }

    if (use_rmlui && rmlui_screen_title && title_tex_flag) {
        /* CSS blink animation handles the visibility cycle (title screen only) */
        return;
    }

    if (use_rmlui && rmlui_screen_attract_overlay && g_state.fsm[0] == 1 && g_state.fsm[1] >= 3) {
        /* Attract overlay handles the logo + blink during demo fights */
        return;
    }

    if (ENTRY_TEXT_GATED) {
        return;
    }

    SSPutStr(16, g_state.Insert_Y, 9, "PRESS ANY BUTTON");

    if (!(g_state.fsm[1] == 3 || g_state.fsm[1] == 5)) {
        return;
    }

    SSPutStr(5, 0, 9, "PRESS 1P START");
    SSPutStr(30, 0, 9, "PRESS 2P START");
}

/** @brief Entry phase 1 — wait for a start button press and route to the first player init. */
static void Entry_WaitStart() {
    switch (g_state.entry_phase[2]) {
    case ENTRY_PL_INIT:
        g_state.entry_phase[2] += 1;
        g_state.entry_phase[1] = ENTRY_SUB_ACTIVE;
        g_state.Break_Into = 0;
        if (use_rmlui && rmlui_screen_title)
            rmlui_title_screen_show();
        break;

    case ENTRY_PL_CREDIT:
        Entry_TitleBlink();

        if (~p1sw_1 & p1sw_0 & (SWK_START | SWK_ATTACKS)) {
            Entry_01_Sub(0);
        } else if (~p2sw_1 & p2sw_0 & (SWK_START | SWK_ATTACKS)) {
            Entry_01_Sub(1);
        }

        break;

    case ENTRY_PL_NAMING:
        if (g_state.Request_E_No) {
            g_state.entry_phase[2] += 1;
        }

        break;

    default:
        Exit_Title_Entry();
        break;
    }
}

/** @brief Initialise the player who pressed start (operator flag, champion, grades). */
static void Entry_01_Sub(s16 PL_id) {
    g_state.entry_phase[2] += 1;
    g_state.Request_G_No = 1;
    g_state.plw[PL_id].wu.pl_operator = 1;
    g_state.Operator_Status[PL_id] = 1;
    g_state.Champion = PL_id;
    g_state.plw[PL_id ^ 1].wu.pl_operator = 0;
    g_state.Operator_Status[PL_id ^ 1] = 0;
    g_state.Ignore_Entry[0] = 0;
    g_state.Ignore_Entry[1] = 0;

    if (use_rmlui && rmlui_screen_title) {
        rmlui_title_screen_hide();
        rmlui_copyright_hide();
    }

    if (g_state.Continue_Coin[PL_id] == 0) {
        grade_check_work_1st_init(PL_id, 0);
    }
}

/** @brief Reset all entry/flash counters and advance to entry phase 2. */
static void Exit_Title_Entry() {
    s16 i;
    s16 j;

    g_state.entry_phase[0] = ENTRY_MID_GAME_ENTRY;
    g_state.entry_phase[1] = ENTRY_SUB_INIT;
    g_state.entry_phase[2] = 0;
    g_state.entry_phase[3] = 0;
    g_state.F_No1[0] = g_state.F_No2[0] = g_state.F_No3[0] = 0;
    g_state.F_No1[1] = g_state.F_No2[1] = g_state.F_No3[1] = 0;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 4; j++) {
            g_state.E_Number[i][j] = 0;
        }
    }
}

/** @brief Entry phase 2 — standard mid-game entry; dispatch both players' entry sub-states. */
static void Entry_MidGameEntry() {
    switch (g_state.entry_phase[1]) {
    case ENTRY_PL_INIT:
        g_state.entry_phase[1] += 1;
        break;
    }

    Entry_Main_Sub(0, 2);
    Entry_Main_Sub(1, 2);
}

/** @brief Entry phase 3 — pre-fight break-in check or post-round screen switch. */
static void Entry_PreFightBreak() {
    switch (g_state.entry_phase[1]) {
    case ENTRY_PL_INIT:
        Entry_PreFightBreak_Phase1();
        break;

    default:
        Entry_PreFightBreak_Phase2();
        break;
    }
}

/** @brief Entry_PreFightBreak first sub-phase — dispatch both players' entry sub-states. */
static void Entry_PreFightBreak_Phase1() {
    entry_phase_1st(4);
}

/** @brief Entry_PreFightBreak second sub-phase — screen-switch transition and set up new challenger. */
static void Entry_PreFightBreak_Phase2() {
    switch (g_state.entry_phase[2]) {
    case ENTRY_PL_INIT:
        if (--g_state.entry_timer == 0) {
            if (Check_LDREQ_Break() == 0) {
                g_state.entry_phase[2] += 1;
                Switch_Screen_Init(1);
                return;
            }

            g_state.entry_timer = 1;
            return;
        }

        break;

    case ENTRY_PL_CREDIT:
        if (Switch_Screen(1) != 0) {
            g_state.Cover_Timer = 23;
            FSM_SetMode(MODE_CHAR_SELECT);
            g_state.entry_phase[0] = ENTRY_MID_GAME_ENTRY;
            g_state.entry_phase[1] = ENTRY_SUB_INIT;
            g_state.entry_phase[2] = 0;
            g_state.entry_phase[3] = 0;
            g_state.plw[g_state.New_Challenger].wu.pl_operator = 1;
            g_state.Operator_Status[g_state.New_Challenger] = 1;
            g_state.Sel_Arts_Complete[g_state.Champion] = -1;

            if (g_state.Continue_Coin[g_state.New_Challenger] == 0) {
                grade_check_work_1st_init(g_state.New_Challenger, 0);
            }
        }

        break;
    }
}

/** @brief Entry phase 4 — mid-round break-in (pause-aware). */
static void Entry_MidRoundBreak() {
    switch (g_state.entry_phase[1]) {
    case ENTRY_PL_INIT:
        Entry_MidRoundBreak_Phase1();
        break;

    default:
        Entry_MidRoundBreak_Phase2();
        break;
    }
}

/** @brief Entry_MidRoundBreak first sub-phase — both players' break-in check (skipped during g_state.Game_pause). */
static void Entry_MidRoundBreak_Phase1() {
    switch (g_state.entry_phase[2]) {
    case ENTRY_PL_INIT:
        g_state.entry_phase[2] += 1;
        break;
    }

    if (g_state.Game_pause != 0x81) {
        Entry_Main_Sub(0, 5);
        Entry_Main_Sub(1, 5);
    }
}

/** @brief Entry_MidRoundBreak second sub-phase — screen wipe, correct break-in data, set up challenger. */
static void Entry_MidRoundBreak_Phase2() {
    switch (g_state.entry_phase[2]) {
    case ENTRY_PL_INIT:
        if (--g_state.entry_timer == 0) {
            if (Check_LDREQ_Break() == 0) {
                g_state.entry_phase[2] += 1;
                Switch_Screen_Init(1);
                return;
            }

            g_state.entry_timer = 1;
            return;
        }

        break;

    case ENTRY_PL_CREDIT:
        if (Switch_Screen(0) != 0) {
            g_state.entry_phase[2] += 1;
            g_state.Cover_Timer = 23;
            FSM_SetMode(MODE_CHAR_SELECT);

            if (g_state.entry_phase[3] == 0xFF) {
                g_state.E_Number[g_state.LOSER][0] = ENTRY_PL_CREDIT;
                g_state.E_Number[g_state.LOSER][1] = 0;
                g_state.E_Number[g_state.LOSER][2] = 0;
                g_state.E_Number[g_state.LOSER][3] = 0;
            } else {
                Correct_BI_Data();
            }

            g_state.entry_phase[0] = ENTRY_MID_GAME_ENTRY;
            g_state.entry_phase[1] = ENTRY_SUB_INIT;
            g_state.entry_phase[2] = 0;
            g_state.entry_phase[3] = 0;
            g_state.Game_pause = 0;
            g_state.plw[g_state.New_Challenger].wu.pl_operator = 1;
            g_state.Operator_Status[g_state.New_Challenger] = 1;

            if (g_state.Continue_Coin[g_state.New_Challenger] == 0) {
                grade_check_work_1st_init(g_state.New_Challenger, 0);
            }
        }

        break;
    }
}

/** @brief Entry phase 6 — post-continue break-in with screen switch. */
static void Entry_PostContinueBreak() {
    switch (g_state.entry_phase[1]) {
    case ENTRY_PL_INIT:
        Entry_PostContinueBreak_Phase1();
        break;

    default:
        Entry_PostContinueBreak_Phase2();
        break;
    }
}

/** @brief Common first sub-phase — increment g_state.entry_phase[2] and dispatch both players. */
static void entry_phase_1st(s16 jump_index) {
    switch (g_state.entry_phase[2]) {
    case ENTRY_PL_INIT:
        g_state.entry_phase[2] += 1;
        break;
    }

    Entry_Main_Sub(0, jump_index);
    Entry_Main_Sub(1, jump_index);
}

/** @brief Entry_PostContinueBreak first sub-phase — dispatch both players' entry sub-states. */
static void Entry_PostContinueBreak_Phase1() {
    entry_phase_1st(7);
}

/** @brief Activates newly-joined operators and resets break-in flags. */
static void activate_new_operators(void) {
    s16 i;

    for (i = 0; i < 2; i++) {
        if (g_state.E_07_Flag[i]) {
            g_state.plw[i].wu.pl_operator = 1;
            g_state.Operator_Status[i] = 1;

            if (g_state.Continue_Coin[i] == 0) {
                grade_check_work_1st_init(i, 0);
            }
        }
    }

    g_state.E_07_Flag[0] = 0;
    g_state.E_07_Flag[1] = 0;
}

/** @brief Entry_PostContinueBreak second sub-phase — screen-switch and activate new operators. */
static void Entry_PostContinueBreak_Phase2() {
    if (g_state.E_07_Flag[0] == 0) {
        Entry_Main_Sub(0, 7);
    }

    if (g_state.E_07_Flag[1] == 0) {
        Entry_Main_Sub(1, 7);
    }

    switch (g_state.entry_phase[2]) {
    case ENTRY_PL_INIT:
        g_state.entry_phase[2] += 1;
        Switch_Screen_Init(1);
        break;

    case ENTRY_PL_CREDIT:
        if (Switch_Screen(1) != 0) {
            g_state.entry_phase[2] += 1;
            g_state.Cover_Timer = 23;
            return;
        }

        break;

    default:
        Switch_Screen(1);
        FSM_SetMode(MODE_CHAR_SELECT);
        g_state.entry_phase[0] = ENTRY_MID_GAME_ENTRY;
        g_state.entry_phase[1] = ENTRY_SUB_INIT;
        g_state.entry_phase[2] = 0;
        g_state.entry_phase[3] = 0;
        g_state.Fade_Flag = 0;

        activate_new_operators();

        if (g_state.E_Number[g_state.LOSER][0] == ENTRY_PL_LOSER) {
            g_state.E_Number[g_state.LOSER][0] = ENTRY_PL_CREDIT;
        }

        break;
    }
}

/** @brief Entry phase 7 — post-fight break-in with timed delay before transition. */
static void Entry_PostFightBreak() {
    switch (g_state.entry_phase[1]) {
    case ENTRY_PL_INIT:
        Entry_PostFightBreak_Phase1();
        break;

    default:
        Entry_PostFightBreak_Phase2();
        break;
    }
}

/** @brief Entry_PostFightBreak first sub-phase — dispatch both players' entry sub-states. */
static void Entry_PostFightBreak_Phase1() {
    entry_phase_1st(8);
}

/** @brief Entry_PostFightBreak second sub-phase — timer-based screen switch and activate new operators. */
static void Entry_PostFightBreak_Phase2() {
    if (g_state.E_07_Flag[0] == 0) {
        Entry_Main_Sub(0, 8);
    }

    if (g_state.E_07_Flag[1] == 0) {
        Entry_Main_Sub(1, 8);
    }

    switch (g_state.entry_phase[2]) {
    case ENTRY_PL_INIT:
        if (!--g_state.entry_timer) {
            g_state.entry_phase[2] += 1;
            Switch_Screen_Init(1);
        }

        break;

    default:
        if (Switch_Screen(1) != 0) {
            g_state.Cover_Timer = 23;
            FSM_SetMode(MODE_CHAR_SELECT);
            g_state.entry_phase[0] = ENTRY_MID_GAME_ENTRY;
            g_state.entry_phase[1] = ENTRY_SUB_INIT;
            g_state.entry_phase[2] = 0;
            g_state.entry_phase[3] = 0;

            activate_new_operators();
        }

        break;
    }
}

/** @brief Entry phase 8 — end-of-game break-in with ranking data cleanup. */
static void Entry_EndGameBreak() {
    switch (g_state.entry_phase[1]) {
    case ENTRY_PL_INIT:
        Entry_EndGameBreak_Phase1();
        break;

    default:
        Entry_EndGameBreak_Phase2();
        break;
    }
}

/** @brief Entry_EndGameBreak first sub-phase — dispatch both players' entry sub-states (with fallthrough). */
static void Entry_EndGameBreak_Phase1() {
    switch (g_state.entry_phase[2]) {
    case ENTRY_PL_INIT:
        g_state.entry_phase[2] += 1;
        /* fallthrough */

    case ENTRY_PL_CREDIT:
        Entry_Main_Sub(0, 9);
        Entry_Main_Sub(1, 9);
        break;
    }
}

/** @brief Entry_EndGameBreak second sub-phase — clear personal data, screen switch, reset rank displays. */
static void Entry_EndGameBreak_Phase2() {
    entry_end_2nd(9);
}

/** @brief Entry phase 10 — final/ending entry phase, compute rankings and dispatch sub-states. */
static void Entry_FinalEnding() {
    if ((g_state.E_Number[0][0] == 0x63) && (g_state.E_Number[1][0] == 0x63)) {
        cpExitTask(TASK_ENTRY);
        return;
    }

    switch (g_state.entry_phase[1]) {
    case ENTRY_PL_INIT:
        Entry_FinalEnding_Phase1();
        break;

    default:
        Entry_FinalEnding_Phase2();
        break;
    }
}

/** @brief Entry_FinalEnding first sub-phase — compute final grade, check ranking, dispatch players. */
static void Entry_FinalEnding_Phase1() {
    switch (g_state.entry_phase[2]) {
    case ENTRY_PL_INIT:
        g_state.entry_phase[2] += 1;
        break;

    case ENTRY_PL_CREDIT:
        g_state.entry_phase[2] += 1;
        Setup_Final_Grade();

        if (Check_Ranking(g_state.WINNER) != 0) {
            g_state.E_Number[g_state.WINNER][0] = ENTRY_PL_NAMING;
            g_state.E_Number[g_state.WINNER][1] = 0;
            g_state.E_Number[g_state.WINNER][2] = 0;
            g_state.E_Number[g_state.WINNER][3] = 0;
            g_state.Request_Disp_Rank[g_state.WINNER][0] = g_state.Rank_In[g_state.WINNER][0];
            g_state.Request_Disp_Rank[g_state.WINNER][1] = g_state.Rank_In[g_state.WINNER][1];
            g_state.Request_Disp_Rank[g_state.WINNER][2] = g_state.Rank_In[g_state.WINNER][2];
            g_state.Request_Disp_Rank[g_state.WINNER][3] = g_state.Rank_In[g_state.WINNER][3];
        } else {
            g_state.E_Number[g_state.WINNER][0] = ENTRY_PL_GAME_OVER;
            g_state.E_Number[g_state.WINNER][1] = 0;
        }

        /* fallthrough */

    default:
        Entry_Main_Sub(0, 10);
        Entry_Main_Sub(1, 10);
        break;
    }
}

/** @brief Common end-phase second sub-phase — dispatch players, clear personal data, screen switch, reset ranks. */
static void entry_end_2nd(s16 jump_index) {
    if (g_state.E_07_Flag[0] == 0) {
        Entry_Main_Sub(0, jump_index);
    }

    if (g_state.E_07_Flag[1] == 0) {
        Entry_Main_Sub(1, jump_index);
    }

    switch (g_state.entry_phase[2]) {
    case ENTRY_PL_INIT:
        g_state.entry_phase[2] += 1;

        if ((g_state.E_Number[g_state.LOSER][0] == ENTRY_PL_GAME_OVER) && (g_state.E_Number[g_state.LOSER][1] == 1)) {
            Clear_Personal_Data(g_state.LOSER);
        }

        Switch_Screen_Init(1);
        break;

    default:
        if (Switch_Screen(1) != 0) {
            g_state.Cover_Timer = 23;
            FSM_SetMode(MODE_CHAR_SELECT);
            g_state.entry_phase[0] = ENTRY_MID_GAME_ENTRY;
            g_state.entry_phase[1] = ENTRY_SUB_INIT;
            g_state.entry_phase[2] = 0;
            g_state.entry_phase[3] = 0;

            activate_new_operators();
            g_state.Request_Disp_Rank[0][0] = -1;
            g_state.Request_Disp_Rank[0][1] = -1;
            g_state.Request_Disp_Rank[1][0] = -1;
            g_state.Request_Disp_Rank[1][1] = -1;
        }

        break;
    }
}

/** @brief Entry_FinalEnding second sub-phase — screen switch, clear personal data, reset rank displays. */
static void Entry_FinalEnding_Phase2() {
    entry_end_2nd(10);
}

/** @brief Per-player entry sub-state dispatcher — handles credit, continue, naming, game-over flow. */
static void Entry_Main_Sub(s16 PL_id, s16 Jump_Index) {
    g_state.ENTRY_X = 0;

    switch (g_state.E_Number[PL_id][0]) {
    case ENTRY_PL_INIT:
        if (!g_state.Ignore_Entry[g_state.LOSER]) {
            if ((g_state.entry_phase[0] == ENTRY_FINAL_ENDING) || (g_state.entry_phase[0] == ENTRY_END_GAME_BREAK)) {
                g_state.E_Number[PL_id][0] = 99;
                return;
            }

            if (g_state.plw[PL_id].wu.pl_operator == 0) {
                Entry_Common_Sub(PL_id, Jump_Index);
                return;
            }
        }

        break;

    case ENTRY_PL_CREDIT:
        if (PL_id) {
            if (Credit_Continue_2P() != 0) {
                Break_Into_Sub(PL_id, Jump_Index);
            }
        } else if (Credit_Continue_1P() != 0) {
            Break_Into_Sub(PL_id, Jump_Index);
        }

        if (g_state.Request_Break[PL_id]) {
            g_state.E_Number[PL_id][0] = ENTRY_PL_INIT;
            g_state.E_Number[PL_id][1] = 0;
            g_state.E_Number[PL_id][2] = 0;
            g_state.E_Number[PL_id][3] = 0;
            return;
        }

        if ((g_state.E_Number[PL_id][0] == ENTRY_PL_CREDIT) && (g_state.E_07_Flag[PL_id ^ 1] == 0)) {
            Entry_Continue_Sub(PL_id);
            return;
        }

        break;

    case ENTRY_PL_NAMING:
        switch (g_state.E_Number[PL_id][1]) {
        case ENTRY_PL_INIT:
            g_state.E_Number[PL_id][1] += 1;
            g_state.Personal_Timer[PL_id] = 30;
            break;

        case ENTRY_PL_CREDIT:
            if (!--g_state.Personal_Timer[PL_id]) {
                g_state.E_Number[PL_id][1] += 1;
                Naming_Init(PL_id);
                return;
            }

            break;

        case ENTRY_PL_NAMING:
            if (g_state.Forbid_Break != 1) {
                if (PL_id == 0) {
                    Naming_Cut_1P();
                } else {
                    Naming_Cut_2P();
                }

                if (Name_Input(PL_id)) {
                    Name_In_Sub(PL_id);

                    if (g_state.Naming_Cut[PL_id]) {
                        Clear_Personal_Data(PL_id);
                        return;
                    }

                    g_state.E_Number[PL_id][2] = 0;
                    g_state.E_Number[PL_id][3] = 0;

                    if (g_state.entry_phase[0] == ENTRY_END_GAME_BREAK) {
                        g_state.E_Number[PL_id][0] = ENTRY_PL_GAME_OVER;
                        g_state.E_Number[PL_id][1] = 1;
                        return;
                    }

                    g_state.E_Number[PL_id][0] = ENTRY_PL_GAME_OVER;
                    g_state.E_Number[PL_id][1] = 0;
                    return;
                }
            }

            break;
        }

        break;

    case ENTRY_PL_RANKING:
        switch (g_state.E_Number[PL_id][1]) {
        case ENTRY_PL_INIT:
            if ((g_state.entry_phase[0] == ENTRY_END_GAME_BREAK) || (g_state.entry_phase[0] == ENTRY_MID_GAME_ENTRY)) {
                g_state.E_Number[PL_id][0] = ENTRY_PL_NAMING;
                g_state.E_Number[PL_id][1] = 2;
                g_state.E_Number[PL_id][2] = 0;
                g_state.E_Number[PL_id][3] = 0;
                Naming_Init(PL_id);
                return;
            }

            break;

        case ENTRY_PL_CREDIT:
            if ((g_state.entry_phase[0] == ENTRY_END_GAME_BREAK) || (g_state.entry_phase[0] == ENTRY_MID_GAME_ENTRY)) {
                g_state.E_Number[PL_id][0] = ENTRY_PL_GAME_OVER;
                g_state.E_Number[PL_id][1] = 1;
                g_state.E_Number[PL_id][2] = 0;
                g_state.E_Number[PL_id][3] = 0;

                if (g_state.entry_phase[0] == ENTRY_MID_GAME_ENTRY) {
                    g_state.E_Number[PL_id][1] = 0;
                    return;
                }
            }

            break;
        }

        break;

    case ENTRY_PL_GAME_OVER:
        switch (g_state.E_Number[PL_id][1]) {
        case ENTRY_PL_INIT:
            In_Game_Sub(PL_id);
            break;

        case ENTRY_PL_CREDIT:
            In_Over_Sub(PL_id);
            break;
        }

        break;

    case ENTRY_PL_LOSER:
        Loser_Scene_Sub(PL_id, Jump_Index);
        break;
    }
}

/** @brief Initialise name-entry state for the given player. */
static void Naming_Init(s16 PL_id) {
    g_state.Naming_Cut[PL_id] = 0;
    Name_00[PL_id] = 0;
    name_wk[PL_id].r_no_0 = 0;
    name_wk[PL_id].r_no_1 = 0;
    end_name_cut[PL_id] = 0;
}

/** @brief If 1P pressed start during naming, cut short and flag the name entry as complete. */
static void Naming_Cut_1P() {
    if (!g_state.Naming_Cut[0] && (Ck_Break_Into_SP(p1sw_0, p1sw_1, 0) != 0)) {
        g_state.Game_pause = 0;
        g_state.Naming_Cut[0] = 1;
        g_state.Request_Break[0] = 1;
    }
}

/** @brief If 2P pressed start during naming, cut short and flag the name entry as complete. */
static void Naming_Cut_2P() {
    if (!g_state.Naming_Cut[1] && (Ck_Break_Into_SP(p2sw_0, p2sw_1, 1) != 0)) {
        g_state.Game_pause = 0;
        g_state.Naming_Cut[1] = 1;
        g_state.Request_Break[1] = 1;
    }
}

/** @brief Copy the entered name into all applicable ranking slots for this player. */
static void Name_In_Sub(s16 PL_id) {
    if (g_state.Rank_In[PL_id][0] >= 0) {
        Name_In_Sub0(PL_id, g_state.Rank_In[PL_id][0] + 0);
    }

    if (g_state.Rank_In[PL_id][1] >= 0) {
        Name_In_Sub0(PL_id, g_state.Rank_In[PL_id][1] + 5);
    }

    if (g_state.Rank_In[PL_id][2] >= 0) {
        Name_In_Sub0(PL_id, g_state.Rank_In[PL_id][2] + 10);
    }

    if (g_state.Rank_In[PL_id][3] >= 0) {
        Name_In_Sub0(PL_id, g_state.Rank_In[PL_id][3] + 15);
    }
}

/** @brief Write the player's 3-letter name into a single ranking slot. */
static void Name_In_Sub0(s16 PL_id, s16 xx) {
    Ranking_Data[xx].name[0] = rank_name_w[PL_id].code[0];
    Ranking_Data[xx].name[1] = rank_name_w[PL_id].code[1];
    Ranking_Data[xx].name[2] = rank_name_w[PL_id].code[2];
}

/** @brief Common entry sub — check credit and break-in for a non-operator player. */
static void Entry_Common_Sub(s16 PL_id, s16 Jump_Index) {
    if (PL_id) {
        if (Credit_2P() != 0) {
            Break_Into_Sub(PL_id, Jump_Index);
        }
    } else if (Credit_1P() != 0) {
        Break_Into_Sub(PL_id, Jump_Index);
    }
}

/** @brief Loser-side entry sub — check credit and break-in for the losing player. */
static void Loser_Scene_Sub(s16 PL_id, s16 Jump_Index) {
    if (PL_id) {
        if (Loser_2P() != 0) {
            Break_Into_Sub(PL_id, Jump_Index);
        }
    } else if (Loser_1P() != 0) {
        Break_Into_Sub(PL_id, Jump_Index);
    }
}

/** @brief 1P loser credit/continue check — display "CONTINUE?" or flash start prompt. */
static s32 Loser_1P() {
    if ((Ck_Break_Into(p1sw_0, p1sw_1, 0) == 0) && !g_state.Request_Break[0]) {
        if (g_state.LOSER == 0) {
            if (save_w[1].extra_option.contents[3][5]) {
                if (!ENTRY_TEXT_GATED)
                    SSPutStr(g_state.DE_X[0], 0, 9, "     CONTINUE?");
            }
        } else {
            Flash_Start(0);
        }
    }

    return g_state.ENTRY_X;
}

/** @brief 2P loser credit/continue check — display "CONTINUE?" or flash start prompt. */
static s32 Loser_2P() {
    if ((Ck_Break_Into(p2sw_0, p2sw_1, 1) == 0) && !g_state.Request_Break[1]) {
        if (g_state.LOSER == 1) {
            if (save_w[1].extra_option.contents[3][5]) {
                if (!ENTRY_TEXT_GATED)
                    SSPutStr(g_state.DE_X[1], 0, 9, "     CONTINUE?");
            }
        } else {
            Flash_Start(1);
        }
    }

    return g_state.ENTRY_X;
}

/** @brief 1P credit check — flash "PRESS START" or "PLEASE WAIT" depending on break state. */
static s32 Credit_1P() {
    if (Ck_Break_Into(p1sw_0, p1sw_1, 0) == 0) {
        if (g_state.Request_Break[0]) {
            Flash_Please(0);
        } else {
            Flash_Start(0);
        }
    }

    return g_state.ENTRY_X;
}

/** @brief 2P credit check — flash "PRESS START" or "PLEASE WAIT" depending on break state. */
static s32 Credit_2P() {
    if (Ck_Break_Into(p2sw_0, p2sw_1, 1) == 0) {
        if (g_state.Request_Break[1]) {
            Flash_Please(1);
        } else {
            Flash_Start(1);
        }
    }

    return g_state.ENTRY_X;
}

/** @brief 1P continue credit check — just call Ck_Break_Into and return the entry flag. */
static s32 Credit_Continue_1P() {
    Ck_Break_Into(p1sw_0, p1sw_1, 0);
    return g_state.ENTRY_X;
}

/** @brief 2P continue credit check — just call Ck_Break_Into and return the entry flag. */
static s32 Credit_Continue_2P() {
    Ck_Break_Into(p2sw_0, p2sw_1, 1);
    return g_state.ENTRY_X;
}

/** @brief Continue-screen sub — countdown timer, check for cut, advance to ranking or game-over. */
static void Entry_Continue_Sub(s16 PL_id) {
    if ((g_state.Continue_Count_Down[PL_id] == 0) && save_w[1].extra_option.contents[3][5]) {
        if (!ENTRY_TEXT_GATED) {
            SSPutStr(g_state.DE_X[PL_id], 0, 9, "     CONTINUE?");
            Disp_Personal_Count(PL_id, g_state.Continue_Count[PL_id]);
        }
    }

    switch (g_state.E_Number[PL_id][1]) {
    case ENTRY_PL_INIT:
        if (g_state.Continue_Count_Down[PL_id] == 0) {
            g_state.E_Number[PL_id][1] += 1;
            g_state.Personal_Timer[PL_id] = 60;
            return;
        }

        break;

    case ENTRY_PL_CREDIT:
        if (Check_Count_Cut(PL_id, 8)) {
            g_state.Continue_Cut[PL_id] = 1;
        } else if (--g_state.Personal_Timer[PL_id]) {
            break;
        }

        if (--g_state.Continue_Count[PL_id] >= 0) {
            g_state.Personal_Timer[PL_id] = 60;
            return;
        }

        Setup_Next_Step(PL_id);

        break;
    }
}

/** @brief Advance the player to the next entry step after continue expires (ranking or game-over). */
static void Setup_Next_Step(s16 PL_id) {
    s16 xx;

    g_state.E_Number[PL_id][1] = 0;
    g_state.E_Number[PL_id][2] = 0;
    g_state.E_Number[PL_id][3] = 0;

    for (xx = 0; xx < 20; xx++) {
        g_state.Break_Com[PL_id][xx] = 0;
    }

    if (g_state.entry_phase[0] != ENTRY_POST_FIGHT_BREAK) {
        Setup_Final_Grade();

        if (Check_Ranking(PL_id) != 0) {
            g_state.E_Number[PL_id][0] = ENTRY_PL_NAMING;
            g_state.Request_Disp_Rank[PL_id][0] = g_state.Rank_In[PL_id][0];
            g_state.Request_Disp_Rank[PL_id][1] = g_state.Rank_In[PL_id][1];
            g_state.Request_Disp_Rank[PL_id][2] = g_state.Rank_In[PL_id][2];
            g_state.Request_Disp_Rank[PL_id][3] = g_state.Rank_In[PL_id][3];
            return;
        }

        g_state.E_Number[PL_id][0] = ENTRY_PL_GAME_OVER;
        g_state.E_Number[PL_id][1] = 0;
        return;
    }

    Setup_Final_Grade();

    if (Check_Ranking(PL_id) != 0) {
        g_state.Request_Disp_Rank[PL_id][0] = g_state.Rank_In[PL_id][0];
        g_state.Request_Disp_Rank[PL_id][1] = g_state.Rank_In[PL_id][1];
        g_state.Request_Disp_Rank[PL_id][2] = g_state.Rank_In[PL_id][2];
        g_state.Request_Disp_Rank[PL_id][3] = g_state.Rank_In[PL_id][3];

        if (g_state.E_Number[PL_id ^ 1][0] != ENTRY_PL_INIT) {
            g_state.E_Number[PL_id][0] = ENTRY_PL_NAMING;
            return;
        }

        g_state.E_Number[PL_id][0] = ENTRY_PL_RANKING;
        g_state.E_Number[PL_id][1] = 0;
        return;
    }

    if (g_state.E_Number[PL_id ^ 1][0] != ENTRY_PL_INIT) {
        g_state.E_Number[PL_id][0] = ENTRY_PL_GAME_OVER;
        g_state.E_Number[PL_id][1] = 0;
        return;
    }

    g_state.E_Number[PL_id][0] = ENTRY_PL_RANKING;
    g_state.E_Number[PL_id][1] = 1;
}

/** @brief In-game sub — timed "GAME OVER" display, then clear personal data. */
static void In_Game_Sub(s16 PL_id) {
    switch (g_state.E_Number[PL_id][2]) {
    case ENTRY_PL_INIT:
        g_state.E_Number[PL_id][2] += 1;
        g_state.Personal_Timer[PL_id] = 30;
        break;

    case ENTRY_PL_CREDIT:
        if (--g_state.Personal_Timer[PL_id] == 0) {
            g_state.E_Number[PL_id][2] += 1;
            g_state.Personal_Timer[PL_id] = 60;
            return;
        }

        break;

    case ENTRY_PL_NAMING:
        if (save_w[1].extra_option.contents[3][5]) {
            if (!ENTRY_TEXT_GATED)
                SSPutStr(g_state.DE_X[PL_id], 0, 9, "     GAME OVER");
        }

        if (--g_state.Personal_Timer[PL_id] == 0) {
            g_state.E_Number[PL_id][2] += 1;
            g_state.Personal_Timer[PL_id] = 30;
            return;
        }

        break;

    default:
        if (--g_state.Personal_Timer[PL_id] == 0) {
            if ((g_state.entry_phase[0] == ENTRY_FINAL_ENDING) || (g_state.entry_phase[0] == ENTRY_END_GAME_BREAK)) {
                g_state.E_Number[PL_id][0] = 99;
                return;
            }

            Clear_Personal_Data(PL_id);
            Clear_Flash_No();
        }

        break;
    }
}

/** @brief In-game over sub — display "GAME OVER" text persistently. */
static void In_Over_Sub(s16 PL_id) {
    switch (g_state.E_Number[PL_id][2]) {
    case ENTRY_PL_INIT:
        g_state.E_Number[PL_id][2] += 1;
        break;
    }

    if (save_w[1].extra_option.contents[3][5]) {
        if (!ENTRY_TEXT_GATED)
            SSPutStr(g_state.DE_X[PL_id], 0, 9, "     GAME OVER");
    }
}

/** @brief Flash "PRESS START" prompt with timed blink cycle for the given player. */
static s32 Flash_Start(s16 PL_id) {
    switch (g_state.F_No1[PL_id]) {
    case ENTRY_PL_INIT:
        g_state.F_No1[PL_id] += 1;
        g_state.F_No0[PL_id] = 0;
        g_state.F_No2[PL_id] = 0;
        g_state.F_No3[PL_id] = 0;
        g_state.F_Timer[PL_id] = 1;

        if ((g_state.entry_phase[0] == ENTRY_POST_CONTINUE_BREAK) && (PL_id == g_state.LOSER)) {
            g_state.F_No1[PL_id] = 3;
        }

        break;

    case ENTRY_PL_CREDIT:
        if (!--g_state.F_Timer[PL_id]) {
            g_state.F_No1[PL_id] += 1;
            g_state.F_Timer[PL_id] = 50;

            if (save_w[1].extra_option.contents[3][5]) {
                if (PL_id) {
                    if (!ENTRY_TEXT_GATED)
                        SSPutStr(g_state.DE_X[1], 0, 9, "   PRESS 2P START");
                } else {
                    if (!ENTRY_TEXT_GATED)
                        SSPutStr(g_state.DE_X[0], 0, 9, "   PRESS 1P START");
                }
            }
        }

        break;

    case ENTRY_PL_NAMING:
        if (--g_state.F_Timer[PL_id]) {
            if (save_w[1].extra_option.contents[3][5]) {
                if (PL_id) {
                    if (!ENTRY_TEXT_GATED)
                        SSPutStr(g_state.DE_X[1], 0, 9, "   PRESS 2P START");
                } else {
                    if (!ENTRY_TEXT_GATED)
                        SSPutStr(g_state.DE_X[0], 0, 9, "   PRESS 1P START");
                }
            }
        } else {
            g_state.F_No1[PL_id] -= 1;
            g_state.F_Timer[PL_id] = 30;
        }

        break;

    case ENTRY_PL_RANKING:
        g_state.F_No1[PL_id] = 99;
        /* fallthrough */

    default:
        if (save_w[1].extra_option.contents[3][5]) {
            if (!ENTRY_TEXT_GATED)
                SSPutStr(g_state.DE_X[1], 0, 9, "     CONTINUE?");
        }

        break;
    }

    return 0;
}

/** @brief Flash "PLEASE WAIT" prompt when the other player has already broken in. */
static s32 Flash_Please(s16 PL_id) {
    if (g_state.entry_phase[0] == ENTRY_POST_CONTINUE_BREAK || g_state.entry_phase[0] == ENTRY_END_GAME_BREAK) {
        return 0;
    }

    switch (g_state.F_No3[PL_id]) {
    case ENTRY_PL_INIT:
        g_state.F_No3[PL_id] += 1;
        g_state.F_No1[PL_id] = 0;
        g_state.F_Timer[PL_id] = 1;
        break;

    case ENTRY_PL_CREDIT:
        if (--g_state.F_Timer[PL_id] == 0) {
            g_state.F_No3[PL_id] += 1;
            g_state.F_Timer[PL_id] = 50;
        }

        break;

    default:
        if (--g_state.F_Timer[PL_id]) {
            if (!ENTRY_TEXT_GATED)
                SSPutStr(g_state.DE_X[PL_id], 0, 9, "    PLEASE WAIT");
        } else {
            g_state.F_No3[PL_id] -= 1;
            g_state.F_Timer[PL_id] = 30;
        }

        break;
    }

    return 0;
}

/** @brief Route a break-in to the correct Break_Into_XX handler based on Jump_Index. */
static void Break_Into_Sub(s16 PL_id, s16 Jump_Index) {
    switch (Jump_Index) {
    case ENTRY_PL_INIT:
    case ENTRY_PL_CREDIT:
    case ENTRY_PL_NAMING:
    case ENTRY_PL_RANKING:
        Break_Into_02(PL_id);
        break;

    case 4:
    case 6:
        Break_Into_04(PL_id);
        break;

    case ENTRY_PL_LOSER:
        Break_Into_05(PL_id);
        break;

    case 7:
        Break_Into_07(PL_id);
        break;

    case ENTRY_PL_GAME_OVER:
        Break_Into_08(PL_id);
        break;

    case 9:
        Break_Into_09(PL_id);
        break;

    case 10:
        Break_Into_10(PL_id);
        break;

    default:
        break;
    }
}

/** @brief Check for break-in input — if start pressed, set challenger/champion and signal entry. */
s32 Ck_Break_Into(u16 Sw_0, u16 Sw_1, s16 PL_id) {
    if ((g_state.entry_phase[0] != ENTRY_FINAL_ENDING) && g_state.Request_Break[PL_id ^ 1]) {
        return 0;
    }

    if (g_state.Request_Break[PL_id]) {
        if (g_state.Forbid_Break || g_state.Extra_Break) {
            return 0;
        }

        g_state.Game_pause = 1;
        g_state.New_Challenger = PL_id;
        g_state.Champion = g_state.New_Challenger ^ 1;
        g_state.Request_Break[PL_id] = 0;
        return g_state.ENTRY_X = 1;
    }

    if (!(~Sw_1 & Sw_0 & 0x4000)) {
        return 0;
    }

    Continue_Score_Sub(PL_id);

    if (g_state.Forbid_Break || g_state.Extra_Break) {
        g_state.Request_Break[PL_id] = 1;
    } else {
        g_state.Game_pause = 1;
        g_state.New_Challenger = PL_id;
        g_state.Champion = g_state.New_Challenger ^ 1;
        return g_state.ENTRY_X = 1;
    }

    return 0;
}

/** @brief Simplified break-in check for special contexts (no forbid/extra logic). */
s32 Ck_Break_Into_SP(u16 Sw_0, u16 Sw_1, s16 PL_id) {
    if (!(~Sw_1 & Sw_0 & 0x4000)) {
        return 0;
    }

    g_state.New_Challenger = PL_id;
    g_state.Champion = g_state.New_Challenger ^ 1;
    return g_state.ENTRY_X = 1;
}

/** @brief Break-in type 02 — activate challenger, reset entry state, init grades. */
static void Break_Into_02(s16 /* unused */) {
    g_state.plw[g_state.New_Challenger].wu.pl_operator = 1;
    g_state.Operator_Status[g_state.New_Challenger] = 1;
    g_state.E_Number[g_state.New_Challenger][0] = ENTRY_PL_INIT;
    g_state.E_Number[g_state.New_Challenger][1] = 0;
    g_state.E_Number[g_state.New_Challenger][2] = 0;
    g_state.E_Number[g_state.New_Challenger][3] = 0;

    if (g_state.Continue_Coin[g_state.New_Challenger] == 0) {
        grade_check_work_1st_init(g_state.New_Challenger, 0);
    }

    g_state.Select_Timer = 0x30;
    g_state.Unit_Of_Timer = UNIT_OF_TIMER_MAX;
}

/** @brief Break-in type 04 — full interrupt with A2 effect, sound off, and load request. */
static void Break_Into_04(s16 /* unused */) {
    g_state.Break_Into = 1;
    g_state.entry_phase[1] += 1;
    g_state.entry_phase[2] = 0;
    g_state.entry_timer = 150;
    g_state.E_Number[g_state.New_Challenger][0] = ENTRY_PL_INIT;
    g_state.E_Number[g_state.New_Challenger][1] = 0;
    g_state.E_Number[g_state.New_Challenger][2] = 0;
    g_state.E_Number[g_state.New_Challenger][3] = 0;
    effect_A2_init(0);
    sound_all_off();
    Sound_SE(0xB6);
    Request_LDREQ_Break();
}

/** @brief Break-in type 05 — mid-fight interrupt; handle conclusion flag and score stock. */
static void Break_Into_05(s16 PL_id) {
    g_state.Break_Into = 1;
    g_state.Stop_Combo = 1;
    g_state.entry_phase[1] += 1;
    g_state.entry_phase[2] = 0;
    g_state.E_Number[g_state.New_Challenger][0] = ENTRY_PL_INIT;
    g_state.E_Number[g_state.New_Challenger][1] = 0;
    g_state.E_Number[g_state.New_Challenger][2] = 0;
    g_state.E_Number[g_state.New_Challenger][3] = 0;

    if ((g_state.Play_Type == 0) && (g_state.Conclusion_Flag != 0) &&
        (g_state.plw[g_state.Champion].wu.pl_operator == 0)) {
        g_state.entry_timer = 1;

        if (g_state.LOSER != g_state.New_Challenger) {
            g_state.entry_phase[3] = 0xFF;
        } else {
            g_state.entry_phase[3] = 0;
        }
    } else {
        g_state.entry_timer = 150;

        if (g_state.Conclusion_Flag == 0) {
            g_state.Score[g_state.Champion][0] = g_state.Stage_Stock_Score[g_state.Champion];
        }

        effect_A2_init(0);
        sound_all_off();
        Sound_SE(0xB6);
        Request_LDREQ_Break();
    }

    g_state.Stop_Update_Score = 1;
    cpExitTask(TASK_PAUSE);
}

/** @brief Break-in type 07 — flag player and trigger screen switch when both flagged. */
static void Break_Into_07(s16 PL_id) {
    g_state.E_Number[g_state.New_Challenger][0] = ENTRY_PL_INIT;
    g_state.E_Number[g_state.New_Challenger][1] = 0;
    g_state.E_Number[g_state.New_Challenger][2] = 0;
    g_state.E_Number[g_state.New_Challenger][3] = 0;
    g_state.E_07_Flag[PL_id] = 1;

    if (g_state.E_07_Flag[0] != 0 && g_state.E_07_Flag[1] != 0) {
        return;
    }

    g_state.entry_phase[1] += 1;
    g_state.entry_phase[2] = 0;
    g_state.Break_Into = 1;
}

/** @brief Break-in type 08 — flag player, trigger switch with timer based on continue count. */
static void Break_Into_08(s16 PL_id) {
    g_state.E_Number[g_state.New_Challenger][0] = ENTRY_PL_INIT;
    g_state.E_Number[g_state.New_Challenger][1] = 0;
    g_state.E_Number[g_state.New_Challenger][2] = 0;
    g_state.E_Number[g_state.New_Challenger][3] = 0;
    g_state.E_07_Flag[PL_id] = 1;

    if (g_state.E_07_Flag[0] != 0 && g_state.E_07_Flag[1] != 0) {
        return;
    }

    g_state.Break_Into = 1;
    g_state.entry_phase[1] += 1;
    g_state.entry_phase[2] = 0;

    if (g_state.Continue_Count[PL_id ^ 1] >= 0) {
        g_state.entry_timer = 60;
        return;
    }

    g_state.entry_timer = 10;
}

/** @brief Break-in type 09 — flag player, screen switch, and set champion. */
static void Break_Into_09(s16 PL_id) {
    g_state.E_Number[g_state.New_Challenger][0] = ENTRY_PL_INIT;
    g_state.E_Number[g_state.New_Challenger][1] = 0;
    g_state.E_Number[g_state.New_Challenger][2] = 0;
    g_state.E_Number[g_state.New_Challenger][3] = 0;
    g_state.E_07_Flag[PL_id] = 1;

    if (g_state.E_07_Flag[0] != 0 && g_state.E_07_Flag[1] != 0) {
        return;
    }

    g_state.Break_Into = 1;
    g_state.entry_phase[1] += 1;
    g_state.entry_phase[2] = 0;
    g_state.Champion = g_state.New_Challenger;
}

/** @brief Break-in type 10 — flag player, screen switch, and set champion (final stage). */
static void Break_Into_10(s16 PL_id) {
    g_state.E_Number[g_state.New_Challenger][0] = ENTRY_PL_INIT;
    g_state.E_Number[g_state.New_Challenger][1] = 0;
    g_state.E_Number[g_state.New_Challenger][2] = 0;
    g_state.E_Number[g_state.New_Challenger][3] = 0;
    g_state.E_07_Flag[PL_id] = 1;

    if (g_state.E_07_Flag[0] != 0 && g_state.E_07_Flag[1] != 0) {
        return;
    }

    g_state.Break_Into = 1;
    g_state.entry_phase[1] += 1;
    g_state.entry_phase[2] = 0;
    g_state.Champion = g_state.New_Challenger;
}

/** @brief Increment the player's continue-coin counter (clamped at 99). */
static void Continue_Score_Sub(s16 PL_id) {
    if ((g_state.E_Number[PL_id][0] == ENTRY_PL_CREDIT) || (g_state.E_Number[PL_id][0] == ENTRY_PL_LOSER)) {
        g_state.Continue_Coin[PL_id] += 1;

        if (g_state.Continue_Coin[PL_id] >= 99) {
            g_state.Continue_Coin[PL_id] = 99;
        }
    }
}

/** @brief Undo stage-level stats from the player's totals after a mid-round break-in. */
void Correct_BI_Data() {
    g_state.Super_Arts_Finish[g_state.Player_id] -= g_state.Stage_SA_Finish[g_state.Player_id];
    g_state.Lost_Round[g_state.Player_id] -= g_state.Stage_Lost_Round[g_state.Player_id];
    g_state.Perfect_Finish[g_state.Player_id] -= g_state.Stage_Perfect_Finish[g_state.Player_id];
    g_state.Cheap_Finish[g_state.Player_id] -= g_state.Stage_Cheap_Finish[g_state.Player_id];
}
