/**
 * @file sc_timer.c
 * @brief HNC overlays, combo/score rendering, training-mode data display,
 *        button images, save/load title, and SF3 logo animation.
 *
 * Part of the ui module. Split from sc_sub.c (task #21).
 */

#include "sf33rd/Source/Game/ui/sc_timer.h"
#include "sf33rd/Source/Game/effect/eff76.h" /* chkNameAkuma */
#include "common.h"

#include "port/rendering/legacy_matrix.h"
#include "port/rendering/renderer.h"
#include "port/sdl/renderer/sdl_game_renderer.h"
#include "port/sdl/rmlui/rmlui_attract_overlay.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/opening/opening.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/ui/sc_data.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "structs.h"
#include <stdbool.h>

#define TO_UV_256(val) ((val) / 256.0f)
#define TO_UV_128(val) ((val) / 128.0f)

#define SA_FRAME_ROWS 3
#define SA_FRAME_COLS 48
#define SA_FRAME_Y_OFFSET 25
#define SCRN_ADD_TEX_COUNT 9
#define SC_RAM_VRAM_COUNT 8
#define DAMAGE_DISPLAY_CAP 999

/* ── Private helpers ───────────────────────────────────────────── */

/** @brief Render a single scaled character from the bigger font sheet. */
static void SSPutStrTexInputB2(f32 x, f32 y, s8 str) {
    s32 u = str * 11;

    scrscrntex[0].u = scrscrntex[1].u = TO_UV_256(u);
    scrscrntex[2].u = scrscrntex[3].u = TO_UV_256(u + 11);
    scrscrntex[0].v = scrscrntex[2].v = TO_UV_256(200.0f);
    scrscrntex[1].v = scrscrntex[3].v = TO_UV_256(208.0f);
    scrscrntex[0].x = scrscrntex[1].x = x;
    scrscrntex[2].x = scrscrntex[3].x = (11.0f + x);
    scrscrntex[0].y = scrscrntex[2].y = y;
    scrscrntex[1].y = scrscrntex[3].y = (8.0f + y);
}

/** @brief Render a decimal number with custom gradient and priority. */
static void SSPutDec3(u16 x, u16 y, u8 atr, s16 dec, u8 size, u8 gr, u16 priority) {
    s8 str[3];
    s16 work;
    u8 num;
    u8 i;
    u8 zero_sw;
    f32 xx;
    f32 yy;

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    for (i = 0; i < 4; i++) {
        scrscrntex[i].color = bigger_col_tbl[gr][i];
    }

    scrscrntex[0].z = scrscrntex[1].z = scrscrntex[2].z = scrscrntex[3].z = PrioBase[priority];
    ppgSetupCurrentPaletteNumber(0, atr & 0x3F);

    xx = x;
    yy = y;
    zero_sw = 0;
    work = 100;

    for (i = 0; i < 3; i++) {
        for (num = 0; dec + 1 > work; dec = dec - work, num++) {}

        str[i] = num;
        work = work / 10;
    }

    SSPutStrTexInputB2(xx, yy, str[2]);
    Renderer_SetTexture(4);
    Renderer_DrawTexturedQuad(scrscrntex, 4);

    if (size == 0) {
        return;
    }

    xx -= 22.0f;

    if (size == 3 && str[0] != 0) {
        SSPutStrTexInputB2(xx, yy, str[0]);
        Renderer_SetTexture(4);
        Renderer_DrawSprite(scrscrntex, 4);
        zero_sw = 1;
    }

    xx += 11.0f;

    if (zero_sw == 1) {
        SSPutStrTexInputB2(xx, yy, str[1]);
        Renderer_SetTexture(4);
        Renderer_DrawSprite(scrscrntex, 4);
    } else if (size > 1 && str[1] != 0) {
        SSPutStrTexInputB2(xx, yy, str[1]);
        Renderer_SetTexture(4);
        Renderer_DrawSprite(scrscrntex, 4);
    }
}

