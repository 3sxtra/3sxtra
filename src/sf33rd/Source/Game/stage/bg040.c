/**
 * @file bg_040.c
 * Main Street, England
 */

#include "sf33rd/Source/Game/stage/bg040.h"
#include "common.h"
#include "game_state.h"
#include "sf33rd/Source/Game/effect/eff06.h"
#include "sf33rd/Source/Game/effect/eff12.h"
#include "sf33rd/Source/Game/effect/eff44.h"
#include "sf33rd/Source/Game/effect/eff53.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/stage/ta_sub.h"
#include "sf33rd/Source/Game/system/work_sys.h"

/** @brief Main handler for Main Street, England stage. */
void BG040() {
    bgw_ptr = &bg_w.bgw[1];
    bg0402();
    bgw_ptr = &bg_w.bgw[0];
    bg0401();
    zoom_ud_check();
    bg_pos_hosei2();
    Bg_Family_Set_2();
}

/** @brief Background layer handler for Main Street, England. */
void bg0401() {
    void (*bg0401_jmp[3])() = { bg0401_init00, bg0401_init01, bg_move_common };
    if (bgw_ptr->r_no_0 >= 3)
        return;
    bg0401_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Main Street, England. */
void bg0401_init00() {
    bgw_ptr->r_no_0++;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x200;
    bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->zuubun = 0;
    effect_12_init(1);
    effect_06_init();
    effect_44_init(3);
    effect_53_init();
}

/** @brief Secondary init for background layer (Main Street, England). */
void bg0401_init01() {
    bgw_ptr->r_no_0++;
}

/** @brief Background layer handler for Main Street, England. */
void bg0402() {
    void (*bg0402_jmp[3])() = { bg0402_init00, bg0402_init01, bg_base_move_common };
    if (bgw_ptr->r_no_0 >= 3)
        return;
    bg0402_jmp[bgw_ptr->r_no_0]();
}

/** @brief Initialize background layer for Main Street, England. */
void bg0402_init00() {
    bgw_ptr->r_no_0++;
    bgw_ptr->old_pos_x = bgw_ptr->xy[0].disp.pos = bgw_ptr->pos_x_work = 0x200;
    bgw_ptr->hos_xy[0].cal = bgw_ptr->wxy[0].cal = bgw_ptr->xy[0].cal;
    bgw_ptr->zuubun = 0;
}

/** @brief Secondary init for background layer (Main Street, England). */
void bg0402_init01() {
    bgw_ptr->r_no_0++;
}
