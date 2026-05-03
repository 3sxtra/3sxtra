/**
 * @file sb_ryu.c
 * StageBackground wrapper – Ryu (Japan) (bg020)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/stage_020_suzaku_castle_rooftop_japan.h"

__attribute__((constructor)) static void sb_ryu_register(void) {
    StageBg_Register(STAGE_BG_RYU,
                     (StageBgCallbacks) {
                         .on_enter = BG020,
                         .on_tick = BG020,
                     });
}