/** @brief Render a rectangular font region with custom gradient and priority. */
static void scfont_sqput3(u16 x, u16 y, u8 atr, u8 page, u16 cx1, u16 cy1, u16 cx2, u16 cy2, u8 gr, u16 priority) {
    s32 u1;
    s32 u2;
    s32 v1;
    s32 v2;
    u8 i;

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    for (i = 0; i < 4; i++) {
        scrscrntex[i].color = bigger_col_tbl[gr][i];
    }

    scrscrntex[0].z = scrscrntex[1].z = scrscrntex[2].z = scrscrntex[3].z = PrioBase[priority];
    ppgSetupCurrentPaletteNumber(0, atr & 0x3F);
    u1 = cx1;
    u2 = u1 + cx2;
    v1 = cy1;
    v2 = v1 + cy2;

    scrscrntex[0].u = scrscrntex[1].u = TO_UV_256(u1);
    scrscrntex[2].u = scrscrntex[3].u = TO_UV_256(u2);
    scrscrntex[0].v = scrscrntex[2].v = TO_UV_256(v1);
    scrscrntex[1].v = scrscrntex[3].v = TO_UV_256(v2);
    scrscrntex[0].x = scrscrntex[1].x = x;
    scrscrntex[2].x = scrscrntex[3].x = (x + (u2 - u1));
    scrscrntex[0].y = scrscrntex[2].y = y;
    scrscrntex[1].y = scrscrntex[3].y = (y + (v2 - v1));
    Renderer_SetTexture(page);
    Renderer_DrawTexturedQuad(scrscrntex, 4);
}

/* ── HNC overlays ──────────────────────────────────────────────── */

/** @brief Set up and display an HNC (hit/name/combo) overlay. */
void hnc_set(u8 num, u8 atr) {
    u8 i;

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);
    scrscrntex[0].z = scrscrntex[3].z = PrioBase[2];
    ppgSetupCurrentPaletteNumber(0, atr & 0x3F);

    for (i = 0; i < 2; i++) {
        if (i) {
            scrscrntex[0].u = TO_UV_256(0.0f);
            scrscrntex[3].u = TO_UV_256(num * 8);
            scrscrntex[0].v = TO_UV_256(96.0f);
            scrscrntex[3].v = TO_UV_256(120.0f);
            scrscrntex[0].x = 184.0f;
            scrscrntex[3].x = ((num + 23) * 8);
        } else {
            scrscrntex[0].u = TO_UV_256((23 - num) * 8);
            scrscrntex[3].u = TO_UV_256(184.0f);
            scrscrntex[0].v = TO_UV_256(72.0f);
            scrscrntex[3].v = TO_UV_256(96.0f);
            scrscrntex[0].x = ((23 - num) * 8);
            scrscrntex[3].x = 184.0f;
        }

        scrscrntex[0].y = 88.0f;
        scrscrntex[3].y = 112.0f;
        scrscrntex[0].color = scrscrntex[3].color = -1;
        Renderer_SetTexture(1);
        Renderer_DrawSprite(scrscrntex, 4);
    }
}

/** @brief Initialize the HNC wipe-reveal animation. */
void hnc_wipeinit(u8 atr) {
    RendererVertex dmyvtx[4];
    u8 i;
    u8 j;
    u8 k;

    ppgSetupCurrentDataList(&ppgScrList);
    Hnc_Num = 0;
    scrscrntex[0].z = scrscrntex[1].z = scrscrntex[2].z = scrscrntex[3].z = PrioBase[2];
    ppgSetupCurrentPaletteNumber(0, atr & 0x3F);

    scrscrntex[0].color = scrscrntex[1].color = scrscrntex[2].color = scrscrntex[3].color = -1;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 26; j++) {
            for (k = 0; k < 4; k++) {
                scrscrntex[k].u = hnc_wipe_tbl1[j][k * 2] / 256.0f;
                scrscrntex[k].v = ((i * 24) + hnc_wipe_tbl1[j][(k * 2) + 1]) / 256.0f;
                scrscrntex[k].x = ((i * 184) + hnc_wipe_tbl1[j][k * 2]);
                scrscrntex[k].y = (hnc_wipe_tbl1[j][(k * 2) + 1] + 16);
                dmyvtx[k] = scrscrntex[k];
            }

            if (!No_Trans) {
                Renderer_SetTexture(1);
                Renderer_DrawTexturedQuad(dmyvtx, 4);
            }
        }
    }
}
/** @brief Execute one frame of the HNC wipe-out animation. */
s32 hnc_wipeout(u8 atr) {
    RendererVertex vtx[4];
    u8 i;
    u8 j;
    u8 k;
    s32 ipx;
    s32 ipy;
    s32 ipu;
    s32 ipv;
    s32 len;

    if (!No_Trans) {
        ppgSetupCurrentDataList(&ppgScrList);
        ppgSetupCurrentPaletteNumber(0, atr & 0x3F);

        vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = PrioBase[2];
        vtx[0].color = vtx[1].color = vtx[2].color = vtx[3].color = -1;
        ipx = 8;
        ipy = 88;
        ipu = 8;
        ipv = 72;
        len = 8 - Hnc_Num;

        for (i = 0; i < 2; i++) {
            for (j = 0; j < 23; j++) {
                vtx[0].x = ipx;
                vtx[1].x = vtx[0].x - len;
                vtx[2].x = 16.0f + vtx[0].x;
                vtx[3].x = vtx[2].x - len;
                vtx[0].y = vtx[1].y = ipy;
                vtx[2].y = vtx[3].y = ipy + 24;
                vtx[0].u = ipu;
                vtx[1].u = vtx[0].u - len;
                vtx[2].u = 16.0f + vtx[0].u;
                vtx[3].u = vtx[2].u - len;
                vtx[0].v = vtx[1].v = ipv;
                vtx[2].v = vtx[3].v = ipv + 24;

                for (k = 0; k < 4; k++) {
                    vtx[k].u /= 256.0f;
                    vtx[k].v /= 256.0f;
                }

                Renderer_SetTexture(1);
                Renderer_DrawTexturedQuad(vtx, 4);
                ipx += 8;
                ipu += 8;
            }

            ipu = 8;
            ipv += 24;
        }

        ipx = 184;
        ipu = 0;
        ipv -= 24;

        for (j = 0; j < 2; j++) {
            vtx[0].x = vtx[1].x = ipx;
            vtx[2].x = vtx[0].x + (16 - (j * 8));
            vtx[3].x = vtx[2].x - len;
            vtx[0].y = ipy;
            vtx[1].y = vtx[0].y + ((24.0f * len) / 16.0f);
            vtx[2].y = vtx[3].y = vtx[0].y + (0x18 - (j * 12));
            vtx[0].u = vtx[1].u = ipu;
            vtx[2].u = vtx[0].u + (16 - (j * 8));
            vtx[3].u = vtx[2].u - len;
            vtx[0].v = ipv;
            vtx[1].v = vtx[0].v + ((24.0f * len) / 16.0f);
            vtx[2].v = vtx[3].v = vtx[0].v + (24 - (j * 12));

            for (k = 0; k < 4; k++) {
                vtx[k].u /= 256.0f;
                vtx[k].v /= 256.0f;
            }

            Renderer_SetTexture(1);
            Renderer_DrawTexturedQuad(vtx, 4);
            ipy += 12;
            ipv += 12;
        }
    }

    Hnc_Num++;

    if (Hnc_Num == 8) {
        return 1;
    }

    return 0;
}

