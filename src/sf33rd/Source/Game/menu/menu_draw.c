/**
 * @file menu_draw.c
 * @brief Menu display/rendering helper functions.
 *
 * Contains functions that render UI elements without owning
 * state-machine logic.  Split from menu.c for maintainability.
 */

#include "common.h"
#include "sf33rd/Source/Game/effect/eff10.h"
#include "sf33rd/Source/Game/effect/eff66.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/menu/menu_internal.h"
#include "sf33rd/Source/Game/system/pause.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "structs.h"

/* RmlUi Phase 3 bypass */
#include "port/sdl/rmlui_phase3_toggles.h"
extern bool use_rmlui;

/* ---------- imgSelectGameButton ---------- */

/** @brief Draw two select-game button images. */
void imgSelectGameButton() {
    dispButtonImage2(0x74, 0x6B, 0x18, 0x20, 0x1A, 0, 4);
    dispButtonImage2(0xB2, 0x6B, 0x18, 0x20, 0x1A, 0, 5);
}

/* ---------- Setup_Win_Lose_OBJ ---------- */

/** @brief Set up Win/Lose result objects for VS screen. */
void Setup_Win_Lose_OBJ() {
    s16 x[2];

    if (WINNER == 0) {
        x[0] = 26;
        x[1] = 27;
    } else {
        x[0] = 27;
        x[1] = 26;
    }

    effect_66_init(140, x[0], 0, 0, 71, 12, 0);
    Order[140] = 3;
    Order_Timer[140] = 1;
    effect_66_init(141, x[1], 0, 0, 71, 13, 0);
    Order[141] = 3;
    Order_Timer[141] = 1;
    effect_66_init(142, 26, 0, 0, 71, 14, 1);
    Order[142] = 3;
    Order_Timer[142] = 1;
    effect_66_init(143, 27, 0, 0, 71, 14, 01);
    Order[143] = 3;
    Order_Timer[143] = 1;
}

/* ---------- Setup_Button_Sub ---------- */

/** @brief Set up button config display at given position. */
void Setup_Button_Sub(s16 x, s16 y, s16 master_player) {
    s16 ix;
    s16 s1;

    effect_10_init(0, 7, 99, 0, master_player, x + 7, y + 20);
    effect_10_init(0, 7, 99, 1, master_player, x + 29, y + 20);

    for (ix = 0; ix < 8; ix++, s1 = y += 2) {
        effect_10_init(0, 5, ix, ix, master_player, x, y);
        effect_10_init(1, 5, ix, ix, master_player, x + 22, y);
        effect_10_init(0, 2, ix, Convert_Buff[1][0][ix], master_player, x + 3, y);
        effect_10_init(1, 2, ix, Convert_Buff[1][1][ix], master_player, x + 25, y);
    }

    effect_10_init(0, 3, 8, Convert_Buff[1][0][8], master_player, x, y);
    effect_10_init(1, 3, 8, Convert_Buff[1][1][8], master_player, x + 22, y);
    effect_10_init(0, 4, 9, 0, master_player, x, y + 2);
    effect_10_init(1, 4, 9, 0, master_player, x + 22, y + 2);
    effect_10_init(0, 0, 10, 2, master_player, x, y + 4);
    effect_10_init(1, 0, 10, 2, master_player, x + 22, y + 4);
}

/* ---------- Flash_1P_or_2P ---------- */

/** @brief Flash 1P/2P indicator while paused. */
void Flash_1P_or_2P(struct _TASK* task_ptr) {
    switch (task_ptr->r_no[3]) {
    case 0:
        if (--task_ptr->free[0]) {
            if (!use_rmlui || !rmlui_screen_pause) {
                if (Pause_ID == 0) {
                    SSPutStr2(20, 9, 9, "1P PAUSE");
                } else {
                    SSPutStr2(20, 9, 9, "2P PAUSE");
                }
            }
            break;
        }

        task_ptr->r_no[3] = 1;
        task_ptr->free[0] = 0x1E;
        break;

    case 1:
        if (--task_ptr->free[0] == 0) {
            task_ptr->r_no[3] = 0;
            task_ptr->free[0] = 0x3C;
        }

        break;
    }
}

/* ---------- Training_Disp_Sub ---------- */

/** @brief Training display sub-routine — show current settings. */
void Training_Disp_Sub(struct _TASK* task_ptr) {
    if (Mode_Type == MODE_NORMAL_TRAINING || Mode_Type == MODE_TRIALS) {
        task_ptr->r_no[1] = 1;
        Training_Index = 0;
        return;
    }

    task_ptr->r_no[1] = 2;
    Training_Index = 1;
}
