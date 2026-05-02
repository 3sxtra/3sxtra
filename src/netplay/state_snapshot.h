#ifndef STATE_SNAPSHOT_H
#define STATE_SNAPSHOT_H

#include "game_state.h"

// 8 frames per keyframe
#define SNAPSHOT_KEYFRAME_STRIDE 8
#define SNAPSHOT_RING_SIZE 32

typedef union {
    GameState full;
    uint8_t compressed[sizeof(GameState)]; // worst case RLE
} SnapshotData;

typedef struct {
    int frame;
    uint32_t checksum;
    int size;
    int is_keyframe;
    SnapshotData data;
} SnapshotEntry;

void Snapshot_Init(void);
void Snapshot_SaveFromState(int frame, const GameState* state, uint32_t checksum);
int Snapshot_Get(int frame, GameState* out_state, uint32_t* out_checksum);

#endif
