/**
 * @file zlibApp.c
 * @brief zlib inflate wrapper with custom memory allocator.
 *
 * Wraps zlib's inflate API using a dedicated MemMan heap for
 * zlib's internal allocations instead of the system malloc.
 *
 * On PS3, uses the hardware-accelerated edgeZlib pipeline via SPURS
 * to decompress on SPU, avoiding the missing libz dependency.
 *
 * Part of the Compress module.
 * Originally from the PS2 compression module.
 */

#ifdef PLATFORM_PS3
#include <cell/spurs.h>
#include <cell/spurs/task.h>
#include <cell/spurs/event_flag.h>
#include <edge/zlib/edgezlib_ppu.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "port/ps3/app/ps3_app.h"

static CellSpursTaskset2 s_zlib_taskset;
static EdgeZlibInflateQHandle s_zlib_queue = 0;
static void* s_zlib_task_context = NULL;
static void* s_zlib_queue_buffer = NULL;
__attribute__((aligned(128))) static CellSpursEventFlag s_zlib_event_flag;
#endif

#include "common.h"
#include "sf33rd/Source/Common/MemMan.h"
#include "structs.h"

struct internal_state {
    s32 dummy;
};

#include "zlib.h"

typedef struct {
    struct z_stream_s info;
    s32 state;
    _MEMMAN_OBJ mobj;
} ZLIB;

ZLIB zlib;

static void* zlib_Malloc(void*, u32, u32);
static void zlib_Free(void*, void*);

/**
 * Early SPURS initialization for the edgeZlib pipeline.
 * Must be called immediately after cellSpursInitializeWithAttribute to avoid
 * CELL_ENOTCONN errors from SPU kernel threads trying to signal event ports
 * that haven't been bound yet.
 */
void zlib_InitSpurs(void) {
#ifdef PLATFORM_PS3
    struct CellSpurs* spurs = PS3App_GetSpurs();
    if (spurs && !s_zlib_queue) {
        CellSpursTasksetAttribute2 attr;
        cellSpursTasksetAttribute2Initialize(&attr);
        cellSpursCreateTaskset2(spurs, &s_zlib_taskset, &attr);

        uint32_t contextSize = edgeZlibGetInflateTaskContextSaveSize();
        s_zlib_task_context = memalign(128, contextSize);
        /* S-MED-01: Queue depth 32 to handle rapid scene transitions */
        uint32_t queueSize = edgeZlibGetInflateQueueSize(32);
        s_zlib_queue_buffer = memalign(128, queueSize);

        s_zlib_queue = edgeZlibCreateInflateQueue(spurs, 32, s_zlib_queue_buffer, queueSize);

        cellSpursEventFlagInitializeIWL(spurs, &s_zlib_event_flag,
                                        CELL_SPURS_EVENT_FLAG_CLEAR_AUTO,
                                        CELL_SPURS_EVENT_FLAG_SPU2PPU);

        edgeZlibCreateInflateTask2(&s_zlib_taskset, s_zlib_task_context, s_zlib_queue);
        printf("[PS3] edgeZlib SPURS taskset and event flag initialized\n");
    }
#endif
}

/**
 * @brief Initialise the zlib wrapper with a dedicated memory region.
 *
 * @param tempAdrs Pointer to the temporary memory region for zlib allocations.
 * @param tempSize Size of the memory region in bytes.
 */
void zlib_Initialize(void* tempAdrs, s32 tempSize) {
    if (tempAdrs == NULL) {
        return;
    }

#ifdef PLATFORM_PS3
    /* Ensure SPURS-side is set up (no-op if zlib_InitSpurs was already called) */
    zlib_InitSpurs();
#endif

    mmHeapInitialize(&zlib.mobj, tempAdrs, tempSize, ALIGN_UP(sizeof(_MEMMAN_CELL), 16), "- for zlib -");

    zlib.info.zalloc = zlib_Malloc;
    zlib.info.zfree = zlib_Free;
    zlib.info.opaque = NULL;
}

/** @brief Custom zlib allocator — allocates from the dedicated heap. */
static void* zlib_Malloc(void* opaque, u32 items, u32 size) {
    return mmAlloc(&zlib.mobj, size * items, 0);
}

/** @brief Custom zlib deallocator — frees back to the dedicated heap. */
static void zlib_Free(void* opaque, void* adrs) {
    mmFree(&zlib.mobj, (u8*)adrs);
}

