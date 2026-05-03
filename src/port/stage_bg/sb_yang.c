/**
 * @file sb_yang.c
 * StageBackground wrapper – Yang (Hong Kong Alt) (bg100)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/stage_100_shopping_district_hong_kong.h"

__attribute__((constructor)) static void sb_yang_register(void) {
    StageBg_Register(STAGE_BG_YANG,
                     (StageBgCallbacks) {
                         .on_enter = BG100,
                         .on_tick = BG100,
                     });
}
