/**
 * @file memfound.c
 * @brief System memory facade — thin wrappers over the plmem manager.
 *
 * Maintains the global system memory manager (sysmemmgr) and its
 * backing block array. All mfl* functions simply delegate to the
 * corresponding plmem* function on this global instance.
 *
 * Part of the AcrSDK common module.
 * Originally from the PS2 SDK abstraction layer.
 */
#include "sf33rd/AcrSDK/common/memfound.h"
#include "common.h"
#include "sf33rd/AcrSDK/common/memmgr.h"

#define SYS_MEM_BLOCK_COUNT 4096

// bss
MEM_BLOCK sysmemblock[SYS_MEM_BLOCK_COUNT];

// sbss
MEM_MGR sysmemmgr;

/** @brief Initialise the global system memory manager. */
void mflInit(void* mem_ptr, s32 memsize, s32 memalign) {
    plmemInit(&sysmemmgr, sysmemblock, SYS_MEM_BLOCK_COUNT, mem_ptr, memsize, memalign, 1);
}

/** @brief Return total usable space minus used space. */
u32 mflGetSpace() {
    return plmemGetSpace(&sysmemmgr);
}

/** @brief Return the contiguous free space available for new allocations. */
size_t mflGetFreeSpace() {
    return plmemGetFreeSpace(&sysmemmgr);
}

/** @brief Register a memory block using gap-search (fits into existing holes). */
u32 mflRegisterS(s32 len) {
    return plmemRegisterS(&sysmemmgr, len);
}

/** @brief Register a memory block at the current allocation frontier. */
u32 mflRegister(s32 len) {
    return plmemRegister(&sysmemmgr, len);
}

/** @brief Get a temporary (non-persistent) memory pointer of the given size. */
void* mflTemporaryUse(s32 len) {
    return plmemTemporaryUse(&sysmemmgr, len);
}

/** @brief Retrieve the raw pointer for a registered memory handle. */
void* mflRetrieve(u32 handle) {
    return plmemRetrieve(&sysmemmgr, handle);
}

/** @brief Release a registered memory handle back to the pool. */
s32 mflRelease(u32 handle) {
    return plmemRelease(&sysmemmgr, handle);
}

/** @brief Compact the memory pool, defragmenting allocated blocks. */
void* mflCompact() {
    return plmemCompact(&sysmemmgr);
}
