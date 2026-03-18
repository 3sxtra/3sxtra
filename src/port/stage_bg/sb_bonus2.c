/**
 * @file sb_bonus2.c
 * StageBackground wrapper – Bonus Stage 2 (bns_bg2)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/bns_bg2.h"

__attribute__((constructor)) static void sb_bonus2_register(void) {
    StageBg_Register(STAGE_BG_BONUS2,
                     (StageBgCallbacks) {
                         .on_enter = Bonus_bg2,
                         .on_tick = Bonus_bg2,
                     });
}
