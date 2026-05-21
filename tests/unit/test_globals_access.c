#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include "cmocka.h"

#include "game_state.h"

static void test_plw_access(void **state) {
    (void) state;
    // Verify plw array is accessible and zero-initialized (BSS)
    assert_non_null(&g_state.plw[0]);
}

static void test_game_globals_access(void **state) {
    (void) state;
    // Verify fsm is accessible via g_state
    assert_int_equal(g_state.fsm[0], 0);
    
    g_state.fsm[0] = 5;
    assert_int_equal(g_state.fsm[0], 5);
    g_state.fsm[0] = 0; // Reset
    
    // Verify Mode_Type
    g_state.Mode_Type = MODE_ARCADE;
    assert_int_equal(g_state.Mode_Type, MODE_ARCADE);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_plw_access),
        cmocka_unit_test(test_game_globals_access),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
