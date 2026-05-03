/**
 * @file sb_dudley.c
 * StageBackground wrapper – Dudley (England) (bg040)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/stage_040_main_street_england.h"

__attribute__((constructor)) static void sb_dudley_register(void) {
    StageBg_Register(STAGE_BG_DUDLEY,
                     (StageBgCallbacks) {
                         .on_enter = BG040,
                         .on_tick = BG040,
                     });
}
