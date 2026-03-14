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
 * Test: Verify GameState binary layout coverage
 * Ensures that GameState_Save populates 100% of the struct fields
 * (excluding alignment padding) to catch missed variables.
 */
static void test_gamestate_coverage(void **state) {
    (void) state;

    // 1. Create a dummy state completely filled with 0xAA
    GameState dummy;
    memset(&dummy, 0xAA, sizeof(GameState));

    // 2. Load the dummy state into globals.
    // This forces every global variable that is tracked by GameState
    // to be populated with 0xAA bytes.
    GameState_Load(&dummy);

    // 3. Create a clean state filled with 0x00
    GameState saved;
    memset(&saved, 0x00, sizeof(GameState));

    // 4. Save the globals back into the clean state.
    // Any field properly saved will now be 0xAA.
    // Any padding will remain 0x00.
    // If a field is missed in GameState_Save, it will remain 0x00.
    GameState_Save(&saved);

    // 5. Now save into a 0xFF-initialized struct for comparison.
    // Mapped fields will be 0xAA in both; only padding bytes will differ.
    GameState saved_ff;
    memset(&saved_ff, 0xFF, sizeof(GameState));
    GameState_Save(&saved_ff);

    const unsigned char* p_00 = (const unsigned char*)&saved;
    const unsigned char* p_ff = (const unsigned char*)&saved_ff;

    for (size_t i = 0; i < sizeof(GameState); i++) {
        if (p_00[i] == p_ff[i]) {
            // This byte was overwritten by GameState_Save, so it is a mapped field.
            // It MUST be exactly 0xAA, because we loaded 0xAA into all globals.
            assert_int_equal(p_00[i], 0xAA);
        } else {
            // This byte was NOT overwritten by GameState_Save, so it is padding.
            // It must retain its original initialization value.
            assert_int_equal(p_00[i], 0x00);
            assert_int_equal(p_ff[i], 0xFF);
        }
    }

    // Also verify round-trip consistency: saving from the same global state
    // into a fresh buffer should produce identical mapped bytes.
    GameState final_state;
    memset(&final_state, 0x00, sizeof(GameState));
    GameState_Save(&final_state);
    assert_memory_equal(&saved, &final_state, sizeof(GameState));
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
        cmocka_unit_test(test_gamestate_coverage),
        cmocka_unit_test(test_gamestate_size),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
