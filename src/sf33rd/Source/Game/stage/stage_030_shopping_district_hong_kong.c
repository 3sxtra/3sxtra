/**
 * @file bg_030.c
 * Shopping District, Hong Kong
 */

#include "sf33rd/Source/Game/stage/stage_030_shopping_district_hong_kong.h"
#include "common.h"
#include "game_state.h"
#include "sf33rd/Source/Game/effect/effect_05_background.h"
#include "sf33rd/Source/Game/effect/effect_06_data_screen_object.h"
#include "sf33rd/Source/Game/effect/effect_71_time_table_slow.h"
#include "sf33rd/Source/Game/effect/effect_l2_direction_table.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"

/** @brief Main handler for Shopping District, Hong Kong stage. */
void BG030() {
    bgw_ptr = &g_state.bg_w.bgw[1];
    bg0301();
    bgw_ptr = &g_state.bg_w.bgw[0];
    bg0300();
    zoom_ud_check();
    bg_pos_adjust2();
    Bg_Family_Set();
}

/** @brief Background layer handler for Shopping District, Hong Kong. */
void bg0300() {
    void (*bg0300_jmp[2])() = { bg0300_init00, bg_move_common };
    if (bgw_ptr->r_no_0 >= 2)
        return;
    bg0300_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Shopping District, Hong Kong. */
void bg0300_init00() {
    bgw_ptr->r_no_0++;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x200;
    bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->zuubun = 0;
}

/** @brief Background layer handler for Shopping District, Hong Kong. */
void bg0301() {
    void (*bg0301_jmp[2])() = { bg0301_init00, bg_base_move_common };
    if (bgw_ptr->r_no_0 >= 2)
        return;
    bg0301_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Shopping District, Hong Kong. */
void bg0301_init00() {
    bgw_ptr->r_no_0++;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x200;
    bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->zuubun = 0;
    effect_05_init();
    effect_06_init();
    effect_71_init();
    effect_L2_init();
}
