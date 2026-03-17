/**
 * @file ms_exit_confirm.c
 * @brief Migrated Exit Confirm (toSelectGame) screen — Task 10.
 *
 * Replaces the toSelectGame() function in menu.c with MenuScreen registry
 * callbacks.  This is the "Return / Exit" confirm dialog that has two
 * outcomes:
 *   1. Return to Mode Select (SWK_EAST / cancel)
 *   2. Exit to desktop      (SWK_SOUTH / confirm)
 *
 * Legacy location: menu.c lines 1974–2079 (AT_Jmp_Tbl index 8).
 * The screen uses free[0] internally to distinguish between the two paths:
 *   free[0] == 0  →  return to Mode Select
 *   free[0] == 1  →  exit to desktop
 *
 * The screen has ~10 internal phases in legacy code (r_no[2] values:
 * 0, 1, 2, 3, 8, 9, 10, default).  We decompose into an internal
 * enum for clarity.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 2).
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/effect/eff45.h"       /* effect_45_init, Message_Data */
#include "sf33rd/Source/Game/effect/eff66.h"       /* effect_66_init */
#include "sf33rd/Source/Game/engine/workuser.h"    /* plsw_00, plsw_01, Forbid_Reset, Menu_Suicide, Order/Timer */
#include "sf33rd/Source/Game/menu/menu.h"          /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* Menu_in_Sub, Menu_Sub_case1 */
#include "sf33rd/Source/Game/system/sys_sub.h"     /* Setup_BG */
#include "sf33rd/Source/Game/sound/sound3rd.h"     /* SE_selected, Exit_sound_system, sound_all_off */
#include "sf33rd/Source/Game/ui/sc_sub.h"          /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                               /* struct _TASK */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_exit_confirm.h"   /* rmlui_exit_confirm_show/hide */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_screen_exit_confirm */

/* Platform */
#include "port/sdl/app/sdl_app.h"                    /* SDLApp_Exit */
#include "port/sdl/input/controller_image_overlay.h" /* ControllerImageOverlay_Init/Shutdown */

/* Pad definitions */
#include "sf33rd/AcrSDK/common/pad.h" /* SWK_SOUTH, SWK_EAST */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal phase state machine
 *
 *  Legacy toSelectGame uses r_no[2] with values 0/1/2/3/8/9/10/default.
 *  We use an explicit enum for readability.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum ExitConfirmPhase {
    EC_PHASE_INIT,       /* case 0: setup effects/BG/overlay/RmlUi */
    EC_PHASE_INIT_WAIT,  /* set timer then fallthrough */
    EC_PHASE_WAIT,       /* case 1: Menu_Sub_case1 timer + message */
    EC_PHASE_FADE_IN,    /* case 2: FadeIn + draw buttons */
    EC_PHASE_ACTIVE,     /* case 3: input handling */
    EC_PHASE_FADE_OUT,   /* case 8: FadeOut after decision */
    EC_PHASE_RETURN,     /* case 9: return to Mode Select */
    EC_PHASE_EXIT_SOUND, /* case 10: Exit_sound_system */
    EC_PHASE_EXIT_APP,   /* default: SDLApp_Exit */
} ExitConfirmPhase;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 * ═══════════════════════════════════════════════════════════════════════════ */

static ExitConfirmPhase s_phase;
static bool s_exit_to_desktop; /* free[0] equivalent: true = exit app */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Forward declarations for legacy functions called from menu_draw.c
 * ═══════════════════════════════════════════════════════════════════════════ */

