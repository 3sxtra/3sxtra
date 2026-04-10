/**
 * @file ps3_stubs.c
 * @brief Stub implementations for all symbols required by the engine
 *        that are normally provided by the excluded SDL port layer.
 *
 * This file lets the PS3 build link without the SDL, config, sound,
 * netplay, paths, utils, and test modules.
 *
 * Every function here is a no-op or returns a safe default.
 * They should be replaced with real PS3 implementations over time.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/process.h>

#include "port/ps3/app/ps3_app.h"

#include "types.h"
#include "structs.h"
#include "port/sdl/renderer/sdl_game_renderer.h"
#include "port/sdl/input/sdl_pad.h"
#include "port/config/config.h"
#include "port/sound/adx.h"
#include "port/sound/spu.h"
#include "port/sound/emlShim.h"
#include "port/resources.h"
#include "port/config/paths.h"

/* ========================================================================= */
/*  SDLGameRenderer stubs                                                     */
/* ========================================================================= */

#include "renderer/ps3_renderer_gcm.h"
SDL_Texture* cps3_canvas = NULL;

void SDLGameRenderer_Init(SDL_Renderer* renderer)       { (void)renderer; CRS_Renderer_Init(); }
void SDLGameRenderer_BeginFrame(void)                   { CRS_Renderer_BeginFrame(); }
void SDLGameRenderer_RenderFrame(void)                  { CRS_Renderer_RenderFrame(); }
void SDLGameRenderer_EndFrame(void)                     { CRS_Renderer_EndFrame(); }

void SDLGameRenderer_CreateTexture(unsigned int th)     { CRS_Renderer_CreateTexture(th); }
void SDLGameRenderer_DestroyTexture(unsigned int th)    { CRS_Renderer_DestroyTexture(th); }
void SDLGameRenderer_UnlockTexture(unsigned int th)     { CRS_Renderer_UnlockTexture(th); }
void SDLGameRenderer_CreatePalette(unsigned int ph)     { CRS_Renderer_CreatePalette(ph); }
void SDLGameRenderer_DestroyPalette(unsigned int ph)    { CRS_Renderer_DestroyPalette(ph); }
void SDLGameRenderer_UnlockPalette(unsigned int ph)     { CRS_Renderer_UnlockPalette(ph); }
void SDLGameRenderer_SetTexture(unsigned int th)        { CRS_Renderer_SetTexture(th); }

void SDLGameRenderer_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    CRS_Renderer_DrawTexturedQuad(sprite, color);
}

void SDLGameRenderer_DrawSolidQuad(const Quad* vertices, unsigned int color) {
    CRS_Renderer_DrawSolidQuad(vertices, color);
}

void SDLGameRenderer_DrawSprite(const Sprite* sprite, unsigned int color) {
    CRS_Renderer_DrawSprite(sprite, color);
}

void SDLGameRenderer_DrawSprite2(const Sprite2* sprite2) {
    CRS_Renderer_DrawSprite2(sprite2);
}

/* ========================================================================= */
/*  SDLMessageRenderer stubs                                                  */
/* ========================================================================= */

SDL_Texture* message_canvas = NULL;

void SDLMessageRenderer_Initialize(SDL_Renderer* renderer)  { (void)renderer; }
void SDLMessageRenderer_BeginFrame(void)                    { }

void SDLMessageRenderer_CreateTexture(int width, int height, void* pixels, int format) {
    (void)width; (void)height; (void)pixels; (void)format;
}

void SDLMessageRenderer_DrawTexture(int x0, int y0, int x1, int y1,
                                     int u0, int v0, int u1, int v1,
                                     unsigned int color) {
    (void)x0; (void)y0; (void)x1; (void)y1;
    (void)u0; (void)v0; (void)u1; (void)v1;
    (void)color;
}

/* ========================================================================= */
/*  SDLDebugText stubs                                                        */
/* ========================================================================= */

void SDLDebugText_Initialize(SDL_Renderer* renderer)    { (void)renderer; }
void SDLDebugText_Render(void)                          { }
void SDLDebugText_Destroy(void)                         { }

// Removed Config, Keymap, Resources, and Paths stubs to let the native port system compile.
/* ========================================================================= */
/*  Utils                                                                     */
/* ========================================================================= */

void fatal_error(const char* fmt, ...) {
    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);
    fprintf(stderr, "\n=== FATAL ERROR ===\n");
    fprintf(stderr, "[fmt pointer: %p]\n", fmt);
    fprintf(stderr, "RAW FMT: ");
    fprintf(stderr, "%s", fmt);
    fprintf(stderr, "\nFORMATTED: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n===================\n");
    fflush(stderr);
    
    // Also write to file so we can read it easily outside of the emulator
    FILE* f = fopen("/dev_hdd0/game/3SX00001/USRDIR/fatal.log", "w");
    if (f) {
        fprintf(f, "=== FATAL ERROR ===\n");
        vfprintf(f, fmt, args_copy);
        fprintf(f, "\nRAW FMT: %s\n", fmt);
        fclose(f);
    }
    
    va_end(args_copy);
    va_end(args);
    // SYS-HIGH-01 Audit Fix: Clean shutdown before exit to prevent corrupted
    // audio DSP state and SPU kernel panics on real hardware.
    PS3App_Quit();
    sys_process_exit(1);
    while (1) {} // Suppress warning 1319: function declared with "noreturn" does return
}

