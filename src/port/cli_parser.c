/**
 * @file cli_parser.c
 * @brief Command-line argument parser for the 3SX application.
 *
 * Handles CLI flags for resolution scaling, broadcast enable,
 * window geometry overrides, and shared-memory suffix.
 */
#include "port/broadcast.h"
#include "port/config.h"
#include "port/sdl/sdl_app.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock or include needed headers
#include "types.h"

extern BroadcastConfig broadcast_config;
extern int g_resolution_scale;
extern const char* g_shm_suffix;
extern float g_master_volume;

// Font test mode — boots into a debug font visualization screen
bool g_font_test_mode = false;

// Netplay game port (default 50000). Set via --port to allow multiple local instances.
unsigned short g_netplay_port = 50000;

// These might need to be mocked in tests
// void SDLApp_SetWindowPosition(int x, int y);
// void SDLApp_SetWindowSize(int w, int h);
int SDL_atoi(const char* str);

/**
 * @brief Parse command-line arguments and configure application state.
 *
 * Supports: --scale, --volume, --renderer, --enable-broadcast,
 * --window-pos, --window-size, --shm-suffix, --port.
 */
void ParseCLI(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [OPTIONS]\n\n", argv[0]);
            printf("Options:\n");
            printf("  --scale <factor>          Internal resolution multiplier (default: 1)\n");
            printf("  --volume <0-100>          Master volume percentage (default: 100)\n");
            printf("  --renderer <gl|gpu|sdl>   Renderer backend (default: gl)\n");
            printf("  --port <number>           Netplay game port (default: 50000)\n");
            printf("  --window-pos <x>,<y>      Initial window position\n");
            printf("  --window-size <w>x<h>     Initial window size\n");
            printf("  --enable-broadcast        Enable Spout/shared-memory broadcast\n");
            printf("  --shm-suffix <suffix>     Shared-memory name suffix for broadcast\n");
            printf("  --font-test               Boot into font debug visualization screen\n");
            printf("  --ui <imgui|rmlui>        UI toolkit for overlay menus (default: imgui)\n");
            printf("  --help                    Show this help message\n");
            exit(0);
        } else if (strcmp(argv[i], "--volume") == 0 && i + 1 < argc) {
            int vol = SDL_atoi(argv[++i]);
            if (vol < 0)
                vol = 0;
            if (vol > 100)
                vol = 100;
            g_master_volume = vol / 100.0f;
            printf("[CLI] Master volume: %d%%\n", vol);
        } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            g_resolution_scale = SDL_atoi(argv[++i]);
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            int p = SDL_atoi(argv[++i]);
            if (p > 0 && p <= 65535) {
                g_netplay_port = (unsigned short)p;
                printf("[CLI] Netplay port: %d\n", p);
            }
        } else if (strcmp(argv[i], "--enable-broadcast") == 0) {
            broadcast_config.enabled = true;
        } else if (strcmp(argv[i], "--window-pos") == 0 && i + 1 < argc) {
            int x, y;
            if (sscanf(argv[++i], "%d,%d", &x, &y) == 2) {
                SDLApp_SetWindowPosition(x, y);
            }
        } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
            int w, h;
            if (sscanf(argv[++i], "%dx%d", &w, &h) == 2) {
                SDLApp_SetWindowSize(w, h);
            }
        } else if (strcmp(argv[i], "--shm-suffix") == 0 && i + 1 < argc) {
            g_shm_suffix = argv[++i];
        } else if (strcmp(argv[i], "--renderer") == 0 && i + 1 < argc) {
            const char* backend = argv[++i];
            if (strcmp(backend, "gpu") == 0) {
                SDLApp_SetRenderer(RENDERER_SDLGPU);
            } else if (strcmp(backend, "sdl") == 0 || strcmp(backend, "sdl2d") == 0) {
                SDLApp_SetRenderer(RENDERER_SDL2D);
            } else {
                SDLApp_SetRenderer(RENDERER_OPENGL);
            }
        } else if (strcmp(argv[i], "--font-test") == 0) {
            g_font_test_mode = true;
        } else if (strcmp(argv[i], "--ui") == 0 && i + 1 < argc) {
            const char* mode = argv[++i];
            Config_SetString(CFG_KEY_UI_MODE, mode);
            printf("[CLI] UI mode: %s\n", mode);
        }
    }
}
