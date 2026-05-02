/**
 * @file ms_char_change_ig.c
 * @brief Migrated Character Change In-Game screen — Task 20.
 *
 * Thin wrapper around the legacy Character_Change() function from menu.c.
 * This is the in-game (VS mode) character change screen (r_no[0]=1, r_no[1]=3)
 * that transitions back to the character select screen.
 *
 * The legacy function has 3 phases:
 *   case 0: timer=0xA, g_state.Game_pause=0x81
 *   case 1: timer countdown, Check_LDREQ_Break, Switch_Screen_Init
 *   case 2: Switch_Screen, then cpExitTask(TASK_MENU)
 *
 * Character_Change is shared between Training (index 6) and In_Game (index 3).
 * The training version is already wrapped in ms_training.c (MENU_SCREEN_CHAR_CHANGE_TR).
 * This wrapper is for the In_Game dispatch path only.
 *
 * Exit detection: Character_Change exits by cpExitTask(TASK_MENU) which kills
 * the menu task entirely. It can also change r_no[0] via Check_Pad_in_Pause.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 5b).
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/menu/menu_internal.h" /* Character_Change, IN_GAME_JMP_COUNT */
#include "structs.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — sets up r_no for legacy Character_Change dispatch
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_enter for Character Change In-Game — initializes r_no state. */
static void char_change_ig_enter(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 3;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    /* timer=0 so dispatcher's WAIT phase completes immediately */
    task_ptr->timer = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — delegates to legacy Character_Change() body
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_tick for Character Change In-Game — delegates to legacy function. */
static void char_change_ig_tick(struct _TASK* task_ptr) {
    Character_Change(task_ptr);

    /* Detect exit: Character_Change exits by cpExitTask(TASK_MENU) which
     * kills the menu task entirely (r_no[0] will no longer be 1).
     * Check_Pad_in_Pause can set r_no[1]=4 (Pad_Come_Out).
     * When r_no changes, hand off to legacy dispatch. */
    if (task_ptr->r_no[1] != 3 || task_ptr->r_no[0] != 1) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_exit for Character Change In-Game — no special cleanup needed. */
static void char_change_ig_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_CHAR_CHANGE_IG]
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_char_change_ig_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_char_change_ig_reg_ptr)(void) = ms_char_change_ig_register;
static void ms_char_change_ig_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_char_change_ig_register(void) {
#else
void ms_char_change_ig_register(void) {
#endif
    g_screens[MENU_SCREEN_CHAR_CHANGE_IG] = (MenuScreen) {
        .name = "char_change_ig",
        .id = MENU_SCREEN_CHAR_CHANGE_IG,
        .parent = MENU_SCREEN_PAUSE_MENU,
        .on_enter = char_change_ig_enter,
        .on_tick = char_change_ig_tick,
        .on_exit = char_change_ig_exit,
        .cursor_max = 1, /* minimum valid value (not a real cursor menu — game state transition) */
        .cancel_item = 0,
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_MODE_MENU,
        .effect_slot = 0,
    };
}
