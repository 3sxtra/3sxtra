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
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "structs.h"

#include <string.h>
#include <stdlib.h>

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
s32 flGetFrame(FrameHeapSlot* frame) {
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
// System Memory Management (Decoupled from plmem)
// ============================================================================

#define MAX_SYS_MEM_HANDLES 4096
static void* sys_mem_ptrs[MAX_SYS_MEM_HANDLES] = { NULL };

/** @brief Register a system memory handle using standard malloc. */
u32 flPS2GetSystemMemoryHandle(s32 len, s32 type) {
    void* ptr = malloc(len);
    if (!ptr) {
        flPS2SystemError(0, "ERROR flPS2GetSystemMemoryHandle malloc failed");
        return 0;
    }

    // Handle 0 is invalid
    for (u32 i = 1; i < MAX_SYS_MEM_HANDLES; i++) {
        if (sys_mem_ptrs[i] == NULL) {
            sys_mem_ptrs[i] = ptr;
            return i;
        }
    }

    free(ptr);
    flPS2SystemError(0, "ERROR flPS2GetSystemMemoryHandle out of handles");
    return 0;
}

/** @brief Release a system memory handle using standard free. */
void flPS2ReleaseSystemMemory(u32 handle) {
    if (handle > 0 && handle < MAX_SYS_MEM_HANDLES) {
        if (sys_mem_ptrs[handle]) {
            free(sys_mem_ptrs[handle]);
            sys_mem_ptrs[handle] = NULL;
        }
    }
}

/** @brief Retrieve the address of a system memory handle. */
void* flPS2GetSystemBuffAdrs(u32 handle) {
    if (handle > 0 && handle < MAX_SYS_MEM_HANDLES) {
        return sys_mem_ptrs[handle];
    }
    return NULL;
}

#define TEMPORARY_USE_SCRATCHPAD_SIZE (1024 * 1024 * 16) // 16MB
static u8 temporary_use_scratchpad[TEMPORARY_USE_SCRATCHPAD_SIZE];

/** @brief Provide a static scratchpad for temporary memory use, replacing plmem temporary buffer. */
void* mflTemporaryUse(s32 len) {
    if (len > TEMPORARY_USE_SCRATCHPAD_SIZE) {
        flPS2SystemError(0, "ERROR mflTemporaryUse size too large");
        return NULL;
    }
    // Return an address at the end of the scratchpad
    return (void*)(temporary_use_scratchpad + TEMPORARY_USE_SCRATCHPAD_SIZE - len);
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
