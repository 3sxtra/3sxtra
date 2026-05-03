/**
 * @file ms_training_mode.c
 * @brief Migrated Training Mode selector screen — Task 8.
 *
 * Replaces the Training_Mode() function in menu.c with MenuScreen registry
 * callbacks.  This is the training sub-menu: Normal Training / Parrying /
 * Trials / Exit.
 *
 * Legacy location: menu.c lines 2084–2190 (AT_Jmp_Tbl index 4).
 *
 * The screen has 4 items:
 *   0 = Normal Training  (MODE_NORMAL_TRAINING, g_state.Present_Mode=4)
 *   1 = Parry Training   (MODE_PARRY_TRAINING, g_state.Present_Mode=5)
 *   2 = Trials           (MODE_TRIALS, g_state.Present_Mode=4)
 *   3 = Exit             (returns to Mode_Select)
 *
 * Selecting items 0–2 sets up mode globals and calls Setup_VS_Mode() to
 * enter character select via r_no[0]=5.  Item 3 and Cancel both return
 * to Mode_Select.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 2).
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/effect/eff04.h"       /* effect_04_init */
#include "sf33rd/Source/Game/effect/eff57.h"       /* effect_57_init, MenuHeader */
#include "sf33rd/Source/Game/effect/eff61.h"       /* effect_61_init */
#include "sf33rd/Source/Game/engine/grade.h"       /* grade_check_work_1st_init */
#include "sf33rd/Source/Game/engine/state_user.h"    /* g_state.Menu_Cursor_Y, g_state.Mode_Type, etc. */
#include "sf33rd/Source/Game/menu/menu.h"          /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* MC_Move_Sub, Check_Menu_Lever, Exit_Sub */
#include "sf33rd/Source/Game/sound/sound3rd.h"     /* SE_selected */
#include "sf33rd/Source/Game/system/reset.h"       /* g_state.Suicide */
#include "sf33rd/Source/Game/system/sysdir.h"      /* Setup_Training_Difficulty */
#include "sf33rd/Source/Game/system/work_sys.h"    /* cpExitTask, system_dir */
#include "sf33rd/Source/Game/ui/sc_sub.h"          /* FadeOut, FadeIn, FadeInit */
#include "main.h"                                  /* mpp_w */
#include "structs.h"                               /* struct _TASK */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_training_menus.h" /* rmlui_training_mode_show/hide */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_menu_training */
#include "port/sdl/rmlui/rmlui_wrapper.h"        /* rmlui_wrapper_hide_all_game_documents */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool s_wait_done = false;

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from Training_Mode case 0
 *
 *  Sets up mpp_w.initTrainingData, header bar, menu items, and RmlUi.
 *  Also copies system_dir[4] = system_dir[5] = system_dir[1] to set up
 *  training difficulty parameters.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void training_mode_enter(struct _TASK* task_ptr) {
    s16 ix;

    s_wait_done = false;

    /* ── Replicate Menu_in_Sub pattern ── */
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 1; /* advance so Menu_Sub_case1 works in WAIT phase */
    task_ptr->timer = 5;
    Menu_Common_Init();

    /* Hide all Phase 3 game documents when entering a new sub-menu. */
    if (use_rmlui)
        rmlui_wrapper_hide_all_game_documents();

    g_state.Menu_Cursor_Y[0] = g_state.Cursor_Y_Pos[0][1];
    g_state.Menu_Suicide[0] = 1;
    g_state.Menu_Suicide[1] = 0;
    g_state.Order[0x64] = 4;
    g_state.Order_Timer[0x64] = 1;

    /* ── Training_Mode case 0 specific setup ── */
    mpp_w.initTrainingData = true;

    if (!use_rmlui || !rmlui_menu_training) {
        effect_57_init(0x6F, MENU_HEADER_TRAINING, 0, 0x3F, 2);
        g_state.Order[0x6F] = 1;
        g_state.Order_Dir[0x6F] = 8;
        g_state.Order_Timer[0x6F] = 1;
    }

    if (use_rmlui && rmlui_menu_training) {
        rmlui_training_mode_show();
    } else {
        effect_04_init(1, 5, 0, 0x48);

        static const s16 menu_strings[] = { 0x35, 0x36, 66, 0x37 };
        for (ix = 0; ix < 4; ix++) {
            effect_61_init(0, ix + 0x50, 0, 1, menu_strings[ix], ix, 0x7047);
            g_state.Order[ix + 0x50] = 1;
            g_state.Order_Dir[ix + 0x50] = 4;
            g_state.Order_Timer[ix + 0x50] = ix + 0x14;
        }
        g_state.Menu_Cursor_Move = 4;
    }

    /* Copy system_dir difficulty parameters for training modes */
    system_dir[4] = system_dir[1];
    system_dir[5] = system_dir[1];
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — extracted from Training_Mode case 3
 *
 *  Input handling:
 *    - P1 and P2 cursor movement with MC_Move_Sub + Check_Menu_Lever.
 *    - Only respond to Confirm (0x100) or Cancel (0x200) — ignore other input.
 *    - Cancel or Exit item (cursor=3): return to Mode_Select.
 *    - Items 0–2: set g_state.Mode_Type/g_state.Present_Mode, call Setup_VS_Mode, then exit.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void training_mode_tick(struct _TASK* task_ptr) {
    s16 PL_id;

    /* ── One-time post-wait-phase setup ── */
    if (!s_wait_done) {
        s_wait_done = true;
        g_state.Suicide[3] = 0;
    }

    /* ── Cursor movement ── */
    PL_id = 0;

    if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, 3, 0xFF) == 0) {
        PL_id = 1;
        MC_Move_Sub(Check_Menu_Lever(1, 0), 0, 3, 0xFF);
    }

    /* ── Only respond to Confirm/Cancel ── */
    switch (g_state.IO_Result) {
    case 0x100:
    case 0x200:
        break;

    default:
        return;
    }

    SE_selected();

    /* ── Exit / Cancel path ── */
    if (g_state.Menu_Cursor_Y[0] == 3 || g_state.IO_Result == 0x200) {
        g_state.Menu_Suicide[0] = 0;
        g_state.Menu_Suicide[1] = 1;
        task_ptr->r_no[1] = 1; /* Mode_Select AT index */
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        task_ptr->free[0] = 0;
        g_state.Order[0x6F] = 4;
        g_state.Order_Timer[0x6F] = 4;
        if (use_rmlui && rmlui_menu_training)
            rmlui_training_mode_hide();
        /* Exit to legacy so the integration hook picks up r_no[1]=1 (Mode_Select) */
        MenuScreen_ExitToLegacy(task_ptr);
        return;
    }

    /* ── Training mode selection (items 0–2) ── */
    g_state.Decide_ID = PL_id;

    /* Hide the training-mode overlay before going to char select */
    if (use_rmlui && rmlui_menu_training)
        rmlui_training_mode_hide();

    if (g_state.Menu_Cursor_Y[0] == 0) {
        g_state.Mode_Type = MODE_NORMAL_TRAINING;
        g_state.Present_Mode = 4;
    } else if (g_state.Menu_Cursor_Y[0] == 1) {
        g_state.Mode_Type = MODE_PARRY_TRAINING;
        g_state.Present_Mode = 5;
    } else {
        g_state.Mode_Type = MODE_TRIALS;
        g_state.Present_Mode = 4; /* Reuse normal training data */
    }

    Setup_VS_Mode(task_ptr);
    g_state.fsm[2] += 1;
    task_ptr->r_no[0] = 5;
    cpExitTask(TASK_SAVER);
    g_state.Champion = PL_id;
    g_state.Pause_ID = PL_id;
    g_state.Training_ID = PL_id;
    g_state.New_Challenger = PL_id ^ 1;
    cpExitTask(TASK_ENTRY);

    /* Exit to legacy — game takes over for char select */
    MenuScreen_ExitToLegacy(task_ptr);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void training_mode_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_wait_done = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks (nullable in MenuScreen struct)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void training_mode_rmlui_show(void) {
    if (use_rmlui && rmlui_menu_training)
        rmlui_training_mode_show();
}