extern void imgSelectGameButton(void);

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from toSelectGame case 0
 *
 *  Sets up Forbid_Reset, controller overlay, BG, effect_66 (select game
 *  buttons), Order/Timer, and RmlUi exit confirm document.
 *
 *  NOTE: We do NOT use the dispatcher's automatic ENTER→WAIT→FADE_IN
 *  pipeline for this screen, because toSelectGame has its OWN custom
 *  phase machine (cases 0→1→2→3→8→9→10→default).  Instead, we drive
 *  the entire lifecycle from on_tick using the internal s_phase enum.
 *  The dispatcher just calls on_enter once, then on_tick every frame.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void exit_confirm_enter(struct _TASK* task_ptr) {
    s_phase = EC_PHASE_INIT;
    s_exit_to_desktop = false;

    /* ── Replicate toSelectGame case 0 ── */
    Forbid_Reset = 1;

    /* Menu_in_Sub: FadeOut, advance r_no[2], timer=5, Menu_Common_Init,
     * restore cursor, kill parent items, activate sub items */
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 1;  /* so Menu_Sub_case1 works in wait phase */
    task_ptr->timer = 0;    /* bypass dispatcher wait */
    Menu_Common_Init();
    Menu_Cursor_Y[0] = Cursor_Y_Pos[0][1];
    Menu_Suicide[0] = 1;
    Menu_Suicide[1] = 0;
    Order[0x64] = 4;
    Order_Timer[0x64] = 1;

    ControllerImageOverlay_Init();
    Setup_BG(1, 0x200, 0);

    /* CPS3 sprite-based select game buttons (when not using RmlUi) */
    if (!use_rmlui || !rmlui_screen_exit_confirm)
        effect_66_init(0x8A, 8, 1, 0, -1, -1, -0x7FF2);

    Order[0x8A] = 3;
    Order_Timer[0x8A] = 1;

    if (use_rmlui && rmlui_screen_exit_confirm)
        rmlui_exit_confirm_show();

    /* Advance internal phase to INIT_WAIT — next on_tick will process it */
    s_phase = EC_PHASE_INIT_WAIT;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — drives the internal phase state machine
 *
 *  This screen doesn't use the dispatcher's standard WAIT/FADE_IN phases
 *  because toSelectGame has unique timer (0x10), unique input (raw pad
 *  reads instead of MC_Move_Sub), and a branching exit path (return vs
 *  exit-to-desktop).
 * ═══════════════════════════════════════════════════════════════════════════ */

static void exit_confirm_tick(struct _TASK* task_ptr) {
    u16 sw;

    switch (s_phase) {

    /* ── WAIT: timer countdown + message setup ── */
    case EC_PHASE_INIT_WAIT:
        task_ptr->timer = 0x10;
        s_phase = EC_PHASE_WAIT;
        // fallthrough

    case EC_PHASE_WAIT:
        if (Menu_Sub_case1(task_ptr) != 0) {
            if (!use_rmlui || !rmlui_screen_exit_confirm) {
                Message_Data->kind_req = 5;
                Message_Data->request = 0;
                Message_Data->order = 1;
                Message_Data->timer = 2;
                Message_Data->pos_x = 0;
                Message_Data->pos_y = 0xA0;
                Message_Data->pos_z = 0x18;
                effect_45_init(0, 0, 2);
            }
            s_phase = EC_PHASE_FADE_IN;
        }
        break;

    /* ── FADE_IN: fade in + draw buttons ── */
    case EC_PHASE_FADE_IN:
        if (FadeIn(1, 0x19, 8) != 0) {
            s_phase = EC_PHASE_ACTIVE;
        }
        if (!use_rmlui || !rmlui_screen_exit_confirm)
            imgSelectGameButton();
        break;

    /* ── ACTIVE: input handling ── */
    case EC_PHASE_ACTIVE:
        if (!use_rmlui || !rmlui_screen_exit_confirm)
            imgSelectGameButton();

        /* Read edge-triggered input from both players */
        sw = (~plsw_01[0] & plsw_00[0]) | (~plsw_01[1] & plsw_00[1]);
        sw &= (SWK_SOUTH | SWK_EAST);

        if (sw != 0) {
            /* Guard: if BOTH buttons pressed simultaneously, ignore */
            if (sw != (SWK_SOUTH | SWK_EAST)) {
                if (sw & SWK_SOUTH) {
                    s_exit_to_desktop = true; /* exit to desktop */
                }
                /* SWK_EAST alone: s_exit_to_desktop stays false = return */

                SE_selected();
                FadeInit();
                s_phase = EC_PHASE_FADE_OUT;
            }
        }
        break;

    /* ── FADE_OUT: fade out after decision ── */
    case EC_PHASE_FADE_OUT:
        if (!use_rmlui || !rmlui_screen_exit_confirm)
            imgSelectGameButton();

        if (FadeOut(1, 0x19, 8) != 0) {
            if (s_exit_to_desktop) {
                s_phase = EC_PHASE_EXIT_SOUND;
                sound_all_off();
            } else {
                s_phase = EC_PHASE_RETURN;
            }
        }
        break;

    /* ── RETURN: go back to Mode Select ── */
    case EC_PHASE_RETURN:
        ControllerImageOverlay_Shutdown();
        Menu_Suicide[0] = 0;
        Menu_Suicide[1] = 1;
        task_ptr->r_no[1] = 1; /* Mode_Select */
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        task_ptr->free[0] = 0;
        if (use_rmlui && rmlui_screen_exit_confirm)
            rmlui_exit_confirm_hide();
        FadeOut(1, 0xFF, 8);
        Forbid_Reset = 0;
        /* Exit to legacy so Mode_Select (or migrated Mode_Select) picks up */
        MenuScreen_ExitToLegacy(task_ptr);
        break;

    /* ── EXIT_SOUND: shut down sound system ── */
    case EC_PHASE_EXIT_SOUND:
        Exit_sound_system();
        s_phase = EC_PHASE_EXIT_APP;
        break;

    /* ── EXIT_APP: terminate application ── */
    case EC_PHASE_EXIT_APP:
        SDLApp_Exit();
        break;

    default:
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void exit_confirm_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_phase = EC_PHASE_INIT;
    s_exit_to_desktop = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void exit_confirm_rmlui_show(void) {
    if (use_rmlui && rmlui_screen_exit_confirm)
        rmlui_exit_confirm_show();
}

static void exit_confirm_rmlui_hide(void) {
    if (use_rmlui && rmlui_screen_exit_confirm)
        rmlui_exit_confirm_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_EXIT_CONFIRM]
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_exit_confirm_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_exit_confirm_reg_ptr)(void) = ms_exit_confirm_register;
static void ms_exit_confirm_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_exit_confirm_register(void) {
#else
void ms_exit_confirm_register(void) {
#endif
    g_screens[MENU_SCREEN_EXIT_CONFIRM] = (MenuScreen) {
        .name = "exit_confirm",
        .id = MENU_SCREEN_EXIT_CONFIRM,
        .parent = MENU_SCREEN_MODE_SELECT,
        .on_enter = exit_confirm_enter,
        .on_tick = exit_confirm_tick,
        .on_exit = exit_confirm_exit,
        .cursor_max = 1,  /* two choices: Return (0) / Exit (1) */
        .cancel_item = 0, /* Return choice */
        .rmlui_show = exit_confirm_rmlui_show,
        .rmlui_hide = exit_confirm_rmlui_hide,
        .header_type = MENU_HEADER_MODE_MENU, /* no dedicated header */
        .effect_slot = 0x8A,
    };
}
