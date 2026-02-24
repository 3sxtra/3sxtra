#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <stdlib.h>
#include "port/config.h"

// Forward declarations for API that doesn't exist yet
void Config_SetInt(const char* key, int value);
void Config_SetBool(const char* key, bool value);
void Config_Save();

static int setup(void **state) {
    (void) state;
#ifdef _WIN32
    system("mkdir test_config_dir > NUL 2>&1");
#else
    system("mkdir -p test_config_dir");
#endif
    return 0;
}

static int teardown(void **state) {
    (void) state;
    // system("rm -rf test_config_dir"); // Dangerous if not careful
    return 0;
}

static void test_config_workflow(void **state) {
    (void) state;
    
    Config_Init();
    
    Config_SetInt("test_int", 42);
    Config_SetBool("test_bool", true);
    
    assert_int_equal(Config_GetInt("test_int"), 42);
    assert_true(Config_GetBool("test_bool"));
    
    Config_Save();
    Config_Destroy();
    
    // Reload
    Config_Init();
    assert_int_equal(Config_GetInt("test_int"), 42);
    assert_true(Config_GetBool("test_bool"));
    Config_Destroy();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_config_workflow, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
