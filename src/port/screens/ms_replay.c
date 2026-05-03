/**
 * @file ms_replay.c
 * @brief Migrated Load Replay screen — Task 9.
 *
 * Replaces the Load_Replay() function in menu.c (AT_Jmp_Tbl index 6).
 * This screen loads and plays back a saved replay file.
 *
 * Legacy location: menu.c lines 2644–2701.
 *
 * The screen has 5 internal phases (mapped from legacy r_no[2]):
 *   0: Init — Menu_in_Sub, setup BG, replay header, flash.
 *   1: Wait — Menu_Sub_case1, then open RmlUi replay picker.
 *   2: FadeIn — standard fade, then setup cursor.
 *   3: Replay picker poll — pick/load/cancel.
 *   4: Load_Replay_Sub — multi-phase game transition.
 *
 * The dispatcher drives ENTER→WAIT→FADE_IN→ACTIVE as usual, but the
 * screen's on_tick manages additional internal phases beyond the standard
 * ACTIVE phase for the replay picker poll loop and game transition.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 2).
 */

#include "port/menu_screen.h"
#include "game_state.h"
#include <stdio.h>

#include "sf33rd/Source/Game/effect/eff45.h"                /* Message_Data */
#include "sf33rd/Source/Game/menu/menu_network_constants.h" /* NET_BG_MODE_BLUE, EFF_Z_BLUE_BG */
#include "sf33rd/Source/Game/engine/state_user.h" /* g_state.Menu_Cursor_X, g_state.Decide_ID, Interface_Type, etc. */
#include "sf33rd/Source/Game/menu/menu.h" /* Menu_Common_Init, Setup_Replay_Sub, Setup_Final_Cursor_Pos, Load_Replay_MC_Sub */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* Menu_Sub_case1, Exit_Sub */
#include "sf33rd/Source/Game/system/sys_sub.h"     /* Setup_BG, Clear_Flash_Sub, Clear_Flash_Init */
#include "sf33rd/Source/Game/ui/sc_sub.h"          /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                               /* struct _TASK */

/* Native save */
#include "port/save/native_save.h" /* NativeSave_LoadReplay */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_menu_replay */
#include "port/sdl/rmlui/rmlui_replay_picker.h"  /* rmlui_replay_picker_* */
#include "port/sdl/rmlui/rmlui_wrapper.h"        /* rmlui_wrapper_hide_all_game_documents */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state — tracks the sub-phase within the screen's lifecycle.
 *
 *  The standard dispatcher phases (ENTER→WAIT→FADE_IN→ACTIVE) only handle
 *  the first three stages. Once the screen becomes ACTIVE, on_tick manages
 *  the remaining replay picker → game load stages internally.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    REPLAY_PHASE_PICKER_OPEN, /* first tick: open the replay picker */
    REPLAY_PHASE_FADE_IN,     /* fade in, setup cursor */
    REPLAY_PHASE_PICKER_POLL, /* poll replay picker result */
    REPLAY_PHASE_GAME_LOAD,   /* Load_Replay_Sub multi-phase game transition */
} ReplayInternalPhase;

