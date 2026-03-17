/**
 * @file menu_screen_registry.c
 * @brief Data-driven MenuScreen registry and dispatcher.
 *
 * Implements the core screen registry, lifecycle phase state machine,
 * and legacy ↔ migrated screen transition infrastructure.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §4.2).
 *
 * During migration, all MENU_USE_NEW_* toggles default to 0 so the legacy
 * jump-table dispatch handles every screen.  As screens are migrated, their
 * toggle is set to 1 and the registry interceptor routes them here instead.
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/menu/menu_internal.h"  /* AT_JMP_COUNT */
#include "sf33rd/Source/Game/ui/sc_sub.h"           /* FadeOut/FadeIn/FadeInit */

#include <string.h>  /* memset (if needed) */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Compile-time rollback toggles — set to 0 to revert a screen to legacy.
 *  Each screen gets its own toggle; they're all 0 until the screen is
 *  migrated and verified.
 * ═══════════════════════════════════════════════════════════════════════════ */

#define MENU_USE_NEW_MODE_SELECT      1
#define MENU_USE_NEW_OPTION_SELECT    1
#define MENU_USE_NEW_GAME_OPTION      1
#define MENU_USE_NEW_BUTTON_CONFIG    1
#define MENU_USE_NEW_SOUND_TEST       1
#define MENU_USE_NEW_MEMORY_CARD      1
#define MENU_USE_NEW_SYSTEM_DIRECTION 1
#define MENU_USE_NEW_EXTRA_OPTION     1
#define MENU_USE_NEW_DIRECTION_MENU   1
#define MENU_USE_NEW_TRAINING_MODE    1
#define MENU_USE_NEW_LOAD_REPLAY      1
#define MENU_USE_NEW_EXIT_CONFIRM     1
#define MENU_USE_NEW_VS_RESULT        1
#define MENU_USE_NEW_SAVE_REPLAY      1
#define MENU_USE_NEW_NETWORK_LOBBY    1
#define MENU_USE_NEW_NETWORK_LAN      0
#define MENU_USE_NEW_LEADERBOARD      0
#define MENU_USE_NEW_SCREEN_ADJUST    0
#define MENU_USE_NEW_PAUSE_MENU       0
#define MENU_USE_NEW_BUTTON_CONFIG_IG 0
#define MENU_USE_NEW_CHAR_CHANGE_IG   0

/* ═══════════════════════════════════════════════════════════════════════════
 *  Per-screen enabled flags — populated from the MENU_USE_NEW_* toggles.
 *  MenuScreen_FromLegacyIndex() checks this before returning a mapped id.
 * ═══════════════════════════════════════════════════════════════════════════ */

static const bool g_screen_enabled[MENU_SCREEN_COUNT] = {
    [MENU_SCREEN_MODE_SELECT]      = MENU_USE_NEW_MODE_SELECT,
    [MENU_SCREEN_OPTION_SELECT]    = MENU_USE_NEW_OPTION_SELECT,
    [MENU_SCREEN_GAME_OPTION]      = MENU_USE_NEW_GAME_OPTION,
    [MENU_SCREEN_BUTTON_CONFIG]    = MENU_USE_NEW_BUTTON_CONFIG,
    [MENU_SCREEN_SOUND_TEST]       = MENU_USE_NEW_SOUND_TEST,
    [MENU_SCREEN_MEMORY_CARD]      = MENU_USE_NEW_MEMORY_CARD,
    [MENU_SCREEN_SYSTEM_DIRECTION] = MENU_USE_NEW_SYSTEM_DIRECTION,
    [MENU_SCREEN_EXTRA_OPTION]     = MENU_USE_NEW_EXTRA_OPTION,
    [MENU_SCREEN_DIRECTION_MENU]   = MENU_USE_NEW_DIRECTION_MENU,
    [MENU_SCREEN_TRAINING_MODE]    = MENU_USE_NEW_TRAINING_MODE,
    [MENU_SCREEN_LOAD_REPLAY]      = MENU_USE_NEW_LOAD_REPLAY,
    [MENU_SCREEN_EXIT_CONFIRM]     = MENU_USE_NEW_EXIT_CONFIRM,
    [MENU_SCREEN_VS_RESULT]        = MENU_USE_NEW_VS_RESULT,
    [MENU_SCREEN_SAVE_REPLAY]      = MENU_USE_NEW_SAVE_REPLAY,
    [MENU_SCREEN_NETWORK_LOBBY]    = MENU_USE_NEW_NETWORK_LOBBY,
    [MENU_SCREEN_NETWORK_LOBBY_LAN]= MENU_USE_NEW_NETWORK_LAN,
    [MENU_SCREEN_LEADERBOARD]      = MENU_USE_NEW_LEADERBOARD,
    [MENU_SCREEN_SCREEN_ADJUST]    = MENU_USE_NEW_SCREEN_ADJUST,
    [MENU_SCREEN_PAUSE_MENU]       = MENU_USE_NEW_PAUSE_MENU,
    [MENU_SCREEN_BUTTON_CONFIG_IG] = MENU_USE_NEW_BUTTON_CONFIG_IG,
    [MENU_SCREEN_CHAR_CHANGE_IG]   = MENU_USE_NEW_CHAR_CHANGE_IG,
};

