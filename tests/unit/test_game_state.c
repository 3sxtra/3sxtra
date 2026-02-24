#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include "cmocka.h"

#include "game_state.h"



/**
 * Test: Save/Load round-trip preserves all data
 */
static void test_save_load_roundtrip(void **state) {
    (void) state;

    // Setup: Initialize globals with known values
    Round_num = 3;
    Winner_id = 1;
    Game_timer = 9999;
    My_char[0] = 5;
    My_char[1] = 10;
    Score[0][0] = 123456;
    Score[1][0] = 654321;

    // Save the state
    GameState buffer;
    GameState_Save(&buffer);

    // Modify the globals
    Round_num = 99;
    Winner_id = -1;
    Game_timer = 0;
    My_char[0] = 0;
    My_char[1] = 0;
    Score[0][0] = 0;
    Score[1][0] = 0;

    // Load the saved state
    GameState_Load(&buffer);

    // Verify restoration
    assert_int_equal(Round_num, 3);
    assert_int_equal(Winner_id, 1);
    assert_int_equal(Game_timer, 9999);
    assert_int_equal(My_char[0], 5);
    assert_int_equal(My_char[1], 10);
    assert_int_equal(Score[0][0], 123456);
    assert_int_equal(Score[1][0], 654321);
}

/**
 * Test: Save with NULL pointer should not crash
 */
static void test_save_null_safety(void **state) {
    (void) state;

    // This should not crash
    GameState_Save(NULL);
}

/**
 * Test: Load with NULL pointer should not crash
 */
static void test_load_null_safety(void **state) {
    (void) state;

    // Setup a known state
    Round_num = 42;

    // This should not crash and should not modify globals
    GameState_Load(NULL);

    // Verify state unchanged
    assert_int_equal(Round_num, 42);
}

/**
 * Test: Verify GameState struct size is reasonable
 * This helps catch accidental struct changes that could break netplay
 */
static void test_gamestate_size(void **state) {
    (void) state;

    // GameState should be large (500+ fields) but not excessively so
    // Current size is around 5-10KB based on the struct definition
    size_t size = sizeof(GameState);
    
    assert_true(size > 1024);   // At least 1KB
    assert_true(size < 65536);  // Less than 64KB
    
    // Log the actual size for documentation
    print_message("GameState size: %zu bytes\n", size);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_save_load_roundtrip),
        cmocka_unit_test(test_save_null_safety),
        cmocka_unit_test(test_load_null_safety),
        cmocka_unit_test(test_gamestate_size),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
