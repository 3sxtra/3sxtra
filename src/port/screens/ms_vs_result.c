/**
 * @file ms_vs_result.c
 * @brief Migrated VS Result screen — Task 16.
 *
 * Replaces the VS_Result() function in menu.c with MenuScreen registry
 * callbacks.  This screen displays the match outcome (win/loss tally,
 * percentages), offers Continue/Save Replay/Exit choices, and handles
 * netplay FT rotation wait-poll.
 *
 * Legacy location: menu.c lines 3521–3692 (AT_Jmp_Tbl index 16).
 * The screen uses r_no[2] internally with values 0–7 for sub-phases:
 *   case 0: System_all_clear_Level_B + Menu_Init bootstrap
 *   case 1: FadeOut + timer + win/loss effects setup
 *   case 2: FadeOut + timer → FadeInit (+ netplay report branch)
 *   case 3: FadeIn completion
 *   case 4: VS_Result_Select_Sub input handling
 *   case 5: Timer countdown → Exit_Sub to Save_Replay
 *   case 6: Netplay FT session wait-poll
 *   case 7: Netplay exit + System_all_clear_Level_B + BGM restart
 *
 * We decompose these into a VsResultPhase enum for clarity.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 4).
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/effect/effect_66_quake_half_object_flash.h" /* effect_66_init */
#include "sf33rd/Source/Game/effect/effect_91_position_data.h"           /* effect_91_init */
#include "sf33rd/Source/Game/effect/effect_a0_position_data.h"           /* effect_A0_init */
#include "sf33rd/Source/Game/engine/state_user.h" /* plsw, Menu_Cursor_*, g_state.Order/Timer, g_state.VS_Win_Record, g_state.Sel_PL_Complete, g_state.Sel_Arts_Complete, g_state.Suicide, g_state.Cursor_Y_Pos, g_state.Mode_Type etc. */
#include "sf33rd/Source/Game/menu/menu.h"         /* Menu_Common_Init, Menu_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* Menu_Sub_case1, Exit_Sub, VS_Result_Select_Sub, Setup_VS_Mode, Setup_Win_Lose_OBJ */
#include "sf33rd/Source/Game/sound/sound_effects.h"       /* BGM_Request_Code_Check, BGM_Stop */
#include "sf33rd/Source/Game/system/system_subroutines.h" /* System_all_clear_Level_B, Clear_Flash_Init, Clear_Flash_Sub */
#include "sf33rd/Source/Game/ui/hud_subroutines.h"        /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                                      /* struct _TASK */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_vs_result.h"      /* rmlui_vs_result_show/hide */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_screen_vs_result */

/* Netplay */
#include "netplay/netplay.h" /* Netplay_GetSessionState, Netplay_HandleMenuExit, NETPLAY_SESSION_RUNNING */
#include "port/sdl/netplay/sdl_netplay_ui.h" /* SDLNetplayUI_ReportNaturalMatchEnd, SDLNetplayUI_ConsumeSessionComplete */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal phase state machine
 *
 *  Legacy VS_Result uses r_no[2] with values 0–7.  We use an explicit
 *  enum for readability.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum VsResultPhase {
    VR_PHASE_BOOTSTRAP,    /* case 0: System_all_clear + Menu_Init */
    VR_PHASE_SETUP,        /* case 1: FadeOut + timer + effects */
    VR_PHASE_WAIT_TIMER,   /* case 2: FadeOut + timer → FadeInit (+ netplay branch) */
    VR_PHASE_FADE_IN,      /* case 3: FadeIn */
    VR_PHASE_ACTIVE,       /* case 4: VS_Result_Select_Sub input */
    VR_PHASE_SAVE_DELAY,   /* case 5: timer countdown → Exit_Sub to Save_Replay */
    VR_PHASE_NETPLAY_POLL, /* case 6: wait-poll for FT session complete */
    VR_PHASE_NETPLAY_EXIT, /* case 7: Netplay_HandleMenuExit + Exit_Sub */
} VsResultPhase;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 * ═══════════════════════════════════════════════════════════════════════════ */

