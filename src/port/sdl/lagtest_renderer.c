/**
 * @file lagtest_renderer.c
 * @brief Native OSD overlay for GPIO input lag test mode.
 *
 * Renders frame tracking data in big text (3x scale) using the game's
 * native SSPutStrPro_Scale font engine. Positioned at middle-right of
 * the 384×224 native screen.
 *
 * Display format:
 *   F:12345       — current frame (system_timer)
 *   R:12340       — receive frame (when GPIO button was pressed)
 *   A:12342 [2]   — active frame + delta (when game state changed)
 *
 * Follows the pattern of netstats_renderer.c (netplay R:X P:Y overlay).
 */

#include "port/sdl/lagtest_renderer.h"
#include "port/linux/gpio_lag_test.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "sf33rd/Source/Game/engine/workuser_system.h"

#include <SDL3/SDL.h>

/* ── Layout constants ──────────────────────────────────────────────── */

#define LAG_SCALE     3.0f   /* 3x native 8px font = 24px tall glyphs */
#define LAG_LINE_H    (8.0f * LAG_SCALE + 2.0f)  /* Line height with 2px gap */
#define LAG_ATR       6      /* Palette index (matches netstats) */

/* Position: middle-right area of the 384x224 native screen */
#define LAG_X         220.0f /* Right-of-center horizontally */
#define LAG_Y         80.0f  /* Vertically centered region */

void LagtestRenderer_Render(void) {
    /* Prevent rendering outside of an active match to avoid trying to draw 
       before engine/font textures are fully loaded in memory. */
    if (Play_Game != 1) {
        return;
    }

    if (!GpioLagTest_IsEnabled()) {
        return;
    }

    GpioLagTestState st = GpioLagTest_GetState();
    if (!st.enabled) {
        return;
    }

    char buf[32];
    float y = LAG_Y;

    /* Line 1: Current frame — always visible when GPIO test is on AND active */
    SDL_snprintf(buf, sizeof(buf), "F:%u", (unsigned)st.current_frame);
    SSPutStrPro_Scale(0, LAG_X, y, LAG_ATR, 0xFFFFFFFF, (s8*)buf, LAG_SCALE);
    y += LAG_LINE_H;

    /* Lines 2-3: Only show when tracking a press or displaying persisted result */
    if (st.tracking || st.display_timer > 0) {
        /* Line 2: Receive frame (when GPIO button was first detected) */
        SDL_snprintf(buf, sizeof(buf), "R:%u", (unsigned)st.receive_frame);
        SSPutStrPro_Scale(0, LAG_X, y, LAG_ATR, 0xFFFFFF00, (s8*)buf, LAG_SCALE);
        y += LAG_LINE_H;

        /* Line 3: Active frame + delta (when game state reacted) */
        if (st.result_ready) {
            SDL_snprintf(buf, sizeof(buf), "A:%u", (unsigned)st.active_frame);
            SSPutStrPro_Scale(0, LAG_X, y, LAG_ATR, 0xFF00FF00, (s8*)buf, LAG_SCALE);
            y += LAG_LINE_H;

            /* Line 4: Lag delta in big green text */
            SDL_snprintf(buf, sizeof(buf), "[%d]", (int)st.lag_frames);
            SSPutStrPro_Scale(0, LAG_X, y, LAG_ATR, 0xFF00FF00, (s8*)buf, LAG_SCALE);
        } else {
            /* Still waiting for game reaction */
            SSPutStrPro_Scale(0, LAG_X, y, LAG_ATR, 0xFF888888, (s8*)"A:...", LAG_SCALE);
        }
    }

    /* Update frame tracking (check for routine_no changes + persistence timer) */
    GpioLagTest_UpdateFrameTracking();
}
