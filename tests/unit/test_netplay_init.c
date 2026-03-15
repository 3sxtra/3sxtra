#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "cmocka.h"

#include "netplay/netplay.h"
#include "game_state.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/debug/debug_config.h"
#include <SDL3/SDL.h>

// Mocks for functions called by setup_vs_mode that are missing in mocks_netplay.c
void Setup_Default_Game_Option(void) {}
void Setup_ID(void) {}
void Copy_Save_w(void) {}

// Global debug state mock
DebugConfig debug_config = {0};

// Global application state mock
MPP mpp_w = {0};

// test_netplay_init.c
extern GameState g_GameState;

// Test case to verify "states/" directory creation
void test_states_directory_creation(void **state) {
    (void) state;

    // Ensure the directory does not exist initially
#ifdef _WIN32
    system("rmdir /s /q states >nul 2>&1");
#else
    system("rm -rf states");
#endif

    struct stat st = {0};
    assert_int_equal(stat("states", &st), -1);

    // We need Netplay_Run to see SESSION_TRANSITIONING.
    // Netplay_Begin sets it to SESSION_TRANSITIONING.
    Netplay_Begin();

    // Prepare state for Netplay_Run to enter configuration
    // We need game_ready_to_run_character_select() to return true.
    // In netplay.c: return g_GameState.gs_G_No[1] == 1;
    // NOTE: Netplay_Begin calls setup_vs_mode which sets G_No[1] = 12. 
    // We must overwrite it here to simulate the game reaching the character select screen.
    G_No[1] = 1;

    // Run one step of netplay loop
    Netplay_Run(); 

    // Verify directory exists
    // NOTE: This assertion is expected to FAIL until we implement the fix
    assert_int_equal(stat("states", &st), 0);
    assert_true(S_ISDIR(st.st_mode));
}

void test_deterministic_initialization(void **state) {
    (void) state;

    // First run
    Netplay_Begin();

    // Introduce noise (simulate previous state or random garbage)
    Random_ix16 = 999;
    Round_num = 5;
    Game_timer = 1234;

    // Second run
    Netplay_Begin();

    // Compare
    // This expects Netplay_Begin -> setup_vs_mode to fully reset critical fields.
    assert_int_equal(Random_ix16, 0);
    assert_int_equal(Round_num, 0);
    assert_int_equal(Game_timer, 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_states_directory_creation),
        cmocka_unit_test(test_deterministic_initialization),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}