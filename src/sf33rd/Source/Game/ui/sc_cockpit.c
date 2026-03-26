/**
 * @file sc_cockpit.c
 * @brief HUD cockpit gauges: health bars, stun gauges, super-art bars,
 *        stun marks, and the Akaobi health-bar stripe overlay.
 *
 * Part of the ui module. Split from sc_sub.c (task #21).
 */

#include "sf33rd/Source/Game/ui/sc_cockpit.h"
#include "common.h"

#include "port/rendering/renderer.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/ui/sc_data.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "structs.h"
#include <stdbool.h>

#define TO_UV_256(val) ((val) / 256.0f)

/* ── Health bars ────────────────────────────────────────────────── */

/** @brief Draw a player's health bar (filled/empty portions). */
void vital_put(u8 Pl_Num, s8 atr, s16 vital, u8 kind, u16 priority) {
    if (No_Trans) {
        return;
    }

    if (vital == 0) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    if (vital == -1) {
        vital = 0;
    }

    scrscrntex[0].z = scrscrntex[3].z = PrioBase[priority];
    ppgSetupCurrentPaletteNumber(0, atr & 0x3F);

    if (kind) {
        scrscrntex[0].u = 0.0f;
        scrscrntex[3].u = 8.0f / 256.0f;
        scrscrntex[0].v = TO_UV_256(64.0f);
        scrscrntex[3].v = TO_UV_256(72.0f);
    } else {
        scrscrntex[0].u = 0.0f;
        scrscrntex[3].u = 8.0f / 256.0f;
        scrscrntex[0].v = TO_UV_256(72.0f);
        scrscrntex[3].v = TO_UV_256(80.0f);
    }

    if (Pl_Num == 0) {
        scrscrntex[0].x = (168 - vital);
        scrscrntex[3].x = 168.0f;
    } else {
        scrscrntex[0].x = 216.0f;
        scrscrntex[3].x = (vital + 216);
    }

    scrscrntex[0].y = 16.0f;
    scrscrntex[3].y = 24.0f;

    scrscrntex[0].color = scrscrntex[3].color = -1;
    Renderer_SetTexture(0);
    Renderer_DrawSprite(scrscrntex, 4);
}

/** @brief Draw the silver (recoverable) health bar overlay. */
void silver_vital_put(u8 Pl_Num) {
    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);
    scrscrntex[0].z = scrscrntex[3].z = PrioBase[TopHUDPriority];
    ppgSetupCurrentPaletteNumber(0, 9);
    scrscrntex[0].u = 224.0f / 256.0f;
    scrscrntex[3].u = 232.0f / 256.0f;
    scrscrntex[0].v = TO_UV_256(176.0f);
    scrscrntex[3].v = TO_UV_256(184.0f);

    if (Pl_Num == 0) {
        scrscrntex[0].x = 8.0f;
        scrscrntex[3].x = 168.0f;
    } else {
        scrscrntex[0].x = 216.0f;
        scrscrntex[3].x = 376.0f;
    }

    scrscrntex[0].y = 16.0f;
    scrscrntex[3].y = 24.0f;

    scrscrntex[0].color = scrscrntex[3].color = -1;
    Renderer_SetTexture(0);
    Renderer_DrawSprite(scrscrntex, 4);
}

/** @brief Draw the health bar background/frame. */
void vital_base_put(u8 Pl_Num) {
    PAL_CURSOR vtx;
    PAL_CURSOR_P pos[4];
    PAL_CURSOR_COL col;

    if (No_Trans || SA_shadow_on) {
        return;
    }

    vtx.p = pos;
    vtx.col = &col;
    col.color = 0x40000000;

    if (Pl_Num == 0) {
        pos[0].x = 8.0f;
        pos[3].x = 168.0f;
    } else {
        pos[0].x = 216.0f;
        pos[3].x = 376.0f;
    }

    pos[0].y = 18.0f;
    pos[3].y = 23.0f;
    pos[1].x = pos[3].x;
    pos[1].y = pos[0].y;
    pos[2].x = pos[0].x;
    pos[2].y = pos[3].y;
    Renderer_Queue2DPrimitive((f32*)vtx.p, PrioBase[TopHUDFacePriority], (uintptr_t)col.color, 0);
}

/* ── Super-art gauge ───────────────────────────────────────────── */

