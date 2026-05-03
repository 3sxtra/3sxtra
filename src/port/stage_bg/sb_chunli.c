/**
 * @file sb_chunli.c
 * StageBackground wrapper – Chun-Li (China) (bg150)
 */

#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/stage/stage_150_chinese_restaurant_china.h"

__attribute__((constructor)) static void sb_chunli_register(void) {
    StageBg_Register(STAGE_BG_CHUNLI,
                     (StageBgCallbacks) {
                         .on_enter = BG150,
                         .on_tick = BG150,
                     });
}
