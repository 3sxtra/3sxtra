/**
 * @file sb_necro.c
 * StageBackground wrapper – Necro/Q (Russia) (bg050)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/bg050.h"

__attribute__((constructor))
static void sb_necro_register(void) {
    StageBg_Register(STAGE_BG_NECRO, (StageBgCallbacks){
        .on_enter = BG050,
        .on_tick  = BG050,
    });
}