/* ── Character-info and name/win overlays ──────────────────────── */

/** @brief Set character-info overlay elements. */
void ci_set(u8 type, u8 atr) {
    if (No_Trans || type >= 7) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);
    scfont_sqput(ci_tbl[type][4],
                 ci_tbl[type][5],
                 atr,
                 cip_tbl[type],
                 ci_tbl[type][0],
                 ci_tbl[type][1],
                 ci_tbl[type][2],
                 ci_tbl[type][3],
                 2);
}

/** @brief Set name/win data overlay for a player. */
void nw_set(u8 PL_num, u8 atr) {
    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);
    PL_num += chkNameAkuma(PL_num, 6);
    scfont_sqput(nwdata_tbl[PL_num][3],
                 9,
                 atr,
                 nwdata_tbl[PL_num][4],
                 nwdata_tbl[PL_num][0],
                 nwdata_tbl[PL_num][1],
                 nwdata_tbl[PL_num][2],
                 4,
                 2);
    scfont_sqput(nwdata_tbl[PL_num][5], 9, atr, 2, 17, 22, 13, 4, 2);
}

/* ── Score fonts ───────────────────────────────────────────────── */

/** @brief Render an 8×16 score-font character. */
void score8x16_put(u16 x, u16 y, u8 atr, u8 chr) {
    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);
    scfont_sqput(x, y, atr, 0, chr, 6, 1, 2, 2);
}

/** @brief Render a 16×24 score-font character. */
void score16x24_put(u16 x, u16 y, u8 atr, u8 chr) {
    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);
    scfont_sqput(x, y, atr, 2, chr * 2, 6, 2, 3, 2);
}

/* ── Combo messages ────────────────────────────────────────────── */

