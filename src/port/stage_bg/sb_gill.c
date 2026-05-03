/**
 * @file sb_gill.c
 * StageBackground wrapper – Gill Stage (bg000)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/stage_000_gill_unknown.h"

__attribute__((constructor)) static void sb_gill_register(void) {
    StageBg_Register(STAGE_BG_GILL,
                     (StageBgCallbacks) {
                         .on_enter = BG000,
                         .on_tick = BG000,
                     });
}
