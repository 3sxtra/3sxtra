/**
 * @file sb_sean.c
 * StageBackground wrapper – Sean (Brazil) (bg120)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/bg120.h"

__attribute__((constructor))
static void sb_sean_register(void) {
    StageBg_Register(STAGE_BG_SEAN, (StageBgCallbacks){
        .on_enter = BG120,
        .on_tick  = BG120,
    });
}
