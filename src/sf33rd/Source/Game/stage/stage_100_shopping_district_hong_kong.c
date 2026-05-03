/**
 * @file bg_100.c
 * Shopping District, Hong Kong
 */

#include "sf33rd/Source/Game/stage/stage_100_shopping_district_hong_kong.h"
#include "common.h"
#include "game_state.h"
#include "sf33rd/Source/Game/effect/effect_05_background.h"
#include "sf33rd/Source/Game/effect/effect_06_data_screen_object.h"
#include "sf33rd/Source/Game/effect/effect_29_vanish_timeout.h"
#include "sf33rd/Source/Game/effect/effect_44_screen_object_multiple.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"

/** @brief Main handler for Underground Cave stage. */
void BG100() {
    bgw_ptr = &g_state.bg_w.bgw[1];
    bg1001();
    bgw_ptr = &g_state.bg_w.bgw[0];
    bg1000();
    zoom_ud_check();
    bg_pos_adjust2();
    Bg_Family_Set();
}

/** @brief Background layer handler for Underground Cave. */
void bg1000() {
    void (*bg1000_jmp[2])() = { bg1000_init00, bg_move_common };
    if (bgw_ptr->r_no_0 >= 2)
        return;
    bg1000_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Underground Cave. */
void bg1000_init00() {
    bgw_ptr->r_no_1 = 0;
    bgw_ptr->r_no_0++;
    bgw_ptr->zuubun = 0;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x200;
    bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->hos_xy[0].disp.pos = bgw_ptr->wxy[0].disp.pos - g_state.bg_w.pos_offset;
    bgw_ptr->hos_xy[0].disp.low = 0;
    effect_29_init();
}

/** @brief Background layer handler for Underground Cave. */
void bg1001() {
    void (*bg1001_jmp[2])() = { bg1001_init00, bg_base_move_common };
    if (bgw_ptr->r_no_0 >= 2)
        return;
    bg1001_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Underground Cave. */
void bg1001_init00() {
    bgw_ptr->r_no_1 = 0;
    bgw_ptr->r_no_0++;
    bgw_ptr->zuubun = 0;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x200;
    bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->hos_xy[0].disp.pos = bgw_ptr->wxy[0].disp.pos - g_state.bg_w.pos_offset;
    bgw_ptr->hos_xy[0].disp.low = 0;
    bgw_ptr->wxy[1].cal = 0;
    bgw_ptr->xy[1].cal = 0;
    effect_05_init();
    effect_06_init();
    effect_44_init(5);
}
