/**
 * @file menu_task_phases.h
 * @brief Named constants for TASK_MENU r_no[] magic numbers.
 *
 * These replace raw integer assignments/comparisons used across the codebase
 * when external systems read or write task[TASK_MENU].r_no[].
 *
 * Internal menu.c usage (via task_ptr->r_no[1] as jump-table indices) is
 * intentionally NOT covered — those values are context-dependent sub-states
 * within each r_no[0] dispatch table.
 */
#ifndef MENU_TASK_PHASES_H
#define MENU_TASK_PHASES_H

/**
 * @brief Top-level phases of the Menu task (task[TASK_MENU].r_no[0]).
 *
 * The menu task uses r_no[0] as the primary state selector.
 * Each value corresponds to a different dispatcher function
 * inside Menu_Task().
 */
typedef enum MenuTaskPhase {
    MTP_AFTER_TITLE = 0,     /**< menu.c: After_Title() — initial post-title dispatcher */
    MTP_IDLE = 1,            /**< pause.c: re-activate menu after pause exit */
    MTP_NETPLAY_IDLE = 5,    /**< netplay.c: park menu in idle routine during matchmaking */
    MTP_IN_GAME = 7,         /**< manage.c: in-game menu (pause UI, training, etc.) */
    MTP_SCREEN_DISPATCH = 8, /**< game.c: post-match/pre-match screen dispatch */
    MTP_GOTO_GAME = 9,       /**< game.c: transition into gameplay */
    MTP_TRAINING = 10,       /**< manage.c/input_converter.c: training mode dispatcher */
    MTP_RESET = 13,          /**< sys_sub.c: soft reset flow */
    MTP_PHASE_MAX_           /**< sentinel — do not use as a value */
} MenuTaskPhase;

/**
 * @brief Cross-slot sub-phase constants for task[TASK_MENU].r_no[1].
 *
 * Only values written or checked by files OTHER than menu.c are named here.
 * Internal menu.c jump-table indices are left as raw values.
 */
typedef enum MenuTaskSubPhase {
    MTSP_INIT = 0,           /**< Initial sub-state after phase change */
    MTSP_MODE_SELECT = 1,    /**< Mode select screen (test_runner, menu.c) */
    MTSP_IN_GAME_ACTIVE = 7, /**< sys_sub.c: in-game menu is fully active */
    MTSP_SA_CUT = 16,        /**< game.c: super-arts select cut-in */
    MTSP_NETWORK_LOBBY = 21, /**< menu.c: network lobby entry (for reference) */
} MenuTaskSubPhase;

#endif /* MENU_TASK_PHASES_H */
