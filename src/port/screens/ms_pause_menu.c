/**
 * @file ms_pause_menu.c
 * @brief Migrated g_state.Pause Menu (Menu_Select) screen — Task 20.
 *
 * Thin wrapper around the legacy Menu_Select() function from menu_input.c.
 * Menu_Select is the in-game pause menu (r_no[0]=1, r_no[1]=1) with items:
 *   - Return to Game
 *   - Exit / Character Change / Replay End (depending on g_state.Mode_Type)
 *   - Button Config
 *
 * The legacy function manages its own r_no[2] phases (init, menu, input,
 * yes/no confirm) and uses Check_Pad_in_Pause to detect controller disconnect.
 * Exit paths set r_no[1] to other In_Game indices (2=Button_Config, 3=Char_Change,
 * 4=Pad_Come_Out) or change r_no[0] (e.g. r_no[0]=0xC for replay end).
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 5b).
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/menu/menu_internal.h" /* Menu_Select, IN_GAME_JMP_COUNT */
#include "structs.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — sets up r_no for legacy Menu_Select dispatch
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_enter for g_state.Pause Menu — initializes r_no state. */
static void pause_menu_enter(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 1;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    /* timer=0 so dispatcher's WAIT phase completes immediately */
    task_ptr->timer = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — delegates to legacy Menu_Select() body
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_tick for g_state.Pause Menu — delegates to legacy function. */
static void pause_menu_tick(struct _TASK* task_ptr) {
    Menu_Select(task_ptr);

    /* Detect exit: the legacy code changes r_no[1] when navigating to:
     *   - Button Config in-game (r_no[1]=2)
     *   - Character Change (r_no[1]=3, in VS mode)
     *   - Pad_Come_Out (r_no[1]=4, controller disconnect)
     * Or changes r_no[0] (e.g. 0x0C for replay end).
     * When r_no changes, hand off to legacy dispatch. */
    if (task_ptr->r_no[1] != 1 || task_ptr->r_no[0] != 1) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_exit for g_state.Pause Menu — no special cleanup needed. */
static void pause_menu_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_PAUSE_MENU]
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_pause_menu_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_pause_menu_reg_ptr)(void) = ms_pause_menu_register;
static void ms_pause_menu_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_pause_menu_register(void) {
#else
void ms_pause_menu_register(void) {
#endif
    g_screens[MENU_SCREEN_PAUSE_MENU] = (MenuScreen) {
        .name = "pause_menu",
        .id = MENU_SCREEN_PAUSE_MENU,
        .parent = MENU_SCREEN_MODE_SELECT,
        .on_enter = pause_menu_enter,
        .on_tick = pause_menu_tick,
        .on_exit = pause_menu_exit,
        .cursor_max = 2,  /* 3 items (0–2): Return, Exit/Change, Button Config */
        .cancel_item = 0, /* Return to game */
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_MODE_MENU,
        .effect_slot = 0,
    };
}
