/**
 * @file lagtest_renderer.c
 * @brief Native OSD overlay for GPIO input lag test mode.
 *
 * Renders frame tracking data centered on the 384×224 native screen
 * using the game's native SSPutStrPro_Scale font engine.
 *
 * Display format (centered):
 *   F:12345
 *   R:12340  @0.00ms
 *   A:12342  @33.5ms
 *   LAG: 2f  33.52ms
 *
 * Each value shows both the frame number and the precise wall-clock
 * time measured via SDL_GetPerformanceCounter(), giving sub-ms accuracy
 * independent of frame counting.
 *
 * Follows the pattern of netstats_renderer.c (netplay R:X P:Y overlay).
 */

#include "port/sdl/lagtest_renderer.h"
#include "game_state.h"
#include "port/linux/gpio_lag_test.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "sf33rd/Source/Game/engine/workuser_system.h"

#include <SDL3/SDL.h>

/* ── Layout constants ──────────────────────────────────────────────── */

#define LAG_SCALE 2.5f                       /* 2.5x native 8px font = 20px tall glyphs */
#define LAG_CHAR_W (8.0f * LAG_SCALE)        /* Approximate glyph width */
#define LAG_LINE_H (8.0f * LAG_SCALE + 4.0f) /* Line height with 4px gap */
#define LAG_ATR 6                            /* Palette index (matches netstats) */

/* Native CPS3 screen dimensions */
#define SCREEN_W 384.0f
#define SCREEN_H 224.0f

/* Colors */
#define COL_WHITE 0xFFFFFFFF
#define COL_YELLOW 0xFFFFFF00
#define COL_GREEN 0xFF00FF00
#define COL_GRAY 0xFF888888
#define COL_CYAN 0xFF00FFFF
#define COL_BG 0x80000000 /* Semi-transparent black (if we had bg support) */

/* ── Helpers ───────────────────────────────────────────────────────── */

/** @brief Draw a string centered horizontally on screen. */
static void draw_centered(float y, uint8_t atr, uint32_t color, const char* text) {
    int len = (int)SDL_strlen(text);
    float text_width = (float)len * LAG_CHAR_W;
    float x = (SCREEN_W - text_width) * 0.5f;
    if (x < 0.0f)
        x = 0.0f;
    SSPutStrPro_Scale(0, x, y, atr, color, (s8*)text, LAG_SCALE);
}

void LagtestRenderer_Render(void) {
    /* Prevent rendering outside of an active match to avoid trying to draw
       before engine/font textures are fully loaded in memory. */
    if (g_state.Play_Game != 1) {
        return;
    }

    if (!GpioLagTest_IsEnabled()) {
        return;
    }

    GpioLagTestState st = GpioLagTest_GetState();
    if (!st.enabled) {
        return;
    }

    char buf[48];

    /* Calculate vertical centering for the block of text.
     * Max lines: F + R + A + LAG = 4 lines when result is ready,
     * 3 lines when waiting (F + R + A:...).
     * Center the full 4-line block so it doesn't jump around. */
    const int max_lines = 4;
    float block_height = (float)max_lines * LAG_LINE_H;
    float y = (SCREEN_H - block_height) * 0.5f;

    /* ── Line 1: Current frame — always visible ── */
    SDL_snprintf(buf, sizeof(buf), "F:%u", (unsigned)st.current_frame);
    draw_centered(y, LAG_ATR, COL_WHITE, buf);
    y += LAG_LINE_H;

    /* ── Lines 2-4: Only show when tracking or displaying result ── */
    if (st.tracking || st.display_timer > 0) {

        /* Line 2: Receive frame + @0.00ms anchor */
        SDL_snprintf(buf, sizeof(buf), "R:%-6u @0.00ms", (unsigned)st.receive_frame);
        draw_centered(y, LAG_ATR, COL_YELLOW, buf);
        y += LAG_LINE_H;

        /* Line 3: Active frame + measured time offset from R */
        if (st.result_ready) {
            SDL_snprintf(buf, sizeof(buf), "A:%-6u @%.2fms", (unsigned)st.active_frame, st.lag_ms);
            draw_centered(y, LAG_ATR, COL_GREEN, buf);
            y += LAG_LINE_H;

            /* Line 4: Summary lag — big centered text */
            SDL_snprintf(buf, sizeof(buf), "LAG %df  %.2fms", (int)st.lag_frames, st.lag_ms);
            draw_centered(y, LAG_ATR, COL_CYAN, buf);
        } else {
            /* Still waiting for game reaction — show elapsed so far */
            uint64_t now = SDL_GetPerformanceCounter();
            double elapsed_ms = 0.0;
            if (st.receive_ticks > 0 && now > st.receive_ticks) {
                elapsed_ms = (double)(now - st.receive_ticks) * 1000.0 / (double)SDL_GetPerformanceFrequency();
            }
            SDL_snprintf(buf, sizeof(buf), "A:...    @%.1fms", elapsed_ms);
            draw_centered(y, LAG_ATR, COL_GRAY, buf);
        }
    }

    /* Update frame tracking (check for routine_no changes + persistence timer) */
    GpioLagTest_UpdateFrameTracking();
}
