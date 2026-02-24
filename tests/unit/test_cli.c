#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "port/cli_parser.h"
#include "port/broadcast.h"
#include "port/sdl/sdl_app.h"

// Globals needed by ParseCLI
BroadcastConfig broadcast_config;
int g_resolution_scale = 1;
const char* g_shm_suffix = NULL;

// Mock state
static RendererBackend last_renderer_backend = RENDERER_OPENGL;

// Mocks
void SDLApp_SetWindowPosition(int x, int y) {
    (void)x; (void)y;
}
void SDLApp_SetWindowSize(int w, int h) {
    (void)w; (void)h;
}
void SDLApp_SetRenderer(RendererBackend backend) {
    last_renderer_backend = backend;
}
int SDL_atoi(const char* str) {
    return atoi(str);
}

static void test_cli_enable_broadcast(void **state) {
    (void) state;
    broadcast_config.enabled = false;
    
    char* argv[] = {"3sx", "--enable-broadcast"};
    int argc = 2;
    
    s32 player = 1;
    const char* ip = "127.0.0.1";
    bool netplay_mode = false;
    bool sync_test = false;
    
    ParseCLI(argc, argv, &player, &ip, &netplay_mode, &sync_test);
    
    assert_true(broadcast_config.enabled);
}

static void test_cli_sync_test(void **state) {
    (void) state;
    char* argv[] = {"3sx", "--sync-test"};
    int argc = 2;
    
    s32 player = 0;
    const char* ip = NULL;
    bool netplay_mode = false;
    bool sync_test = false;
    
    ParseCLI(argc, argv, &player, &ip, &netplay_mode, &sync_test);
    
    assert_int_equal(player, 1);
    assert_true(netplay_mode);
    assert_true(sync_test);
    assert_string_equal(ip, "127.0.0.1");
}

static void test_cli_renderer_gpu(void **state) {
    (void) state;
    last_renderer_backend = RENDERER_OPENGL;
    
    char* argv[] = {"3sx", "--renderer", "gpu"};
    int argc = 3;
    
    s32 player = 1;
    const char* ip = NULL;
    bool netplay_mode = false;
    bool sync_test = false;
    
    ParseCLI(argc, argv, &player, &ip, &netplay_mode, &sync_test);
    
    assert_int_equal(last_renderer_backend, RENDERER_SDLGPU);
}

static void test_cli_renderer_gl(void **state) {
    (void) state;
    last_renderer_backend = RENDERER_SDLGPU;
    
    char* argv[] = {"3sx", "--renderer", "gl"};
    int argc = 3;
    
    s32 player = 1;
    const char* ip = NULL;
    bool netplay_mode = false;
    bool sync_test = false;
    
    ParseCLI(argc, argv, &player, &ip, &netplay_mode, &sync_test);
    
    assert_int_equal(last_renderer_backend, RENDERER_OPENGL);
}

static void test_cli_renderer_sdl(void **state) {
    (void) state;
    last_renderer_backend = RENDERER_OPENGL;
    
    char* argv[] = {"3sx", "--renderer", "sdl"};
    int argc = 3;
    
    s32 player = 1;
    const char* ip = NULL;
    bool netplay_mode = false;
    bool sync_test = false;
    
    ParseCLI(argc, argv, &player, &ip, &netplay_mode, &sync_test);
    
    assert_int_equal(last_renderer_backend, RENDERER_SDL2D);
}

static void test_cli_renderer_sdl2d(void **state) {
    (void) state;
    last_renderer_backend = RENDERER_OPENGL;
    
    char* argv[] = {"3sx", "--renderer", "sdl2d"};
    int argc = 3;
    
    s32 player = 1;
    const char* ip = NULL;
    bool netplay_mode = false;
    bool sync_test = false;
    
    ParseCLI(argc, argv, &player, &ip, &netplay_mode, &sync_test);
    
    assert_int_equal(last_renderer_backend, RENDERER_SDL2D);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cli_enable_broadcast),
        cmocka_unit_test(test_cli_sync_test),
        cmocka_unit_test(test_cli_renderer_gpu),
        cmocka_unit_test(test_cli_renderer_gl),
        cmocka_unit_test(test_cli_renderer_sdl),
        cmocka_unit_test(test_cli_renderer_sdl2d),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
