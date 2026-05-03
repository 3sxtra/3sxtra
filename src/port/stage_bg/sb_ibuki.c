/**
 * @file sb_ibuki.c
 * StageBackground wrapper – Ibuki (Japan) (bg070)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/stage_070_a_road_in_kyoto_japan.h"

__attribute__((constructor)) static void sb_ibuki_register(void) {
    StageBg_Register(STAGE_BG_IBUKI,
                     (StageBgCallbacks) {
                         .on_enter = BG070,
                         .on_tick = BG070,
                     });
}
