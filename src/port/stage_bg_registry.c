/**
 * @file stage_bg_registry.c
 * StageBackground Registry implementation.
 */

#include "port/stage_bg_registry.h"
#include <stddef.h>

static StageBgCallbacks registry[STAGE_BG_COUNT];

void StageBg_Register(StageBgId id, StageBgCallbacks callbacks) {
    if (id >= 0 && id < STAGE_BG_COUNT) {
        registry[id] = callbacks;
    }
}

const StageBgCallbacks* StageBg_Get(StageBgId id) {
    if (id >= 0 && id < STAGE_BG_COUNT && registry[id].on_tick != NULL) {
        return &registry[id];
    }
    return NULL;
}
