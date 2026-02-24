/**
 * @file foundaps2.c
 * @brief Foundation layer — global state, system init, frame flip, logging.
 *
 * Contains the top-level system initialisation (memory, pads, debug,
 * render buffers), the per-frame flip call (temporary buffers + sound),
 * and the debug logging function.
 *
 * Part of the AcrSDK ps2 module.
 * Originally from the PS2 SDK abstraction layer.
 */
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "common.h"
#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/emlTSB.h"
#include "sf33rd/AcrSDK/common/fbms.h"
#include "sf33rd/AcrSDK/common/memfound.h"
#include "sf33rd/AcrSDK/common/mlPAD.h"
#include "sf33rd/AcrSDK/common/prilay.h"
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/flps2vram.h"
#include "sf33rd/AcrSDK/ps2/ps2PAD.h"
#include "structs.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#define FMS_HEAP_SIZE 0x01800000
#define FMS_ALIGNMENT 0x40

FLPS2State flPs2State;
FLTexture flTexture[FL_TEXTURE_MAX];
FLTexture flPalette[1088];
s32 flWidth;
s32 flHeight;
u32 flSystemRenderOperation;
FL_FMS flFMS;
s32 flVramStaticNum;
u32 flDebugStrHan;
u32 flDebugStrCol;
u32 flDebugStrCtr;

// forward decls
static s32 system_work_init();
static void flPS2InitRenderBuff();

/** @brief Initialise all AcrSDK subsystems (memory, pads, debug, render). */
s32 flInitialize() {
    if (system_work_init() == 0) {
        return 0;
    }

    flPS2SystemTmpBuffInit();
    flPS2InitRenderBuff();
    flPADInitialize();
    flPS2DebugInit();

    return 1;
}

static s32 system_work_init() {
    void* temp;

    flMemset(&flPs2State, 0, sizeof(FLPS2State));
    temp = malloc(FMS_HEAP_SIZE);

    if (temp == NULL) {
        return 0;
    }

    fmsInitialize(&flFMS, temp, FMS_HEAP_SIZE, FMS_ALIGNMENT);
    const int system_memory_size = 0xA00000;
    temp = flAllocMemoryS(system_memory_size);
    mflInit(temp, system_memory_size, FMS_ALIGNMENT);

    return 1;
}

/** @brief End-of-frame call — flushes temporary buffers and ticks the sound server. */
s32 flFlip(u32 flag) {
    flPS2SystemTmpBuffFlush();
    // NOTE: mlTsbExecServer() is called here and in the main loop. This is intentional:
    // - This call processes sound requests queued during the frame
    // - The main loop call handles any late requests
    // Both calls are safe as mlTsbExecServer() is idempotent.
    mlTsbExecServer();
    return 1;
}

static void flPS2InitRenderBuff() {
    s32 width;
    s32 height;
    s32 disp_height;

    width = 512;
    height = 448;
    disp_height = 448;
    flWidth = width;
    flHeight = height;
    flPs2State.DispWidth = width;
    flPs2State.DispHeight = disp_height;
    flPs2State.ZBuffMax = (f32)65535;
}

/** @brief Print a formatted debug log message via SDL_Log. */
s32 flLogOut(s8* format, ...) {
    s8 str[2048];
    va_list args;

    va_start(args, format);
    vsnprintf(str, sizeof(str), format, args);
    va_end(args);

    SDL_Log("[flLogOut] %s", str);
    return 1;
}
