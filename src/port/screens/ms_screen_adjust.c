/**
 * @file ms_screen_adjust.c
 * @brief Migrated Screen Adjust screen — Task 13.
 *
 * Replaces the Screen_Adjust_Sub() / Screen_Exit_Check() functions
 * in menu_input.c with MenuScreen registry callbacks.
 *
 * Screen Adjust is a sub-screen within the Option_Select tree.
 * Unlike most screens, it does NOT have a dedicated AT_Jmp_Tbl index —
 * it was historically embedded within Game_Option or driven by
 * the Option_Select dispatch through menu_input.c helper functions.
 *
 * The screen has 7 items (cursor_max=6):
 *   Item 0: X Adjust (L/R)
 *   Item 1: Y Adjust (L/R)
 *   Item 2: H Size (L/R)
 *   Item 3: V Size (L/R)
 *   Item 4: Screen Mode (L/R toggle)
 *   Item 5: Default
 *   Item 6: Exit
 *
 * L/R value changes are live — they modify X_Adjust_Buff, Y_Adjust_Buff,
 * Disp_Size_H/V, and screen_mode via Screen_Move_Sub_LR().
 * On exit, commits X_Adjust and Y_Adjust from buffers.
 * On "Default", resets all adjust values to zero and sizes to 100.
 *
 * Legacy location: menu_input.c lines 918–973 (Screen_Adjust_Sub / Screen_Exit_Check).
 * Parent: MENU_SCREEN_OPTION_SELECT
 *
 * NOTE: Screen_Adjust_Sub and Screen_Exit_Check are NOT called from
 * any AT_Jmp_Tbl entry. They are legacy helper functions that were
 * intended to be called from the menu dispatch but are currently
 * unreferenced in the codebase. This migrated screen provides a
 * proper entry point through the MenuScreen registry for potential
 * future activation.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 3).
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/effect/eff57.h"       /* effect_57_init, MenuHeader */
#include "sf33rd/Source/Game/engine/workuser.h"    /* Menu_Cursor_Y, save_w, etc. */
#include "sf33rd/Source/Game/menu/menu.h"          /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* Screen_Adjust_Sub, Screen_Exit_Check, etc. */
#include "sf33rd/Source/Game/sound/sound3rd.h"     /* SE_selected */
#include "sf33rd/Source/Game/system/reset.h"       /* Suicide */
#include "sf33rd/Source/Game/system/sys_sub.h"     /* Save_Game_Data */
#include "sf33rd/Source/Game/system/work_sys.h"    /* save_w, sys_w */
#include "sf33rd/Source/Game/ui/sc_sub.h"          /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                               /* struct _TASK */

/* RmlUi Phase 3 — Screen Adjust shares the sound menu's RmlUi toggle */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool s_wait_done = false;

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — sets up the Screen Adjust display
 *
 *  Initialize fade, timer, cursor, and header bar for MENU_HEADER_SCREEN_ADJUST.
 *  X/Y Adjust buffers are initialized from the option_select_enter flow
 *  which sets X_Adjust_Buff/Y_Adjust_Buff prior to entering sub-screens.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void screen_adjust_enter(struct _TASK* task_ptr) {
    s_wait_done = false;

    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 1;
    task_ptr->timer = 5;
    Menu_Common_Init();
    Menu_Cursor_Y[0] = 0;
    Menu_Suicide[1] = 1;
    Menu_Suicide[2] = 0;

    /* Kill/setup parent effect slots */
    Order[0x4F] = 4;
    Order_Timer[0x4F] = 1;
    Order[0x4E] = 2;
    Order_Dir[0x4E] = 2;
    Order_Timer[0x4E] = 1;

    /* Header bar — CPS3 effect for Screen Adjust */
    effect_57_init(0x65, MENU_HEADER_SCREEN_ADJUST, 0, 0x3F, 2);
    Order[0x65] = 1;
    Order_Dir[0x65] = 8;
    Order_Timer[0x65] = 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — delegates to Screen_Adjust_Sub + Screen_Exit_Check
 *
 *  Screen_Adjust_Sub handles cursor movement and L/R value changes.
 *  Screen_Exit_Check handles confirm/cancel/default exit paths.
 *  On exit, Return_Option_Mode_Sub sets r_no[1]=7.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void screen_adjust_tick(struct _TASK* task_ptr) {
    if (!s_wait_done) {
        s_wait_done = true;
        Suicide[3] = 0;
    }

    Screen_Adjust_Sub(0);
    Screen_Exit_Check(task_ptr, 0);

    if (IO_Result == 0) {
        Screen_Adjust_Sub(1);
        Screen_Exit_Check(task_ptr, 1);
    }

    Save_Game_Data();

    /* Screen_Exit_Check calls Return_Option_Mode_Sub on exit,
     * which sets r_no[1] to 7 (or 1 for in-game). Detect this. */
    if (task_ptr->r_no[1] != 12 && task_ptr->r_no[1] != 13) {
        /* Exit path was triggered — but Screen_Exit_Check
         * already sets r_no[1] directly, so we hand off to legacy. */
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void screen_adjust_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_wait_done = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks — Screen Adjust has no dedicated RmlUi document
 * ═══════════════════════════════════════════════════════════════════════════ */

static void screen_adjust_rmlui_show(void) {
    /* No dedicated RmlUi overlay for Screen_Adjust */
}

static void screen_adjust_rmlui_hide(void) {
    /* No dedicated RmlUi overlay for Screen_Adjust */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_SCREEN_ADJUST]
 *
 *  Uses GCC/MSVC constructor attribute to register at startup.
 *
 *  NOTE: MENU_USE_NEW_SCREEN_ADJUST is 0 — this screen is registered
 *  but NOT enabled because Screen_Adjust_Sub/Screen_Exit_Check are
 *  currently unreferenced dead code. When a dedicated AT_Jmp_Tbl entry
 *  or Option_Select dispatch path for Screen_Adjust is added, set the
 *  toggle to 1.
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
/* MSVC: use CRT initializer section */
#pragma section(".CRT$XCU", read)
static void ms_screen_adjust_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_screen_adjust_reg_ptr)(void) = ms_screen_adjust_register;
static void ms_screen_adjust_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_screen_adjust_register(void) {
#else
/* Fallback: must be called manually from init code */
void ms_screen_adjust_register(void) {
#endif
    g_screens[MENU_SCREEN_SCREEN_ADJUST] = (MenuScreen) {
        .name = "screen_adjust",
        .id = MENU_SCREEN_SCREEN_ADJUST,
        .parent = MENU_SCREEN_OPTION_SELECT,
        .on_enter = screen_adjust_enter,
        .on_tick = screen_adjust_tick,
        .on_exit = screen_adjust_exit,
        .cursor_max = 6,  /* 7 items (0–6) */
        .cancel_item = 6, /* last item = "Exit" */
        .rmlui_show = screen_adjust_rmlui_show,
        .rmlui_hide = screen_adjust_rmlui_hide,
        .header_type = MENU_HEADER_SCREEN_ADJUST,
        .effect_slot = 0x65,
    };
}
