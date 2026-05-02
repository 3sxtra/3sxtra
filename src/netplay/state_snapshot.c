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

static void delta_xor(const uint8_t* base, const uint8_t* next, uint8_t* out_delta, size_t size) {
    for (size_t i = 0; i < size; i++) {
        out_delta[i] = base[i] ^ next[i];
    }
}

static void delta_unxor(const uint8_t* base, const uint8_t* delta, uint8_t* out_next, size_t size) {
    for (size_t i = 0; i < size; i++) {
        out_next[i] = base[i] ^ delta[i];
    }
}

static int rle_encode(const uint8_t* in, size_t in_size, uint8_t* out, size_t out_max) {
    size_t in_idx = 0;
    size_t out_idx = 0;
    while (in_idx < in_size) {
        if (out_idx >= out_max) return -1;
        uint8_t val = in[in_idx];
        size_t count = 1;
        while (in_idx + count < in_size && in[in_idx + count] == val && count < 255) {
            count++;
        }
        if (out_idx + 2 > out_max) return -1;
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
        if (in_idx + 2 > in_size) return -1;
        uint8_t count = in[in_idx++];
        uint8_t val = in[in_idx++];
        if (out_idx + count > out_expected) return -1;
        for (int i = 0; i < count; i++) {
            out[out_idx++] = val;
        }
    }
    if (out_idx != out_expected) return -1;
    return 0;
}

void Snapshot_SaveFromState(int frame, const GameState* state, uint32_t checksum) {
    if (!initialized) Snapshot_Init();
    
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
    if (!initialized) return -1;
    
    int target_idx = -1;
    for (int i = 0; i < SNAPSHOT_RING_SIZE; i++) {
        if (ring[i].frame == frame && ring[i].size > 0) {
            target_idx = i;
            break;
        }
    }
    
    if (target_idx < 0) return -1;
    
    if (out_checksum) *out_checksum = ring[target_idx].checksum;
    
    int seq[SNAPSHOT_RING_SIZE];
    int seq_cnt = 0;
    
    int curr_idx = target_idx;
    while (curr_idx >= 0 && !ring[curr_idx].is_keyframe) {
        seq[seq_cnt++] = curr_idx;
        int found = -1;
        for (int i = 0; i < SNAPSHOT_RING_SIZE; i++) {
            if (ring[i].frame == ring[curr_idx].frame - 1 && ring[i].size > 0) {
                found = i;
                break;
            }
        }
        if (found < 0) return -1;
        curr_idx = found;
    }
    
    if (curr_idx < 0) return -1;
    
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