/** @brief Draw the super-art gauge bar background. */
void spgauge_base_put(u8 Pl_Num, s16 len) {
    PAL_CURSOR vtx;
    PAL_CURSOR_P pos[4];
    PAL_CURSOR_COL col;

    if (omop_cockpit == 0) {
        return;
    }

    if (omop_sa_bar_disp[Pl_Num] == 0) {
        return;
    }

    if (No_Trans || SA_shadow_on) {
        return;
    }

    vtx.p = pos;
    vtx.col = &col;
    col.color = 0x80000000;

    if (Pl_Num == 0) {
        pos[0].x = 48.0f;
        pos[3].x = ((len * 8) + 48);
    } else {
        pos[0].x = 336.0f;
        pos[3].x = (336 - (len * 8));
    }

    pos[0].y = 210.0f;
    pos[3].y = 217.0f;
    pos[1].x = pos[3].x;
    pos[1].y = pos[0].y;
    pos[2].x = pos[0].x;
    pos[2].y = pos[3].y;
    Renderer_Queue2DPrimitive((f32*)vtx.p, PrioBase[4], (uintptr_t)col.color, 0);
}

/* ── Stun gauge ────────────────────────────────────────────────── */

/** @brief Draw the stun gauge fill. */
void stun_put(u8 Pl_Num, u8 stun) {
    if (No_Trans) {
        return;
    }
    if (use_rmlui && rmlui_hud_stun)
        return;

    if (stun == 0) {
        return;
    }

    if (omop_st_bar_disp[Pl_Num] == 0) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);
    scrscrntex[0].z = scrscrntex[3].z = PrioBase[TopHUDPriority];
    ppgSetupCurrentPaletteNumber(0, 10);
    scrscrntex[0].u = 0.0f;
    scrscrntex[3].u = 8.0f / 256.0f;
    scrscrntex[0].v = TO_UV_256(96.0f);
    scrscrntex[3].v = TO_UV_256(104.0f);

    if (Pl_Num == 0) {
        scrscrntex[0].x = (168 - stun);
        scrscrntex[3].x = 168.0f;
    } else {
        scrscrntex[0].x = 216.0f;
        scrscrntex[3].x = (stun + 216);
    }

    scrscrntex[0].y = 24.0f;
    scrscrntex[3].y = 32.0f;
    scrscrntex[0].color = scrscrntex[3].color = -1;
    Renderer_SetTexture(0);
    Renderer_DrawSprite(scrscrntex, 4);
}

/** @brief Draw the stun gauge bar background. */
void stun_base_put(u8 Pl_Num, s16 len) {
    PAL_CURSOR vtx;
    PAL_CURSOR_P pos[4];
    PAL_CURSOR_COL col;

    if (No_Trans || SA_shadow_on) {
        return;
    }
    if (use_rmlui && rmlui_hud_stun)
        return;

    vtx.p = pos;
    vtx.col = &col;
    col.color = 0x90000000;

    if (Pl_Num == 0) {
        pos[0].x = (168 - (len * 8));
        pos[3].x = 168.0f;
    } else {
        pos[0].x = 216.0f;
        pos[3].x = ((len * 8) + 216);
    }

    pos[0].y = 25.0f;
    pos[3].y = 31.0f;
    pos[1].x = pos[3].x;
    pos[1].y = pos[0].y;
    pos[2].x = pos[0].x;
    pos[2].y = pos[3].y;
    Renderer_Queue2DPrimitive((f32*)vtx.p, PrioBase[TopHUDFacePriority], (uintptr_t)col.color, 0);
}

/* ── Stun marks & MAX indicator ────────────────────────────────── */

/** @brief Draw the stun-mark indicator for a player. */
void stun_mark_write(u8 Pl_Num, s16 Len) {
    s16 tlen;

    if (No_Trans) {
        return;
    }

    if (omop_st_bar_disp[Pl_Num] == 0) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);
    tlen = Len - 7;
    scfont_sqput(
        smark_pos_tbl[tlen][Pl_Num], 3, 10, 0, (smark_kind_tbl[tlen] * 4) + 1, 2, smark_kind_tbl[tlen] + 4, 1, TopHUDPriority);
}

/** @brief Draw the "MAX" indicator when super-art gauge is full. */
void max_mark_write(s8 Pl_Num, u8 Gauge_Len, u8 Mchar, u8 Mass_Len) {
    if (Pl_Num == 0) {
        scfont_sqput2(Mass_Len + 6, 26, 17, 0, 0, Max_Pos_TBL[Mchar - 5][0], Max_Pos_TBL[Mchar - 5][1], Mchar, 1);
    } else {
        scfont_sqput2(
            42 - Gauge_Len + Mass_Len, 26, 17, 0, 0, Max_Pos_TBL[Mchar - 5][0], Max_Pos_TBL[Mchar - 5][1], Mchar, 1);
    }
}

/* ── Stun gauge frame & silver stun ────────────────────────────── */

