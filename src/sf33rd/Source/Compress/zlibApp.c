/**
 * @file zlibApp.c
 * @brief zlib inflate wrapper with custom memory allocator.
 *
 * Wraps zlib's inflate API using a dedicated MemMan heap for
 * zlib's internal allocations instead of the system malloc.
 *
 * Part of the Compress module.
 * Originally from the PS2 compression module.
 */
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
 * @brief Initialise the zlib wrapper with a dedicated memory region.
 *
 * @param tempAdrs Pointer to the temporary memory region for zlib allocations.
 * @param tempSize Size of the memory region in bytes.
 */
void zlib_Initialize(void* tempAdrs, s32 tempSize) {
    if (tempAdrs == NULL || tempSize <= 0) {
        while (1) {}
    }

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
}