static ReplayInternalPhase s_phase = REPLAY_PHASE_PICKER_OPEN;

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from Load_Replay case 0
 *
 *  Replicates Menu_in_Sub pattern + screen-specific BG/replay/flash setup.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void load_replay_enter(struct _TASK* task_ptr) {
    s_phase = REPLAY_PHASE_PICKER_OPEN;
    printf("[ms_replay] >>> load_replay_enter called (use_rmlui=%d, rmlui_menu_replay=%d)\n",
           use_rmlui,
           rmlui_menu_replay);
    fflush(stdout);

    if (rmlui_menu_replay) {
        /* ── RmlUI path: destroy old effects and rebuild blue BG ──
         * Same pattern as Network_Lobby case 11: effect_work_init() kills all
         * existing effects (Mode Select's menu items, cursor, header), then
         * we create a fresh blue BG on 0x4E. */
        g_state.Menu_Cursor_X[1] = g_state.Menu_Cursor_X[0];

        effect_work_init();
        Menu_Common_Init();

        Message_Data->kind_req = NET_BG_MODE_BLUE;
        g_state.Order[0x4E] = 5;
        g_state.Order_Timer[0x4E] = 1;
        g_state.Order_Dir[0x4E] = 1;
        effect_57_init(0x4E, MENU_HEADER_MODE_MENU, 0, EFF_Z_BLUE_BG, 0);

        task_ptr->timer = 0;

        rmlui_wrapper_hide_all_game_documents();
    } else {
        /* ── Legacy native path: full Menu_in_Sub + CPS3 effects ── */
        g_state.Menu_Cursor_X[1] = g_state.Menu_Cursor_X[0];
        Clear_Flash_Sub();

        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] = 1;
        task_ptr->timer = 5;
        Menu_Common_Init();

        g_state.Menu_Cursor_Y[0] = g_state.Cursor_Y_Pos[0][1];
        g_state.Menu_Suicide[0] = 1;
        g_state.Menu_Suicide[1] = 0;
        g_state.Order[0x64] = 4;
        g_state.Order_Timer[0x64] = 1;

        g_state.Menu_Cursor_X[0] = 0;
        Setup_BG(1, 0x200, 0);
        Setup_Replay_Sub(0x6E, MENU_HEADER_REPLAY, 1);
        Clear_Flash_Init(4);
        Message_Data->kind_req = 5;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — drives the internal state machine
 *
 *  The dispatcher handles WAIT and FADE_IN automatically via its phase
 *  state machine (calling Menu_Sub_case1 and FadeIn). However, Load_Replay
 *  has custom behavior:
 *    - After WAIT completes: open the RmlUi replay picker (case 1).
 *    - After FADE_IN: set g_state.Menu_Cursor_X (case 2).
 *    - During ACTIVE: poll the picker (case 3) then load game (case 4).
 *
 *  We handle the WAIT→picker transition in the first tick of ACTIVE phase.
 *  The dispatcher's WAIT phase calls Menu_Sub_case1 and opens picker
 *  automatically — actually no, Menu_Sub_case1 in the dispatcher just
 *  advances the phase. We need to open the picker ourselves.
 *
 *  Strategy: when on_tick is first called (phase just became ACTIVE),
 *  we open the picker and manage our own internal fade-in and poll loop.
 *  This means the standard FADE_IN phase has already completed by the
 *  time we get here... but we need to open the picker BEFORE fade-in,
 *  and do a custom cursor setup AFTER fade-in.
 *
 *  REVISED strategy: intercept the dispatcher's WAIT completion by
 *  opening the picker as part of the first on_tick call, then manage
 *  our own internal fade-in and picker poll.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void load_replay_tick(struct _TASK* task_ptr) {
    g_state.Menu_Cursor_X[1] = g_state.Menu_Cursor_X[0];
    Clear_Flash_Sub();

    switch (s_phase) {
    case REPLAY_PHASE_PICKER_OPEN:
        /* Open the replay picker now that the standard WAIT+FADE_IN are done.
         * In legacy, the picker opens in case 1 (after Menu_Sub_case1).
         * The dispatcher already handled the wait + fade-in, so we open the
         * picker now and set up the cursor. */
        rmlui_replay_picker_open(0);
        task_ptr->free[3] = 0;
        g_state.Menu_Cursor_X[0] = Setup_Final_Cursor_Pos(0, 8);
        s_phase = REPLAY_PHASE_PICKER_POLL;
        break;

    case REPLAY_PHASE_FADE_IN:
        /* Not used — the dispatcher handled fade-in automatically */
        s_phase = REPLAY_PHASE_PICKER_POLL;
        break;

    case REPLAY_PHASE_PICKER_POLL: {
        /* Poll the RmlUi replay picker */
        int pick_result = rmlui_replay_picker_poll();
        if (pick_result == 0) {
            /* User picked a replay — try to load it */
            const char* filename = rmlui_replay_picker_get_filename();
            if (NativeSave_LoadReplay(filename) == 0) {
                /* Load successful — set up for game transition */
                g_state.Decide_ID = 0;
                if (Interface_Type[0] == 0) {
                    g_state.Decide_ID = 1;
                }
                task_ptr->r_no[2] = 4; /* for Load_Replay_Sub compatibility */
                task_ptr->r_no[3] = 0;
                s_phase = REPLAY_PHASE_GAME_LOAD;
            } else {
                /* Load failed — cancel back to Mode_Select */
                g_state.IO_Result = 0x200;
                Load_Replay_MC_Sub(task_ptr, 0);
                /* Load_Replay_MC_Sub sets r_no to return to Mode_Select */
                MenuScreen_ExitToLegacy(task_ptr);
            }
        } else if (pick_result == -1) {
            /* User cancelled the picker — go back to Mode_Select */
            g_state.IO_Result = 0x200;
            Load_Replay_MC_Sub(task_ptr, 0);
            /* Load_Replay_MC_Sub sets r_no to return to Mode_Select */
            MenuScreen_ExitToLegacy(task_ptr);
        }
        break;
    }

    case REPLAY_PHASE_GAME_LOAD:
        /* Multi-phase game transition — Load_Replay_Sub manages its own
         * r_no[3] state machine internally.  When it reaches the final
         * phase, it exits to game via cpExitTask(TASK_MENU). */
        Load_Replay_Sub(task_ptr);
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void load_replay_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_phase = REPLAY_PHASE_PICKER_OPEN;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks (nullable in MenuScreen struct)
 *
 *  Load_Replay uses `rmlui_menu_replay` toggle but the RmlUi picker is
 *  always opened (ImGui path removed). The rmlui_show/hide callbacks
 *  handle the picker overlay if needed.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void load_replay_rmlui_show(void) {
    /* The replay picker is opened explicitly in on_tick (PICKER_OPEN phase)
     * after the wait/fade-in is complete, so no show action needed here. */
}

static void load_replay_rmlui_hide(void) {
    /* The replay picker auto-hides on completion/cancel via rmlui_replay_picker_poll.
     * Call hide explicitly for safety in case we exit early. */
    rmlui_replay_picker_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_LOAD_REPLAY]
 *
 *  Uses GCC/MSVC constructor attribute to register at startup.
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
/* MSVC: use CRT initializer section */
#pragma section(".CRT$XCU", read)
static void ms_load_replay_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_load_replay_reg_ptr)(void) = ms_load_replay_register;
static void ms_load_replay_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_load_replay_register(void) {
#else
/* Fallback: must be called manually from init code */
void ms_load_replay_register(void) {
#endif
    g_screens[MENU_SCREEN_LOAD_REPLAY] = (MenuScreen) {
        .name = "load_replay",
        .id = MENU_SCREEN_LOAD_REPLAY,
        .parent = MENU_SCREEN_MODE_SELECT,
        .on_enter = load_replay_enter,
        .on_tick = load_replay_tick,
        .on_exit = load_replay_exit,
        .cursor_max = 1,   /* file list — managed by RmlUi picker, min 1 for validation */
        .cancel_item = -1, /* cancel handled by picker UI, not cursor position */
        .rmlui_show = load_replay_rmlui_show,
        .rmlui_hide = load_replay_rmlui_hide,
        .header_type = MENU_HEADER_REPLAY,
        .effect_slot = 0x6E,
    };
}
