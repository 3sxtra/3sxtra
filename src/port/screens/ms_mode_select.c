/**
 * @file ms_mode_select.c
 * @brief Migrated Mode Select screen — first screen migration (Task 6).
 *
 * Replaces the Mode_Select() function in menu.c with MenuScreen registry
 * callbacks.  This is the top-level menu: Arcade / VS / Training / Network /
 * Options / Exit.
 *
 * Legacy location: menu.c lines 289–444 (AT_Jmp_Tbl index 1).
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 2).
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/animation/appear.h"   /* Clear_Personal_Data */
#include "sf33rd/Source/Game/effect/eff04.h"       /* effect_04_init */
#include "sf33rd/Source/Game/effect/eff45.h"       /* Message_Data */
#include "sf33rd/Source/Game/effect/eff57.h"       /* effect_57_init, MenuHeader */
#include "sf33rd/Source/Game/effect/eff61.h"       /* effect_61_init */
#include "sf33rd/Source/Game/engine/grade.h"       /* grade_check_work_1st_init */
#include "sf33rd/Source/Game/engine/workuser.h"    /* Menu_Cursor_Y, Mode_Type, etc. */
#include "sf33rd/Source/Game/io/pulpul.h"          /* pulpul_stop */
#include "sf33rd/Source/Game/menu/menu.h"          /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* MC_Move_Sub, Check_Menu_Lever, Decide_PL, Exit_Sub */
#include "sf33rd/Source/Game/rendering/texcash.h"  /* checkAdxFileLoaded, checkSelObjFileLoaded */
#include "sf33rd/Source/Game/rendering/texgroup.h" /* load_any_texture_patnum */
#include "sf33rd/Source/Game/screen/entry.h"       /* Entry_Task, TASK_ENTRY */
#include "sf33rd/Source/Game/sound/se.h"           /* SE_selected */
#include "sf33rd/Source/Game/sound/sound3rd.h"     /* SE_selected, etc. */
#include "sf33rd/Source/Game/system/reset.h"       /* Suicide */
#include "sf33rd/Source/Game/system/saver.h"       /* Saver_Task, TASK_SAVER */
#include "sf33rd/Source/Game/system/sysdir.h"      /* Setup_Training_Difficulty */
#include "sf33rd/Source/Game/system/work_sys.h"    /* cpExitTask */
#include "sf33rd/Source/Game/ui/sc_sub.h"          /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                               /* struct _TASK */
#include "main.h"                                  /* TASK_MENU, TASK_ENTRY, task[] */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_mode_menu.h"      /* rmlui_mode_menu_show/hide */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_menu_mode */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 *
 *  The legacy Mode_Select uses free[0]/free[1] for the exit-fade sub-state.
 *  We preserve that pattern so Exit_Sub still works during the transition
 *  to un-migrated screens (e.g. Network_Lobby sets free[1]=21).
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Track whether we are in the "exiting" sub-state (free[0]/free[1] fade) */
static bool s_exiting = false;
static s16 s_exit_target = 0; /* AT_Jmp_Tbl index to transition to */

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from Mode_Select case 0 (~55 lines of init)
 *
 *  Sets up Mode_Type, Entry task, cursor, effects, RmlUi.
 *  The dispatcher handles FadeOut/timer/wait/FadeIn automatically via
 *  MENU_PHASE_ENTER → WAIT → FADE_IN, but Mode_Select has custom
 *  logic in case 1 (Order, checkAdx, checkSelObj) that we handle
 *  by setting them up here and relying on the WAIT phase timer.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void mode_select_enter(struct _TASK* task_ptr) {
    s16 ix;
    s16 loop_counter = 7;

    s_exiting = false;
    s_exit_target = 0;

    /* ── Replicate Mode_Select case 0 ── */
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 1; /* advance so Menu_Sub_case1 works in WAIT phase */
    task_ptr->timer = 5;
    Mode_Type = MODE_ARCADE;
    Present_Mode = 1;

    if (task[TASK_ENTRY].condition != 1) {
        E_No[0] = 1;
        E_No[1] = 2;
        E_No[2] = 2;
        E_No[3] = 0;
        cpReadyTask(TASK_ENTRY, Entry_Task);
    }

    Menu_Common_Init();

    for (ix = 0; ix < 4; ix++) {
        Menu_Suicide[ix] = 0;
    }

    Clear_Personal_Data(0);
    Clear_Personal_Data(1);
    Menu_Cursor_Y[0] = Cursor_Y_Pos[0][0];
    Cursor_Y_Pos[0][1] = 0;
    Cursor_Y_Pos[0][2] = 0;
    Cursor_Y_Pos[0][3] = 0;

    for (ix = 0; ix < 4; ix++) {
        Vital_Handicap[ix][0] = 7;
        Vital_Handicap[ix][1] = 7;
    }

    VS_Stage = 0x14;
    Order[0x8A] = 4;
    Order_Timer[0x8A] = 1;

    for (ix = 0; ix < 4; ix++) {
        Message_Data[ix].order = 3;
    }

    if (!use_rmlui || !rmlui_menu_mode) {
        effect_57_init(0x64, MENU_HEADER_MODE_MENU, 0, 0x3F, 2);
        Order[0x64] = 1;
        Order_Dir[0x64] = 8;
        Order_Timer[0x64] = 1;
    }
    Menu_Suicide[0] = 0;

    if (use_rmlui && rmlui_menu_mode) {
        rmlui_mode_menu_show();
    } else {
        effect_04_init(0, 0, 0, 0x48);

        for (ix = 0; ix < loop_counter; ix++) {
            effect_61_init(0, ix + 0x50, 0, 0, (u32)ix, ix, 0x7047);
            Order[ix + 0x50] = 1;
            Order_Dir[ix + 0x50] = 4;
            Order_Timer[ix + 0x50] = ix + 0x14;
        }
        Menu_Cursor_Move = loop_counter;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — extracted from Mode_Select case 3 (~60 lines of input)
 *
 *  The dispatcher guarantees on_tick is only called during MENU_PHASE_ACTIVE.
 *  The WAIT→FADE_IN transition is handled by the dispatcher.
 *
 *  However, Mode_Select case 1 has custom code (Order update, asset checks)
 *  that runs during the wait phase.  We handle that in a custom wait-phase
 *  hook by checking whether the dispatcher's Menu_Sub_case1 just completed
 *  (we detect this via r_no[2] still being 1 → need to do the case 1 work).
 *  Actually, the dispatcher calls Menu_Sub_case1 in MENU_PHASE_WAIT and
 *  handles the transition.  But the custom code in Mode_Select case 1
 *  (Order[0x4E], checkAdx/SelObj) only runs ONCE when the timer expires.
 *  Since on_tick is not called during WAIT, we put this code in on_enter's
 *  tail... no — the timer hasn't expired yet at on_enter time.
 *
 *  SOLUTION: We replicate the case 1 check at the top of on_tick.  The first
 *  time on_tick is called (phase just became ACTIVE → fade-in already done),
 *  we do the asset checks.  This is functionally equivalent because the
 *  Order[0x4E] and asset-check calls just need to happen after the wait timer
 *  expires and before or during fade-in.  We call them once via a flag.
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool s_wait_done = false;

static void mode_select_tick(struct _TASK* task_ptr) {
    s16 PL_id;
    s16 loop_counter = 7;

    /* ── Handle the "exiting" sub-state ── */
    if (s_exiting) {
        /* We need to fade out and transition to the target AT index.
         * Use Exit_Sub which handles the multi-frame fade-out pattern. */
        if (Exit_Sub(task_ptr, 0, s_exit_target) != 0) {
            /* Exit_Sub sets r_no[1] = s_exit_target, r_no[2]=0, r_no[3]=0.
             * Now exit the registry so legacy dispatch picks it up. */
            MenuScreen_ExitToLegacy(task_ptr);
        }
        return;
    }

    /* ── One-time post-wait-phase setup (replaces Mode_Select case 1) ── */
    if (!s_wait_done) {
        s_wait_done = true;
        Order[0x4E] = 2;
        Order_Dir[0x4E] = 0;
        Order_Timer[0x4E] = 1;
        checkAdxFileLoaded();
        checkSelObjFileLoaded();
        Suicide[3] = 0;
    }

    /* ── VS skip guard: if no P2, skip item 1 (VS) ── */
    if (Connect_Status == 0 && Menu_Cursor_Y[0] == 1) {
        Menu_Cursor_Y[0] = 2;
    } else {
        PL_id = 0;

        if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, loop_counter - 1, 1) == 0) {
            PL_id = 1;
            MC_Move_Sub(Check_Menu_Lever(1, 0), 0, loop_counter - 1, 1);
        }
    }

    /* ── Input dispatch ── */
    switch (IO_Result) {
    case 0x100: /* Confirm button */
        switch (Menu_Cursor_Y[0]) {
        case 0: /* Arcade */
            G_No[2] += 1;
            Mode_Type = MODE_ARCADE;
            if (use_rmlui && rmlui_menu_mode)
                rmlui_mode_menu_hide();
            task_ptr->r_no[0] = 5;
            cpExitTask(TASK_SAVER);
            Decide_PL(PL_id);
            /* Exit to legacy — game takes over */
            MenuScreen_ExitToLegacy(task_ptr);
            break;

        case 1: /* VS Mode */
            Setup_VS_Mode(task_ptr);
            G_No[1] = 12;
            G_No[2] = 1;
            Mode_Type = MODE_VERSUS;
            if (use_rmlui && rmlui_menu_mode)
                rmlui_mode_menu_hide();
            cpExitTask(TASK_MENU);
            /* Exit to legacy — game takes over */
            MenuScreen_ExitToLegacy(task_ptr);
            break;

        case 3: /* Network */
            s_exiting = true;
            s_exit_target = 21; /* AT index for Network_Lobby */
            task_ptr->free[0] = 0;
            break;

        case 2: /* Training (cursor 2 → AT index 4) */
        case 4: /* System Direction (cursor 4 → AT index 6) */
        case 5: /* Options (cursor 5 → AT index 7) */
        case 6: /* Exit (cursor 6 → AT index 8) */
            s_exiting = true;
            s_exit_target = Menu_Cursor_Y[0] + 2;
            task_ptr->free[0] = 0;
            break;

        default:
            break;
        }

        SE_selected();
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup / hide RmlUi
 * ═══════════════════════════════════════════════════════════════════════════ */

static void mode_select_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    /* RmlUi hide is handled by the rmlui_hide callback in the registration,
     * but also called explicitly in some transition paths above.
     * Calling it again is safe (idempotent). */
    s_exiting = false;
    s_wait_done = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks (nullable in MenuScreen struct)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void mode_select_rmlui_show(void) {
    if (use_rmlui && rmlui_menu_mode)
        rmlui_mode_menu_show();
}

static void mode_select_rmlui_hide(void) {
    if (use_rmlui && rmlui_menu_mode)
        rmlui_mode_menu_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_MODE_SELECT]
 *
 *  This uses a GCC/MSVC constructor attribute to register at startup.
 *  Alternative: explicit init function called from main().
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
/* MSVC: use CRT initializer section */
#pragma section(".CRT$XCU", read)
static void ms_mode_select_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_mode_select_reg_ptr)(void) = ms_mode_select_register;
static void ms_mode_select_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_mode_select_register(void) {
#else
/* Fallback: must be called manually from init code */
void ms_mode_select_register(void) {
#endif
    g_screens[MENU_SCREEN_MODE_SELECT] = (MenuScreen) {
        .name = "mode_select",
        .id = MENU_SCREEN_MODE_SELECT,
        .parent = MENU_SCREEN_NONE,
        .on_enter = mode_select_enter,
        .on_tick = mode_select_tick,
        .on_exit = mode_select_exit,
        .cursor_max = 6,   /* 7 items: 0–6 */
        .cancel_item = -1, /* no cancel (top-level menu) */
        .rmlui_show = mode_select_rmlui_show,
        .rmlui_hide = mode_select_rmlui_hide,
        .header_type = MENU_HEADER_MODE_MENU,
        .effect_slot = 0x64,
    };
}
