/**
 * @file ms_training.c
 * @brief Migrated Training sub-screens — Task 19.
 *
 * Wraps all 7 Training_Jmp_Tbl sub-screens as MenuScreen registry entries
 * using thin delegation wrappers.  Each screen's on_tick calls the legacy
 * function directly.  Exit is detected by monitoring r_no[1] changes.
 *
 * Training sub-screens are unique in the menu system:
 *   - They don't use the standard fade-in/fade-out lifecycle.
 *   - They manage their own r_no[2] phases internally.
 *   - They run inside Training_Menu() which wraps them with Akaobi(),
 *     ToneDown(), and SSPutStr_Bigger() post-dispatch rendering.
 *   - They navigate between each other by setting r_no[1] (which goes
 *     through Training_Jmp_Tbl).
 *
 * The dispatcher's WAIT and FADE_IN phases are bypassed by setting timer=0
 * in on_enter so they complete immediately, getting to ACTIVE within ~2 frames.
 *
 * Migrated screens:
 *   [1] Normal_Training      → MENU_SCREEN_NORMAL_TRAINING
 *   [2] Blocking_Training    → MENU_SCREEN_BLOCKING_TRAINING
 *   [3] Dummy_Setting        → MENU_SCREEN_DUMMY_SETTING
 *   [4] Training_Option      → MENU_SCREEN_TRAINING_OPTION
 *   [5] Button_Config_Tr     → MENU_SCREEN_BUTTON_CONFIG_TR
 *   [6] Character_Change     → MENU_SCREEN_CHAR_CHANGE_TR
 *   [7] Blocking_Tr_Option   → MENU_SCREEN_BLOCKING_TR_OPTION
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 5a).
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/engine/workuser.h"    /* g_state.Menu_Cursor_Y, etc. */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* training functions, TRAINING_JMP_COUNT */
#include "structs.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  Normal Training (Training_Jmp_Tbl index 1)
 *
 *  The most complex training sub-screen: 8-item menu with recording,
 *  playback, settings, and exit-confirm sub-states.  ~130 lines.
 *  Manages 4 r_no[2] phases internally (init, active, yes/no, exit).
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_enter for Normal Training — sets up r_no for legacy dispatch. */
static void normal_training_enter(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 1;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    /* timer=0 so dispatcher's WAIT phase completes immediately */
    task_ptr->timer = 0;
}

