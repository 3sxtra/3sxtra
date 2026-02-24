#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include "cmocka.h"

#include "game_state.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/workuser.h"

// Prototypes come from game_state.h included above

// Mock objects for files not linked but used in headers
// (If any are needed by headers included)

static void test_roundtrip_basic(void **state) {
    (void) state;
    GameState buffer;
    memset(&buffer, 0, sizeof(GameState));
    
    // Set some global values
    G_No[0] = 1;
    G_No[1] = 2;
    G_No[2] = 3;
    G_No[3] = 4;
    Mode_Type = MODE_VERSUS;
    
    // Save to buffer
    GameState_Save(&buffer);
    
    // Verify buffer contains values
    assert_int_equal(buffer.G_No[0], 1);
    assert_int_equal(buffer.G_No[1], 2);
    assert_int_equal(buffer.G_No[2], 3);
    assert_int_equal(buffer.G_No[3], 4);
    assert_int_equal(buffer.Mode_Type, MODE_VERSUS);
    
    // Change globals
    G_No[0] = 0;
    Mode_Type = MODE_ARCADE;
    
    // Load from buffer
    GameState_Load(&buffer);
    
    // Verify globals restored
    assert_int_equal(G_No[0], 1);
    assert_int_equal(Mode_Type, MODE_VERSUS);
}

static void test_roundtrip_complex(void **state) {
    (void) state;
    GameState buffer;
    memset(&buffer, 0, sizeof(GameState));
    
    // Set some complex global values (plw)
    plw[0].wu.position_x = 12345;
    plw[1].wu.id = 1;
    
    // Save to buffer
    GameState_Save(&buffer);
    
    // Verify buffer
    assert_int_equal(buffer.plw[0].wu.position_x, 12345);
    assert_int_equal(buffer.plw[1].wu.id, 1);
    
    // Change globals
    plw[0].wu.position_x = 0;
    
    // Load
    GameState_Load(&buffer);
    
    // Verify restored
    assert_int_equal(plw[0].wu.position_x, 12345);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_roundtrip_basic),
        cmocka_unit_test(test_roundtrip_complex),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
