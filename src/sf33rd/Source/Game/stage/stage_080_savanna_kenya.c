/**
 * @file bg_080.c
 * Savanna, Kenya
 */

#include "sf33rd/Source/Game/stage/stage_080_savanna_kenya.h"
#include "common.h"
#include "game_state.h"
#include "sf33rd/Source/Game/effect/effect_05_background.h"
#include "sf33rd/Source/Game/effect/effect_06_data_screen_object.h"
#include "sf33rd/Source/Game/effect/effect_21_speed_motion_blur.h"
#include "sf33rd/Source/Game/effect/effect_44_screen_object_multiple.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"

/** @brief Main handler for Savanna, Kenya stage. */
void BG080() {
    bgw_ptr = &g_state.bg_w.bgw[1];
    bg0802();
    bgw_ptr = &g_state.bg_w.bgw[0];
    bg0801();
    bgw_ptr = &g_state.bg_w.bgw[2];
    bg080_sync_common();
    bgw_ptr = &g_state.bg_w.bgw[5];
    bg080_sync_common();
    bgw_ptr = &g_state.bg_w.bgw[6];
    bg080_sync_common();
    zoom_ud_check();
    bg_pos_adjust2();
    Bg_Family_Set();
}

/** @brief Background layer handler for Savanna, Kenya. */
void bg0801() {
    void (*bg0801_jmp[2])() = { bg0801_init00, bg_move_common };
    if (bgw_ptr->r_no_0 >= 2)
        return;
    bg0801_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Savanna, Kenya. */
void bg0801_init00() {
    bgw_ptr->r_no_0++;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x200;
    bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->zuubun = 0;
}

/** @brief Background layer handler for Savanna, Kenya. */
void bg0802() {
    void (*bg0802_jmp[2])() = { bg0802_init00, bg_base_move_common };
    if (bgw_ptr->r_no_0 >= 2)
        return;
    bg0802_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Savanna, Kenya. */
void bg0802_init00() {
    bgw_ptr->r_no_0++;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x200;
    bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->zuubun = 0;
    effect_05_init();
    effect_06_init();
    effect_44_init(0);
    effect_21_init(0);
}

/** @brief Synchronized parallax common handler for Savanna, Kenya. */
void bg080_sync_common() {
    void (*bg080_sync_jmp[2])() = { bg080_sync_init, bg080_sync_move };
    if (bgw_ptr->r_no_0 >= 2)
        return;
    bg080_sync_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize synchronized parallax layer for Savanna, Kenya. */
void bg080_sync_init() {
    bgw_ptr->r_no_0++;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x200;
    bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->zuubun = 0;
    bgw_ptr->y_limit = bgw_ptr->y_limit2 = 0xF0;
    bgw_ptr->pos_y_work = 0;
    bgw_ptr->xy[1].disp.pos = 0;

    switch (bgw_ptr->fam_no) {
    case 2:
        bgw_ptr->speed_x = 0xC000;
        bgw_ptr->speed_y = 0xFC00;
        break;

    case 5:
        bgw_ptr->speed_x = 0xB000;
        bgw_ptr->speed_y = 0xFC00;
        break;

    case 6:
        bgw_ptr->speed_x = 0x7000;
        bgw_ptr->speed_y = 0xF100;
        break;
    }

    sync_fam_set3(bgw_ptr->fam_no);
}

/** @brief Per-frame movement handler for Savanna, Kenya layer. */
void bg080_sync_move() {
    bg_x_move_check();
    bg_y_move_check();
    sync_fam_set3(bgw_ptr->fam_no);
}