/** @brief Display a combo message (hit count and type label). */
void combo_message_set(u8 pl, u8 kind, u8 x, u8 num, u8 hi, u8 low) {
    u8 xw;
    u8 xw2;

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    if (num > combo_mtbl[kind][2]) {
        xw = combo_mtbl[kind][2];
    } else {
        xw = num;
    }

    if (num > combo_mtbl[kind][2]) {
        xw2 = (num - (combo_mtbl[kind][2]));
    } else {
        xw2 = 0;
    }

    switch (kind) {
    case 2:
    case 1:
    case 0:
        if (pl == 0) {
            if (hi != 0) {
                scfont_sqput(x, 7, 8, 0, hi, 6, 1, 2, 2);
            }

            if (num > 1) {
                scfont_sqput(x + 1, 7, 8, 0, low, 6, 1, 2, 2);
            }

            if (num > 3) {
                scfont_sqput(x + 3, 7, 8, 2, combo_mtbl[kind][0], combo_mtbl[kind][1], xw, 2, 2);
                return;
            }
        } else {
            scfont_sqput(xw2, 7, 8, 2, (combo_mtbl[kind][0] + combo_mtbl[kind][2]) - xw, combo_mtbl[kind][1], xw, 2, 2);

            if (xw2 > 1) {
                scfont_sqput(xw2 - 2, 7, 8, 0, low, 6, 1, 2, 2);
            }

            if ((xw2 > 2) && (hi != 0)) {
                scfont_sqput(xw2 - 3, 7, 8, 0, hi, 6, 1, 2, 2);
                return;
            }
        }

        break;

    case 3:
    case 4:
    case 5:
    case 6:
        if (pl == 0) {
            scfont_sqput(x, 7, 8, 2, combo_mtbl[kind][0], combo_mtbl[kind][1], xw, 2, 2);
        } else {
            scfont_sqput(xw2, 7, 8, 2, (combo_mtbl[kind][0] + combo_mtbl[kind][2]) - xw, combo_mtbl[kind][1], xw, 2, 2);
        }

        break;
    }
}

/** @brief Display combo point values. */
void combo_pts_set(u8 pl, u8 x, u8 num, s8* pts, s8 digit) {
    s8 i;
    s8 j;

    s8 assign1;
    u8 assign2;
    u8 assign3;

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    if (pl == 0) {
        for (i = digit, assign1 = j = 1; i >= 0; i--, j++, assign2 = x += 1) {
            score8x16_put(x, 10, 8, pts[i]);

            if (num - j == 0) {
                return;
            }
        }

        if (num < digit + 1) {
            return;
        }

        score8x16_put(x, 10, 8, 0);

        if (num < digit + 2) {
            return;
        }

        score8x16_put(x + 1, 10, 8, 0);

        if (num < digit + 3) {
            return;
        }

        scfont_put(x + 2, 11, 8, 0, 6, 13, 2);

        if (num < digit + 4) {
            return;
        }

        scfont_put(x + 3, 11, 8, 0, 7, 13, 2);

    } else {
        scfont_put(x, 11, 8, 0, 7, 13, 2);

        if (num > 1) {
            scfont_put(x - 1, 11, 8, 0, 6, 13, 2);
        }

        if (num > 2) {
            score8x16_put(x - 2, 10, 8, 0);
        }

        if (num > 3) {
            score8x16_put(x - 3, 10, 8, 0);
        }

        if (num > 4) {
            for (i = 0; i <= digit; i++, assign3 = x -= 1) {
                score8x16_put(x - 4, 10, 8, pts[i]);

                if (num - i == 0) {
                    break;
                }
            }
        }
    }
}

/* ── Screen-RAM transfer ───────────────────────────────────────── */

/** @brief Transfer screen-RAM tile data to VRAM for rendering. */
void sc_ram_to_vram(s8 sc_num) {
    uintptr_t* sc_tbl_ptr;
    u8* sc_pos_ptr;
    u8* sc_uv_ptr;
    u16 loop;
    u16 i;

    if (sc_num < 0 || sc_num >= SC_RAM_VRAM_COUNT) {
        return;
    }

    sc_tbl_ptr = (uintptr_t*)sc_ram_vram_tbl[sc_num];
    sc_pos_ptr = (u8*)*sc_tbl_ptr;
    sc_tbl_ptr++;
    sc_uv_ptr = (u8*)*sc_tbl_ptr;
    loop = *sc_uv_ptr++;

    for (i = 0; i < loop; i++) {
        sa_frame[sc_pos_ptr[1] - 25][sc_pos_ptr[0]].atr = sa_ram_vram_col[sc_num][0];
        sa_frame[sc_pos_ptr[1] - 25][sc_pos_ptr[0]].page = sa_ram_vram_col[sc_num][1];
        sa_frame[sc_pos_ptr[1] - 25][sc_pos_ptr[0]].cx = *sc_uv_ptr++;
        sa_frame[sc_pos_ptr[1] - 25][sc_pos_ptr[0]].cy = *sc_uv_ptr++;
        sc_pos_ptr += 2;
    }
}

