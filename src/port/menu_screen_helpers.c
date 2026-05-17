/**
 * @file menu_screen_helpers.c
 * @brief Shared helper functions for the MenuScreen registry.
 *
 * Replaces duplicated per-screen boilerplate for fade transitions, cursor
 * handling, enter/exit sequences, and hard-reset paths.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §4.3).
 *
 * These are the building blocks that migrated screen callbacks call instead
 * of directly using FadeOut/FadeIn/FadeInit, MC_Move_Sub/Check_Menu_Lever,
 * Exit_Sub, Menu_in_Sub, and Back_to_Mode_Select.
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/engine/state_user.h"  /* g_state.Menu_Cursor_Y, g_state.Menu_Cursor_Move, etc. */
#include "sf33rd/Source/Game/io/rumble.h"          /* pulpul_stop()  */
#include "sf33rd/Source/Game/menu/menu.h"          /* Menu_Common_Init(), Check_Menu_Lever() */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* MC_Move_Sub(), Exit_Sub(), Back_to_Mode_Select() */
#include "sf33rd/Source/Game/ui/hud_subroutines.h" /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                               /* struct _TASK */

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_FadeOut — simplified FadeOut wrapper.
 *
 *  Calls FadeOut(1, speed, 8). Returns true when the fade is complete.
 *  Legacy pattern: `if (FadeOut(1, 0x19, 8) != 0) { ... }`
 * ═══════════════════════════════════════════════════════════════════════════ */

