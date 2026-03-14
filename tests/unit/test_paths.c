#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include "port/config/paths.h"

static void test_paths_get_pref_path(void **state) {
    (void) state;
    const char* path = Paths_GetPrefPath();
    assert_non_null(path);
}

static void test_paths_get_base_path(void **state) {
    (void) state;
    const char* path = Paths_GetBasePath();
    assert_non_null(path);
}

/* Task 5: Paths_IsPortable edge cases */

/* In the test environment there is no config/ folder next to the test
   executable, so portable mode should NOT be detected. */
static void test_is_portable_without_marker(void **state) {
    (void) state;
    int result = Paths_IsPortable();
    assert_int_equal(result, 0);
}

/* The return value must always be exactly 0 or 1 (not -1 or other). */
static void test_is_portable_valid_range(void **state) {
    (void) state;
    int result = Paths_IsPortable();
    assert_true(result == 0 || result == 1);
}

/* Calling GetPrefPath twice returns the same stable, non-NULL pointer
   even when no config/ folder exists beside the test binary. */
static void test_pref_path_fallback_stable(void **state) {
    (void) state;
    const char* path1 = Paths_GetPrefPath();
    assert_non_null(path1);
    const char* path2 = Paths_GetPrefPath();
    assert_non_null(path2);
    /* Cached — should return the same pointer */
    assert_ptr_equal(path1, path2);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_paths_get_pref_path),
        cmocka_unit_test(test_paths_get_base_path),
        /* Task 5 additions */
        cmocka_unit_test(test_is_portable_without_marker),
        cmocka_unit_test(test_is_portable_valid_range),
        cmocka_unit_test(test_pref_path_fallback_stable),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
