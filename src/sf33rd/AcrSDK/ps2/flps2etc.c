/**
 * @file flps2etc.c
 * @brief Memory wrappers, system memory handles, and temporary buffers.
 *
 * Convenience wrappers around standard memory functions, frame memory
 * stack allocation helpers, system memory handle management, and a
 * double-buffered temporary buffer pool.
 *
 * Part of the AcrSDK ps2 module.
 * Originally from the PS2 SDK abstraction layer.
 */
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "common.h"
#include "sf33rd/AcrSDK/common/fbms.h"
#include "sf33rd/AcrSDK/common/memfound.h"
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "structs.h"

#include <string.h>

#define SYSTEM_TMP_BUFF_SIZE 0x80000

// ============================================================================
// Memory Wrappers (used by vram/texture code)
// ============================================================================

/** @brief Fill memory with a byte pattern (memset wrapper). */
void flMemset(void* dst, u32 pat, s32 size) {
    memset(dst, pat, size);
}

/** @brief Copy memory (memcpy wrapper). */
void flMemcpy(void* dst, void* src, s32 size) {
    memcpy(dst, src, size);
}

/** @brief Allocate from the bottom of the frame memory stack. */
void* flAllocMemory(s32 size) {
    return fmsAllocMemory(&flFMS, size, 0);
}

/** @brief Snapshot the current frame pointer for heap 0. */
s32 flGetFrame(FMS_FRAME* frame) {
    return fmsGetFrame(&flFMS, 0, frame);
}

/** @brief Return remaining space in the frame memory stack. */
s32 flGetSpace() {
    return fmsCalcSpace(&flFMS);
}

/** @brief Allocate from the top of the frame memory stack. */
void* flAllocMemoryS(s32 size) {
    return fmsAllocMemory(&flFMS, size, 1);
}

// ============================================================================
// System Memory Management
// ============================================================================

/** @brief Register a system memory handle, compacting if needed. */
u32 flPS2GetSystemMemoryHandle(s32 len, s32 type) {
    u32 handle = mflRegisterS(len);

    if (handle == 0) {
        mflCompact();
        handle = mflRegister(len);

        if (handle == 0) {
            flPS2SystemError(0, "ERROR flPS2GetSystemMemoryHandle flps2etc.c");
            while (1) {}
        }
    }

    return handle;
}

/** @brief Release a system memory handle. */
void flPS2ReleaseSystemMemory(u32 handle) {
    mflRelease(handle);
}

/** @brief Retrieve the address of a system memory handle. */
void* flPS2GetSystemBuffAdrs(u32 handle) {
    return mflRetrieve(handle);
}

// ============================================================================
// Temporary Buffer Management
// ============================================================================

/** @brief Allocate the double-buffered temporary buffer pool. */
void flPS2SystemTmpBuffInit() {
    s32 lp0;

    for (lp0 = 0; lp0 < 2; lp0++) {
        flPs2State.SystemTmpBuffHandle[lp0] = flPS2GetSystemMemoryHandle(SYSTEM_TMP_BUFF_SIZE, 1);
    }

    flPS2SystemTmpBuffFlush();
}

/** @brief Reset the current temporary buffer pointer to the start. */
void flPS2SystemTmpBuffFlush() {
    u32 len;

    switch (flPs2State.SystemStatus) {
    case 0:
    case 2:
    case 1:
        len = SYSTEM_TMP_BUFF_SIZE;
        flPs2State.SystemTmpBuffStartAdrs =
            (uintptr_t)flPS2GetSystemBuffAdrs(flPs2State.SystemTmpBuffHandle[flPs2State.SystemIndex]);
        flPs2State.SystemTmpBuffNow = flPs2State.SystemTmpBuffStartAdrs;
        flPs2State.SystemTmpBuffEndAdrs = flPs2State.SystemTmpBuffStartAdrs + len;

        break;

    default:
        break;
    }
}

/** @brief Allocate an aligned chunk from the current temporary buffer. */
uintptr_t flPS2GetSystemTmpBuff(s32 len, s32 align) {
    uintptr_t now;
    uintptr_t new_now;

    now = flPs2State.SystemTmpBuffNow;
    now = ~(align - 1) & (now + align - 1);
    new_now = now + len;

    if (flPs2State.SystemTmpBuffEndAdrs < new_now) {
        flPS2SystemError(0, "ERROR flPS2GetSystemTmpBuff flps2etc.c");
        now = flPs2State.SystemTmpBuffStartAdrs;
        new_now = now + len;
    }

    flPs2State.SystemTmpBuffNow = new_now;
    return now;
}