bool MenuScreen_FadeOut(struct _TASK* task_ptr, int speed) {
    (void)task_ptr;
    return FadeOut(1, (u8)speed, 8) != 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_FadeIn — simplified FadeIn wrapper.
 *
 *  Calls FadeIn(1, speed, 8). Returns true when the fade is complete.
 *  Legacy pattern: `if (FadeIn(1, 0x19, 8) != 0) { ... }`
 * ═══════════════════════════════════════════════════════════════════════════ */

bool MenuScreen_FadeIn(struct _TASK* task_ptr, int speed) {
    (void)task_ptr;
    return FadeIn(1, (u8)speed, 8) != 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_WaitTimer — wait for the init timer to expire.
 *
 *  Wraps the `Menu_Sub_case1(task_ptr)` pattern used in every screen's
 *  case 1 (the WAIT phase). Returns true when the timer has expired and
 *  the screen can proceed to FADE_IN.
 *
 *  Note: The dispatcher in menu_screen_registry.c calls Menu_Sub_case1()
 *  directly during MENU_PHASE_WAIT, so this helper exists for cases where
 *  a screen needs to call it from its own code (e.g., a custom wait
 *  phase that checks additional conditions).
 * ═══════════════════════════════════════════════════════════════════════════ */

bool MenuScreen_WaitTimer(struct _TASK* task_ptr) {
    return Menu_Sub_case1(task_ptr) != 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_HandleCursor — unified vertical cursor movement.
 *
 *  Replaces the per-screen `MC_Move_Sub(Check_Menu_Lever(0, 0), ...)` call.
 *  Chains Check_Menu_Lever(0, 0) → MC_Move_Sub(sw, 0, cursor_max, skip_item).
 *
 *  Also reads P2: if P1 returned no input, calls the same for P2 so that
 *  either player can navigate (legacy pattern from Mode_Select, Option_Select,
 *  etc.).
 *
 *  @param cursor_max  Maximum cursor index (0-based: 6 for 7 items).
 *  @param skip_item   Item index to skip when g_state.Connect_Status == 0,
 *                     e.g. 1 to skip VS when no P2. Pass 0xFF to disable.
 *  @return            g_state.IO_Result value (SWK_UP, SWK_DOWN, button bits, or 0).
 * ═══════════════════════════════════════════════════════════════════════════ */

u16 MenuScreen_HandleCursor(int cursor_max, int skip_item) {
    u16 result;

    /* P1 input */
    result = MC_Move_Sub(Check_Menu_Lever(0, 0), 0, (s16)cursor_max, (s16)skip_item);

    /* P2 input — only if P1 had no input (legacy two-player menu pattern) */
    if (result == 0) {
        result = MC_Move_Sub(Check_Menu_Lever(1, 0), 0, (s16)cursor_max, (s16)skip_item);
    }

    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_HandleCursorLR — left/right value toggle on current item.
 *
 *  Wraps the Check_Menu_Lever(0, 0) call for reads that only care about
 *  SWK_LEFT / SWK_RIGHT. Returns the raw switch value so the caller can
 *  dispatch L/R handling. Does NOT move the cursor — call HandleCursor
 *  first for up/down, then HandleCursorLR for left/right.
 *
 *  @return The debounced switch value from Check_Menu_Lever (both players).
 * ═══════════════════════════════════════════════════════════════════════════ */

u16 MenuScreen_HandleCursorLR(void) {
    u16 sw;

    sw = Check_Menu_Lever(0, 0);
    if (sw == 0) {
        sw = Check_Menu_Lever(1, 0);
    }

    return sw;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_HandleGridCursor — 2D grid cursor for Memory Card / Replay.
 *
 *  Wraps Dir_Move_Sub2(Check_Menu_Lever(0, 0)) for screens that navigate
 *  a file grid. Updates g_state.Menu_Cursor_X[] and g_state.Menu_Cursor_Y[].
 *
 *  @param max_x  Maximum X position (columns - 1).
 *  @param max_y  Maximum Y position (rows - 1).
 *  @return       g_state.IO_Result value.
 *
 *  Note: The legacy Dir_Move_Sub2 uses the global g_state.Menu_Max for wrapping.
 *  The caller must set g_state.Menu_Max before calling this helper if the
 *  screen uses the global-based wrapping (as Memory Card and Replay do).
 * ═══════════════════════════════════════════════════════════════════════════ */

u16 MenuScreen_HandleGridCursor(int max_x, int max_y) {
    u16 sw;
    u16 result;

    (void)max_x;
    (void)max_y;

    sw = Check_Menu_Lever(0, 0);
    result = Dir_Move_Sub2(sw);

    /* P2 fallback */
    if (result == 0) {
        sw = Check_Menu_Lever(1, 0);
        result = Dir_Move_Sub2(sw);
    }

    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_EnterSub — standard screen entry sequence.
 *
 *  Replaces the Menu_in_Sub() pattern (§A.5 of migration plan).
 *  Sets up fade, timer, common init, cursor restore, and effect slots.
 *
 *  @param task_ptr    The menu task pointer.
 *  @param hdr         MenuHeader enum value for the header bar effect.
 *  @param slot        g_state.Order[] slot for the header bar.
 * ═══════════════════════════════════════════════════════════════════════════ */

void MenuScreen_EnterSub(struct _TASK* task_ptr, MenuHeader hdr, u8 slot) {
    (void)hdr; /* Will be used when effect_57_init is wired up per-screen */

    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] += 1;
    task_ptr->timer = 5;
    Menu_Common_Init();

    /* Restore cursor from saved position (level 1 — same as Menu_in_Sub) */
    g_state.Menu_Cursor_Y[0] = g_state.Cursor_Y_Pos[0][1];

    /* Effect layer management */
    g_state.Menu_Suicide[0] = 1; /* kill parent effects */
    g_state.Menu_Suicide[1] = 0; /* enable our effects */
    g_state.Order[slot] = 4;
    g_state.Order_Timer[slot] = 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_ExitFade — multi-frame fade-out exit sequence.
 *
 *  Replaces Exit_Sub()'s two-phase free[0] pattern (§A.3 of migration plan).
 *  Call from on_tick every frame. Returns true when the fade-out is
 *  complete — the caller should then call MenuScreen_Goto() or
 *  MenuScreen_Back() to trigger the actual transition.
 *
 *  Saves the current cursor position to g_state.Cursor_Y_Pos[cursor_ix] and
 *  stops vibration (pulpul_stop).
 *
 *  @param task_ptr    The menu task pointer.
 *  @param cursor_ix   Index into g_state.Cursor_Y_Pos to save cursor position.
 *  @return            true when fade-out is complete.
 * ═══════════════════════════════════════════════════════════════════════════ */

bool MenuScreen_ExitFade(struct _TASK* task_ptr, s16 cursor_ix) {
    switch (task_ptr->free[0]) {
    case 0:
        task_ptr->free[0] += 1;
        FadeInit();
        /* fallthrough — same as Exit_Sub */

    case 1:
        if (FadeOut(1, 0x19, 8) != 0) {
            task_ptr->free[0] = 0;
            g_state.Cursor_Y_Pos[0][cursor_ix] = g_state.Menu_Cursor_Y[0];
            g_state.Cursor_Y_Pos[1][cursor_ix] = g_state.Menu_Cursor_Y[1];
            pulpul_stop();
            return true;
        }
        return false;

    default:
        return false;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_ExitToParent — standard exit-to-parent sequence.
 *
 *  Wraps the Exit_Sub() pattern (§A.3) using the MenuScreen registry
 *  instead of r_no[1] assignment. Initiates a fade-out and, when complete,
 *  navigates to the current screen's parent.
 *
 *  Call from on_tick every frame once exit is decided.
 *
 *  @param task_ptr    The menu task pointer.
 * ═══════════════════════════════════════════════════════════════════════════ */

void MenuScreen_ExitToParent(struct _TASK* task_ptr) {
    /* Use the ExitFade helper with cursor index 0 (default save slot).
     * Individual screens can override the cursor_ix if needed. */
    if (MenuScreen_ExitFade(task_ptr, 0)) {
        MenuScreen_Back();
    }
}
