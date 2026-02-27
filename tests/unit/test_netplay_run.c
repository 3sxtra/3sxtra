#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include "cmocka.h"

#include "netplay/netplay.h"
#include "game_state.h"
#include "sf33rd/Source/Game/Game.h"
#include "netplay/discovery.h"

// Globals used by netplay.c
extern GameState g_GameState;
unsigned short g_netplay_port = 50000;

// Need to reset session state for testing. Since Netplay_GetSessionState is available, we can read it.
// We can manipulate it by calling HandleMenuExit to go to EXITING, then Run to IDLE.
static void reset_netplay_state() {
    while (Netplay_GetSessionState() != NETPLAY_SESSION_IDLE) {
        Netplay_HandleMenuExit();
        Netplay_Run();
    }
}

static void test_netplay_run_idle(void **state) {
    (void) state;
    
    reset_netplay_state();
    assert_int_equal(Netplay_GetSessionState(), NETPLAY_SESSION_IDLE);
    
    // In IDLE, Netplay_Run does nothing and stays in IDLE.
    Netplay_Run();
    assert_int_equal(Netplay_GetSessionState(), NETPLAY_SESSION_IDLE);
}

static void test_netplay_run_lobby(void **state) {
    (void) state;
    
    reset_netplay_state();
    
    // Enter lobby
    Netplay_EnterLobby();
    assert_int_equal(Netplay_GetSessionState(), NETPLAY_SESSION_LOBBY);
    
    // In LOBBY, Netplay_Run updates discovery. We won't simulate a full connection here,
    // just ensure it stays in LOBBY if no peer connects.
    Netplay_Run();
    assert_int_equal(Netplay_GetSessionState(), NETPLAY_SESSION_LOBBY);
}

static void test_netplay_run_transitioning(void **state) {
    (void) state;
    
    reset_netplay_state();
    
    // Begin session, moves to TRANSITIONING
    Netplay_Begin();
    assert_int_equal(Netplay_GetSessionState(), NETPLAY_SESSION_TRANSITIONING);
    
    // Simulate game not ready
    g_GameState.gs_G_No[1] = 0;
    Netplay_Run();
    assert_int_equal(Netplay_GetSessionState(), NETPLAY_SESSION_TRANSITIONING);
    
    // Simulate game ready (G_No[1] == 1)
    g_GameState.gs_G_No[1] = 1;
    Netplay_Run(); // Transition_ready_frames = 1
    assert_int_equal(Netplay_GetSessionState(), NETPLAY_SESSION_TRANSITIONING);
    
    Netplay_Run(); // Transition_ready_frames = 2, moves to CONNECTING
    assert_int_equal(Netplay_GetSessionState(), NETPLAY_SESSION_CONNECTING);
}

static void test_netplay_run_exiting(void **state) {
    (void) state;
    
    reset_netplay_state();
    
    // Move to some state
    Netplay_Begin();
    
    // Trigger exit
    Netplay_HandleMenuExit();
    assert_int_equal(Netplay_GetSessionState(), NETPLAY_SESSION_EXITING);
    
    // Run will clean up and go to IDLE
    Netplay_Run();
    assert_int_equal(Netplay_GetSessionState(), NETPLAY_SESSION_IDLE);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_netplay_run_idle),
        cmocka_unit_test(test_netplay_run_lobby),
        cmocka_unit_test(test_netplay_run_transitioning),
        cmocka_unit_test(test_netplay_run_exiting),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
