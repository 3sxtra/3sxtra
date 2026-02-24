#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include "port/broadcast.h"
#include "game_state.h"

// Note: config.h is in src/port/ so we might need special include path or use relative
#include "../../src/port/config.h"

static void test_broadcast_interface_exists(void **state) {
    (void) state;
    BroadcastConfig config;
    config.enabled = true;
    config.source = BROADCAST_SOURCE_NATIVE;
    config.show_ui = false;
    
    assert_true(config.enabled);
    assert_int_equal(config.source, BROADCAST_SOURCE_NATIVE);
    
    BroadcastPort port = {0};
    assert_null(port.Init); // Just checking it exists
}

static void test_game_state_has_broadcast_config(void **state) {
    (void) state;
    GameState gs;
    gs.broadcast_config.enabled = true;
    assert_true(gs.broadcast_config.enabled);
}

static void test_game_state_roundtrip(void **state) {
    (void) state;
    GameState src = {0};
    GameState dst = {0};
    
    src.broadcast_config.enabled = true;
    src.broadcast_config.source = BROADCAST_SOURCE_FINAL;
    src.broadcast_config.show_ui = true;
    
    // We need to provide the actual globals that GS_SAVE uses, or mock them.
    // Wait, GS_SAVE in game_state.c uses LOCAL variables, not globals!
    // Wait, let's look at GameState_Save again.
}

static void test_config_keys_exist(void **state) {
    (void) state;
    // These should be defined in src/port/config.h
    assert_string_equal(CFG_KEY_BROADCAST_ENABLED, "broadcast-enabled");
    assert_string_equal(CFG_KEY_BROADCAST_SOURCE, "broadcast-source");
    assert_string_equal(CFG_KEY_BROADCAST_SHOW_UI, "broadcast-show-ui");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_broadcast_interface_exists),
        cmocka_unit_test(test_game_state_has_broadcast_config),
        cmocka_unit_test(test_config_keys_exist),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