/** @brief Decompress a zlib-compressed buffer into the destination. */
ssize_t zlib_Decompress(void* srcBuff, s32 srcSize, void* dstBuff, s32 dstSize) {
#ifdef PLATFORM_PS3
    if (!s_zlib_queue) return 0;
    if (srcSize <= 0 || dstSize <= 0) return 0;

    const uint8_t* realSrc = (const uint8_t*)srcBuff;
    uint32_t realSrcSize = srcSize;

    /* Strip 2-byte zlib header (CMF+FLG) if present — edgeZlib expects raw deflate. */
    uint8_t cmf = realSrc[0];
    uint8_t flg = realSrc[1];

    if (cmf == 0x78 && (((cmf << 8) + flg) % 31 == 0)) {
        realSrc += 2;
        realSrcSize -= 2;
    }

    /* edgeZlib (SPU DMA) requires 16-byte alignment for all source and destination buffers. */
    void* actualSrc = (void*)realSrc;
    void* actualDst = dstBuff;
    bool freeSrc = false;
    bool freeDst = false;

    if (((uintptr_t)realSrc & 0xF) != 0 || ((uintptr_t)dstBuff & 0xF) != 0) {
        void *caller = __builtin_return_address(0);
        printf("[PS3] zlib_Decompress: Alignment proxy engaged (SRC: %p [%s], DST: %p [%s], caller: %p, srcSize: %d, dstSize: %d)\n",
               realSrc, ((uintptr_t)realSrc & 0xF) ? "UNALIGNED" : "ok",
               dstBuff, ((uintptr_t)dstBuff & 0xF) ? "UNALIGNED" : "ok",
               caller, realSrcSize, dstSize);

        if (((uintptr_t)realSrc & 0xF) != 0) {
            actualSrc = memalign(16, (realSrcSize + 15) & ~15);
            if (!actualSrc) return 0;
            memcpy(actualSrc, realSrc, realSrcSize);
            freeSrc = true;
        }

        if (((uintptr_t)dstBuff & 0xF) != 0) {
            actualDst = memalign(16, (dstSize + 15) & ~15);
            if (!actualDst) {
                if (freeSrc) free(actualSrc);
                return 0;
            }
            freeDst = true;
        }
    }

    const uint8_t* compressedData = (const uint8_t*)actualSrc;
    uint32_t compressedSize = realSrcSize;

    /* Guarantee that SPU DMA doesn't overwrite adjacent stack bytes by putting work counter in .bss.
     * S-LOW-01 Audit Note: This function is NOT reentrant — concurrent calls will race on
     * s_workCounter. Currently safe because zlib_Decompress is only called from the main thread. */
    static __attribute__((aligned(128))) uint32_t s_workCounter = 0;

    /* Drain any lingering completion from a previous call.
     * If the previous SPU task hasn't finished yet (s_workCounter != 0),
     * wait for it now to prevent queue saturation and deadlock.
     * This is the fix for the SPURS producer-consumer deadlock: without this
     * guard, rapid sequential calls can overwhelm the event queue (capacity 32)
     * causing SPUs to block on sys_spu_thread_send_event while the PPU blocks
     * on cellSpursEventFlagWait — circular deadlock. */
    if (s_workCounter != 0) {
        uint16_t drainBits = 1;
        cellSpursEventFlagWait(&s_zlib_event_flag, &drainBits, CELL_SPURS_EVENT_FLAG_AND);
        s_workCounter = 0;
    }
    s_workCounter = 1;

    edgeZlibAddInflateQueueElement(s_zlib_queue,
                                   compressedData, compressedSize,
                                   actualDst, dstSize,
                                   &s_workCounter, &s_zlib_event_flag, 1,
                                   kEdgeZlibInflateTask_Inflate);

    uint16_t outBits = 1;
    cellSpursEventFlagWait(&s_zlib_event_flag, &outBits, CELL_SPURS_EVENT_FLAG_AND);

    if (freeDst) {
        memcpy(dstBuff, actualDst, dstSize);
        free(actualDst);
    }
    if (freeSrc) {
        free(actualSrc);
    }

    return dstSize;
#else
    if (srcBuff == NULL || dstBuff == NULL) {
        return 0;
    }

    zlib.info.next_in = srcBuff;
    zlib.info.avail_in = srcSize;
    zlib.info.next_out = dstBuff;
    zlib.info.avail_out = dstSize;
    zlib.state = Z_OK;

    if (inflateInit_(&zlib.info, ZLIB_VERSION, sizeof(z_stream)) != Z_OK) {
        return 0;
    }

    while (1) {
        zlib.state = inflate(&zlib.info, Z_NO_FLUSH);

        if (zlib.state == Z_STREAM_END) {
            break;
        }

        if (zlib.state == Z_OK) {
            continue;
        } else {
            return 0;
        }
    }

    if (inflateEnd(&zlib.info) != Z_OK) {
        return 0;
    }

    return zlib.info.total_out;
#endif
}
