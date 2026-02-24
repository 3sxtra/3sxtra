#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include "netplay/netplay.h"

static void test_netplay_event_queue(void **state) {
    (void) state; /* unused */
    
    NetplayEvent event;
    
    // Initially empty
    assert_false(Netplay_PollEvent(&event));
    
    // We can't easily push events from here without exposing internal functions or mocking GekkoNet events.
    // But we can check the API exists.
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_netplay_event_queue),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
