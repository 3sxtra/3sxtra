/**
 * @file sb_oro.c
 * StageBackground wrapper – Oro (Brazil) (bg090)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/stage_090_santos_harbor_brazil.h"

__attribute__((constructor)) static void sb_oro_register(void) {
    StageBg_Register(STAGE_BG_ORO,
                     (StageBgCallbacks) {
                         .on_enter = BG090,
                         .on_tick = BG090,
                     });
}
