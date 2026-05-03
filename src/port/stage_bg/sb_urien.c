/**
 * @file sb_urien.c
 * StageBackground wrapper – Urien (Egypt) (bg130)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/stage_130_ormeca_ruins_mexico.h"

__attribute__((constructor)) static void sb_urien_register(void) {
    StageBg_Register(STAGE_BG_URIEN,
                     (StageBgCallbacks) {
                         .on_enter = BG130,
                         .on_tick = BG130,
                     });
}
