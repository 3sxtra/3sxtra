#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "sf33rd/Source/Common/MemMan.h"

static void test_mmRoundUp(void **state) {
    (void) state;
    // Align 10 to 4 -> 12
    // Cast to int for assert_int_equal if uintptr_t is larger, or use assert_true
    assert_int_equal(mmRoundUp(4, 10), 12);
    assert_int_equal(mmRoundUp(4, 12), 12);
    assert_int_equal(mmRoundUp(4, 0), 0);
    assert_int_equal(mmRoundUp(16, 17), 32);
}

static void test_mmRoundOff(void **state) {
    (void) state;
    // Round 10 down to 4 -> 8
    assert_int_equal(mmRoundOff(4, 10), 8);
    assert_int_equal(mmRoundOff(4, 12), 12);
    assert_int_equal(mmRoundOff(16, 31), 16);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mmRoundUp),
        cmocka_unit_test(test_mmRoundOff),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
