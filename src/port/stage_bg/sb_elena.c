/**
 * @file sb_elena.c
 * StageBackground wrapper – Elena (Kenya) (bg080)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/stage_080_savanna_kenya.h"

__attribute__((constructor)) static void sb_elena_register(void) {
    StageBg_Register(STAGE_BG_ELENA,
                     (StageBgCallbacks) {
                         .on_enter = BG080,
                         .on_tick = BG080,
                     });
}
