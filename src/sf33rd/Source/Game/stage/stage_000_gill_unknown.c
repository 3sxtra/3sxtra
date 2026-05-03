/**
 * @file bg_000.c
 * Gill Stage, Unknown
 */

#include "sf33rd/Source/Game/stage/stage_000_gill_unknown.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/animation/appear.h"
#include "sf33rd/Source/Game/effect/effect_06_data_screen_object.h"
#include "sf33rd/Source/Game/effect/effect_44_screen_object_multiple.h"
#include "sf33rd/Source/Game/effect/effect_60_flash_screen_flash.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"

/** @brief Main handler for Gill's Stage stage. */
void BG000() {
    bgw_ptr = &g_state.bg_w.bgw[1];
    bg0001();
    bgw_ptr = &g_state.bg_w.bgw[0];
    bg0000();
    zoom_ud_check();
    bg_pos_adjust2();
    Bg_Family_Set();
}

/** @brief Background layer handler for Gill's Stage. */
void bg0001() {
    void (*bg0002_jmp[3])() = { bg0001_init00, bg0000_demo, bg_base_move_common };
    if (bgw_ptr->r_no_0 >= 3)
        return;
    bg0002_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Gill's Stage. */
void bg0001_init00() {
    bgw_ptr->r_no_0++;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x1D0;
    bgw_ptr->hos_xy[0].disp.pos = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->zuubun = 0;
    g_state.Gill_Appear_Flag = gill_appear_check();

    if (g_state.Gill_Appear_Flag == 0) {
        Allocate_Texture_Cache(0x10);
        setup_GILL_Opening_Ceremony();
    }

    effect_06_init();
    effect_44_init(7);
    effect_60_init(2);

    if (g_state.bg_w.area) {
        bgw_ptr->r_no_0 = 2;
        return;
    } else if (g_state.Gill_Appear_Flag) {
        bgw_ptr->r_no_0 = 2;
        return;
    }

    if (g_state.plw->player_number == 0) {
        bgw_ptr->u_line = 0;
        bgw_ptr->xy[0].cal += bgw_ptr->speed_x * 0xE0;
        bgw_ptr->old_pos_x = bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
        return;
    }

    bgw_ptr->u_line = 1;
    bgw_ptr->xy[0].cal -= bgw_ptr->speed_x * 0xC0;
    bgw_ptr->old_pos_x = bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
}

/** @brief Background layer handler for Gill's Stage. */
void bg0000() {
    void (*bg0000_jmp[3])() = { bg0000_init00, bg0000_demo, bg_move_common };
    if (bgw_ptr->r_no_0 >= 3)
        return;
    bg0000_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Gill's Stage. */
void bg0000_init00() {
    bgw_ptr->r_no_0++;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x1D0;
    bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->zuubun = 0;

    if (g_state.Gill_Appear_Flag) {
        bgw_ptr->r_no_0 = 2;
        g_state.bg_app = 0;
        return;
    }

    g_state.bg_app = 1;

    if (g_state.plw->player_number == 0) {
        bgw_ptr->u_line = 0;
        bgw_ptr->xy[0].cal += bgw_ptr->speed_x * 0xE0;
        bgw_ptr->old_pos_x = bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
        return;
    }

    bgw_ptr->u_line = 1;
    bgw_ptr->xy[0].cal -= bgw_ptr->speed_x * 0xC0;
    bgw_ptr->old_pos_x = bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
}

/** @brief Demo/intro animation handler for Gill's Stage. */
void bg0000_demo() {
    switch (bgw_ptr->r_no_1) {
    case 0:
        bgw_ptr->r_no_1++;
        bgw_ptr->free = 0x1E;
        break;

    case 1:
        bgw_ptr->free--;

        if (bgw_ptr->free <= 0) {
            bgw_ptr->r_no_1 += 1;
        }

        break;

    case 2:
        if (bgw_ptr->u_line) {
            bgw_ptr->wxy[0].cal += bgw_ptr->speed_x;

            if (bgw_ptr->wxy[0].disp.pos > 0x1D0) {
                bgw_ptr->r_no_1 += 1;
                bgw_ptr->wxy[0].disp.pos = 0x1D0;
                bgw_ptr->wxy[0].disp.low = 0;
                bgw_ptr->xy[0].cal = bgw_ptr->wxy[0].cal;
                bgw_ptr->old_pos_x = 0x1D0;
                break;
            }
        } else {
            bgw_ptr->wxy[0].cal -= bgw_ptr->speed_x;

            if (bgw_ptr->wxy[0].disp.pos < 0x1D0) {
                bgw_ptr->r_no_1 += 1;
                bgw_ptr->wxy[0].disp.pos = 0x1D0;
                bgw_ptr->wxy[0].disp.low = 0;
                bgw_ptr->xy[0].cal = bgw_ptr->wxy[0].cal;
                bgw_ptr->old_pos_x = 0x1D0;
                break;
            }
        }
        break;

    case 3:
        g_state.bg_app = 0;
        bgw_ptr->r_no_0 = 2;
        break;
    }
}
