#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <stdlib.h>
#include "port/config/config.h"

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

/* Config_Init should handle missing config file gracefully — it creates
   defaults and continues. After init, the default 'fullscreen' value is true. */
static void test_config_init_missing_file_fallback(void **state) {
    (void) state;

    /* Config_Init() reads from Paths_GetPrefPath(), which our mock
       returns a test directory. If no config file exists there,
       it should dump defaults and continue without crashing. */
    Config_Init();

    /* The default for fullscreen is 'true' per default_entries */
    assert_true(Config_GetBool(CFG_KEY_FULLSCREEN));

    /* Default window dimensions */
    assert_int_equal(Config_GetInt(CFG_KEY_WINDOW_WIDTH), 640);
    assert_int_equal(Config_GetInt(CFG_KEY_WINDOW_HEIGHT), 480);

    Config_Destroy();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_config_workflow, setup, teardown),
        cmocka_unit_test(test_config_init_missing_file_fallback),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
