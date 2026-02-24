#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <SDL3/SDL.h>
#include "port/sdl_bezel.h"

// We'll mock the texture size function in the test or expect it to be linked
// CalculateLayout calls imgui_wrapper_get_texture_size.
// Provided by mocks_imgui_wrapper.c

static void test_bezel_layout_basic(void **state) {
    (void) state;
    BezelSystem_Init();
    
    // Set mock textures
    BezelSystem_SetTextures((void*)0x1, (void*)0x2);
    
    int win_w = 1920;
    int win_h = 1080;
    
    // 4:3 game window in 1920x1080 centered
    // 1080 * (4/3) = 1440
    // x = (1920 - 1440) / 2 = 240
    SDL_FRect game_rect = { 240.0f, 0.0f, 1440.0f, 1080.0f };
    
    SDL_FRect left_dst, right_dst;
    BezelSystem_CalculateLayout(win_w, win_h, &game_rect, &left_dst, &right_dst);
    
    // Left bezel: aspect 0.5, height 1080 -> width 540
    // x = 240 - 540 = -300 (offscreen if window is only 1920, but math is correct)
    // Wait, if game is 1440 wide, and window is 1920 wide, we have (1920-1440)/2 = 240 pixels on each side.
    // Left bezel should ideally fit in those 240 pixels if it was 240 wide.
    // But our mock is 100/200 = 0.5 aspect. 1080 * 0.5 = 540 wide.
    
    assert_float_equal(left_dst.h, 1080.0f, 0.001f);
    assert_float_equal(left_dst.w, 540.0f, 0.001f);
    assert_float_equal(left_dst.x, 240.0f - 540.0f, 0.001f);
    
    // Right bezel: aspect 0.75, height 1080 -> width 810
    assert_float_equal(right_dst.h, 1080.0f, 0.001f);
    assert_float_equal(right_dst.w, 810.0f, 0.001f);
    assert_float_equal(right_dst.x, 240.0f + 1440.0f, 0.001f);
}

static void test_bezel_layout_4k(void **state) {
    (void) state;
    BezelSystem_Init();
    BezelSystem_SetTextures((void*)0x1, (void*)0x2);
    
    int win_w = 3840;
    int win_h = 2160;
    
    // 4:3 game window in 3840x2160 centered
    // 2160 * (4/3) = 2880
    // x = (3840 - 2880) / 2 = 480
    SDL_FRect game_rect = { 480.0f, 0.0f, 2880.0f, 2160.0f };
    
    SDL_FRect left_dst, right_dst;
    BezelSystem_CalculateLayout(win_w, win_h, &game_rect, &left_dst, &right_dst);
    
    // Left bezel: aspect 0.5, height 2160 -> width 1080
    assert_float_equal(left_dst.h, 2160.0f, 0.001f);
    assert_float_equal(left_dst.w, 1080.0f, 0.001f);
    assert_float_equal(left_dst.x, 480.0f - 1080.0f, 0.001f);
    
    // Right bezel: aspect 0.75, height 2160 -> width 1620
    assert_float_equal(right_dst.h, 2160.0f, 0.001f);
    assert_float_equal(right_dst.w, 1620.0f, 0.001f);
    assert_float_equal(right_dst.x, 480.0f + 2880.0f, 0.001f);
}

static void test_bezel_layout_null_textures(void **state) {
    (void) state;
    BezelSystem_Init();
    BezelSystem_SetTextures(NULL, NULL);
    
    SDL_FRect game_rect = { 240.0f, 0.0f, 1440.0f, 1080.0f };
    SDL_FRect left_dst, right_dst;
    BezelSystem_CalculateLayout(1920, 1080, &game_rect, &left_dst, &right_dst);
    
    assert_float_equal(left_dst.w, 0.0f, 0.001f);
    assert_float_equal(right_dst.w, 0.0f, 0.001f);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bezel_layout_basic),
        cmocka_unit_test(test_bezel_layout_4k),
        cmocka_unit_test(test_bezel_layout_null_textures),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}