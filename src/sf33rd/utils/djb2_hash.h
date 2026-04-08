#ifndef DJB2_HASH_H
#define DJB2_HASH_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline uint32_t djb2_init() {
    return 5381;
}

// turbo
// What: Coalesce memory reads in djb2_update_mem for netplay rollback state scanning.
// Target: CPU Memory & Cache (Cache Locality / Scan Bandwidth).
// Expected Impact: Reduces loop iterations by 4x across 20KB game state snapshots.
static inline uint32_t djb2_update_mem(uint32_t hash, const uint8_t* data, size_t len) {
    size_t dwords = len / 4;
    for (size_t i = 0; i < dwords; i++) {
        uint32_t block;
        memcpy(&block, data + (i * 4), 4);
        hash = (hash * 33) ^ block;
    }

    const uint8_t* tail = data + (dwords * 4);
    size_t rem = len % 4;
    for (size_t i = 0; i < rem; i++) {
        hash = (hash * 33) ^ tail[i];
    }

    return hash;
}

#define djb2_update(hash, v) djb2_update_mem(hash, &v, sizeof(v))
#define djb2_updatep(hash, p) djb2_update_mem(hash, p, sizeof(*p))
#define djb2_updatea(hash, a) djb2_update_mem(hash, a, sizeof(a))

#endif
