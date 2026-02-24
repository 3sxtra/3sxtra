#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include "port/sdl/sdl_netplay_ui.h"
#include "netplay/netplay.h" // For event types
#include <SDL3/SDL.h>
#include <string.h>

// Mock control
void MockNetplay_SetStats(int delay, int ping, int rollback);
void MockNetplay_PushEvent(NetplayEventType type);

// Test Helper control
void TestHelper_CreateImGuiContext();
void TestHelper_SetDeltaTime(float dt);
void TestHelper_NewFrame();
void TestHelper_EndFrame();
void TestHelper_DestroyImGuiContext();

static int setup_imgui(void **state) {
    (void) state;
    TestHelper_CreateImGuiContext();
    SDLNetplayUI_Init();
    return 0;
}

static int teardown_imgui(void **state) {
    (void) state;
    SDLNetplayUI_Shutdown();
    TestHelper_DestroyImGuiContext();
    return 0;
}

static void test_netplay_ui_init(void **state) {
    (void) state; /* unused */
    
    // Just verify lifecycle calls don't crash with context active
    TestHelper_NewFrame();
    SDLNetplayUI_Render();
    TestHelper_EndFrame();
}

static void test_hud_text_formatting(void **state) {
    (void) state; /* unused */
    char buffer[128];
    
    // Set mock data
    MockNetplay_SetStats(0, 120, 3);
    
    // Get text
    SDLNetplayUI_GetHUDText(buffer, sizeof(buffer));
    
    // Check format: "R:3 P:120"
    assert_string_equal(buffer, "R:3 P:120");
}

static void test_toast_notifications(void **state) {
    (void) state;
    
    // Setup: Ensure no toasts initially
    assert_int_equal(SDLNetplayUI_GetActiveToastCount(), 0);
    
    // Push event
    MockNetplay_PushEvent(NETPLAY_EVENT_CONNECTED);
    
    // Render frame (should poll event and create toast)
    TestHelper_NewFrame();
    SDLNetplayUI_Render();
    TestHelper_EndFrame();
    
    // Verify toast active
    assert_int_equal(SDLNetplayUI_GetActiveToastCount(), 1);
    
    // Simulate time passing (5.0f seconds). Toast should dismiss.
    TestHelper_SetDeltaTime(5.0f);
    TestHelper_NewFrame();
    SDLNetplayUI_Render();
    TestHelper_EndFrame();
    
    // Verify toast gone
    assert_int_equal(SDLNetplayUI_GetActiveToastCount(), 0);
}

static void test_hud_visibility(void **state) {
    (void) state;
    
    SDLNetplayUI_SetHUDVisible(true);
    assert_true(SDLNetplayUI_IsHUDVisible());
    
    SDLNetplayUI_SetHUDVisible(false);
    assert_false(SDLNetplayUI_IsHUDVisible());
    
    SDLNetplayUI_SetHUDVisible(true);
    assert_true(SDLNetplayUI_IsHUDVisible());
}

static void test_diagnostics_history(void **state) {
    (void) state;
    
    float ping_hist[128];
    float rb_hist[128];
    int count;
    
    // Set metric 1
    MockNetplay_SetStats(0, 50, 1);
    TestHelper_NewFrame();
    SDLNetplayUI_Render();
    TestHelper_EndFrame();
    
    // Set metric 2
    MockNetplay_SetStats(0, 100, 3);
    TestHelper_NewFrame();
    SDLNetplayUI_Render();
    TestHelper_EndFrame();
    
    SDLNetplayUI_GetHistory(ping_hist, rb_hist, &count);
    
    assert_int_equal(count, 2);
    assert_float_equal(ping_hist[0], 50.0f, 0.001f);
    assert_float_equal(ping_hist[1], 100.0f, 0.001f);
    assert_float_equal(rb_hist[0], 1.0f, 0.001f);
    assert_float_equal(rb_hist[1], 3.0f, 0.001f);
}

static void test_hotkey_toggle(void **state) {
    (void) state;
    
    SDLNetplayUI_SetDiagnosticsVisible(false);
    
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_F10;
    event.key.down = true;
    event.key.repeat = false;
    
    SDLNetplayUI_ProcessEvent(&event);
    assert_true(SDLNetplayUI_IsDiagnosticsVisible());
    
    SDLNetplayUI_ProcessEvent(&event);
    assert_false(SDLNetplayUI_IsDiagnosticsVisible());
}

static void test_extreme_conditions(void **state) {
    (void) state;
    
    // Set extreme mock data
    MockNetplay_SetStats(0, 2500, 15);
    
    char buffer[128];
    SDLNetplayUI_GetHUDText(buffer, sizeof(buffer));
    
    // Ensure it doesn't crash and contains the values
    assert_non_null(strstr(buffer, "2500"));
    assert_non_null(strstr(buffer, "15"));
    
    // Verify diagnostics render with extreme values doesn't crash
    SDLNetplayUI_SetDiagnosticsVisible(true);
    TestHelper_NewFrame();
    SDLNetplayUI_Render();
    TestHelper_EndFrame();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_netplay_ui_init, setup_imgui, teardown_imgui),
        cmocka_unit_test_setup_teardown(test_hud_text_formatting, setup_imgui, teardown_imgui),
        cmocka_unit_test_setup_teardown(test_toast_notifications, setup_imgui, teardown_imgui),
        cmocka_unit_test_setup_teardown(test_hud_visibility, setup_imgui, teardown_imgui),
        cmocka_unit_test_setup_teardown(test_diagnostics_history, setup_imgui, teardown_imgui),
        cmocka_unit_test_setup_teardown(test_hotkey_toggle, setup_imgui, teardown_imgui),
        cmocka_unit_test_setup_teardown(test_extreme_conditions, setup_imgui, teardown_imgui),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}