void not_implemented(const char* func) {
    fatal_error("Not implemented: %s", func);
}

void debug_print(const char* fmt, ...) {
#if DEBUG
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
#else
    (void)fmt;
#endif
}

void stop_if(bool condition) {
    (void)condition;
}

/* ========================================================================= */
/*  Netplay stubs (for main.c references)                                     */
/* ========================================================================= */

/* Netplay is excluded via CMake's source filter, but if any of main.c's
 * netplay calls leak through via includes, these satisfy the linker. */

/* ========================================================================= */
/*  Arcade stubs (rom_load.c excluded — depends on minizip-ng)                */
/* ========================================================================= */

#include "arcade/rom_load.h"

void* Rom_Load(const char* path, size_t* size) {
    (void)path;
    if (size) *size = 0;
    return NULL;
}

/* ========================================================================= */
/*  zlib stubs (inflate decompression — no-op for now)                        */
/* ========================================================================= */
// zlib isn't compiled or linked for PS3 (no zlib.h in toolchain).
typedef void z_stream;
#define Z_OK 0
#define Z_STREAM_END 1

int inflateInit_(z_stream* strm, const char* version, int stream_size) {
    (void)strm; (void)version; (void)stream_size;
    printf("[PS3] inflateInit_ stub — FATAL: PPU-side zlib decompression is not implemented.\n");
    printf("[PS3] Ensure all memory buffers are 16-byte aligned to use the edgeZlib pipeline.\n");
    fatal_error("zlib fallback path reached (unaligned buffers)");
    return Z_OK;
}

int inflate(z_stream* strm, int flush) {
    (void)strm; (void)flush;
    return Z_STREAM_END;
}

int inflateEnd(z_stream* strm) {
    (void)strm;
    return Z_OK;
}

/* ========================================================================= */
/*  PS2 Legacy stubs (ps2PAD.c, mcsub.c, pulpul.c)                           */
/*  These are old PS2 SDK functions compiled as part of the engine but        */
/*  never actually called on PS3. Provide no-op stubs to satisfy linker.     */
/* ========================================================================= */

int scePad2GetState(int port, int slot)        { (void)port; (void)slot; return 0; }
int scePad2Read(int port, int slot, void* buf) { (void)port; (void)slot; (void)buf; return 0; }
int scePad2GetButtonProfile(int port, int slot, void* p) { (void)port; (void)slot; (void)p; return 0; }
int sceVibGetProfile(int port, int slot, void* p) { (void)port; (void)slot; (void)p; return 0; }
int sceVibSetActParam(int port, int slot, void* p) { (void)port; (void)slot; (void)p; return 0; }

int sceMcInit(void) { return 0; }
int sceMcSync(int mode, int* cmd, int* result) { (void)mode; (void)cmd; if(result) *result = 0; return 0; }
int sceMcGetDir(int port, int slot, const char* name, unsigned int mode, int max, void* table) {
    (void)port; (void)slot; (void)name; (void)mode; (void)max; (void)table; return 0;
}
int sceMcGetInfo(int port, int slot, int* type, int* free_c, int* format) {
    (void)port; (void)slot; if(type) *type = 0; if(free_c) *free_c = 0; if(format) *format = 0; return 0;
}
int sceMcOpen(int port, int slot, const char* name, int flags) {
    (void)port; (void)slot; (void)name; (void)flags; return -1;
}
int sceMcRead(int fd, void* buf, int size) { (void)fd; (void)buf; (void)size; return 0; }
int sceMcClose(int fd) { (void)fd; return 0; }
int sceMcMkdir(int port, int slot, const char* name) { (void)port; (void)slot; (void)name; return 0; }
int sceMcWrite(int fd, const void* buf, int size) { (void)fd; (void)buf; (void)size; return 0; }
int sceMcDelete(int port, int slot, const char* name) { (void)port; (void)slot; (void)name; return 0; }
int sceMcFormat(int port, int slot) { (void)port; (void)slot; return 0; }
int sceMcUnformat(int port, int slot) { (void)port; (void)slot; return 0; }

/* ========================================================================= */
/*  SDLApp_Exit — called from menu.c directly                                */
/* ========================================================================= */

#include "port/ps3/app/ps3_app.h"

void SDLApp_Exit(void) {
    PS3App_Exit();
}

/* ========================================================================= */
/*  Netplay stubs — referenced directly from engine code                      */
/* ========================================================================= */

void Netplay_HandleMenuExit(void) { }
void Netplay_CancelMatchmaking(void) { }

/* ========================================================================= */
/*  Test runner stubs (guarded by DEBUG)                                       */
/* ========================================================================= */

/* test_runner.c is excluded. If DEBUG is defined and main.c references
 * TestRunner_*, these provide safe no-ops. */

/* ========================================================================= */
/*  RmlUI stubs                                                               */
/* ========================================================================= */

void rmlui_wrapper_hide_all_game_documents(void) { }
