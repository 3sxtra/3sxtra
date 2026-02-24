#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include "port/paths.h"

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

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_paths_get_pref_path),
        cmocka_unit_test(test_paths_get_base_path),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
