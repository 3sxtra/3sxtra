/**
 * @file sb_akuma.c
 * StageBackground wrapper – Akuma (Japan) (bg140)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/bg140.h"

__attribute__((constructor)) static void sb_akuma_register(void) {
    StageBg_Register(STAGE_BG_AKUMA,
                     (StageBgCallbacks) {
                         .on_enter = BG140,
                         .on_tick = BG140,
                     });
}
