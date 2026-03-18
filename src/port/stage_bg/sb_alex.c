/**
 * @file sb_alex.c
 * StageBackground wrapper – Alex (New York) (bg010)
 * Registers both STAGE_BG_ALEX (slot 1) and STAGE_BG_KEN (slot 11).
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/bg010.h"

__attribute__((constructor))
static void sb_alex_register(void) {
    StageBgCallbacks cb = {
        .on_enter = BG010,
        .on_tick  = BG010,
    };
    StageBg_Register(STAGE_BG_ALEX, cb);
    StageBg_Register(STAGE_BG_KEN, cb);
}
