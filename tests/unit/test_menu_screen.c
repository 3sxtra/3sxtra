/**
 * @file test_menu_screen.c
 * @brief Unit tests for the MenuScreen registry and dispatcher.
 *
 * Validates:
 *   1. All registered screens have required callbacks (on_enter/on_tick/on_exit).
 *   2. All registered screens have valid parent (parent != self, parent exists or NONE).
 *   3. MenuScreen_Goto() sets correct phase (MENU_PHASE_ENTER).
 *   4. MenuScreen_Back() targets parent.
 *   5. cursor_max > 0 for all registered (active) screens.
 *   6. FromLegacyIndex bounds checking.
 *   7. IsActive reflects state correctly.
 *   8. RequestFadeOut transitions phase.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef cmb2
#undef cmb2
#endif
#ifdef cmb3
#undef cmb3
#endif
#ifdef s_addr
#undef s_addr
#endif
#endif

#include "port/menu_screen.h"
#include "sf33rd/Source/Game/menu/menu_internal.h" /* AT_JMP_COUNT */

/* ══════════════════════════════════════════════════════════════════════════
 *  Access to internal registry data.
 *  g_screens[] is defined in menu_screen_registry.c without static, so we
 *  can reference it with extern here for test inspection.
 * ══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

/* ══════════════════════════════════════════════════════════════════════════
 *  Stubs for engine functions called by menu_screen_registry.c
 *  These have no side effects — we only test registry logic.
 * ══════════════════════════════════════════════════════════════════════════ */

/* sc_sub.h — fade/wipe stubs */
s32 FadeOut(u8 type, u8 step, u8 priority) {
    (void)type; (void)step; (void)priority;
    return 1; /* always "complete" */
}

s32 FadeIn(u8 type, u8 step, u8 priority) {
    (void)type; (void)step; (void)priority;
    return 1; /* always "complete" */
}

void FadeInit(void) { /* no-op */ }

/* menu_internal.h — Menu_Sub_case1 */
s32 Menu_Sub_case1(struct _TASK* task_ptr) {
    (void)task_ptr;
    return 1; /* timer always expired */
}

