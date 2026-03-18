/**
 * @file sb_necro_alt.c
 * StageBackground wrapper – Q/Necro Alt & Twelve (bg180)
 * Registers both STAGE_BG_NECRO_ALT (slot 17) and STAGE_BG_TWELVE (slot 18).
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/bg180.h"

__attribute__((constructor)) static void sb_necro_alt_register(void) {
    StageBgCallbacks cb = {
        .on_enter = BG180,
        .on_tick = BG180,
    };
    StageBg_Register(STAGE_BG_NECRO_ALT, cb);
    StageBg_Register(STAGE_BG_TWELVE, cb);
}
