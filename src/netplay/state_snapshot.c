#include "state_snapshot.h"
#include <SDL3/SDL.h>
#include <string.h>

static SnapshotEntry ring[SNAPSHOT_RING_SIZE];
static int ring_head = 0;
static int initialized = 0;

void Snapshot_Init(void) {
    memset(ring, 0, sizeof(ring));
    ring_head = 0;
    initialized = 1;
}

/* --- SIMD XOR acceleration ---
 * SSE2 (x86-64, always available) and NEON (ARM64/Pi4, always available)
 * process 16 bytes per iteration. Fallback uses uint64_t (8 bytes).
 * GameState is ~18.8KB so SIMD reduces iterations from ~18800 to ~1200.
 */
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#include <emmintrin.h>
#define SNAPSHOT_SIMD_WIDTH 16
#define SNAPSHOT_HAS_SSE2 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define SNAPSHOT_SIMD_WIDTH 16
#define SNAPSHOT_HAS_NEON 1
#else
#define SNAPSHOT_SIMD_WIDTH 8
#endif

static void delta_xor(const uint8_t* base, const uint8_t* next, uint8_t* out_delta, size_t size) {
    size_t i = 0;

#if defined(SNAPSHOT_HAS_SSE2)
    for (; i + 16 <= size; i += 16) {
        __m128i b = _mm_loadu_si128((const __m128i*)(base + i));
        __m128i n = _mm_loadu_si128((const __m128i*)(next + i));
        _mm_storeu_si128((__m128i*)(out_delta + i), _mm_xor_si128(b, n));
    }
#elif defined(SNAPSHOT_HAS_NEON)
    for (; i + 16 <= size; i += 16) {
        uint8x16_t b = vld1q_u8(base + i);
        uint8x16_t n = vld1q_u8(next + i);
        vst1q_u8(out_delta + i, veorq_u8(b, n));
    }
#endif
    /* Scalar tail — handles remainder and non-SIMD platforms */
    for (; i + 8 <= size; i += 8) {
        uint64_t bv, nv;
        memcpy(&bv, base + i, 8);
        memcpy(&nv, next + i, 8);
        bv ^= nv;
        memcpy(out_delta + i, &bv, 8);
    }
    for (; i < size; i++) {
        out_delta[i] = base[i] ^ next[i];
    }
}

static void delta_unxor(const uint8_t* base, const uint8_t* delta, uint8_t* out_next, size_t size) {
    size_t i = 0;

#if defined(SNAPSHOT_HAS_SSE2)
    for (; i + 16 <= size; i += 16) {
        __m128i b = _mm_loadu_si128((const __m128i*)(base + i));
        __m128i d = _mm_loadu_si128((const __m128i*)(delta + i));
        _mm_storeu_si128((__m128i*)(out_next + i), _mm_xor_si128(b, d));
    }
#elif defined(SNAPSHOT_HAS_NEON)
    for (; i + 16 <= size; i += 16) {
        uint8x16_t b = vld1q_u8(base + i);
        uint8x16_t d = vld1q_u8(delta + i);
        vst1q_u8(out_next + i, veorq_u8(b, d));
    }
#endif
    for (; i + 8 <= size; i += 8) {
        uint64_t bv, dv;
        memcpy(&bv, base + i, 8);
        memcpy(&dv, delta + i, 8);
        bv ^= dv;
        memcpy(out_next + i, &bv, 8);
    }
    for (; i < size; i++) {
        out_next[i] = base[i] ^ delta[i];
    }
}

static int rle_encode(const uint8_t* in, size_t in_size, uint8_t* out, size_t out_max) {
    size_t in_idx = 0;
    size_t out_idx = 0;
    while (in_idx < in_size) {
        if (out_idx >= out_max)
            return -1;
        uint8_t val = in[in_idx];
        size_t count = 1;
        while (in_idx + count < in_size && in[in_idx + count] == val && count < 255) {
            count++;
        }
        if (out_idx + 2 > out_max)
            return -1;
        out[out_idx++] = count;
        out[out_idx++] = val;
        in_idx += count;
    }
    return (int)out_idx;
}