static VsResultPhase s_phase;

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from VS_Result case 0
 *
 *  This screen drives its own phase machine (like Exit Confirm).
 *  The dispatcher's automatic WAIT/FADE_IN are bypassed by driving
 *  everything from on_tick via s_phase.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void vs_result_enter(struct _TASK* task_ptr) {
    s_phase = VR_PHASE_BOOTSTRAP;

    /* ── Replicate VS_Result case 0 ── */
    System_all_clear_Level_B();
    Menu_Init(task_ptr);
    task_ptr->r_no[1] = 16;
    task_ptr->r_no[2] = 1; /* for Menu_Sub_case1 compat */
    task_ptr->r_no[3] = 0;
    g_state.Sel_PL_Complete[0] = 0;
    g_state.Sel_Arts_Complete[0] = 0;
    g_state.Sel_PL_Complete[1] = 0;
    g_state.Sel_Arts_Complete[1] = 0;
    Clear_Flash_Init(4);

    /* Advance to SETUP phase — next on_tick will process it */
    s_phase = VR_PHASE_SETUP;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — drives the internal phase state machine
 * ═══════════════════════════════════════════════════════════════════════════ */

static void vs_result_tick(struct _TASK* task_ptr) {
    s16 ix;
    s16 char_ix2;
    s16 total_battle;
    u16 ave[2];
    s16 s4;
    s16 s3;

    Clear_Flash_Sub();

    switch (s_phase) {

    /* ── SETUP: FadeOut + timer + win/loss effects (case 1) ── */
    case VR_PHASE_SETUP:
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] = 2; /* for legacy compat (Menu_Sub_case1) */
        task_ptr->timer = 5;
        Menu_Common_Init();
        g_state.Menu_Cursor_Y[0] = g_state.Cursor_Y_Pos[0][0];
        g_state.Menu_Cursor_Y[1] = g_state.Cursor_Y_Pos[1][0];
        g_state.Menu_Suicide[0] = 0;
        g_state.Menu_Suicide[1] = 1;
        g_state.Menu_Cursor_X[0] = 0;
        g_state.Menu_Cursor_X[1] = 0;
        g_state.Order[78] = 2;
        g_state.Order_Dir[78] = 0;
        g_state.Order_Timer[78] = 1;

        /* Compute win percentages */
        total_battle = g_state.VS_Win_Record[0] + g_state.VS_Win_Record[1];
        if (total_battle == 0) {
            total_battle = 1;
        }
        if (g_state.VS_Win_Record[0] >= g_state.VS_Win_Record[1]) {
            ave[1] = (g_state.VS_Win_Record[1] * 100) / total_battle;
            if (ave[1] == 0 && g_state.VS_Win_Record[1] > 0) {
                ave[1] = 1;
            }
            ave[0] = 100 - ave[1];
        } else {
            ave[0] = (g_state.VS_Win_Record[0] * 100) / total_battle;
            if (ave[0] == 0 && g_state.VS_Win_Record[0] > 0) {
                ave[0] = 1;
            }
            ave[1] = 100 - ave[0];
        }

        if (use_rmlui && rmlui_screen_vs_result) {
            rmlui_vs_result_show(g_state.VS_Win_Record[0], g_state.VS_Win_Record[1], ave[0], ave[1]);
        } else {
            effect_66_init(91, 12, 0, 0, 71, 9, 0);
            g_state.Order[91] = 3;
            g_state.Order_Timer[91] = 1;
            effect_66_init(138, 24, 0, 0, -1, -1, -0x7FF9);
            g_state.Order[138] = 3;
            g_state.Order_Timer[138] = 1;
            effect_66_init(139, 25, 0, 0, -1, -1, -0x7FF9);
            g_state.Order[139] = 3;
            g_state.Order_Timer[139] = 1;
            effect_A0_init(0, g_state.VS_Win_Record[0], 0, 3, 0, 0, 0);
            effect_A0_init(0, g_state.VS_Win_Record[1], 1, 3, 0, 0, 0);
            effect_A0_init(0, ave[0], 2, 3, 0, 0, 0);
            effect_A0_init(0, ave[1], 3, 3, 0, 0, 0);

            for (ix = 0, s4 = char_ix2 = 22; ix < 3; ix++, s3 = char_ix2++) {
                effect_91_init(0, ix, 0, 71, char_ix2, 0);
                effect_91_init(1, ix, 0, 71, char_ix2, 0);
            }

            Setup_Win_Lose_OBJ();
        }
        g_state.Menu_Cursor_Move = 0;
        s_phase = VR_PHASE_WAIT_TIMER;
        break;

    /* ── WAIT_TIMER: FadeOut + timer countdown (case 2) ── */
    case VR_PHASE_WAIT_TIMER:
        FadeOut(1, 0xFF, 8);

        if (--task_ptr->timer == 0) {
            if (Netplay_GetSessionState() == NETPLAY_SESSION_RUNNING) {
                /* Report match result while game state is intact */
                SDLNetplayUI_ReportNaturalMatchEnd();
                s_phase = VR_PHASE_NETPLAY_POLL;
                task_ptr->r_no[3] = 0;
                task_ptr->timer = 300; /* 5 seconds at 60fps */
            } else {
                s_phase = VR_PHASE_FADE_IN;
            }
            FadeInit();
        }
        break;

    /* ── FADE_IN (case 3) ── */
    case VR_PHASE_FADE_IN:
        if (FadeIn(1, 25, 8)) {
            s_phase = VR_PHASE_ACTIVE;
            g_state.Suicide[3] = 0;
        }
        break;

    /* ── ACTIVE: input handling (case 4) ── */
    case VR_PHASE_ACTIVE:
        if (VS_Result_Select_Sub(task_ptr, 0) == 0) {
            VS_Result_Select_Sub(task_ptr, 1);
        }

        /* VS_Result_Select_Sub/VS_Result_Move_Sub modify r_no[2] on selection:
         *   cursor 0 (Continue): r_no[2]=6, timer=15 → Continue/rematch
         *   cursor 1 (Save Replay): r_no[2]=5, timer=15 → Save delay
         *   cursor 2 (Exit): r_no[2]=7, timer=15 → Exit
         *   cancel on cursor 2: r_no[2]=99 → treated as exit
         * We detect these transitions and map them to our phase enum. */
        switch (task_ptr->r_no[2]) {
        case 5:
            s_phase = VR_PHASE_SAVE_DELAY;
            break;
        case 6:
            s_phase = VR_PHASE_NETPLAY_POLL;
            break;
        case 7:
            s_phase = VR_PHASE_NETPLAY_EXIT;
            break;
        case 99:
            /* Cancel on "Exit" item → treat as exit */
            s_phase = VR_PHASE_NETPLAY_EXIT;
            task_ptr->r_no[2] = 7;
            break;
        }
        break;

    /* ── SAVE_DELAY: timer → Exit_Sub to Save_Replay (case 5) ── */
    case VR_PHASE_SAVE_DELAY:
        if (task_ptr->r_no[3] == 0) {
            if (--task_ptr->timer == 0) {
                task_ptr->r_no[3]++;
            }
            break;
        }

        Exit_Sub(task_ptr, 0, 17);

        /* Exit_Sub sets r_no[1]=17 when complete → hand off to legacy
         * Save_Replay (or migrated Save_Replay) dispatch */
        if (task_ptr->r_no[1] == 17) {
            MenuScreen_ExitToLegacy(task_ptr);
        }
        break;

    /* ── NETPLAY_POLL: wait-poll for FT session (case 6) ── */
    case VR_PHASE_NETPLAY_POLL: {
        char ft_winner[64] = { 0 };
        if (SDLNetplayUI_ConsumeSessionComplete(ft_winner, sizeof(ft_winner))) {
            /* FT set is done — disconnect P2P and return to room lobby */
            s_phase = VR_PHASE_NETPLAY_EXIT;
            task_ptr->r_no[2] = 7;
            task_ptr->r_no[3] = 0;
            break;
        }

        if (--task_ptr->timer <= 0) {
            /* Timeout — server didn't respond. Continue playing VS. */
            Setup_VS_Mode(task_ptr);
            g_state.fsm[1] = 12;
            g_state.fsm[2] = 1;
            g_state.Mode_Type = MODE_VERSUS;
            MenuScreen_ExitToLegacy(task_ptr);
            break;
        }

        /* Still waiting — do nothing this frame */
        break;
    }

    /* ── NETPLAY_EXIT: handle netplay exit (case 7/default) ── */
    case VR_PHASE_NETPLAY_EXIT:
        Netplay_HandleMenuExit();

        if (Exit_Sub(task_ptr, 0, 0)) {
            System_all_clear_Level_B();
            BGM_Request_Code_Check(65);
        }

        /* Exit_Sub drives r_no[1] to 0 on completion → exit to legacy
         * which will route back to Mode_Select via Menu_Init */
        if (task_ptr->r_no[1] == 0 && task_ptr->r_no[0] == 0) {
            MenuScreen_ExitToLegacy(task_ptr);
        }
        break;

    default:
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void vs_result_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_phase = VR_PHASE_BOOTSTRAP;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void vs_result_rmlui_show(void) {
    /* RmlUi VS result is shown explicitly in on_tick (SETUP phase)
     * with the win record data — so this is intentionally a no-op.
     * The dispatcher calls rmlui_show on ENTER, but we need the data
     * computed in SETUP phase first. */
}

static void vs_result_rmlui_hide(void) {
    if (use_rmlui && rmlui_screen_vs_result)
        rmlui_vs_result_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_VS_RESULT]
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_vs_result_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_vs_result_reg_ptr)(void) = ms_vs_result_register;
static void ms_vs_result_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_vs_result_register(void) {
#else
void ms_vs_result_register(void) {
#endif
    g_screens[MENU_SCREEN_VS_RESULT] = (MenuScreen) {
        .name = "vs_result",
        .id = MENU_SCREEN_VS_RESULT,
        .parent = MENU_SCREEN_MODE_SELECT,
        .on_enter = vs_result_enter,
        .on_tick = vs_result_tick,
        .on_exit = vs_result_exit,
        .cursor_max = 2,  /* 3 choices: Continue (0) / Save Replay (1) / Exit (2) */
        .cancel_item = 2, /* Exit choice */
        .rmlui_show = vs_result_rmlui_show,
        .rmlui_hide = vs_result_rmlui_hide,
        .header_type = MENU_HEADER_MODE_MENU, /* no dedicated header */
        .effect_slot = 78,                    /* BG g_state.Order slot used during setup */
    };
}