/** @brief Transfer screen-RAM tile data to VRAM with custom offset and attribute. */
void sc_ram_to_vram_opc(s8 sc_num, s8 x, s8 y, u16 atr) {
    uintptr_t* sc_tbl_ptr;
    u8* sc_pos_ptr;
    u8* sc_uv_ptr;
    u16 loop;
    u16 i;

    if (No_Trans || sc_num < 0 || sc_num >= SC_RAM_VRAM_COUNT) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);
    sc_tbl_ptr = (uintptr_t*)sc_ram_vram_tbl[sc_num];
    sc_pos_ptr = (u8*)*sc_tbl_ptr;
    sc_tbl_ptr++;
    sc_uv_ptr = (u8*)*sc_tbl_ptr;
    loop = *sc_uv_ptr++;

    for (i = 0; i < loop; i++) {
        scfont_put(
            sc_pos_ptr[0] + x, sc_pos_ptr[1] + y, atr, sa_ram_vram_col[sc_num][1], sc_uv_ptr[0], sc_uv_ptr[1], 3);
        sc_uv_ptr += 2;
        sc_pos_ptr += 2;
    }
}

/** @brief Repaint a rectangular area of tile attributes. */
void sq_paint_chenge(u16 x, u16 y, u16 sx, u16 sy, u16 atr) {
    u16 i;
    u16 j;

    for (j = 0; j < sy; j++) {
        if (y - SA_FRAME_Y_OFFSET + j >= SA_FRAME_ROWS)
            break;
        for (i = 0; i < sx; i++) {
            if (x + i >= SA_FRAME_COLS)
                break;
            sa_frame[y - SA_FRAME_Y_OFFSET + j][x + i].atr = atr;
        }
    }
}

/* ── SF3 Logo ──────────────────────────────────────────────────── */

#define LOGO_X_START 128
#define LOGO_X_END 208