/** @brief on_tick for Normal Training — delegates to legacy function. */
static void normal_training_tick(struct _TASK* task_ptr) {
    Normal_Training(task_ptr);

    /* Detect exit: the legacy code changes r_no[1] when navigating to
     * another training sub-screen (e.g. Dummy_Setting=3, Training_Option=4)
     * or exiting to Wait_Pause_in_Tr (r_no[0]=10).
     * When r_no changes, hand off to legacy dispatch. */
    if (task_ptr->r_no[1] != 1 || task_ptr->r_no[0] != 7) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/** @brief on_exit for Normal Training — cleanup. */
static void normal_training_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Blocking Training (Training_Jmp_Tbl index 2)
 *
 *  Parry training sub-menu: 6-item menu with parry recording, playback,
 *  and a yes/no exit confirm.  ~100 lines.
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_enter for Blocking Training. */
static void blocking_training_enter(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 2;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    task_ptr->timer = 0;
}

/** @brief on_tick for Blocking Training — delegates to legacy function. */
static void blocking_training_tick(struct _TASK* task_ptr) {
    Blocking_Training(task_ptr);

    if (task_ptr->r_no[1] != 2 || task_ptr->r_no[0] != 7) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/** @brief on_exit for Blocking Training. */
static void blocking_training_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Dummy Setting (Training_Jmp_Tbl index 3)
 *
 *  Configure training dummy behavior: 7-item menu with L/R value toggles.
 *  ~55 lines.  3 phases (init, active, exit).
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_enter for Dummy Setting. */
static void dummy_setting_enter(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 3;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    task_ptr->timer = 0;
}

/** @brief on_tick for Dummy Setting — delegates to legacy function. */
static void dummy_setting_tick(struct _TASK* task_ptr) {
    Dummy_Setting(task_ptr);

    if (task_ptr->r_no[1] != 3 || task_ptr->r_no[0] != 7) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/** @brief on_exit for Dummy Setting. */
static void dummy_setting_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Training Option (Training_Jmp_Tbl index 4)
 *
 *  Training parameters: 6-item menu with L/R value toggles for
 *  damage level, difficulty, etc.  ~60 lines.
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_enter for Training Option. */
static void training_option_enter(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 4;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    task_ptr->timer = 0;
}

/** @brief on_tick for Training Option — delegates to legacy function. */
static void training_option_tick(struct _TASK* task_ptr) {
    Training_Option(task_ptr);

    if (task_ptr->r_no[1] != 4 || task_ptr->r_no[0] != 7) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/** @brief on_exit for Training Option. */
static void training_option_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Button Config Training (Training_Jmp_Tbl index 5)
 *
 *  Button mapping during training mode.  Uses Button_Config_Tr() from
 *  menu_input.c (already non-static).  ~30 lines.
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_enter for Button Config Training. */
static void button_config_tr_enter(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 5;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    task_ptr->timer = 0;
}

/** @brief on_tick for Button Config Training — delegates to legacy function. */
static void button_config_tr_tick(struct _TASK* task_ptr) {
    Button_Config_Tr(task_ptr);

    /* Button_Exit_Check_in_Tr changes r_no[1] to 1 or 2 on exit */
    if (task_ptr->r_no[1] != 5 || task_ptr->r_no[0] != 7) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/** @brief on_exit for Button Config Training. */
static void button_config_tr_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Character Change (Training_Jmp_Tbl index 6)
 *
 *  Returns to character select from training mode.  Manages its own
 *  3-phase state: timer wait → screen switch → exit task.  ~80 lines.
 *  Uniquely sets r_no[0]=10 (Wait_Pause_in_Tr) and eventually exits the
 *  menu task entirely via cpExitTask(TASK_MENU).
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_enter for Character Change (training). */
static void char_change_tr_enter(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 6;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    task_ptr->timer = 0;
}

/** @brief on_tick for Character Change (training) — delegates to legacy. */
static void char_change_tr_tick(struct _TASK* task_ptr) {
    Character_Change(task_ptr);

    /* Character_Change exits by cpExitTask(TASK_MENU) or r_no[1] change */
    if (task_ptr->r_no[1] != 6 || task_ptr->r_no[0] != 7) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/** @brief on_exit for Character Change (training). */
static void char_change_tr_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Blocking Training Option (Training_Jmp_Tbl index 7)
 *
 *  Parry training options: 6-item menu with L/R value toggles and
 *  category headers.  ~50 lines.
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief on_enter for Blocking Training Option. */
static void blocking_tr_option_enter(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 7;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    task_ptr->timer = 0;
}

/** @brief on_tick for Blocking Training Option — delegates to legacy. */
static void blocking_tr_option_tick(struct _TASK* task_ptr) {
    Blocking_Tr_Option(task_ptr);

    if (task_ptr->r_no[1] != 7 || task_ptr->r_no[0] != 7) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/** @brief on_exit for Blocking Training Option. */
static void blocking_tr_option_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[] for all 7 training sub-screens
 *
 *  Uses GCC/MSVC constructor attribute to register at startup.
 *  Training screens set timer=0 in on_enter to bypass the dispatcher's
 *  WAIT/FADE_IN phases (training screens manage their own wipe/fade).
 *
 *  Parent for all training sub-screens: MENU_SCREEN_NORMAL_TRAINING or
 *  MENU_SCREEN_BLOCKING_TRAINING (the "top-level" training menus).
 *  For simplicity, all use MENU_SCREEN_TRAINING_MODE as parent since
 *  they ultimately return to the Training Mode selector on exit.
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_training_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_training_reg_ptr)(void) = ms_training_register;
static void ms_training_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_training_register(void) {
#else
void ms_training_register(void) {
#endif

    /* Normal Training (index 1) — 8 items: Normal/Record/Playback/
     * Dummy Setting/Training Option/Button Config/Char Change/Exit */
    g_screens[MENU_SCREEN_NORMAL_TRAINING] = (MenuScreen) {
        .name = "normal_training",
        .id = MENU_SCREEN_NORMAL_TRAINING,
        .parent = MENU_SCREEN_TRAINING_MODE,
        .on_enter = normal_training_enter,
        .on_tick = normal_training_tick,
        .on_exit = normal_training_exit,
        .cursor_max = 7,  /* 8 items (0–7) */
        .cancel_item = 7, /* Exit item */
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_TRAINING,
        .effect_slot = 0,
    };

    /* Blocking Training (index 2) — 6 items */
    g_screens[MENU_SCREEN_BLOCKING_TRAINING] = (MenuScreen) {
        .name = "blocking_training",
        .id = MENU_SCREEN_BLOCKING_TRAINING,
        .parent = MENU_SCREEN_TRAINING_MODE,
        .on_enter = blocking_training_enter,
        .on_tick = blocking_training_tick,
        .on_exit = blocking_training_exit,
        .cursor_max = 5,  /* 6 items (0–5) */
        .cancel_item = 5, /* Exit item */
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_TRAINING,
        .effect_slot = 0,
    };

    /* Dummy Setting (index 3) — 7 items */
    g_screens[MENU_SCREEN_DUMMY_SETTING] = (MenuScreen) {
        .name = "dummy_setting",
        .id = MENU_SCREEN_DUMMY_SETTING,
        .parent = MENU_SCREEN_NORMAL_TRAINING,
        .on_enter = dummy_setting_enter,
        .on_tick = dummy_setting_tick,
        .on_exit = dummy_setting_exit,
        .cursor_max = 6,  /* 7 items (0–6) */
        .cancel_item = 6, /* Exit/Cancel item */
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_TRAINING,
        .effect_slot = 0,
    };

    /* Training Option (index 4) — 6 items */
    g_screens[MENU_SCREEN_TRAINING_OPTION] = (MenuScreen) {
        .name = "training_option",
        .id = MENU_SCREEN_TRAINING_OPTION,
        .parent = MENU_SCREEN_NORMAL_TRAINING,
        .on_enter = training_option_enter,
        .on_tick = training_option_tick,
        .on_exit = training_option_exit,
        .cursor_max = 5,  /* 6 items (0–5) */
        .cancel_item = 5, /* Exit item */
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_TRAINING,
        .effect_slot = 0,
    };

    /* Button Config Training (index 5) — 11 items */
    g_screens[MENU_SCREEN_BUTTON_CONFIG_TR] = (MenuScreen) {
        .name = "button_config_tr",
        .id = MENU_SCREEN_BUTTON_CONFIG_TR,
        .parent = MENU_SCREEN_NORMAL_TRAINING,
        .on_enter = button_config_tr_enter,
        .on_tick = button_config_tr_tick,
        .on_exit = button_config_tr_exit,
        .cursor_max = 10,  /* 11 items (0–10) */
        .cancel_item = 10, /* Exit item */
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_TRAINING,
        .effect_slot = 0,
    };

    /* Character Change (index 6) — no cursor (game state transition) */
    g_screens[MENU_SCREEN_CHAR_CHANGE_TR] = (MenuScreen) {
        .name = "char_change_tr",
        .id = MENU_SCREEN_CHAR_CHANGE_TR,
        .parent = MENU_SCREEN_NORMAL_TRAINING,
        .on_enter = char_change_tr_enter,
        .on_tick = char_change_tr_tick,
        .on_exit = char_change_tr_exit,
        .cursor_max = 1, /* minimum valid value (not a real cursor menu) */
        .cancel_item = 0,
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_TRAINING,
        .effect_slot = 0,
    };

    /* Blocking Training Option (index 7) — 6 items */
    g_screens[MENU_SCREEN_BLOCKING_TR_OPTION] = (MenuScreen) {
        .name = "blocking_tr_option",
        .id = MENU_SCREEN_BLOCKING_TR_OPTION,
        .parent = MENU_SCREEN_BLOCKING_TRAINING,
        .on_enter = blocking_tr_option_enter,
        .on_tick = blocking_tr_option_tick,
        .on_exit = blocking_tr_option_exit,
        .cursor_max = 5,  /* 6 items (0–5) */
        .cancel_item = 5, /* Exit item */
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_TRAINING,
        .effect_slot = 0,
    };
}