static int rle_decode(const uint8_t* in, size_t in_size, uint8_t* out, size_t out_expected) {
    size_t in_idx = 0;
    size_t out_idx = 0;
    while (in_idx < in_size) {
        if (in_idx + 2 > in_size)
            return -1;
        uint8_t count = in[in_idx++];
        uint8_t val = in[in_idx++];
        if (out_idx + count > out_expected)
            return -1;
        for (int i = 0; i < count; i++) {
            out[out_idx++] = val;
        }
    }
    if (out_idx != out_expected)
        return -1;
    return 0;
}

void Snapshot_SaveFromState(int frame, const GameState* state, uint32_t checksum) {
    if (!initialized)
        Snapshot_Init();

    int is_keyframe = (frame % SNAPSHOT_KEYFRAME_STRIDE == 0);
    int prev_idx = (ring_head - 1 + SNAPSHOT_RING_SIZE) % SNAPSHOT_RING_SIZE;

    if (!is_keyframe) {
        if (ring[prev_idx].frame != frame - 1) {
            is_keyframe = 1;
        }
    }

    SnapshotEntry* entry = &ring[ring_head];
    entry->frame = frame;
    entry->checksum = checksum;

    if (is_keyframe) {
        entry->is_keyframe = 1;
        entry->size = sizeof(GameState);
        memcpy(&entry->data.full, state, sizeof(GameState));
    } else {
        GameState prev_state;
        uint32_t prev_checksum;
        if (Snapshot_Get(frame - 1, &prev_state, &prev_checksum) < 0) {
            entry->is_keyframe = 1;
            entry->size = sizeof(GameState);
            memcpy(&entry->data.full, state, sizeof(GameState));
        } else {
            uint8_t xor_buf[sizeof(GameState)];
            delta_xor((const uint8_t*)&prev_state, (const uint8_t*)state, xor_buf, sizeof(GameState));

            int c_size = rle_encode(xor_buf, sizeof(GameState), entry->data.compressed, sizeof(GameState));
            if (c_size < 0 || c_size >= sizeof(GameState)) {
                entry->is_keyframe = 1;
                entry->size = sizeof(GameState);
                memcpy(&entry->data.full, state, sizeof(GameState));
            } else {
                entry->is_keyframe = 0;
                entry->size = c_size;
            }
        }
    }

    ring_head = (ring_head + 1) % SNAPSHOT_RING_SIZE;
}

int Snapshot_Get(int frame, GameState* out_state, uint32_t* out_checksum) {
    if (!initialized || frame < 0)
        return -1;

    // Direct indexing O(1) lookup
    int target_idx = frame % SNAPSHOT_RING_SIZE;
    if (ring[target_idx].frame != frame || ring[target_idx].size <= 0) {
        return -1;
    }

    if (out_checksum)
        *out_checksum = ring[target_idx].checksum;

    int seq[SNAPSHOT_RING_SIZE];
    int seq_cnt = 0;

    int curr_idx = target_idx;
    int safety_counter = 0;
    while (!ring[curr_idx].is_keyframe) {
        if (safety_counter++ >= SNAPSHOT_RING_SIZE)
            return -1; // Cycle detection

        seq[seq_cnt++] = curr_idx;
        int prev_idx = (ring[curr_idx].frame - 1) % SNAPSHOT_RING_SIZE;
        if (ring[prev_idx].frame != ring[curr_idx].frame - 1 || ring[prev_idx].size <= 0) {
            return -1; // Chain broken
        }
        curr_idx = prev_idx;
    }

    if (curr_idx < 0)
        return -1;

    memcpy(out_state, &ring[curr_idx].data.full, sizeof(GameState));

    for (int i = seq_cnt - 1; i >= 0; i--) {
        int idx = seq[i];
        uint8_t xor_buf[sizeof(GameState)];
        if (rle_decode(ring[idx].data.compressed, ring[idx].size, xor_buf, sizeof(GameState)) < 0) {
            return -1;
        }
        uint8_t next_state[sizeof(GameState)];
        delta_unxor((const uint8_t*)out_state, xor_buf, next_state, sizeof(GameState));
        memcpy(out_state, next_state, sizeof(GameState));
    }

    return 0;
}

int Snapshot_GetValidCount(void) {
    if (!initialized)
        return 0;
    int count = 0;
    for (int i = 0; i < SNAPSHOT_RING_SIZE; i++) {
        if (ring[i].size > 0) {
            count++;
        }
    }
    return count;
}
