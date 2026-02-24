#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

static void test_smoke_assertion(void **state) {
    (void) state; /* unused */
    assert_int_equal(1, 1);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_smoke_assertion),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