/* ═══════════════════════════════════════════════════════════════════════════
 *  Screen Registry
 *
 *  Initially empty — each screen is populated in its own ms_*.c file via
 *  designated-initializer assignment. Until populated, all callback
 *  pointers are NULL, which means the registry won't dispatch to them.
 * ═══════════════════════════════════════════════════════════════════════════ */

MenuScreen g_screens[MENU_SCREEN_COUNT];

/* ═══════════════════════════════════════════════════════════════════════════
 *  Dispatcher State
 * ═══════════════════════════════════════════════════════════════════════════ */

static MenuScreenId    g_current_screen = MENU_SCREEN_NONE;
static MenuScreenId    g_next_screen    = MENU_SCREEN_NONE;
static MenuScreenPhase g_phase          = MENU_PHASE_ENTER;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Legacy → Migrated lookup table
 *
 *  Maps AT_Jmp_Tbl indices (r_no[1] values inside After_Title) to
 *  MenuScreenId values.  Entries default to MENU_SCREEN_NONE (= -1) for
 *  un-migrated screens so the legacy dispatch handles them.
 *
 *  IMPORTANT: Option_Select has FOUR alias indices (2, 3, 7, 15) —
 *  all must map to MENU_SCREEN_OPTION_SELECT when migrated.
 * ═══════════════════════════════════════════════════════════════════════════ */

static const MenuScreenId g_legacy_to_screen[AT_JMP_COUNT] = {
    [0]  = MENU_SCREEN_NONE,           /* Menu_Init (bootstrap — never migrated) */
    [1]  = MENU_SCREEN_MODE_SELECT,
    [2]  = MENU_SCREEN_OPTION_SELECT,
    [3]  = MENU_SCREEN_OPTION_SELECT,  /* alias */
    [4]  = MENU_SCREEN_TRAINING_MODE,
    [5]  = MENU_SCREEN_SYSTEM_DIRECTION,
    [6]  = MENU_SCREEN_LOAD_REPLAY,
    [7]  = MENU_SCREEN_OPTION_SELECT,  /* alias — Return_Option_Mode_Sub target */
    [8]  = MENU_SCREEN_EXIT_CONFIRM,
    [9]  = MENU_SCREEN_GAME_OPTION,
    [10] = MENU_SCREEN_BUTTON_CONFIG,
    [11] = MENU_SCREEN_SYSTEM_DIRECTION, /* SysDir from Option */
    [12] = MENU_SCREEN_SOUND_TEST,
    [13] = MENU_SCREEN_MEMORY_CARD,
    [14] = MENU_SCREEN_EXTRA_OPTION,
    [15] = MENU_SCREEN_OPTION_SELECT,  /* alias */
    [16] = MENU_SCREEN_VS_RESULT,
    [17] = MENU_SCREEN_SAVE_REPLAY,
    [18] = MENU_SCREEN_DIRECTION_MENU,
    [19] = MENU_SCREEN_NONE,           /* Save_Direction — handled by ms_save_direction.c */
    [20] = MENU_SCREEN_NONE,           /* Load_Direction — handled by ms_load_direction.c */
    [21] = MENU_SCREEN_NETWORK_LOBBY,
};

/* ═══════════════════════════════════════════════════════════════════════════
 *  Core API Implementation
 * ═══════════════════════════════════════════════════════════════════════════ */

void MenuScreen_Goto(MenuScreenId id) {
    g_next_screen = id;
}

void MenuScreen_Back(void) {
    if (g_current_screen != MENU_SCREEN_NONE) {
        g_next_screen = g_screens[g_current_screen].parent;
    }
}

bool MenuScreen_IsActive(void) {
    return g_current_screen != MENU_SCREEN_NONE;
}

MenuScreenPhase MenuScreen_GetPhase(void) {
    return g_phase;
}