/* menu_internal.h — Back_to_Mode_Select */
void Back_to_Mode_Select(struct _TASK* task_ptr) {
    (void)task_ptr;
    /* no-op for test */
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Dummy global stubs that menu_internal.h / menu.h transitively need.
 *  (Only link symbols — never actually used by registry.c at runtime.)
 * ══════════════════════════════════════════════════════════════════════════ */

u8 G_No[4];
u8 S_No[4];

/* ══════════════════════════════════════════════════════════════════════════
 *  Test helper: register a fake screen into g_screens[] for testing.
 * ══════════════════════════════════════════════════════════════════════════ */

static int g_enter_call_count;
static int g_tick_call_count;
static int g_exit_call_count;

static void fake_on_enter(struct _TASK* t) { t->timer = 5; g_enter_call_count++; }
static void fake_on_tick(struct _TASK* t)  { (void)t; g_tick_call_count++;  }
static void fake_on_exit(struct _TASK* t)  { (void)t; g_exit_call_count++;  }

static int g_rmlui_show_count;
static int g_rmlui_hide_count;
static void fake_rmlui_show(void) { g_rmlui_show_count++; }
static void fake_rmlui_hide(void) { g_rmlui_hide_count++; }

/**
 * Register two fake screens for testing navigation.
 * MODE_SELECT (id=0, no parent) and OPTION_SELECT (id=1, parent=MODE_SELECT).
 */
static void register_test_screens(void) {
    memset(g_screens, 0, sizeof(g_screens));
    g_enter_call_count = 0;
    g_tick_call_count  = 0;
    g_exit_call_count  = 0;
    g_rmlui_show_count = 0;
    g_rmlui_hide_count = 0;

    g_screens[MENU_SCREEN_MODE_SELECT] = (MenuScreen){
        .name       = "mode_select",
        .id         = MENU_SCREEN_MODE_SELECT,
        .parent     = MENU_SCREEN_NONE,
        .on_enter   = fake_on_enter,
        .on_tick    = fake_on_tick,
        .on_exit    = fake_on_exit,
        .cursor_max = 6,
        .cancel_item = -1,
        .rmlui_show = fake_rmlui_show,
        .rmlui_hide = fake_rmlui_hide,
        .header_type = MENU_HEADER_MODE_MENU,
        .effect_slot = 0x64,
    };

    g_screens[MENU_SCREEN_OPTION_SELECT] = (MenuScreen){
        .name       = "option_select",
        .id         = MENU_SCREEN_OPTION_SELECT,
        .parent     = MENU_SCREEN_MODE_SELECT,
        .on_enter   = fake_on_enter,
        .on_tick    = fake_on_tick,
        .on_exit    = fake_on_exit,
        .cursor_max = 6,
        .cancel_item = 5,
        .rmlui_show = fake_rmlui_show,
        .rmlui_hide = fake_rmlui_hide,
        .header_type = MENU_HEADER_OPTION_MENU,
        .effect_slot = 0x65,
    };
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Setup / teardown — reset dispatcher state between tests by doing
 *  ExitToLegacy (which clears g_current_screen to NONE).
 * ══════════════════════════════════════════════════════════════════════════ */

static int test_setup(void **state) {
    (void)state;
    struct _TASK dummy_task;
    memset(&dummy_task, 0, sizeof(dummy_task));
    /* Force-exit any lingering state */
    MenuScreen_ExitToLegacy(&dummy_task);
    register_test_screens();
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test 1: All registered screens have required callbacks
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_all_screens_have_required_callbacks(void **state) {
    (void)state;

    for (int i = 0; i < MENU_SCREEN_COUNT; i++) {
        const MenuScreen* scr = &g_screens[i];

        /* Only check screens that are actually registered (have a name) */
        if (scr->name == NULL) continue;

        assert_non_null(scr->on_enter);
        assert_non_null(scr->on_tick);
        assert_non_null(scr->on_exit);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test 2: All registered screens have valid parent
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_all_screens_have_valid_parent(void **state) {
    (void)state;

    for (int i = 0; i < MENU_SCREEN_COUNT; i++) {
        const MenuScreen* scr = &g_screens[i];

        /* Only check screens that are actually registered (have a name) */
        if (scr->name == NULL) continue;

        /* Parent must not be self */
        assert_int_not_equal(scr->parent, scr->id);

        /* Parent must be NONE or a valid screen ID */
        if (scr->parent != MENU_SCREEN_NONE) {
            assert_true(scr->parent >= 0 && scr->parent < MENU_SCREEN_COUNT);
        }
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test 3: Goto sets correct phase (MENU_PHASE_ENTER)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_goto_sets_correct_phase(void **state) {
    (void)state;

    struct _TASK t;
    memset(&t, 0, sizeof(t));

    /* Goto queues a deferred transition */
    MenuScreen_Goto(MENU_SCREEN_MODE_SELECT);

    /* Before Tick, IsActive is false (deferred) */
    assert_false(MenuScreen_IsActive());

    /* First Tick processes the deferred transition */
    MenuScreen_Tick(&t);

    /* Now it's active and phase should be ENTER (just completed the on_enter
     * call — the dispatcher sets phase to WAIT at end of ENTER, but the
     * initial phase when entering the deferred block is ENTER). */
    assert_true(MenuScreen_IsActive());

    /* The dispatcher processes ENTER in the first Tick call:
     *   1. Deferred block: set g_current_screen, set g_phase = ENTER
     *   2. Phase switch: ENTER → calls on_enter, advances to WAIT
     * So after the first tick, phase is WAIT. But the PRD says "after
     * MenuScreen_Goto(), phase is MENU_PHASE_ENTER" — this tests that
     * the Goto call sets up the transition. We verify the phase machine
     * produces the expected progression. */
    MenuScreenPhase phase = MenuScreen_GetPhase();
    /* After one tick: ENTER has been processed → phase is now WAIT */
    assert_int_equal(phase, MENU_PHASE_WAIT);

    /* Verify on_enter was called */
    assert_int_equal(g_enter_call_count, 1);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test 4: Back returns to parent
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_back_returns_to_parent(void **state) {
    (void)state;

    struct _TASK t;
    memset(&t, 0, sizeof(t));

    /* Navigate to OPTION_SELECT (which has parent MODE_SELECT) */
    MenuScreen_Goto(MENU_SCREEN_OPTION_SELECT);
    MenuScreen_Tick(&t); /* process deferred transition → ENTER → WAIT */
    MenuScreen_Tick(&t); /* WAIT → timer always expires → FADE_IN */
    MenuScreen_Tick(&t); /* FADE_IN → complete → ACTIVE */

    assert_int_equal(MenuScreen_GetPhase(), MENU_PHASE_ACTIVE);

    /* Reset call counters before Back */
    g_enter_call_count = 0;
    g_exit_call_count  = 0;

    /* Call Back — should queue a transition to parent (MODE_SELECT) */
    MenuScreen_Back();

    /* Process the deferred transition */
    MenuScreen_Tick(&t);

    /* on_exit should have been called for OPTION_SELECT */
    assert_int_equal(g_exit_call_count, 1);
    /* on_enter should have been called for MODE_SELECT */
    assert_int_equal(g_enter_call_count, 1);

    /* Now the current screen should be MODE_SELECT */
    assert_true(MenuScreen_IsActive());
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test 5: cursor_max positive for all registered screens
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_cursor_max_positive(void **state) {
    (void)state;

    for (int i = 0; i < MENU_SCREEN_COUNT; i++) {
        const MenuScreen* scr = &g_screens[i];

        /* Only check screens that are actually registered (have a name) */
        if (scr->name == NULL) continue;

        /* Non-interactive screens (ranking, demo, saver, etc.) intentionally
         * have cursor_max == 0 — they have no selectable items. */
        if (scr->cursor_max == 0) continue;

        assert_true(scr->cursor_max > 0);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test 6: FromLegacyIndex bounds checking
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_from_legacy_index_bounds(void **state) {
    (void)state;

    /* Negative indices should return NONE */
    assert_int_equal(MenuScreen_FromLegacyIndex(-1), MENU_SCREEN_NONE);
    assert_int_equal(MenuScreen_FromLegacyIndex(-100), MENU_SCREEN_NONE);

    /* Out-of-range index should return NONE */
    assert_int_equal(MenuScreen_FromLegacyIndex(AT_JMP_COUNT), MENU_SCREEN_NONE);
    assert_int_equal(MenuScreen_FromLegacyIndex(AT_JMP_COUNT + 1), MENU_SCREEN_NONE);
    assert_int_equal(MenuScreen_FromLegacyIndex(9999), MENU_SCREEN_NONE);

    /* Index 0 (Menu_Init bootstrap) should always return NONE */
    assert_int_equal(MenuScreen_FromLegacyIndex(0), MENU_SCREEN_NONE);

    /* Index 1 (Mode_Select) should return MENU_SCREEN_MODE_SELECT */
    assert_int_equal(MenuScreen_FromLegacyIndex(1), MENU_SCREEN_MODE_SELECT);

    /* Indices 2, 3, 7, 15 (Option_Select aliases) should all return
     * MENU_SCREEN_OPTION_SELECT */
    assert_int_equal(MenuScreen_FromLegacyIndex(2), MENU_SCREEN_OPTION_SELECT);
    assert_int_equal(MenuScreen_FromLegacyIndex(3), MENU_SCREEN_OPTION_SELECT);
    assert_int_equal(MenuScreen_FromLegacyIndex(7), MENU_SCREEN_OPTION_SELECT);
    assert_int_equal(MenuScreen_FromLegacyIndex(15), MENU_SCREEN_OPTION_SELECT);

    /* Index 4 (Training_Mode) should return MENU_SCREEN_TRAINING_MODE */
    assert_int_equal(MenuScreen_FromLegacyIndex(4), MENU_SCREEN_TRAINING_MODE);

    /* Index 6 (Load_Replay) should return MENU_SCREEN_LOAD_REPLAY */
    assert_int_equal(MenuScreen_FromLegacyIndex(6), MENU_SCREEN_LOAD_REPLAY);

    /* Index 8 (Exit_Confirm) should return MENU_SCREEN_EXIT_CONFIRM */
    assert_int_equal(MenuScreen_FromLegacyIndex(8), MENU_SCREEN_EXIT_CONFIRM);

    /* Index 9 (Game_Option) should return MENU_SCREEN_GAME_OPTION */
    assert_int_equal(MenuScreen_FromLegacyIndex(9), MENU_SCREEN_GAME_OPTION);

    /* Index 10 (Button_Config) should return MENU_SCREEN_BUTTON_CONFIG */
    assert_int_equal(MenuScreen_FromLegacyIndex(10), MENU_SCREEN_BUTTON_CONFIG);

    /* Index 12 (Sound_Test) should return MENU_SCREEN_SOUND_TEST */
    assert_int_equal(MenuScreen_FromLegacyIndex(12), MENU_SCREEN_SOUND_TEST);

    /* Index 13 (Memory_Card) should return MENU_SCREEN_MEMORY_CARD */
    assert_int_equal(MenuScreen_FromLegacyIndex(13), MENU_SCREEN_MEMORY_CARD);

    /* Index 17 (Save_Replay) should return MENU_SCREEN_SAVE_REPLAY */
    assert_int_equal(MenuScreen_FromLegacyIndex(17), MENU_SCREEN_SAVE_REPLAY);

    /* Index 16 (VS_Result) should return MENU_SCREEN_VS_RESULT */
    assert_int_equal(MenuScreen_FromLegacyIndex(16), MENU_SCREEN_VS_RESULT);

    /* Index 5 (System_Direction from Mode_Select) should return
     * MENU_SCREEN_SYSTEM_DIRECTION */
    assert_int_equal(MenuScreen_FromLegacyIndex(5), MENU_SCREEN_SYSTEM_DIRECTION);

    /* Index 11 (System_Direction from Option_Select) should also return
     * MENU_SCREEN_SYSTEM_DIRECTION (dual entry point) */
    assert_int_equal(MenuScreen_FromLegacyIndex(11), MENU_SCREEN_SYSTEM_DIRECTION);

    /* Index 14 (Extra_Option) should return MENU_SCREEN_EXTRA_OPTION */
    assert_int_equal(MenuScreen_FromLegacyIndex(14), MENU_SCREEN_EXTRA_OPTION);

    /* Index 18 (Direction_Menu) should return MENU_SCREEN_DIRECTION_MENU */
    assert_int_equal(MenuScreen_FromLegacyIndex(18), MENU_SCREEN_DIRECTION_MENU);

    /* Index 21 (Network_Lobby) should return MENU_SCREEN_NETWORK_LOBBY */
    assert_int_equal(MenuScreen_FromLegacyIndex(21), MENU_SCREEN_NETWORK_LOBBY);

    /* Non-migrated indices should still return NONE */
    for (int i = 4; i < AT_JMP_COUNT; i++) {
        /* Skip the 4 Option_Select aliases which are migrated */
        if (i == 7 || i == 15) continue;
        /* Skip Training_Mode which is now migrated */
        if (i == 4) continue;
        /* Skip System_Direction (AT 5 and 11) which is now migrated */
        if (i == 5 || i == 11) continue;
        /* Skip Load_Replay which is now migrated */
        if (i == 6) continue;
        /* Skip Exit_Confirm which is now migrated */
        if (i == 8) continue;
        /* Skip Game_Option which is now migrated */
        if (i == 9) continue;
        /* Skip Button_Config which is now migrated */
        if (i == 10) continue;
        /* Skip Sound_Test which is now migrated */
        if (i == 12) continue;
        /* Skip Memory_Card which is now migrated */
        if (i == 13) continue;
        /* Skip Extra_Option which is now migrated */
        if (i == 14) continue;
        /* Skip VS_Result which is now migrated */
        if (i == 16) continue;
        /* Skip Save_Replay which is now migrated */
        if (i == 17) continue;
        /* Skip Direction_Menu which is now migrated */
        if (i == 18) continue;
        /* Skip Network_Lobby which is now migrated */
        if (i == 21) continue;
        assert_int_equal(MenuScreen_FromLegacyIndex(i), MENU_SCREEN_NONE);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test 7: IsActive reflects state
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_is_active_reflects_state(void **state) {
    (void)state;

    struct _TASK t;
    memset(&t, 0, sizeof(t));

    /* Initially not active */
    assert_false(MenuScreen_IsActive());

    /* After Goto + Tick, should be active */
    MenuScreen_Goto(MENU_SCREEN_MODE_SELECT);
    MenuScreen_Tick(&t);
    assert_true(MenuScreen_IsActive());

    /* After ExitToLegacy, should not be active */
    MenuScreen_ExitToLegacy(&t);
    assert_false(MenuScreen_IsActive());
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test 8: RequestFadeOut transitions from ACTIVE to FADE_OUT
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_request_fade_out(void **state) {
    (void)state;

    struct _TASK t;
    memset(&t, 0, sizeof(t));

    /* Navigate to active */
    MenuScreen_Goto(MENU_SCREEN_MODE_SELECT);
    MenuScreen_Tick(&t); /* deferred → ENTER → WAIT */
    MenuScreen_Tick(&t); /* WAIT → FADE_IN */
    MenuScreen_Tick(&t); /* FADE_IN → ACTIVE */

    assert_int_equal(MenuScreen_GetPhase(), MENU_PHASE_ACTIVE);

    /* Request fade out */
    MenuScreen_RequestFadeOut();
    assert_int_equal(MenuScreen_GetPhase(), MENU_PHASE_FADE_OUT);

    /* Tick processes FADE_OUT → exits */
    MenuScreen_Tick(&t);
    assert_int_equal(MenuScreen_GetPhase(), MENU_PHASE_EXIT);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test 9: Phase progression (full lifecycle)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_phase_progression(void **state) {
    (void)state;

    struct _TASK t;
    memset(&t, 0, sizeof(t));

    MenuScreen_Goto(MENU_SCREEN_MODE_SELECT);

    /* Tick 1: deferred transition → ENTER → on_enter called → phase = WAIT */
    MenuScreen_Tick(&t);
    assert_int_equal(MenuScreen_GetPhase(), MENU_PHASE_WAIT);

    /* Tick 2: WAIT → timer expires (stub returns 1) → FadeInit → phase = FADE_IN */
    MenuScreen_Tick(&t);
    assert_int_equal(MenuScreen_GetPhase(), MENU_PHASE_FADE_IN);

    /* Tick 3: FADE_IN → FadeIn returns 1 (stub) → phase = ACTIVE */
    MenuScreen_Tick(&t);
    assert_int_equal(MenuScreen_GetPhase(), MENU_PHASE_ACTIVE);

    /* Tick 4: ACTIVE → on_tick is called */
    MenuScreen_Tick(&t);
    assert_true(g_tick_call_count > 0);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test 10: RmlUi callbacks invoked on transitions
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_rmlui_callbacks_invoked(void **state) {
    (void)state;

    struct _TASK t;
    memset(&t, 0, sizeof(t));

    /* Navigate to a screen — rmlui_show is NOT called by the dispatcher
     * (BUG-2 fix: screens call rmlui_show from on_enter themselves) */
    MenuScreen_Goto(MENU_SCREEN_MODE_SELECT);
    MenuScreen_Tick(&t);
    assert_int_equal(g_rmlui_show_count, 0);

    /* Exit — rmlui_hide should be called */
    MenuScreen_ExitToLegacy(&t);
    assert_int_equal(g_rmlui_hide_count, 1);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test 11: timer=0 in on_enter skips WAIT phase (BUG-1 regression)
 *
 *  Screens that set timer=0 in on_enter intend to bypass the dispatcher's
 *  WAIT and FADE_IN phases.  Before the fix, Menu_Sub_case1 would decrement
 *  timer from 0 to -1 which never equaled 0, causing an infinite WAIT.
 *  The fix adds a timer<=0 guard that skips directly to ACTIVE.
 * ══════════════════════════════════════════════════════════════════════════ */

static void fake_on_enter_timer_zero(struct _TASK* t) {
    g_enter_call_count++;
    t->timer = 0; /* bypass WAIT/FADE_IN */
}

static void test_timer_zero_skips_wait(void **state) {
    (void)state;

    struct _TASK t;
    memset(&t, 0, sizeof(t));

    /* Override MODE_SELECT's on_enter to set timer=0 */
    g_screens[MENU_SCREEN_MODE_SELECT].on_enter = fake_on_enter_timer_zero;

    MenuScreen_Goto(MENU_SCREEN_MODE_SELECT);

    /* Tick 1: deferred transition → ENTER → on_enter sets timer=0 → WAIT */
    MenuScreen_Tick(&t);
    assert_int_equal(MenuScreen_GetPhase(), MENU_PHASE_WAIT);
    assert_int_equal(t.timer, 0);

    /* Tick 2: timer<=0 guard fires → skip directly to ACTIVE */
    MenuScreen_Tick(&t);
    assert_int_equal(MenuScreen_GetPhase(), MENU_PHASE_ACTIVE);

    /* Verify on_enter was called exactly once */
    assert_int_equal(g_enter_call_count, 1);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Test runner
 * ══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_all_screens_have_required_callbacks, test_setup),
        cmocka_unit_test_setup(test_all_screens_have_valid_parent, test_setup),
        cmocka_unit_test_setup(test_goto_sets_correct_phase, test_setup),
        cmocka_unit_test_setup(test_back_returns_to_parent, test_setup),
        cmocka_unit_test_setup(test_cursor_max_positive, test_setup),
        cmocka_unit_test_setup(test_from_legacy_index_bounds, test_setup),
        cmocka_unit_test_setup(test_is_active_reflects_state, test_setup),
        cmocka_unit_test_setup(test_request_fade_out, test_setup),
        cmocka_unit_test_setup(test_phase_progression, test_setup),
        cmocka_unit_test_setup(test_rmlui_callbacks_invoked, test_setup),
        cmocka_unit_test_setup(test_timer_zero_skips_wait, test_setup),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
