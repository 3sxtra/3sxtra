/**
 * @file opening_bg.c
 * @brief Opening BG layer dispatch — scrolling background animation sub-routines.
 *
 * Manages the three scrolling background layers (`op_bg0_*`, `op_bg1_*`,
 * `op_bg2_*`) during the opening cinematic.  Each layer has a dispatch
 * function and multiple sub-routines for tile loading, scrolling, colour
 * fading, and screen-shake effects.
 *
 * Part of the opening module.  Split from opening.c for maintainability.
 */

#include "sf33rd/Source/Game/opening/opening.h"
#include "game_state.h"
#include "common.h"

/* Phase 3 RmlUi bypass */
#include "port/sdl/rmlui/rmlui_copyright.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui/rmlui_title_screen.h"
#include <stdbool.h>

#include "port/config/config.h"
#include "port/rendering/renderer.h"
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Common/MemMan.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/demo/demo00.h"
#include "sf33rd/Source/Game/effect/effect_36_data_table_debug.h"
#include "sf33rd/Source/Game/effect/effect_48_numeric_counter.h"
#include "sf33rd/Source/Game/effect/effect_e1_visual_generic.h"
#include "sf33rd/Source/Game/effect/effect_f5_visual_generic.h"
#include "sf33rd/Source/Game/effect/effect_f6_move_data_table.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/io/afs_loader.h"
#include "sf33rd/Source/Game/opening/op_sub.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/color_palette.h"
#include "sf33rd/Source/Game/rendering/rendering_transform.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/sound/sound_effects.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/system/ram_control.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
#include "sf33rd/Source/Game/system/system_subroutines_2.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/hud_subroutines.h"

static void op_bg0_0000(s16 r_index);
static void op_bg0_0001(s16 r_index);
static void op_bg0_0002(s16 r_index);
static void op_bg0_0003(s16 r_index);
static void op_bg0_0004(s16 r_index);
static void op_bg0_0005(s16 r_index);
static void op_bg0_0006(s16 r_index);
static void op_bg0_0007(s16 r_index);
static void op_bg0_0008(s16 r_index);
static void op_bg0_0010(s16 r_index);
static void op_bg0_0011(s16 r_index);
static void op_bg0_0012(s16 r_index);
static void op_bg0_0013(s16 r_index);
static void op_bg0_0014(s16 r_index);
static void op_bg0_0015(s16 r_index);
static void op_bg0_0016(s16 r_index);
static void op_bg1_0000(s16 r_index);
static void op_bg2_0000(s16 r_index);
static void op_bg1_0001(s16 r_index);
static void op_bg2_0001(s16 r_index);
static void op_bg1_0002(s16 r_index);
static void op_bg2_0002(s16 r_index);
static void op_bg1_0003(s16 r_index);
static void op_bg2_0003(s16 r_index);

/** @brief Dispatch BG-move callbacks for all three background layers. */
void op_bg_move(s16 r_index) {
    op_bg0_move(r_index);
    op_bg1_move(r_index);
    op_bg2_move(r_index);
}

/** @brief Dispatch background layer 0 sub-routine by op_w state. */
void op_bg0_move(s16 r_index) {
    void (*op_bg0_move_jp[OP_BG0_DISPATCH_COUNT])(
        s16) = { op_bg0_0000, op_bg0_0001, op_bg0_0000, op_bg0_0001, op_bg0_0001, op_bg0_0001, op_bg0_0000, op_bg0_0001,
                 op_bg0_0015, op_bg0_0001, op_bg0_0000, op_bg0_0001, op_bg0_0001, op_bg0_0000, op_bg0_0015, op_bg0_0001,
                 op_bg0_0000, op_bg0_0000, op_bg0_0001, op_bg0_0001, op_bg0_0000, op_bg0_0001, op_bg0_0001, op_bg0_0000,
                 op_bg0_0000, op_bg0_0000, op_bg0_0000, op_bg0_0000, op_bg0_0000, op_bg0_0000, op_bg0_0001, op_bg0_0001,
                 op_bg0_0001, op_bg0_0001, op_bg0_0000, op_bg0_0001, op_bg0_0001, op_bg0_0000, op_bg0_0001, op_bg0_0001,
                 op_bg0_0000, op_bg0_0002, op_bg0_0003, op_bg0_0002, op_bg0_0003, op_bg0_0002, op_bg0_0003, op_bg0_0002,
                 op_bg0_0003, op_bg0_0002, op_bg0_0003, op_bg0_0002, op_bg0_0003, op_bg0_0000, op_bg0_0004, op_bg0_0001,
                 op_bg0_0001, op_bg0_0004, op_bg0_0005, op_bg0_0002, op_bg0_0001, op_bg0_0002, op_bg0_0001, op_bg0_0002,
                 op_bg0_0006, op_bg0_0001, op_bg0_0001, op_bg0_0001, op_bg0_0007, op_bg0_0008, op_bg0_0000, op_bg0_0000,
                 op_bg0_0001, op_bg0_0001, op_bg0_0000, op_bg0_0001, op_bg0_0001, op_bg0_0000, op_bg0_0000, op_bg0_0004,
                 op_bg0_0001, op_bg0_0004, op_bg0_0001, op_bg0_0002, op_bg1_0003, op_bg0_0002, op_bg0_0010, op_bg0_0002,
                 op_bg0_0011, op_bg0_0012, op_bg0_0013, op_bg0_0014, op_bg0_0002, op_bg0_0016 };

    opw_ptr = &op_w.bgw[0];
    bgw_ptr = &g_state.bg_w.bgw[0];

    if (r_index < 0 || r_index >= OP_BG0_DISPATCH_COUNT) {
        return;
    }

    op_bg0_move_jp[r_index](r_index);
}

