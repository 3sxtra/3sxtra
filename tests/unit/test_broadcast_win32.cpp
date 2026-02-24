#ifdef __cplusplus
extern "C" {
#endif
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#ifdef __cplusplus
}
#endif
#include "port/broadcast.h"

#ifdef __cplusplus
extern "C" {
#endif

extern BroadcastPort g_broadcast_port_win32;

#ifdef __cplusplus
}
#endif

static void test_win32_broadcast_init(void **state) {
    (void) state;
    bool success = g_broadcast_port_win32.Init("TestSender");
    assert_true(success);
    g_broadcast_port_win32.Shutdown();
}

static void test_win32_broadcast_send_texture(void **state) {
    (void) state;
    g_broadcast_port_win32.Init("TestSenderTexture");
    
    // We don't have a real GL context here in unit tests usually, 
    // but let's see if Spout's SendTexture handles it gracefully or crashes.
    // In a headless environment, it might fail to initialize GL extensions.
    // Spout 2.007+ is quite robust.
    
    // Use a dummy texture ID
    bool success = g_broadcast_port_win32.SendTexture(1, 640, 480, false);
    // It might return false if GL is not initialized, which is acceptable for a unit test
    // that just wants to ensure no crash.
    (void)success;
    
    g_broadcast_port_win32.Shutdown();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_win32_broadcast_init),
        cmocka_unit_test(test_win32_broadcast_send_texture),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
