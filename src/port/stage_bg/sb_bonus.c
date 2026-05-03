/**
 * @file sb_bonus.c
 * StageBackground wrapper – Bonus Stage (bonus_bg)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/stage_bonus_1.h"

__attribute__((constructor)) static void sb_bonus_register(void) {
    StageBg_Register(STAGE_BG_BONUS,
                     (StageBgCallbacks) {
                         .on_enter = Bonus_bg,
                         .on_tick = Bonus_bg,
                     });
}
