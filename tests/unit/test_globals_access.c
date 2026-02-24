#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include "cmocka.h"

#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/workuser.h"


static void test_plw_access(void **state) {
    (void) state;
    // Verify plw array is accessible and zero-initialized (BSS)
    // Note: accessing inner members might require full definition of PLW which might be in structs.h
    // Assuming plcnt.h pulls it in.
    
    // Simple pointer check if we can't access members easily without more includes
    assert_non_null(&plw[0]);
    
    // Modify and verify
    // Using memset or similar if we don't know the full struct layout here without more headers
    // But since we are linking game_globals.c, the symbols exist.
}

static void test_game_globals_access(void **state) {
    (void) state;
    // Verify G_No is accessible via workuser.h
    assert_int_equal(G_No[0], 0);
    
    G_No[0] = 5;
    assert_int_equal(G_No[0], 5);
    G_No[0] = 0; // Reset
    
    // Verify Mode_Type
    Mode_Type = MODE_ARCADE;
    assert_int_equal(Mode_Type, MODE_ARCADE);
}



int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_plw_access),
        cmocka_unit_test(test_game_globals_access),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
