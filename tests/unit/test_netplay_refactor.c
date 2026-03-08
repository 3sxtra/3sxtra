#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include "cmocka.h"

// calculate_sectioned_checksums was removed from the codebase, so these tests are no longer applicable.

static void test_disabled(void** state) {
    (void)state;
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_disabled),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
