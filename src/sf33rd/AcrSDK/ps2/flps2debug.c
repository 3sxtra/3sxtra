/**
 * @file flps2debug.c
 * @brief Debug text rendering and system error display implementation.
 *
 * Queues debug text characters into a RenderBuffer for on-screen display,
 * supports color changes, and provides a blocking system error screen
 * that halts execution until the user presses start.
 *
 * Part of the AcrSDK ps2 module.
 * Originally from the PS2 SDK abstraction layer.
 */
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "common.h"
#include "sf33rd/AcrSDK/common/memfound.h"
#include "sf33rd/AcrSDK/common/mlPAD.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/flps2shader.h"
#include "sf33rd/AcrSDK/ps2/flps2vram.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "structs.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_STR_CAPACITY 0x12C0

/** @brief Initialise the debug text buffer and reset counters. */
void flPS2DebugInit() {
    flDebugStrHan = flPS2GetSystemMemoryHandle(DEBUG_STR_CAPACITY * sizeof(RenderBuffer), 1);
    flDebugStrCtr = 0;
    flDebugStrCol = 0xFFFFFFFF; // White color
}

// Debug text printing function - queues characters to the RenderBuffer for
// later rendering by SDLGLTextRenderer_DrawDebugBuffer()
/** @brief Print formatted debug text at the given grid position. */
s32 flPrintL(s32 posi_x, s32 posi_y, const s8* format, ...) {
    s8 code;
    s8 str[512];
    strlen_t len;
    s32 i;
    RenderBuffer* buff_ptr;

    va_list args;

    buff_ptr = flPS2GetSystemBuffAdrs(flDebugStrHan);
    buff_ptr += flDebugStrCtr;

    va_start(args, format);
    vsnprintf(str, sizeof(str), format, args);
    va_end(args);
    len = strlen(str);

    if (flDebugStrCtr + len >= DEBUG_STR_CAPACITY) {
        len = DEBUG_STR_CAPACITY - flDebugStrCtr;
    }

    for (i = 0; i < len; i++) {
        code = str[i];

        if ((code >= 0x10) && (code != ' ')) {
            buff_ptr->x = posi_x * 8;
            buff_ptr->y = posi_y * 8;
            buff_ptr->code = code;
            buff_ptr->col = flDebugStrCol;
            buff_ptr++;
            flDebugStrCtr += 1;
        }

        posi_x += 1;
    }

    return 1;
}

/** @brief Set the color for subsequent debug text output. */
s32 flPrintColor(u32 color) {
    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = (color) & 0xFF;
    u8 a = (color >> 24) & 0xFF;

    r >>= 1;
    g >>= 1;
    b >>= 1;

    if (a == 0xFF) {
        a = 0x80;
    } else {
        a >>= 1;
    }

    flDebugStrCol = (a << 24) | (r << 16) | (g << 8) | b;
    return 1;
}

/** @brief Display a blocking system error screen with the given message. */
void flPS2SystemError(s32 error_level, s8* format, ...) {
    va_list args;
    s8 str[512];

    flFlip(0);
    va_start(args, format);
    vsnprintf(str, sizeof(str), format, args);
    va_end(args);

    while (1) {
        flPrintL(10, 20, "%s", str);

        if (error_level == 0) {
            flSetRenderState(FLRENDER_BACKCOLOR, 0xFF0000);
        } else {
            flSetRenderState(FLRENDER_BACKCOLOR, 0xFF);
            flPrintL(10, 40, "PRESS 1P START BUTTON TO EXIT");

            if (flpad_adr[0][0].sw_new & 0x8000) {
                break;
            }
        }

        flFlip(0);
        flPADGetALL();
    }
}
