/**
 * @file ms_save_replay.c
 * @brief Migrated Save Replay screen — Task 14.
 *
 * Replaces the Save_Replay() function in menu.c with MenuScreen registry
 * callbacks.  This screen writes a match replay to the save system after
 * a VS match.
 *
 * The screen has 4 internal phases (mapped from legacy r_no[2]):
 *   0: Init — Setup_Save_Replay_1st (FadeOut, timer=5, Menu_Common_Init,
 *             cursor, Setup_BG, Setup_File_Property, Clear_Flash_Init)
 *   1: Wait — Menu_Sub_case1 timer, then open RmlUi replay picker (save mode)
 *   2: FadeIn — Setup_Save_Replay_2nd (FadeIn + cursor position setup)
 *   3: Picker poll — rmlui_replay_picker_poll, NativeSave_SaveReplay on pick,
 *                     Save_Replay_MC_Sub on cancel → exit
 *
 * Exit paths:
 *   - Cancel with Mode_Type==5 → Back_to_Mode_Select
 *   - Cancel otherwise → Exit_Replay_Save → Return_VS_Result_Sub (r_no[1]=16)
 *   Both routes set r_no[1] away from 17, detected by on_tick → ExitToLegacy.
 *
 * Legacy location: menu.c lines 3695–3729 (AT_Jmp_Tbl index 17).
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 3).
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/engine/workuser.h"     /* Menu_Cursor_X, Order, Order_Dir, Order_Timer */
#include "sf33rd/Source/Game/menu/menu.h"           /* Menu_Common_Init, Setup_Save_Replay_2nd */
#include "sf33rd/Source/Game/menu/menu_internal.h"  /* Setup_Save_Replay_1st, Save_Replay_MC_Sub */
#include "sf33rd/Source/Game/system/sys_sub.h"      /* Clear_Flash_Sub, Clear_Flash_Init */
#include "sf33rd/Source/Game/ui/sc_sub.h"           /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                                /* struct _TASK */

/* Native save */
#include "port/save/native_save.h"                  /* NativeSave_SaveReplay */

/* RmlUi replay picker */
#include "port/sdl/rmlui/rmlui_replay_picker.h"    /* rmlui_replay_picker_open/poll/get_slot/hide */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 *
 *  We track the sub-phase with an enum rather than reusing r_no[2] directly.
 *  However, Setup_Save_Replay_2nd reads/writes r_no[2], so we keep r_no[2]
 *  in sync with the legacy values for helper compatibility.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    SR_PHASE_WAIT,          /* case 1: Menu_Sub_case1 timer + open picker */
    SR_PHASE_FADE_IN,       /* case 2: Setup_Save_Replay_2nd (FadeIn) */
    SR_PHASE_PICKER_POLL,   /* case 3: poll replay picker result */
} SaveReplayPhase;

