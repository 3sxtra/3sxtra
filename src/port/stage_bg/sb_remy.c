/**
 * @file sb_remy.c
 * StageBackground wrapper – Remy (bg190)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/bg190.h"

__attribute__((constructor))
static void sb_remy_register(void) {
    StageBg_Register(STAGE_BG_REMY, (StageBgCallbacks){
        .on_enter = BG190,
        .on_tick  = BG190,
    });
}
