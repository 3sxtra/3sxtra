/**
 * @file sb_yun.c
 * StageBackground wrapper – Yun (Hong Kong) (bg030)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/bg030.h"

__attribute__((constructor))
static void sb_yun_register(void) {
    StageBg_Register(STAGE_BG_YUN, (StageBgCallbacks){
        .on_enter = BG030,
        .on_tick  = BG030,
    });
}
