/**
 * @file ms_button_config_ig.c
 * @brief Migrated Button Config In-Game screen — Task 20.
 *
 * Thin wrapper around the legacy Button_Config_in_Game() function from
 * menu_input.c.  This is the in-game (pause) button remapping screen
 * (r_no[0]=1, r_no[1]=2) with 11 items (6 buttons × 1 player + defaults + exit).
 *
 * The legacy function manages its own r_no[2] phases (init, active) and uses
 * Check_Pad_in_Pause to detect controller disconnect.  Exit is done via
 * Return_Pause_Sub which sets r_no[1]=1 (back to Menu_Select pause menu).
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 5b).
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/menu/menu_internal.h" /* Button_Config_in_Game, IN_GAME_JMP_COUNT */
#include "structs.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — sets up r_no for legacy Button_Config_in_Game dispatch
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_enter for Button Config In-Game — initializes r_no state. */
static void button_config_ig_enter(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 2;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    /* timer=0 so dispatcher's WAIT phase completes immediately */
    task_ptr->timer = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — delegates to legacy Button_Config_in_Game() body
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_tick for Button Config In-Game — delegates to legacy function. */
static void button_config_ig_tick(struct _TASK* task_ptr) {
    Button_Config_in_Game(task_ptr);

    /* Detect exit: Return_Pause_Sub sets r_no[1]=1 (back to Menu_Select).
     * Check_Pad_in_Pause sets r_no[1]=4 (Pad_Come_Out) on disconnect.
     * When r_no changes, hand off to legacy dispatch. */
    if (task_ptr->r_no[1] != 2 || task_ptr->r_no[0] != 1) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_exit for Button Config In-Game — no special cleanup needed. */
static void button_config_ig_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_BUTTON_CONFIG_IG]
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_button_config_ig_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_button_config_ig_reg_ptr)(void) = ms_button_config_ig_register;
static void ms_button_config_ig_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_button_config_ig_register(void) {
#else
void ms_button_config_ig_register(void) {
#endif
    g_screens[MENU_SCREEN_BUTTON_CONFIG_IG] = (MenuScreen) {
        .name = "button_config_ig",
        .id = MENU_SCREEN_BUTTON_CONFIG_IG,
        .parent = MENU_SCREEN_PAUSE_MENU,
        .on_enter = button_config_ig_enter,
        .on_tick = button_config_ig_tick,
        .on_exit = button_config_ig_exit,
        .cursor_max = 10,  /* 11 items (0–10): 6 buttons + defaults + exit */
        .cancel_item = 10, /* Exit item */
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_BUTTON_CONFIG,
        .effect_slot = 0,
    };
}
