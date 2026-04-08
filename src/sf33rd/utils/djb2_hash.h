#ifndef DJB2_HASH_H
#define DJB2_HASH_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline uint32_t djb2_init() {
    return 5381;
}

// turbo
// What: Coalesce memory reads in djb2_update_mem into 64-bit boundaries for netplay rollback state scanning.
// Target: CPU Memory & Cache (Cache Locality / Scan Bandwidth).
// Expected Impact: Reduces loop iterations by an additional 2x (total 8x) for netplay game state hashing.
static inline uint32_t djb2_update_mem(uint32_t hash, const uint8_t* data, size_t len) {
    size_t qwords = len / 8;
    for (size_t i = 0; i < qwords; i++) {
        uint64_t block;
        memcpy(&block, data + (i * 8), 8);
        hash = (hash * 33) ^ (uint32_t)(block & 0xFFFFFFFF);
        hash = (hash * 33) ^ (uint32_t)(block >> 32);
    }

    const uint8_t* tail = data + (qwords * 8);
    size_t rem = len % 8;

    if (rem >= 4) {
        uint32_t block;
        memcpy(&block, tail, 4);
        hash = (hash * 33) ^ block;
        tail += 4;
        rem -= 4;
    }

    for (size_t i = 0; i < rem; i++) {
        hash = (hash * 33) ^ tail[i];
    }

    return hash;
}

#define djb2_update(hash, v) djb2_update_mem(hash, &v, sizeof(v))
#define djb2_updatep(hash, p) djb2_update_mem(hash, p, sizeof(*p))
#define djb2_updatea(hash, a) djb2_update_mem(hash, a, sizeof(a))

#endif