static void training_mode_rmlui_hide(void) {
    if (use_rmlui && rmlui_menu_training)
        rmlui_training_mode_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_TRAINING_MODE]
 *
 *  Uses GCC/MSVC constructor attribute to register at startup.
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
/* MSVC: use CRT initializer section */
#pragma section(".CRT$XCU", read)
static void ms_training_mode_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_training_mode_reg_ptr)(void) = ms_training_mode_register;
static void ms_training_mode_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_training_mode_register(void) {
#else
/* Fallback: must be called manually from init code */
void ms_training_mode_register(void) {
#endif
    g_screens[MENU_SCREEN_TRAINING_MODE] = (MenuScreen) {
        .name = "training_mode",
        .id = MENU_SCREEN_TRAINING_MODE,
        .parent = MENU_SCREEN_MODE_SELECT,
        .on_enter = training_mode_enter,
        .on_tick = training_mode_tick,
        .on_exit = training_mode_exit,
        .cursor_max = 3,  /* 4 items: 0–3 */
        .cancel_item = 3, /* last item is "Exit" */
        .rmlui_show = training_mode_rmlui_show,
        .rmlui_hide = training_mode_rmlui_hide,
        .header_type = MENU_HEADER_TRAINING,
        .effect_slot = 0x6F,
    };
}