/** @brief Draw the silver (inactive) stun gauge fill. */
static void silver_stun_put(u8 Pl_Num, s16 len) {
    if (No_Trans) {
        return;
    }
    if (use_rmlui && rmlui_hud_stun)
        return;

    ppgSetupCurrentDataList(&ppgScrList);
    scrscrntex[0].z = scrscrntex[3].z = PrioBase[TopHUDShadowPriority];
    ppgSetupCurrentPaletteNumber(0, 1);

    scrscrntex[0].u = 240.0f / 256.0f;
    scrscrntex[3].u = 248.0f / 256.0f;
    scrscrntex[0].v = TO_UV_256(176.0f);
    scrscrntex[3].v = TO_UV_256(184.0f);

    if (Pl_Num == 0) {
        scrscrntex[0].x = ((21 - len) * 8);
        scrscrntex[3].x = 168.0f;
    } else {
        scrscrntex[0].x = 216.0f;
        scrscrntex[3].x = ((len + 27) * 8);
    }

    scrscrntex[0].y = 24.0f;
    scrscrntex[3].y = 32.0f;

    scrscrntex[0].color = scrscrntex[3].color = 0xFFFFFFFF;
    Renderer_SetTexture(0);
    Renderer_DrawSprite(scrscrntex, 4);
}

void stun_gauge_waku_write(s16 p1len, s16 p2len) {
    if (omop_cockpit == 0) {
        return;
    }
    if (use_rmlui && rmlui_hud_stun)
        return;

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    if (omop_st_bar_disp[0]) {
        scfont_sqput(21 - p1len, 3, 10, 0, 12 - p1len, p1len + 1, p1len, 1, TopHUDShadowPriority);
    } else {
        silver_stun_put(0, p1len);
    }

    scfont_sqput(11, 3, 1, 0, 2, p1len + 1, 10 - p1len, 1, TopHUDShadowPriority);

    if (omop_st_bar_disp[1]) {
        scfont_sqput(27, 3, 10, 0, 2, p2len + 12, p2len, 1, TopHUDShadowPriority);
    } else {
        silver_stun_put(1, p2len);
    }

    scfont_sqput(p2len + 27, 3, 1, 0, p2len + 2, p2len + 12, 10 - p2len, 1, TopHUDShadowPriority);
}

/* ── Super-art stock indicators ────────────────────────────────── */

/** @brief Transfer super-art stock indicator data to VRAM. */
void sa_stock_trans(s16 St_Num, s16 Spg_Col, s8 Stpl_Num) {
    if (Stpl_Num == 0) {
        scfont_put2(3, 25, sa_color_data_tbl[Spg_Col], 2, St_Num + 21, 4);
        scfont_put2(3, 26, sa_color_data_tbl[Spg_Col], 2, St_Num + 21, 5);
    } else {
        scfont_put2(44, 25, sa_color_data_tbl[Spg_Col], 2, St_Num + 21, 4);
        scfont_put2(44, 26, sa_color_data_tbl[Spg_Col], 2, St_Num + 21, 5);
    }
}

/** @brief Transfer full super-art stock data to VRAM. */
void sa_fullstock_trans(s16 St_Num, s16 Spg_Col, s8 Stpl_Num) {
    if (Stpl_Num == 0) {
        scfont_put2(1, 26, sa_color_data_tbl[Spg_Col], 2, St_Num + 21, 6);
    } else {
        scfont_put2(46, 26, sa_color_data_tbl[Spg_Col], 2, St_Num + 21, 7);
    }
}

/** @brief Write the super-art stock number display. */
void sa_number_write(s8 Stpl_Num, u16 x) {
    if (Stpl_Num == 0) {
        if (My_char[0] == 0) {
            scfont_sqput2(x, 26, 14, 0, 2, 27, 2, 2, 2);
        } else {
            scfont_sqput2(x, 26, 14, 0, 2, (Super_Arts[0] * 2) + 21, 2, 2, 2);
        }
    } else if (My_char[1] == 0) {
        scfont_sqput2(x, 26, 142, 1, 2, 27, 2, 2, 2);
    } else {
        scfont_sqput2(x, 26, 142, 1, 2, (Super_Arts[1] * 2) + 21, 2, 2, 2);
    }
}

/* ── Akaobi stripe ─────────────────────────────────────────────── */

/** @brief Draw the red "Akaobi" health-bar stripe overlay. */
void Akaobi() {
    PAL_CURSOR apc;
    PAL_CURSOR_P ap[4];
    PAL_CURSOR_COL acol[4];
    u8 i;

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    apc.p = ap;
    apc.col = acol;
    apc.num = 4;

    for (i = 0; i < 4; i++) {
        ap[i].x = Akaobi_Pos_tbl[i * 2];
        ap[i].y = Akaobi_Pos_tbl[(i * 2) + 1];
        acol[i].color = 0xA0D00000;
    }

    Renderer_Queue2DPrimitive((f32*)ap, PrioBase[TopHUDPriority], (uintptr_t)acol[0].color, 0);
}
