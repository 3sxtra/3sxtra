/**
 * @file sb_hugo.c
 * StageBackground wrapper – Hugo (Germany) (bg060)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/stage_060_home_sweet_home_germany.h"

__attribute__((constructor)) static void sb_hugo_register(void) {
    StageBg_Register(STAGE_BG_HUGO,
                     (StageBgCallbacks) {
                         .on_enter = BG060,
                         .on_tick = BG060,
                     });
}
