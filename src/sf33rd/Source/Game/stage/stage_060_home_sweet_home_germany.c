/**
 * @file bg_060.c
 * Home Sweet Home, Germany
 */

#include "sf33rd/Source/Game/stage/stage_060_home_sweet_home_germany.h"
#include "common.h"
#include "game_state.h"
#include "sf33rd/Source/Game/effect/effect_05_background.h"
#include "sf33rd/Source/Game/effect/effect_24_quake_horizontal_vertical.h"
#include "sf33rd/Source/Game/effect/effect_44_screen_object_multiple.h"
#include "sf33rd/Source/Game/effect/effect_60_flash_screen_flash.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"

/** @brief Main handler for Home Sweet Home, Germany stage. */
void BG060() {
    bgw_ptr = &g_state.bg_w.bgw[1];
    bg0602();
    bgw_ptr = &g_state.bg_w.bgw[0];
    bg0601();
    bgw_ptr = &g_state.bg_w.bgw[2];
    bg0603();
    zoom_ud_check();
    bg_pos_adjust2();
    Bg_Family_Set();
}

/** @brief Background layer handler for Home Sweet Home, Germany. */
void bg0601() {
    void (*bg0601_jmp[2])() = { bg0601_init00, bg_move_common };
    if (bgw_ptr->r_no_0 >= 2)
        return;
    bg0601_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Home Sweet Home, Germany. */
void bg0601_init00() {
    bgw_ptr->r_no_1 = 0;
    bgw_ptr->r_no_0++;
    bgw_ptr->zuubun = 0;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x200;
    bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->hos_xy[0].disp.pos = bgw_ptr->wxy[0].disp.pos - g_state.bg_w.pos_offset;
    bgw_ptr->hos_xy[0].disp.low = 0;
    bgw_ptr->wxy[1].cal = 0;
    bgw_ptr->xy[1].cal = 0;
}

/** @brief Background layer handler for Home Sweet Home, Germany. */
void bg0602() {
    void (*bg0602_jmp[2])() = { bg0602_init00, bg_base_move_common };
    if (bgw_ptr->r_no_0 >= 2)
        return;
    bg0602_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Home Sweet Home, Germany. */
void bg0602_init00() {
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
    effect_60_init(0);
    effect_60_init(1);
    effect_44_init(1);
    effect_24_init();
}

/** @brief Background layer handler for Home Sweet Home, Germany. */
void bg0603() {
    switch (bgw_ptr->r_no_0) {
    case 0:
        bgw_ptr->r_no_0++;
        bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x200;
        bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
        bgw_ptr->xy[1].disp.pos = bgw_ptr->pos_y_work = 0;
        bgw_ptr->fam_no = 2;
        bgw_ptr->xy[0].disp.low = bgw_ptr->xy[1].disp.low = 0;
        bgw_ptr->y_limit = bgw_ptr->y_limit2 = 0xF0;
        bgw_ptr->speed_x = 0x10800;
        bgw_ptr->speed_y = 0x10000;
        sync_fam_set3(2);
        break;

    case 1:
        bg_x_move_check();
        bg_y_move_check();
        sync_fam_set3(2);
        break;
    }
}