static SaveReplayPhase s_phase = SR_PHASE_WAIT;

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from Save_Replay case 0
 *
 *  Delegates to Setup_Save_Replay_1st which handles all init:
 *  FadeOut, r_no[2]++, timer=5, Menu_Common_Init, Menu_Cursor_X[0]=0,
 *  Menu_Suicide setup, Setup_BG, Setup_File_Property, Clear_Flash_Init.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void save_replay_enter(struct _TASK* task_ptr) {
    s_phase = SR_PHASE_WAIT;

    /* Clear_Flash_Sub + cursor sync — same as legacy Save_Replay prologue */
    Menu_Cursor_X[1] = Menu_Cursor_X[0];
    Clear_Flash_Sub();

    /* Setup_Save_Replay_1st sets r_no[2] to 1 (via r_no[2]++),
     * which is what Menu_Sub_case1 expects in the WAIT phase. */
    Setup_Save_Replay_1st(task_ptr);

    /* Set r_no[1]=17 for Save_Replay_MC_Sub / Exit_Replay_Save compatibility */
    task_ptr->r_no[1] = 17;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — extracted from Save_Replay cases 1–3
 *
 *  Manages the internal phase state machine.  The dispatcher's standard
 *  WAIT/FADE_IN are used for the outer lifecycle, but on_tick handles
 *  the heavy lifting:
 *    SR_PHASE_WAIT: Menu_Sub_case1 timer + open replay picker (save mode)
 *    SR_PHASE_FADE_IN: Setup_Save_Replay_2nd (custom FadeIn + cursor)
 *    SR_PHASE_PICKER_POLL: poll picker, save on pick, cancel → exit
 *
 *  Exit detection: Save_Replay_MC_Sub / Exit_Replay_Save / Back_to_Mode_Select
 *  all modify r_no[1] away from 17.  We detect this and call ExitToLegacy.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void save_replay_tick(struct _TASK* task_ptr) {
    /* Per-frame prologue — same as legacy Save_Replay */
    Menu_Cursor_X[1] = Menu_Cursor_X[0];
    Clear_Flash_Sub();

    switch (s_phase) {

    case SR_PHASE_WAIT:
        /* Case 1: wait for timer, then open picker */
        if (Menu_Sub_case1(task_ptr) != 0) {
            rmlui_replay_picker_open(1);  /* mode 1 = save */
        }
        /* Set BG order — same as legacy case 1 */
        Order[0x4E] = 2;
        Order_Dir[0x4E] = 0;
        Order_Timer[0x4E] = 1;

        /* Advance when r_no[2] reaches 2 (Setup_Save_Replay_2nd expects it) */
        if (task_ptr->r_no[2] >= 2) {
            s_phase = SR_PHASE_FADE_IN;
        }
        break;

    case SR_PHASE_FADE_IN:
        /* Case 2: FadeIn + cursor setup via Setup_Save_Replay_2nd */
        Setup_Save_Replay_2nd(task_ptr, 1);
        if (task_ptr->r_no[2] >= 3) {
            s_phase = SR_PHASE_PICKER_POLL;
        }
        break;

    case SR_PHASE_PICKER_POLL: {
        /* Case 3: poll the replay picker */
        int pick_result = rmlui_replay_picker_poll();
        if (pick_result == 0) {
            /* User picked a slot — save the replay */
            int slot = rmlui_replay_picker_get_slot();
            NativeSave_SaveReplay(slot);
        }
        if (pick_result != 1) {
            /* Done or cancelled — trigger cancel path */
            IO_Result = 0x200;
            Save_Replay_MC_Sub(task_ptr, 0);
        }
        break;
    }
    }

    /* ── Exit detection ── */
    /* Save_Replay_MC_Sub (cancel path) calls either:
     *   - Back_to_Mode_Select (if Mode_Type==5): sets r_no to mode_select
     *   - Exit_Replay_Save: calls Return_VS_Result_Sub → sets r_no[1]=16
     * Either way, r_no[1] is no longer 17.  Hand off to legacy. */
    if (task_ptr->r_no[1] != 17) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void save_replay_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_phase = SR_PHASE_WAIT;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks
 *
 *  The replay picker is opened explicitly during on_tick (SR_PHASE_WAIT).
 *  rmlui_hide calls hide for safety in case of early exit.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void save_replay_rmlui_show(void) {
    /* Picker is opened in on_tick after timer completes — no action here */
}

static void save_replay_rmlui_hide(void) {
    rmlui_replay_picker_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_SAVE_REPLAY]
 *
 *  Uses GCC/MSVC constructor attribute to register at startup.
 *
 *  Parent is MENU_SCREEN_MODE_SELECT because VS_Result (the natural parent)
 *  is not yet migrated (Task 16).  This is safe because the exit path
 *  always uses ExitToLegacy with r_no[1] set by the helpers.
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
/* MSVC: use CRT initializer section */
#pragma section(".CRT$XCU", read)
static void ms_save_replay_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_save_replay_reg_ptr)(void) = ms_save_replay_register;
static void ms_save_replay_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void ms_save_replay_register(void) {
#else
/* Fallback: must be called manually from init code */
void ms_save_replay_register(void) {
#endif
    g_screens[MENU_SCREEN_SAVE_REPLAY] = (MenuScreen){
        .name        = "save_replay",
        .id          = MENU_SCREEN_SAVE_REPLAY,
        .parent      = MENU_SCREEN_MODE_SELECT,
        .on_enter    = save_replay_enter,
        .on_tick     = save_replay_tick,
        .on_exit     = save_replay_exit,
        .cursor_max  = 1,   /* file list — managed by RmlUi picker, min 1 for validation */
        .cancel_item = -1,  /* cancel handled by picker UI */
        .rmlui_show  = save_replay_rmlui_show,
        .rmlui_hide  = save_replay_rmlui_hide,
        .header_type = MENU_HEADER_REPLAY,
        .effect_slot = 0x6E,
    };
}