/** @brief BG0 sub 0 — initial tile load and screen setup. */
static void op_bg0_0000(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        Bg_Off_W(1);
        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0;
        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 1 — animated tile-map updates driven by op_w.index. */
static void op_bg0_0001(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 1;
        bgw_ptr->frame_deff = 0;
        Bg_On_W(1);
        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0;

        switch (r_index) {
        case 0x1:
            oh_bg_blk_w(op_w.bgw, 0x30, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x31, 2, 0, 1);
            break;

        case 0x3:
            oh_bg_blk_wv(op_w.bgw, 0x45, 1, 0, 1);
            oh_bg_blk_wv(op_w.bgw, 0x46, 2, 0, 1);
            op_w.bgw[0].map[1][0].col.full = -0x01000000;
            op_w.bgw[0].map[2][0].col.full = -0x01000000;
            break;

        case 0x4:
        case 0x16:
        case 0x21:
            oh_bg_blk_wh(op_w.bgw, 0x35, 1, 0, 1);
            oh_bg_blk_wh(op_w.bgw, 0x34, 2, 0, 1);
            break;

        case 0x7:
            oh_bg_blk_w(op_w.bgw, 0x3F, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x40, 2, 0, 1);
            break;

        case 0x9:
        case 0x48:
            oh_bg_blk_w(op_w.bgw, 0x36, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x37, 2, 0, 1);
            break;

        case 0xB:
        case 0x1E:
            oh_bg_blk_wh(op_w.bgw, 0x33, 1, 0, 1);
            oh_bg_blk_wh(op_w.bgw, 0x32, 2, 0, 1);
            break;

        case 0xC:
            oh_bg_blk_w(op_w.bgw, 0x43, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x44, 2, 0, 1);
            break;

        case 0xF:
            oh_bg_blk_wh(op_w.bgw, 0x3C, 2, 0, 1);
            oh_bg_blk_wh(op_w.bgw, 0x3D, 1, 0, 1);
            break;

        case 0x12:
            oh_bg_blk_wv(op_w.bgw, 0x32, 1, 0, 1);
            oh_bg_blk_wv(op_w.bgw, 0x33, 2, 0, 1);
            break;

        case 0x13:
            oh_bg_blk_w(op_w.bgw, 0x47, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x48, 2, 0, 1);
            break;

        case 0x15:
            oh_bg_blk_w(op_w.bgw, 0x38, 1, 0, 1);
            op_w.bgw[0].map[2][0].g_no = 0;
            break;

        case 0x1F:
            oh_bg_blk_w(op_w.bgw, 0x49, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x4A, 2, 0, 1);
            break;

        case 0x20:
            oh_bg_blk_w(op_w.bgw, 0x4B, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x4C, 2, 0, 1);
            break;

        case 0x23:
            oh_bg_blk_w(op_w.bgw, 0x59, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x5A, 2, 0, 1);
            break;

        case 0x24:
            oh_bg_blk_w(op_w.bgw, 0x22, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x23, 2, 0, 1);
            break;

        case 0x26:
            oh_bg_blk_w(op_w.bgw, 0x3D, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x3E, 2, 0, 1);
            break;

        case 0x27:
            oh_bg_blk_w(op_w.bgw, 0x3D, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x3E, 2, 0, 1);
            break;

        case 0x37:
        case 0x38:
            oh_bg_blk_w(op_w.bgw, 0x4D, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x4E, 2, 0, 1);
            break;

        case 0x3C:
        case 0x3E:
        case 0x50:
        case 0x52:
            oh_bg_blk_w(op_w.bgw, 0x57, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x58, 2, 0, 1);
            break;

        case 0x41:
            oh_bg_blk_w(op_w.bgw, 0x4F, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x50, 2, 0, 1);
            break;

        case 0x42:
            oh_bg_blk_w(op_w.bgw, 0x51, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x52, 2, 0, 1);
            break;

        case 0x49:
            oh_bg_blk_w(op_w.bgw, 0x53, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x54, 2, 0, 1);
            break;

        case 0x4B:
            op_w.bgw[0].map[1][0].g_no = 0;
            op_w.bgw[0].map[2][0].g_no = 0;
            oh_bg_blk_whv(op_w.bgw, 0x53, 2, 0, 1);
            oh_bg_blk_whv(op_w.bgw, 0x54, 1, 0, 1);
            break;

        case 0x4C:
            oh_bg_blk_w(op_w.bgw, 0x55, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x56, 2, 0, 1);
            break;
        }

        /* fallthrough */

    case 1:
        bgw_ptr->free -= 1;

        if (bgw_ptr->free <= 0) {
            bgw_ptr->free = 1;
            bgw_ptr->frame_deff += 1;
            bgw_ptr->frame_deff &= 0xF;
            bgw_ptr->xy[1].disp.pos += op_quake_y_tbl0[bgw_ptr->frame_deff];
        }

        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 2 — timed tile transitions. */
static void op_bg0_0002(s16 r_index) {
    PAL_CURSOR beta_poly;
    PAL_CURSOR_P beta_p[4];
    PAL_CURSOR_COL beta_col[4];

    beta_poly.p = beta_p;
    beta_poly.col = beta_col;
    beta_poly.num = 4;
    beta_col[0].color = beta_col[1].color = beta_col[2].color = beta_col[3].color = 0xFF000000;
    beta_p[0].x = beta_p[2].x = 0.0f;
    beta_p[1].x = beta_p[3].x = 384.0f;
    beta_p[0].y = beta_p[1].y = 0.0f;
    beta_p[2].y = beta_p[3].y = 224.0f;

    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;

        Bg_Off_W(1);

        if (r_index == 87) {
            Zoomf_Init();
        }

        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0;

        if (!No_Trans) {
            Renderer_Queue2DPrimitive((f32*)beta_poly.p, PrioBase[75], (uintptr_t)beta_poly.col[0].color, 0);
        }

        break;

    case 1:
        if (!No_Trans) {
            Renderer_Queue2DPrimitive((f32*)beta_poly.p, PrioBase[75], (uintptr_t)beta_poly.col[0].color, 0);
        }

        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 3 — tile transitions with palette load. */
static void op_bg0_0003(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 1;
        bgw_ptr->frame_deff = 0;
        Bg_On_W(1);
        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0;

        switch (r_index) {
        case 0x2A:
            oh_bg_blk_w(op_w.bgw, 0x24, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x25, 2, 0, 1);
            break;

        case 0x2C:
            oh_bg_blk_w(op_w.bgw, 0x26, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x27, 2, 0, 1);
            break;

        case 0x2E:
            oh_bg_blk_w(op_w.bgw, 0x28, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x29, 2, 0, 1);
            break;

        case 0x30:
            oh_bg_blk_w(op_w.bgw, 0x2A, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x2B, 2, 0, 1);
            break;

        case 0x32:
            oh_bg_blk_w(op_w.bgw, 0x2C, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x2D, 2, 0, 1);
            break;

        case 0x34:
            oh_bg_blk_w(op_w.bgw, 0x2E, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x2F, 2, 0, 1);
            break;
        }

        break;

    case 1:
        break;
    }

    op_scrn_pos_set2(0);
}

const s32 ot_bg0_0004_tbl[OP_COLOR_FADE_STAGES] = { 0xFF00A0B0, 0xFF005888, 0xFF00A0B0,
                                                    0xFF005888, 0xFF000058, 0xFF000000 };

/** @brief BG0 sub 4 — progressive colour fade with tile updates. */
static void op_bg0_0004(s16 r_index) {
    PAL_CURSOR beta_poly;
    PAL_CURSOR_P beta_p[4];
    PAL_CURSOR_COL beta_col[4];

    beta_poly.p = beta_p;
    beta_poly.col = beta_col;
    beta_poly.num = 4;
    beta_col[0].color = beta_col[1].color = beta_col[2].color = beta_col[3].color = 0xFF000000;
    beta_p[0].x = beta_p[2].x = 0.0f;
    beta_p[1].x = beta_p[3].x = 384.0f;
    beta_p[0].y = beta_p[1].y = 0.0f;
    beta_p[2].y = beta_p[3].y = 224.0f;

    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 1;
        bgw_ptr->frame_deff = 0;
        Bg_Off_W(1);
        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0;

        if (!No_Trans) {
            Renderer_Queue2DPrimitive((f32*)beta_poly.p, PrioBase[75], (uintptr_t)beta_poly.col[0].color, 0);
        }

        break;

    case 1:
        if (!No_Trans) {
            Renderer_Queue2DPrimitive((f32*)beta_poly.p, PrioBase[75], (uintptr_t)beta_poly.col[0].color, 0);
        }

        if (!op_scrn_end) {
            break;
        }

        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 1;
        bgw_ptr->l_limit = 0;

        /* fallthrough */

    case 2:
        bgw_ptr->free -= 1;

        if (bgw_ptr->free <= 0) {
            bgw_ptr->l_limit += 1;

            if (bgw_ptr->l_limit >= OP_COLOR_FADE_STAGES) {
                opw_ptr->r_no_0 += 1;
            } else {
                bgw_ptr->free = 1;
                beta_col[0].color = beta_col[1].color = beta_col[2].color = beta_col[3].color =
                    ot_bg0_0004_tbl[bgw_ptr->l_limit];
            }
        }

        if (!No_Trans) {
            Renderer_Queue2DPrimitive((f32*)beta_poly.p, PrioBase[75], (uintptr_t)beta_poly.col[0].color, 0);
        }

        break;

    case 3:
        beta_col[0].color = beta_col[1].color = beta_col[2].color = beta_col[3].color = ot_bg0_0004_tbl[5];

        if (!No_Trans) {
            Renderer_Queue2DPrimitive((f32*)beta_poly.p, PrioBase[75], (uintptr_t)beta_poly.col[0].color, 0);
        }

        break;
    }

    op_scrn_pos_set2(0);
}

const s16 op_bg0_0005_tbl[OP_QUAKE_STEP_COUNT] = { 0x0008, 0xFFF8, 0x0007, 0xFFF9, 0x0005, 0xFFFB, 0x0004, 0xFFFC,
                                                   0x0002, 0xFFFE, 0x0001, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000 };

/** @brief BG0 sub 5 — screen shake via quake table and tile updates. */
static void op_bg0_0005(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 1;
        bgw_ptr->frame_deff = 0;
        Bg_On_W(1);
        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0;
        oh_bg_blk_w(op_w.bgw, 5, 1, 0, 0);
        oh_bg_blk_w(op_w.bgw, 6, 2, 0, 0);
        Zoom_Value_Set(0x40);
        bgw_ptr->frame_deff = 12;
        Frame_Up(0xC0, 0xE0, bgw_ptr->frame_deff);
        break;

    case 1:
        bgw_ptr->frame_deff -= 2;

        if (bgw_ptr->frame_deff <= 0) {
            opw_ptr->r_no_0 += 1;
            Zoomf_Init();
            Zoom_Value_Set(0x40);
            bgw_ptr->free = 0;
            bgw_ptr->wxy[0].disp.pos += op_bg0_0005_tbl[bgw_ptr->free];
            bgw_ptr->xy[1].disp.pos += op_bg0_0005_tbl[bgw_ptr->free];
        } else {
            Frame_Down(0xC0, 0xE0, 2);
        }

        break;

    case 2:
        bgw_ptr->free += 1;

        if (bgw_ptr->free >= OP_QUAKE_STEP_COUNT) {
            opw_ptr->r_no_0 += 1;
        } else {
            bgw_ptr->wxy[0].disp.pos += op_bg0_0005_tbl[bgw_ptr->free];
            bgw_ptr->xy[1].disp.pos += op_bg0_0005_tbl[bgw_ptr->free];
        }

        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 6 — scrolling tile strip animation. */
static void op_bg0_0006(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 0;
        Bg_On_W(1);
        bgw_ptr->wxy[0].cal = 0xC00000;
        bgw_ptr->xy[1].cal = 0;
        oh_bg_blk_w(op_w.bgw, 0xD, 0, 0, 0);
        oh_bg_blk_w(op_w.bgw, 0xE, 1, 0, 0);
        oh_bg_blk_w(op_w.bgw, 0xF, 2, 0, 0);
        break;

    case 1:
        bgw_ptr->wxy[0].cal += 0x40000;

        if (bgw_ptr->wxy[0].disp.pos > 0x2C0) {
            opw_ptr->r_no_0 += 1;
        }

        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 7 — multi-frame tile-map update sequence. */
static void op_bg0_0007(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 1;
        bgw_ptr->frame_deff = 0;
        Bg_On_W(1);
        bgw_ptr->wxy[0].cal = 0x1D00000;
        bgw_ptr->xy[1].cal = 0xFFF00000;
        oh_bg_blk_w(op_w.bgw, 0x16, 1, 0, 0);
        oh_bg_blk_w(op_w.bgw, 0x17, 2, 0, 0);
        break;

    case 1:
        if (bgw_ptr->wxy[0].disp.pos < 0x200) {
            bgw_ptr->wxy[0].cal += 0x7FFF + 0x4001;
        } else {
            bgw_ptr->wxy[0].cal = 0x2000000;
        }

        if (bgw_ptr->xy[1].disp.pos < 0) {
            bgw_ptr->xy[1].cal += 0x4000;
        } else {
            bgw_ptr->xy[1].cal = 0;
        }

        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 8 — rapid tile swap sequence. */
static void op_bg0_0008(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 1;
        bgw_ptr->frame_deff = 0;
        Bg_On_W(1);
        bgw_ptr->wxy[0].cal = 0x2300000;
        bgw_ptr->xy[1].cal = 0xFFF00000;
        oh_bg_blk_w(op_w.bgw, 0x18, 1, 0, 0);
        oh_bg_blk_w(op_w.bgw, 0x19, 2, 0, 0);
        /* fallthrough */

    case 1:
        if (bgw_ptr->wxy[0].disp.pos > 0x200) {
            bgw_ptr->wxy[0].cal -= 0x8000 + 0x4000;
        } else {
            bgw_ptr->wxy[0].cal = 0x2000000;
        }

        if (bgw_ptr->xy[1].disp.pos < 0) {
            bgw_ptr->xy[1].cal += 0x4000;
        } else {
            bgw_ptr->xy[1].cal = 0;
        }

        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 10 — animated tile reveal sequence. */
static void op_bg0_0010(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 1;
        bgw_ptr->frame_deff = 0;
        Bg_On_W(1);
        Zoomf_Init();
        Zoom_Value_Set(0x40);
        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0;
        oh_bg_blk_w(op_w.bgw, 0x12, 1, 0, 0);
        oh_bg_blk_w(op_w.bgw, 0x13, 2, 0, 0);
        break;

    case 1:
        opw_ptr->r_no_0 += 1;
        break;

    case 2:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->frame_deff = 12;
        Frame_Up(0xC0, 0xE0, bgw_ptr->frame_deff);
        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 11 — multi-step tile-map animation. */
static void op_bg0_0011(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        Bg_On_W(1);
        Zoomf_Init();
        Zoom_Value_Set(0x40);
        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0;
        oh_bg_blk_w(op_w.bgw, 0x14, 1, 0, 0);
        oh_bg_blk_w(op_w.bgw, 0x15, 2, 0, 0);
        break;

    case 1:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->frame_deff = 0x38;
        Frame_Up(0xC0, 0x40, bgw_ptr->frame_deff);
        bgw_ptr->xy[1].cal = 0xFFE00000;
        /* fallthrough */

    case 2:
        if (bgw_ptr->xy[1].disp.pos < 0) {
            bgw_ptr->xy[1].cal += 0x20000;
        }

        if (bgw_ptr->frame_deff > 0) {
            bgw_ptr->frame_deff -= 1;
            Frame_Down(0xC0, 0x40, 1);
        }

        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 12 — animated tile update with timing. */
static void op_bg0_0012(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 1;
        bgw_ptr->frame_deff = 0;
        Bg_On_W(1);
        Zoomf_Init();
        Zoom_Value_Set(0x40);
        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0x1000000;
        oh_bg_blk_w(op_w.bgw, 0x1E, 1, 1, 0);
        oh_bg_blk_w(op_w.bgw, 0x1F, 2, 1, 0);
        oh_bg_blk_w(op_w.bgw, 0x20, 1, 0, 0);
        oh_bg_blk_w(op_w.bgw, 0x21, 2, 0, 0);
        break;

    case 1:
        bgw_ptr->xy[1].cal += 0xFFFE0000;

        if (bgw_ptr->xy[1].disp.pos < 0x61) {
            opw_ptr->r_no_0 += 1;
        }

        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 13 — final tile-map transition. */
static void op_bg0_0013(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 1;
        bgw_ptr->frame_deff = 0;
        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0x600000;
        oh_bg_blk_w(op_w.bgw, 0x1A, 1, 1, 0);
        oh_bg_blk_w(op_w.bgw, 0x1B, 2, 1, 0);
        oh_bg_blk_w(op_w.bgw, 0x1C, 1, 0, 0);
        oh_bg_blk_w(op_w.bgw, 0x1D, 2, 0, 0);
        break;

    case 1:
        bgw_ptr->xy[1].cal += 0x20000;

        if (bgw_ptr->xy[1].disp.pos > 0x100) {
            opw_ptr->r_no_0 += 1;
        }

        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 14 — stub (no operation). */
static void op_bg0_0014(s16 r_index) {
    op_bg0_0000(r_index);
}

const s32 ot_bg0_0015_tbl[OP_COLOR_FADE_STAGES] = { 0xFF00A0B0, 0xFF005888, 0xFF00A0B0,
                                                    0xFF005888, 0xFF000058, 0xFF000000 };

/** @brief BG0 sub 15 — progressive colour fade variant. */
static void op_bg0_0015(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->free = 1;
        bgw_ptr->l_limit = 0;
        Bg_On_W(1);
        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0;

        switch (r_index) {
        case 8:
            oh_bg_blk_w(op_w.bgw, 0x41, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x42, 2, 0, 1);
            break;

        case 14:
            oh_bg_blk_w(op_w.bgw, 0x45, 1, 0, 1);
            oh_bg_blk_w(op_w.bgw, 0x46, 2, 0, 1);
            break;
        }

        op_w.bgw[0].map[1][0].col.full = 0xFF000000;
        op_w.bgw[0].map[2][0].col.full = 0xFF000000;
        break;

    case 1:
        op_w.bgw[0].map[1][0].col.full = 0xFF000000;
        op_w.bgw[0].map[2][0].col.full = 0xFF000000;
        bgw_ptr->free -= 1;

        if (bgw_ptr->free <= 0) {
            bgw_ptr->l_limit += 1;

            if (bgw_ptr->l_limit >= OP_COLOR_FADE_STAGES) {
                opw_ptr->r_no_0 += 1;
            } else {
                bgw_ptr->free = 1;
                op_w.bgw[0].map[1][0].col.full = ot_bg0_0015_tbl[bgw_ptr->l_limit];
                op_w.bgw[0].map[2][0].col.full = ot_bg0_0015_tbl[bgw_ptr->l_limit];
            }
        }

        break;

    case 2:
        op_w.bgw[0].map[1][0].col.full = 0xFF000000;
        op_w.bgw[0].map[2][0].col.full = 0xFF000000;
        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief BG0 sub 16 — multi-frame tile animation with timing. */
static void op_bg0_0016(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        Bg_On_W(1);
        bgw_ptr->wxy[0].cal = 0x2000000;
        bgw_ptr->xy[1].cal = 0;
        op_scrn_end = 0;
        bgw_ptr->frame_deff = 0x13;
        Frame_Up(0xC0, 0x70, bgw_ptr->frame_deff);
        bgw_ptr->free = 0xA;
        break;

    case 1:
        bgw_ptr->free -= 1;

        if (bgw_ptr->free <= 0) {
            opw_ptr->r_no_0 += 1;
        }

        break;

    case 2:
        bgw_ptr->frame_deff -= 1;

        if (bgw_ptr->frame_deff >= 0) {
            Frame_Down(0xC0, 0x70, 1);
        } else {
            opw_ptr->r_no_0 += 1;
            op_scrn_end = 1;
        }

        break;

    case 3:
        break;
    }

    op_scrn_pos_set2(0);
}

/** @brief Dispatch background layer 1 sub-routine by op_w state. */
void op_bg1_move(s16 r_index) {
    opw_ptr = &op_w.bgw[1];
    bgw_ptr = &g_state.bg_w.bgw[1];

    switch (r_index) {
    case 55:
    case 56:
        op_bg1_0001(r_index);
        break;

    case 60:
        op_bg1_0002(r_index);
        break;

    case 62:
        op_bg1_0003(r_index);
        break;

    default:
        op_bg1_0000(r_index);
        break;
    }

    op_scrn_pos_set2(1);
}

/** @brief BG1 sub 0 — initial tile load for layer 1. */
static void op_bg1_0000(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->wxy[0].disp.pos = 512;
        bgw_ptr->xy[1].disp.pos = 0;
        Bg_Off_W(2);
        break;

    case 1:
    case 2:
        break;
    }
}

/** @brief BG1 sub 1 — timed tile-map updates for layer 1. */
static void op_bg1_0001(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        Bg_On_W(1 << bgw_ptr->fam_no);
        bgw_ptr->wxy[0].cal = 0x02000000;
        bgw_ptr->xy[1].cal = 0;

        switch (r_index) {
        case 0x37:
            oh_bg_blk_w(&op_w.bgw[1], 1, 1, 0, 0);
            oh_bg_blk_w(&op_w.bgw[1], 2, 2, 0, 0);
            break;

        case 0x38:
            oh_bg_blk_w(&op_w.bgw[1], 3, 1, 0, 0);
            oh_bg_blk_w(&op_w.bgw[1], 4, 2, 0, 0);
            break;
        }

        break;

    case 1:
        break;
    }
}

/** @brief BG1 sub 2 — alternating tile animation for layer 1. */
static void op_bg1_0002(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        Bg_On_W(1 << bgw_ptr->fam_no);

        switch (r_index) {
        case 0x3C:
            bgw_ptr->wxy[0].cal = 0x01000000;
            bgw_ptr->xy[1].cal = 0;
            oh_bg_blk_w(&op_w.bgw[1], 7, 0, 0, 0);
            oh_bg_blk_w(&op_w.bgw[1], 8, 1, 0, 0);
            oh_bg_blk_w(&op_w.bgw[1], 9, 2, 0, 0);
            op_bg_mvxy[bgw_ptr->fam_no].a[0].sp = 0xC0000;
            op_bg_mvxy[bgw_ptr->fam_no].d[0].sp = 0;
            bgw_ptr->r_limit = 0x1E0;
            break;

        default:
            break;
        }

        break;

    case 1:
        op_bg_mvxy[bgw_ptr->fam_no].a[0].sp += op_bg_mvxy[bgw_ptr->fam_no].d[0].sp;
        bgw_ptr->wxy[0].cal += op_bg_mvxy[bgw_ptr->fam_no].a[0].sp;

        if (bgw_ptr->wxy[0].disp.pos >= bgw_ptr->r_limit) {
            opw_ptr->r_no_0 += 1;
        }

        break;

    case 2:
        break;
    }
}

/** @brief BG1 sub 3 — complex multi-step tile animation for layer 1. */
static void op_bg1_0003(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        Bg_On_W(1 << bgw_ptr->fam_no);

        switch (r_index) {
        case 83:
            bgw_ptr->wxy[0].cal = 0x2200000;
            bgw_ptr->xy[1].cal = 0;
            oh_bg_blk_w(op_w.bgw, 0x10, 1, 0, 0);
            oh_bg_blk_w(op_w.bgw, 0x11, 2, 0, 0);
            op_bg_mvxy[bgw_ptr->fam_no].a[0].sp = 0x80000;
            op_bg_mvxy[bgw_ptr->fam_no].d[0].sp = -0x8000;
            bgw_ptr->r_limit = 0x200;
            break;

        case 62:
            bgw_ptr->wxy[0].cal = 0x1E00000; // low = 0, pos = 480
            bgw_ptr->xy[1].cal = 0;
            oh_bg_blk_w(&op_w.bgw[1], 0xA, 0, 0, 0);
            oh_bg_blk_w(&op_w.bgw[1], 0xB, 1, 0, 0);
            oh_bg_blk_w(&op_w.bgw[1], 0xC, 2, 0, 0);
            op_bg_mvxy[bgw_ptr->fam_no].a[0].sp = 0xFFF80000;
            op_bg_mvxy[bgw_ptr->fam_no].d[0].sp = 0;
            break;
        }

        break;

    case 1:
        op_bg_mvxy[bgw_ptr->fam_no].a[0].sp += op_bg_mvxy[bgw_ptr->fam_no].d[0].sp;
        bgw_ptr->wxy[0].cal +=
            op_bg_mvxy[bgw_ptr->fam_no].a[0].sp; // Move background horizontally by the specified offset/speed

        if (bgw_ptr->wxy[0].disp.pos <= bgw_ptr->l_limit) {
            opw_ptr->r_no_0 += 1;
        }

        break;

    case 2:
        break;
    }
}

/** @brief Dispatch background layer 2 sub-routine by op_w state. */
void op_bg2_move(s16 r_index) {
    opw_ptr = &op_w.bgw[2];
    bgw_ptr = &g_state.bg_w.bgw[2];

    switch (r_index) {
    case 0:
    case 24:
        op_bg2_0000(r_index);
        break;

    case 2:
        op_bg2_0002(r_index);
        break;

    case 28:
        op_bg2_0003(r_index);
        break;

    default:
        op_bg2_0001(r_index);
        break;
    }

    op_scrn_pos_set2(2);
}

/** @brief BG2 sub 0 — initial tile load for layer 2. */
static void op_bg2_0000(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->wxy[0].disp.pos = 512;
        bgw_ptr->xy[1].disp.pos = 0;
        break;

    case 1:
        bgw_ptr->wxy[0].cal -= (0x8000 + 0x8000);
        break;

    case 2:
        break;
    }
}

/** @brief BG2 sub 1 — tile-map update for layer 2. */
static void op_bg2_0001(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->wxy[0].disp.pos = 512;
        bgw_ptr->xy[1].disp.pos = 0;
        break;

    case 1:
        break;
    }
}

/** @brief BG2 sub 2 — further tile transitions for layer 2. */
static void op_bg2_0002(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->wxy[0].disp.pos = 0x200;
        bgw_ptr->xy[1].disp.pos = 0;
        break;

    case 1:
        bgw_ptr->xy[1].cal += 0x10000;
        break;

    case 2:
        break;
    }
}

/** @brief BG2 sub 3 — final tile animation for layer 2. */
static void op_bg2_0003(s16 r_index) {
    switch (opw_ptr->r_no_0) {
    case 0:
        opw_ptr->r_no_0 += 1;
        bgw_ptr->wxy[0].disp.pos = 0x200;
        bgw_ptr->xy[1].disp.pos = 0;
        break;

    case 1:
        bgw_ptr->wxy[0].cal += 0x10000;
        break;

    case 2:
        break;
    }
}

/** @brief Set screen scroll position for a given BG layer. */
void op_scrn_pos_set2(s16 bg_no) {
    s16 pos_x = g_state.bg_w.bgw[bg_no].wxy[0].disp.pos;
    s16 pos_y = g_state.bg_w.bgw[bg_no].xy[1].disp.pos;
    Scrn_Move_Set(bg_no, pos_x - g_state.bg_w.pos_offset, pos_y);
}

/** @brief Configure BG family parameters for the opening cinematics. */
void Bg_Family_Set_op() {
    s16 pos_work_x;
    s16 pos_work_y;
    s16 i;

    for (i = 0; i < 4; i++) {
        g_state.bg_w.bgw[i].xy[0].cal = g_state.bg_w.bgw[i].wxy[0].cal;
        g_state.bg_w.bgw[i].position_y = g_state.bg_w.bgw[i].xy[1].disp.pos;
        g_state.bg_w.bgw[i].position_x = g_state.bg_w.bgw[i].wxy[0].disp.pos - g_state.bg_w.pos_offset;
        pos_work_x = -g_state.bg_w.bgw[i].position_x;
        pos_work_y = g_state.bg_w.bgw[i].position_y;
        pos_work_y = 768 - (pos_work_y & 0x3FF);
        Family_Set_W(i + 1, pos_work_x, pos_work_y);
    }

    g_state.bg_w.bgw[5].position_y = g_state.bg_w.bgw[5].xy[1].disp.pos;
    g_state.bg_w.bgw[5].position_x = g_state.bg_w.bgw[5].wxy[0].disp.pos - g_state.bg_w.pos_offset;
    pos_work_x = -g_state.bg_w.bgw[5].position_x;
    pos_work_y = g_state.bg_w.bgw[5].position_y;
    pos_work_y = 768 - (pos_work_y & 0x3FF);
    Family_Set_W(6, pos_work_x, pos_work_y);
}