/** @brief Render the SF3 logo animation (multi-step). */
void SF3_logo(u8 step) {
    s32 i;
    RendererVertex pos[4];

    if (No_Trans) {
        return;
    }

    /* RmlUi bypass: suppress original sprite logo when an RmlUi screen
     * provides its own logo. Two cases:
     *   1. Title screen (title_tex_flag set) — large animated logo
     *   2. Attract demo fights (G_No[0]==1, G_No[1]>=3) — small in-match logo
     *      replaced by attract_overlay.rml's logo_small.png */
    if (use_rmlui &&
        ((rmlui_screen_title && title_tex_flag) || (rmlui_screen_attract_overlay && G_No[0] == 1 && G_No[1] >= 3))) {
        /* Match native SF33rd_Logo timing:
         *   step 0-7  = logo building tile-by-tile  → HD logo stays hidden
         *   step == 8 = logo fully revealed          → show HD logo
         *   step > 8  = logo disappearing            → hide HD logo */
        if (rmlui_screen_attract_overlay && G_No[0] == 1 && G_No[1] >= 3) {
            if (step == 8)
                rmlui_attract_overlay_show_logo();
            else if (step > 8)
                rmlui_attract_overlay_hide_logo();
        }
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    ppgSetupCurrentPaletteNumber(0, 29);
    pos[0].z = pos[1].z = pos[2].z = pos[3].z = PrioBase[2];

    if (step < 9) {
        pos[0].x = pos[1].x = 128.0f;
        pos[2].y = pos[3].y = 128.0f;
        pos[0].u = pos[1].u = TO_UV_256(pos[0].x);
        pos[2].v = pos[3].v = TO_UV_256(240.0f);

        for (i = 48; i > 0; i -= 8) {
            pos[0].y = i + 80;
            pos[1].y = pos[0].y - step;
            pos[2].x = 176 - i;
            pos[3].x = pos[2].x + step;
            pos[0].v = TO_UV_256(i + 192);
            pos[1].v = TO_UV_256((i + 192) - step);
            pos[2].u = TO_UV_256(176 - i);
            pos[3].u = TO_UV_256((176 - i) + step);
            pos[0].color = pos[1].color = pos[2].color = pos[3].color = 0xFFFFFFFF;
            Renderer_SetTexture(0);
            Renderer_DrawTexturedQuad(pos, 4);
        }

        pos[0].y = pos[1].y = 80.0f;
        pos[2].y = pos[3].y = 128.0f;
        pos[0].v = pos[1].v = TO_UV_256(192.0f);
        pos[2].v = pos[3].v = TO_UV_256(240.0f);

        for (i = LOGO_X_START; i < LOGO_X_END; i += 8) {
            pos[0].x = i;
            pos[1].x = i + step;
            pos[2].x = 48.0f + pos[0].x;
            pos[3].x = 48.0f + pos[1].x;
            pos[0].u = TO_UV_256(pos[0].x);
            pos[1].u = TO_UV_256(pos[1].x);
            pos[2].u = TO_UV_256(pos[2].x);
            pos[3].u = TO_UV_256(pos[3].x);
            pos[0].color = pos[1].color = pos[2].color = pos[3].color = 0xFFFFFFFF;
            Renderer_SetTexture(0);
            Renderer_DrawTexturedQuad(pos, 4);
        }

        pos[0].y = pos[1].y = 80.0f;
        pos[2].x = pos[3].x = 256.0f;
        pos[0].v = pos[1].v = TO_UV_256(192.0f);
        pos[2].u = pos[3].u = TO_UV_256(256.0f);

        for (i = 0; i < 48; i += 8) {
            pos[0].x = i + 208;
            pos[1].x = pos[0].x + step;
            pos[2].y = 128 - i;
            pos[3].y = pos[2].y - step;
            pos[0].u = TO_UV_256(pos[0].x);
            pos[1].u = TO_UV_256(pos[1].x);
            pos[2].v = TO_UV_256(240 - i);
            pos[3].v = TO_UV_256((240 - i) - step);
            pos[0].color = pos[1].color = pos[2].color = pos[3].color = 0xFFFFFFFF;
            Renderer_SetTexture(0);
            Renderer_DrawTexturedQuad(pos, 4);
        }
    } else {
        step -= 8;
        pos[0].x = pos[1].x = 128.0f;
        pos[2].y = pos[3].y = 128.0f;
        pos[0].u = pos[1].u = TO_UV_256(pos[0].x);
        pos[2].v = pos[3].v = TO_UV_256(240.0f);

        for (i = 40; i >= 0; i -= 8) {
            pos[1].y = i + 80;
            pos[0].y = (8.0f + pos[1].y) - step;
            pos[3].x = (176 - i);
            pos[2].x = (pos[3].x - 8.0f) + step;
            pos[0].v = TO_UV_256((i + 200) - step);
            pos[1].v = TO_UV_256(i + 192);
            pos[2].u = TO_UV_256((168 - i) + step);
            pos[3].u = TO_UV_256(176 - i);
            pos[0].color = pos[1].color = pos[2].color = pos[3].color = 0xFFFFFFFF;
            Renderer_SetTexture(0);
            Renderer_DrawTexturedQuad(pos, 4);
        }

        pos[0].y = pos[1].y = 80.0f;
        pos[2].y = pos[3].y = 128.0f;
        pos[0].v = pos[1].v = TO_UV_256(192.0f);
        pos[2].v = pos[3].v = TO_UV_256(240.0f);

        for (i = LOGO_X_START; i < LOGO_X_END; i += 8) {
            pos[0].x = (i + step);
            pos[1].x = (i + 8);
            pos[2].x = 48.0f + pos[0].x;
            pos[3].x = 48.0f + pos[1].x;
            pos[0].u = TO_UV_256(pos[0].x);
            pos[1].u = TO_UV_256(pos[1].x);
            pos[2].u = TO_UV_256(pos[2].x);
            pos[3].u = TO_UV_256(pos[3].x);
            pos[0].color = pos[1].color = pos[2].color = pos[3].color = 0xFFFFFFFF;
            Renderer_SetTexture(0);
            Renderer_DrawTexturedQuad(pos, 4);
        }

        pos[0].y = pos[1].y = 80.0f;
        pos[2].x = pos[3].x = 256.0f;
        pos[0].v = pos[1].v = TO_UV_256(192.0f);
        pos[2].u = pos[3].u = TO_UV_256(256.0f);

        for (i = 0; i < 48; i += 8) {
            pos[0].x = i + 208 + step;
            pos[1].x = i + 216;
            pos[2].y = 128 - i - step;
            pos[3].y = 120 - i;
            pos[0].u = TO_UV_256(pos[0].x);
            pos[1].u = TO_UV_256(pos[1].x);
            pos[2].v = TO_UV_256(240 - i - step);
            pos[3].v = TO_UV_256(232 - i);
            pos[0].color = pos[1].color = pos[2].color = pos[3].color = 0xFFFFFFFF;
            Renderer_SetTexture(0);
            Renderer_DrawTexturedQuad(pos, 4);
        }
    }
}

/* ── Training mode ─────────────────────────────────────────────── */

/** @brief Clear all training-mode display work data. */
void Training_Disp_Work_Clear() {
    u8 i;

    for (i = 0; i < 2; i++) {
        tr_data[i].max_hitcombo = 0;
        tr_data[i].new_max_flag = 0;
        tr_data[i].frash_flag = 0;
        tr_data[i].frash_switch = 2;
        tr_data[i].damage = 0;
        tr_data[i].total_damage = 0;
        tr_data[i].disp_total_damage = 0;
    }
}

/** @brief Record a damage value into the training-mode display buffer. */
void Training_Damage_Set(s16 damage, s16 arg1, u8 kezuri) {
    u8 j;

    if (Training_ID == 0) {
        j = 1;
    } else {
        j = 0;
    }

    if (damage == 0) {
        return;
    }

    tr_data[j].damage = damage;

    if (tr_data[j].damage > DAMAGE_DISPLAY_CAP) {
        tr_data[j].damage = DAMAGE_DISPLAY_CAP;
    }

    if (kezuri) {
        tr_data[j].disp_total_damage = damage;
        tr_data[j].total_damage = 0;
    } else {
        tr_data[j].total_damage = damage + tr_data[j].total_damage;
        tr_data[j].disp_total_damage = tr_data[j].total_damage;
    }

    if (tr_data[j].disp_total_damage > DAMAGE_DISPLAY_CAP) {
        tr_data[j].disp_total_damage = DAMAGE_DISPLAY_CAP;
    }
}

/** @brief Render the full training-mode data overlay (damage, combos, inputs). */
void Training_Data_Disp() {
    u8 i;
    u8 j;
    u8 atr;
    u8 gr;

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    if (Disp_Attack_Data == 0) {
        return;
    }

    if (use_rmlui && rmlui_hud_training_data) {
        return;
    }

    if (Training_ID == 0) {
        j = 1;
    } else {
        j = 0;
    }

    for (i = 0; i < 2; i++) {
        scfont_sqput3(i + Training_combo_pos_tbl[j],
                      i + 48,
                      13,
                      4,
                      0,
                      176,
                      76,
                      8,
                      i + 5,
                      Training_combo_prio_tbl[i] + (sa_pa_flag * 14) * i);

        SSPutDec3(i + (Training_combo_pos_tbl[j] + 158),
                  i + 48,
                  13,
                  tr_data[j].damage,
                  3,
                  i + 7,
                  Training_combo_prio_tbl[i] + (sa_pa_flag * 14) * i);
    }

    for (i = 0; i < 2; i++) {
        scfont_sqput3(i + (Training_combo_pos_tbl[j] + 1),
                      i + 58,
                      13,
                      4,
                      0,
                      184,
                      134,
                      8,
                      i + 5,
                      Training_combo_prio_tbl[i] + (sa_pa_flag * 14) * i);

        SSPutDec3(i + (Training_combo_pos_tbl[j] + 158),
                  i + 58,
                  13,
                  tr_data[j].disp_total_damage,
                  3,
                  i + 7,
                  Training_combo_prio_tbl[i] + (sa_pa_flag * 14) * i);
    }

    if (tr_data[j].frash_flag) {
        atr = 0x1E;
        gr = 9;
    } else {
        atr = 13;
        gr = 7;
    }

    tr_data[j].frash_switch--;

    if (tr_data[j].new_max_flag != 0 && tr_data[j].frash_switch == 0) {
        tr_data[j].frash_switch = 2;
        tr_data[j].new_max_flag--;
        tr_data[j].frash_flag = ~tr_data[j].frash_flag;

        if (tr_data[j].frash_flag) {
            atr = 0x1E;
            gr = 0;
        }
    }

    for (i = 0; i < 2; i++) {
        scfont_sqput3(i + (Training_combo_pos_tbl[j] + 1),
                      i + 68,
                      13,
                      4,
                      0,
                      192,
                      98,
                      8,
                      i + 3,
                      Training_combo_prio_tbl[i] + (sa_pa_flag * 14) * i);

        SSPutDec3(i + (Training_combo_pos_tbl[j] + 158),
                  i + 68,
                  atr,
                  tr_data[j].max_hitcombo,
                  2,
                  gr + i,
                  Training_combo_prio_tbl[i] + (sa_pa_flag * 14) * i);
    }
}

/* ── Button images & save/load ─────────────────────────────────── */

const u8 scrnAddTex1UV[SCRN_ADD_TEX_COUNT][4] = { { 96, 0, 32, 32 },  { 63, 0, 32, 32 },  { 0, 96, 32, 32 },
                                                   { 0, 64, 32, 32 },  { 0, 0, 32, 32 },   { 31, 0, 32, 32 },
                                                   { 32, 96, 32, 32 }, { 32, 64, 32, 32 }, { 128, 0, 96, 128 } };

/** @brief Render a button-prompt image from the controller texture atlas. */
void dispButtonImage(s32 px, s32 py, s32 pz, s32 sx, s32 sy, s32 cl, s32 ix) {
    PAL_CURSOR_COL oricol;
    Sprite prm;

    if (No_Trans || ix < 0 || ix >= SCRN_ADD_TEX_COUNT) {
        return;
    }

    oricol.color = -1;
    oricol.argb.a = (0xFF - cl);
    prm.tex_code = ppgGetUsingTextureHandle(&ppgScrTex, 5) | (ppgGetUsingPaletteHandle(&ppgScrPalShot, 0) << 0x10);
    prm.v[0].x = px;
    prm.v[0].y = py;
    prm.v[3].x = (px + sx);
    prm.v[3].y = (py - sy);
    njCalcPoint(NULL, &prm.v[0], &prm.v[0]);
    njCalcPoint(NULL, &prm.v[3], &prm.v[3]);
    prm.v[0].z = prm.v[3].z = PrioBase[pz];
    prm.t[0].s = scrnAddTex1UV[ix][0] / 256.0f;
    prm.t[3].s = (scrnAddTex1UV[ix][0] + scrnAddTex1UV[ix][2]) / 256.0f;
    prm.t[0].t = scrnAddTex1UV[ix][1] / 128.0f;
    prm.t[3].t = (scrnAddTex1UV[ix][1] + scrnAddTex1UV[ix][3]) / 128.0f;
    flSetRenderState(FLRENDER_TEXSTAGE0, prm.tex_code);
    SDLGameRenderer_DrawSprite(&prm, oricol.color);
}

/** @brief Render a button-prompt image variant 2 (alternate UV mapping). */
void dispButtonImage2(s32 px, s32 py, s32 pz, s32 sx, s32 sy, s32 cl, s32 ix) {
    PAL_CURSOR_COL oricol;
    Sprite prm;

    if (No_Trans || ix < 0 || ix >= SCRN_ADD_TEX_COUNT) {
        return;
    }

    oricol.color = -1;
    oricol.argb.a = (0xFF - cl);
    prm.tex_code = ppgGetUsingTextureHandle(&ppgScrTex, 5) | (ppgGetUsingPaletteHandle(&ppgScrPalShot, 0) << 0x10);
    prm.v[0].x = px;
    prm.v[0].y = py;
    prm.v[3].x = (px + sx);
    prm.v[3].y = (py + sy);
    prm.v[0].z = prm.v[3].z = PrioBase[pz];
    prm.t[0].s = scrnAddTex1UV[ix][0] / 256.0f;
    prm.t[3].s = (scrnAddTex1UV[ix][0] + scrnAddTex1UV[ix][2]) / 256.0f;
    prm.t[0].t = scrnAddTex1UV[ix][1] / 128.0f;
    prm.t[3].t = (scrnAddTex1UV[ix][1] + scrnAddTex1UV[ix][3]) / 128.0f;
    flSetRenderState(FLRENDER_TEXSTAGE0, prm.tex_code);
    SDLGameRenderer_DrawSprite(&prm, oricol.color);
}

/** @brief Render the save/load title banner from event-work data. */
void dispSaveLoadTitle(void* ewk) {
    WORK* wk;
    PAL_CURSOR_COL oricol;
    Sprite prm;
    FLVec3 pos[2];
    f32 step_t;
    s32 i;

    if (No_Trans) {
        return;
    }

    wk = (WORK*)ewk;
    mlt_obj_matrix(wk, 0);
    oricol.color = -1;
    oricol.argb.a = (0xFF - wk->my_clear_level);
    prm.tex_code = ppgGetUsingTextureHandle(&ppgScrTex, 6) | (ppgGetUsingPaletteHandle(&ppgScrPalOpt, 0) << 0x10);
    flSetRenderState(FLRENDER_TEXSTAGE0, prm.tex_code);
    prm.t[0].s = 0.0f;
    prm.t[3].s = 1.0f;
    prm.t[0].t = TO_UV_128(0.0f);
    prm.t[3].t = TO_UV_128(36.0f);
    step_t = 36.0f;
    pos[0].x = -192.0f;
    pos[0].y = -12.0f;
    pos[1].x = -64.0f;
    pos[1].y = -48.0f;
    pos[0].z = pos[1].z = 0.0f;

    for (i = 0; i < 3; i++) {
        njCalcPoint(NULL, (Vec3*)&pos[0], &prm.v[0]);
        njCalcPoint(NULL, (Vec3*)&pos[1], &prm.v[3]);
        SDLGameRenderer_DrawSprite(&prm, oricol.color);
        step_t += 36.0f;
        prm.t[0].t = prm.t[3].t;
        prm.t[3].t = step_t / 128.0f;
        pos[0].x += 128.0f;
        pos[1].x += 128.0f;
    }
}
