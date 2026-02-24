/**
 * @file fbms.c
 * @brief Frame memory stack — dual-ended linear allocator implementation.
 *
 * Simple stack-based allocator supporting bottom-up (heap 0) and
 * top-down (heap 1) allocation from a single pre-allocated region.
 *
 * Part of the AcrSDK common module.
 * Originally from the PS2 SDK abstraction layer.
 */
#include "common.h"

#define FMS_HEAP_COUNT 2
#include "structs.h"

/** @brief Calculate the remaining free space between the two frame pointers. */
ptrdiff_t fmsCalcSpace(FL_FMS* lp) {
    return lp->frame[1] - lp->frame[0];
}

/** @brief Initialise a frame memory stack with the given memory region and alignment. */
s32 fmsInitialize(FL_FMS* lp, void* memory_ptr, s32 memsize, s32 memalign) {
    if (lp == NULL || memory_ptr == NULL) {
        return 0;
    }

    memsize = ~(memalign - 1) & (memsize + memalign - 1);
    lp->memoryblock = (u8*)memory_ptr;
    lp->align = memalign;
    lp->baseandcap[0] = (u8*)((uintptr_t)lp->memoryblock + lp->align - 1 & ~(lp->align - 1));
    lp->baseandcap[1] = (u8*)(((uintptr_t)lp->memoryblock + memsize + lp->align - 1) & ~(lp->align - 1));
    lp->frame[0] = lp->baseandcap[0];
    lp->frame[1] = lp->baseandcap[1];
    return 1;
}

/** @brief Allocate memory from heap 0 (bottom) or heap 1 (top). */
void* fmsAllocMemory(FL_FMS* lp, s32 bytes, s32 heapnum) {
    void* pMem;

    if ((u32)heapnum >= FMS_HEAP_COUNT) {
        return NULL;
    }
    bytes = ~(lp->align - 1) & ((uintptr_t)bytes + lp->align - 1);

    if (lp->frame[0] + bytes > lp->frame[1]) {
        return NULL;
    }

    if (heapnum != 0) {
        lp->frame[1] -= bytes;
        pMem = lp->frame[1];
    } else {
        pMem = lp->frame[0];
        lp->frame[0] += bytes;
    }

    return pMem;
}

/** @brief Snapshot the current frame pointer for the given heap into a frame struct. */
s32 fmsGetFrame(FL_FMS* lp, s32 heapnum, FMS_FRAME* frame) {
    if ((u32)heapnum >= FMS_HEAP_COUNT) {
        return 0;
    }
    frame->pFrame = lp->frame[heapnum];
    frame->heapnum = heapnum;
    return 1;
}
