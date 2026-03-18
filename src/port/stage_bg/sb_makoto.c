/**
 * @file sb_makoto.c
 * StageBackground wrapper – Makoto (Japan) (bg160)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/bg160.h"

__attribute__((constructor))
static void sb_makoto_register(void) {
    StageBg_Register(STAGE_BG_MAKOTO, (StageBgCallbacks){
        .on_enter = BG160,
        .on_tick  = BG160,
    });
}