void MenuScreen_RequestFadeOut(void) {
    if (g_phase == MENU_PHASE_ACTIVE) {
        g_phase = MENU_PHASE_FADE_OUT;
    }
}

MenuScreenId MenuScreen_FromLegacyIndex(int legacy_index) {
    if (legacy_index < 0 || legacy_index >= AT_JMP_COUNT) {
        return MENU_SCREEN_NONE;
    }

    MenuScreenId id = g_legacy_to_screen[legacy_index];

    /* If the screen is mapped but its rollback toggle is off,
     * return NONE so legacy dispatch handles it. */
    if (id != MENU_SCREEN_NONE && !g_screen_enabled[id]) {
        return MENU_SCREEN_NONE;
    }

    return id;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_Tick — Phase State Machine
 *
 *  Called once per frame from the integration hook in After_Title()
 *  (and later from Training_Menu() and In_Game()).
 *
 *  Phase flow:
 *    ENTER → WAIT → FADE_IN → ACTIVE → (FADE_OUT → EXIT if requested)
 *
 *  Deferred transitions: if Goto/Back was called during the previous
 *  frame, the current screen's on_exit is called and the new screen's
 *  on_enter is invoked at the top of this function.
 * ═══════════════════════════════════════════════════════════════════════════ */

void MenuScreen_Tick(struct _TASK* task_ptr) {
    /* ── Deferred transition (set by Goto/Back on previous frame) ── */
    if (g_next_screen != MENU_SCREEN_NONE) {
        /* Exit the current screen (if any) */
        if (g_current_screen != MENU_SCREEN_NONE) {
            const MenuScreen* cur = &g_screens[g_current_screen];
            if (cur->on_exit) {
                cur->on_exit(task_ptr);
            }
            if (cur->rmlui_hide) {
                cur->rmlui_hide();
            }
        }

        g_current_screen = g_next_screen;
        g_next_screen    = MENU_SCREEN_NONE;
        g_phase          = MENU_PHASE_ENTER;
    }

    /* Guard: if no screen is active (e.g. after ExitToLegacy), bail out */
    if (g_current_screen == MENU_SCREEN_NONE) {
        return;
    }

    const MenuScreen* scr = &g_screens[g_current_screen];

    /* ── Phase state machine ── */
    switch (g_phase) {

    case MENU_PHASE_ENTER:
        /* One-shot: call on_enter, show RmlUi doc, advance to WAIT */
        if (scr->on_enter) {
            scr->on_enter(task_ptr);
        }
        if (scr->rmlui_show) {
            scr->rmlui_show();
        }
        g_phase = MENU_PHASE_WAIT;
        /* Does NOT fall through — wait starts next frame */
        break;

    case MENU_PHASE_WAIT:
        /* Wait for the timer set by on_enter (Menu_Sub_case1 pattern).
         * Menu_Sub_case1() returns non-zero when the timer expires.
         * Once done, init fade and advance. */
        if (Menu_Sub_case1(task_ptr) != 0) {
            FadeInit();
            g_phase = MENU_PHASE_FADE_IN;
        }
        break;

    case MENU_PHASE_FADE_IN:
        /* FadeIn returns non-zero when the fade is complete */
        if (FadeIn(1, 0x19, 8) != 0) {
            g_phase = MENU_PHASE_ACTIVE;
        }
        break;

    case MENU_PHASE_ACTIVE:
        /* Per-frame input handling — the screen's on_tick does the work.
         * on_tick may call MenuScreen_Goto/Back which sets g_next_screen;
         * the transition is processed on the NEXT frame, never re-entrant. */
        if (scr->on_tick) {
            scr->on_tick(task_ptr);
        }
        break;

    case MENU_PHASE_FADE_OUT:
        /* FadeOut returns non-zero when complete */
        if (FadeOut(1, 0x19, 8) != 0) {
            g_phase = MENU_PHASE_EXIT;
        }
        break;

    case MENU_PHASE_EXIT:
        /* One-shot cleanup for "exit to non-registry code" paths.
         * Normal Goto/Back transitions handle on_exit in the deferred
         * block above; this path is for MenuScreen_ExitToLegacy or
         * screens that drive their own fade-out then manually call
         * Goto/Back after checking GetPhase() == EXIT. */
        if (scr->on_exit) {
            scr->on_exit(task_ptr);
        }
        if (scr->rmlui_hide) {
            scr->rmlui_hide();
        }
        g_current_screen = MENU_SCREEN_NONE;
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_ExitToLegacy
 *
 *  Clear registry state so MenuScreen_IsActive() returns false.
 *  The caller sets r_no directly before calling this.
 *  Calls on_exit and rmlui_hide for the current screen.
 * ═══════════════════════════════════════════════════════════════════════════ */

void MenuScreen_ExitToLegacy(struct _TASK* task_ptr) {
    if (g_current_screen != MENU_SCREEN_NONE) {
        const MenuScreen* cur = &g_screens[g_current_screen];
        if (cur->on_exit) {
            cur->on_exit(task_ptr);
        }
        if (cur->rmlui_hide) {
            cur->rmlui_hide();
        }
    }
    g_current_screen = MENU_SCREEN_NONE;
    g_next_screen    = MENU_SCREEN_NONE;
    g_phase          = MENU_PHASE_ENTER;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen_HardReset — replaces Back_to_Mode_Select()
 *
 *  Stub for now — will be implemented in Task 3 (Shared Helpers) when
 *  the full reset sequence is wired up.
 * ═══════════════════════════════════════════════════════════════════════════ */

void MenuScreen_HardReset(struct _TASK* task_ptr) {
    /* Phase 3 implementation will call:
     *   Back_to_Mode_Select(task_ptr);
     *   MenuScreen_Goto(MENU_SCREEN_MODE_SELECT);
     * For now, forward to the legacy function and transition. */
    Back_to_Mode_Select(task_ptr);
    MenuScreen_Goto(MENU_SCREEN_MODE_SELECT);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Training dispatch hooks — Phase 5 (Task 18)
 *
 *  Training_Menu() dispatches via Training_Jmp_Tbl[r_no[1]] with 8 entries.
 *  These functions let the registry intercept that dispatch for migrated
 *  training sub-screens, exactly as After_Title() uses FromLegacyIndex.
 *
 *  g_training_active is set while a training sub-screen is being driven
 *  by the registry within Training_Menu()'s frame.  This is separate from
 *  the After_Title active flag because Training_Menu has post-dispatch
 *  rendering (Akaobi, ToneDown, SSPutStr_Bigger) that must always run.
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool g_training_active = false;

/**
 * @brief Training_Jmp_Tbl index → MenuScreenId mapping.
 *
 * All entries default to MENU_SCREEN_NONE.  As training sub-screens are
 * migrated (Task 19), their entries will be populated:
 *   [0] Training_Init        — bootstrap, never migrated
 *   [1] Normal_Training      → MENU_SCREEN_NORMAL_TRAINING (Task 19)
 *   [2] Blocking_Training    → MENU_SCREEN_BLOCKING_TRAINING (Task 19)
 *   [3] Dummy_Setting        → ... (Task 19)
 *   [4] Training_Option      → ... (Task 19)
 *   [5] Button_Config_Tr     → ... (Task 19)
 *   [6] Character_Change     → ... (Task 19)
 *   [7] Blocking_Tr_Option   → ... (Task 19)
 */
static const MenuScreenId g_training_to_screen[TRAINING_JMP_COUNT] = {
    [0] = MENU_SCREEN_NONE,  /* Training_Init — bootstrap, not migrated */
    [1] = MENU_SCREEN_NONE,  /* Normal_Training */
    [2] = MENU_SCREEN_NONE,  /* Blocking_Training */
    [3] = MENU_SCREEN_NONE,  /* Dummy_Setting */
    [4] = MENU_SCREEN_NONE,  /* Training_Option */
    [5] = MENU_SCREEN_NONE,  /* Button_Config_Tr */
    [6] = MENU_SCREEN_NONE,  /* Character_Change */
    [7] = MENU_SCREEN_NONE,  /* Blocking_Tr_Option */
};

bool MenuScreen_IsTrainingActive(void) {
    return g_training_active;
}

void MenuScreen_TrainingTick(struct _TASK* task_ptr) {
    g_training_active = true;
    MenuScreen_Tick(task_ptr);

    /* If the screen exited (via ExitToLegacy or natural exit),
     * clear the training-active flag so the next frame falls
     * through to legacy dispatch in Training_Menu(). */
    if (!MenuScreen_IsActive()) {
        g_training_active = false;
    }
}

MenuScreenId MenuScreen_FromTrainingIndex(int training_index) {
    if (training_index < 0 || training_index >= TRAINING_JMP_COUNT) {
        return MENU_SCREEN_NONE;
    }

    MenuScreenId id = g_training_to_screen[training_index];

    /* If the screen is mapped but its rollback toggle is off,
     * return NONE so legacy dispatch handles it. */
    if (id != MENU_SCREEN_NONE && !g_screen_enabled[id]) {
        return MENU_SCREEN_NONE;
    }

    return id;
}

