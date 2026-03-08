#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "cmocka.h"

// Netplay_DiffState was removed from the codebase, so these tests are no longer applicable.

static void test_disabled(void** state) {
    (void)state;
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_disabled),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
