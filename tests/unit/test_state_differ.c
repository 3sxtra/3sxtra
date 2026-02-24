#include "cmocka.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


#include "game_state.h"
#include "netplay/netplay.h"
#include "sf33rd/Source/Game/effect/effect.h"


/**
 * Test that identical states produce no diff output.
 * Verifies Netplay_DiffState doesn't crash on equal states.
 */
static void test_diff_identical_states(void** state) {
    (void)state;
    State s1, s2;
    memset(&s1, 0, sizeof(State));
    memset(&s2, 0, sizeof(State));

    // Should not crash or trigger any warnings
    Netplay_DiffState(&s1, &s2);
}

/**
 * Test detection of Round_num difference.
 */
static void test_diff_round_num(void** state) {
    (void)state;
    State s1, s2;
    memset(&s1, 0, sizeof(State));
    memset(&s2, 0, sizeof(State));

    s2.gs.gs_Round_num = 1;

    // Should detect the round number difference
    Netplay_DiffState(&s1, &s2);
}

/**
 * Test detection of RNG index differences.
 * RNG desync is a common cause of netplay issues.
 */
static void test_diff_rng_indices(void** state) {
    (void)state;
    State s1, s2;
    memset(&s1, 0, sizeof(State));
    memset(&s2, 0, sizeof(State));

    // Test all RNG indices that are synchronized
    s2.gs.gs_Random_ix16 = 100;
    Netplay_DiffState(&s1, &s2);

    memset(&s2, 0, sizeof(State));
    s2.gs.gs_Random_ix32 = 50;
    Netplay_DiffState(&s1, &s2);

    memset(&s2, 0, sizeof(State));
    s2.gs.gs_Random_ix16_com = 25;
    Netplay_DiffState(&s1, &s2);

    memset(&s2, 0, sizeof(State));
    s2.gs.gs_Random_ix16_bg = 10;
    Netplay_DiffState(&s1, &s2);
}

/**
 * Test detection of Game_timer difference.
 */
static void test_diff_game_timer(void** state) {
    (void)state;
    State s1, s2;
    memset(&s1, 0, sizeof(State));
    memset(&s2, 0, sizeof(State));

    s1.gs.gs_Game_timer = 1800;
    s2.gs.gs_Game_timer = 1801;

    Netplay_DiffState(&s1, &s2);
}

/**
 * Test detection of PLW (Player Work) differences.
 */
static void test_diff_plw(void** state) {
    (void)state;
    State s1, s2;
    memset(&s1, 0, sizeof(State));
    memset(&s2, 0, sizeof(State));

    // Modify a field in player 1's work struct
    s2.gs.gs_plw[0].wu.x_pos = 100;

    Netplay_DiffState(&s1, &s2);
}

/**
 * Test detection of Waza Work differences.
 */
static void test_diff_waza_work(void** state) {
    (void)state;
    State s1, s2;
    memset(&s1, 0, sizeof(State));
    memset(&s2, 0, sizeof(State));

    // Modify waza work for player 1
    s2.gs.gs_waza_work[0][0].w_rno = 5;

    Netplay_DiffState(&s1, &s2);
}

/**
 * Test detection of EffectState differences.
 */
static void test_diff_effect_state(void** state) {
    (void)state;
    State s1, s2;
    memset(&s1, 0, sizeof(State));
    memset(&s2, 0, sizeof(State));

    // Modify effect state
    s2.es.frwctr = 10;

    Netplay_DiffState(&s1, &s2);
}

/**
 * Test detection of input buffer differences.
 */
static void test_diff_input_buffer(void** state) {
    (void)state;
    State s1, s2;
    memset(&s1, 0, sizeof(State));
    memset(&s2, 0, sizeof(State));

    s2.gs.gs_plsw_00[0] = 0x1234;

    Netplay_DiffState(&s1, &s2);
}

/**
 * Test detection of Time_Limit and Select_Timer differences.
 */
static void test_diff_config_options(void** state) {
    (void)state;
    State s1, s2;
    memset(&s1, 0, sizeof(State));
    memset(&s2, 0, sizeof(State));

    s2.gs.gs_Time_Limit = 99;
    s2.gs.gs_Select_Timer = 30;

    Netplay_DiffState(&s1, &s2);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_diff_identical_states),
        cmocka_unit_test(test_diff_round_num),
        cmocka_unit_test(test_diff_rng_indices),
        cmocka_unit_test(test_diff_game_timer),
        cmocka_unit_test(test_diff_plw),
        cmocka_unit_test(test_diff_waza_work),
        cmocka_unit_test(test_diff_effect_state),
        cmocka_unit_test(test_diff_input_buffer),
        cmocka_unit_test(test_diff_config_options),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
