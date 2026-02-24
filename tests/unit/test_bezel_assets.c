#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <string.h>
#include <glad/gl.h>
#include "port/sdl_bezel.h"

// Stub GL functions
void stub_glBindTexture(GLenum target, GLuint texture) { (void)target; (void)texture; }
void stub_glTexParameteri(GLenum target, GLenum pname, GLint param) { (void)target; (void)pname; (void)param; }

static int setup_gl_stubs(void **state) {
    (void) state;
    // Initialize glad function pointers to stubs
    glad_glBindTexture = (PFNGLBINDTEXTUREPROC)stub_glBindTexture;
    glad_glTexParameteri = (PFNGLTEXPARAMETERIPROC)stub_glTexParameteri;
    return 0;
}

static void test_bezel_init(void **state) {
    (void) state;
    BezelSystem_Init();
    BezelSystem_Shutdown();
}

static void test_bezel_get_common_paths(void **state) {
    (void) state;
    char left[512] = {0};
    char right[512] = {0};
    bool res = BezelSystem_GetDefaultPaths(left, right, sizeof(left));
    assert_true(res);
    assert_non_null(strstr(left, "bezel_common_left"));
    assert_non_null(strstr(right, "bezel_common_right"));
}

static void test_bezel_textures_initially_null(void **state) {
    (void) state;
    BezelSystem_Init();
    BezelTextures tex;
    BezelSystem_GetTextures(&tex);
    assert_null(tex.left);
    assert_null(tex.right);
}

static void test_bezel_load_success(void **state) {
    (void) state;
    BezelSystem_Init();
    bool res = BezelSystem_LoadTextures();
    assert_true(res);
    
    BezelTextures tex;
    BezelSystem_GetTextures(&tex);
    assert_ptr_equal(tex.left, (void*)0x1234);
    assert_ptr_equal(tex.right, (void*)0x1234);
}

static void test_bezel_character_switch(void **state) {
    (void) state;
    BezelSystem_Init();
    // Test switch to Ryu (index 2)
    BezelSystem_SetCharacters(2, 2);
    
    BezelTextures tex;
    BezelSystem_GetTextures(&tex);
    assert_ptr_equal(tex.left, (void*)0x1234);
    assert_ptr_equal(tex.right, (void*)0x1234);
}

static void test_bezel_visibility_toggle(void **state) {
    (void) state;
    BezelSystem_Init();
    assert_true(BezelSystem_IsVisible());
    BezelSystem_SetVisible(false);
    assert_false(BezelSystem_IsVisible());
}

static void test_bezel_mapping_correctness(void **state) {
    (void) state;
    // 0: Gill -> fallback to "common"
    assert_string_equal(BezelSystem_GetCharacterAssetPrefix(0), "common");
    // 1: Alex
    assert_string_equal(BezelSystem_GetCharacterAssetPrefix(1), "alex");
    // 2: Ryu
    assert_string_equal(BezelSystem_GetCharacterAssetPrefix(2), "ryu");
    // 11: Ken
    assert_string_equal(BezelSystem_GetCharacterAssetPrefix(11), "ken");
    // 14: Akuma
    assert_string_equal(BezelSystem_GetCharacterAssetPrefix(14), "akuma");
    // 15: Chun-Li
    assert_string_equal(BezelSystem_GetCharacterAssetPrefix(15), "chunli");
    // 19: Remy
    assert_string_equal(BezelSystem_GetCharacterAssetPrefix(19), "remy");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bezel_init),
        cmocka_unit_test(test_bezel_get_common_paths),
        cmocka_unit_test(test_bezel_textures_initially_null),
        cmocka_unit_test_setup_teardown(test_bezel_load_success, setup_gl_stubs, NULL),
        cmocka_unit_test_setup_teardown(test_bezel_character_switch, setup_gl_stubs, NULL),
        cmocka_unit_test(test_bezel_visibility_toggle),
        cmocka_unit_test(test_bezel_mapping_correctness),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}