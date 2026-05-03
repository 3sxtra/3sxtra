/**
 * @file bg.c
 * Background/Stage rendering, scroll, zoom, and parallax logic.
 * Texture loading split to bg_load.c, tile rewrite setup split to bg_rewrite.c.
 * See SYSTEM_MODERNIZATION.md #37.
 */

#include "sf33rd/Source/Game/stage/bg.h"
#include "game_state.h"
#include "sf33rd/Source/Game/stage/bg_load.h"
#include "sf33rd/Source/Game/stage/bg_rewrite.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "common.h"
#include "port/renderer_plugin.h"
#include "port/sdl/renderer/sprite_override.h"
#include "port/rendering/legacy_matrix.h"
#include "port/rendering/renderer.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "structs.h"
#include "port/I_System.h"

// sbss
Vertex scrDrawPos[4];
RendererVertex bgpoly[4];
u8 bg_priority[4];

// bss

/** @brief Number of 64-entry palette slots the Shin Gouki XOR diff covers. */
#define GOUKI_XOR_PAL_SLOTS 17

#include "shin_gouki_xor_diff.h"

/** @brief Non-zero when the Shin Gouki palette XOR has been applied to ColorRAM. */
u8 s_gouki_pal_xored;

/** @brief Update read/write work buffers for animated background tiles. */
static void bgRWWorkUpdate();
/** @brief Draw all visible chips for a single background screen. */
static void bgDrawOneScreen(s32 bgnum, s32 gixbase, s32* xx, s32* yy, s32 /* unused */, s32 ofsPal,
                            PPGDataList* curDataList);
/** @brief Draw a single background tile chip at the given position. */
static void bgDrawOneChip(s32 x, s32 y, s32 xs, s32 ys, s32 gbix, u32 vtxCol, s32 ofsPal);
/** @brief Draw the Akebono (dawn sky) background layer. */
static void bgAkebonoDraw();
/** @brief Calculate scroll position for PPG background rendering. */
static void ppgCalScrPosition(s32 x, s32 y, s32 xs, s32 ys);

/** @brief Transfer (render) a background screen layer to the display. */
void scr_trans(u8 bgnm) {
    PPGDataList* curDataList;
    Vec3 point[2];
    s32 xx[2];
    s32 yy[2];
    s32 i;
    s32 x;
    s32 y;
    s32 global_index;
    s32 global_index_real;
    s32 palOffset;
    u32 vtxColor;
    s32 bg_strip_pos;

    njUnitMatrix(0);
    njScale(0, 1.0f, -1.0f, 1.0f);
    njTranslate(0, 0.0f, -1024.0f, 0.0f);
    njTranslate(0, (s16)g_state.bg_prm[bgnm].bg_h_shift, (s16)g_state.bg_prm[bgnm].bg_v_shift, 0.0f);
    njScale(0, 1.0f, -1.0f, 1.0f);
    njTranslate(0, 0.0f, -224.0f, 0.0f);
    njScale(0, 1.0f / g_state.scr_sc, 1.0f / g_state.scr_sc, 1.0f);
    point[0].x = 0.0f;
    point[0].y = 0.0f;
    point[0].z = 00.f;
    point[1].x = 648.0f;
    point[1].y = 488.0f;
    point[1].z = 0.0f;
    njCalcPoints(0, &point[0], &point[0], 2);
    xx[0] = ((s32)point[0].x) & ~0x7F;
    yy[0] = ((s32)point[0].y) & ~0x7F;
    xx[1] = ((s32)point[1].x + 0x7F) & ~0x7F;
    yy[1] = ((s32)point[1].y + 0x7F) & ~0x7F;

    for (x = 0; x < 2; x++) {
        if (xx[x] < 0) {
            xx[x] = 0;
        }

        if (0x3FF < xx[x]) {
            xx[x] = 0x3FF;
        }

        if (yy[x] < 0) {
            yy[x] = 0;
        }

        if (0x3FF < yy[x]) {
            yy[x] = 0x3FF;
        }
    }

    njUnitMatrix(0);
    njScale(0, g_state.scr_sc, g_state.scr_sc, 1.0);
    njTranslate(0, 0, 224.0, 0);
    njScale(0, 1.0, -1.0, 1.0);
    njTranslate(0, (s16)-g_state.bg_prm[bgnm].bg_h_shift, (s16)-g_state.bg_prm[bgnm].bg_v_shift, 0);
    njGetMatrix(&BgMATRIX[bgnm + 1]);
    njTranslate(0, 0, 1024.0, PrioBase[bg_priority[bgnm]]);
    njScale(0, 1.0, -1.0, 1.0);

    if (Debug_w[DEBUG_BG_DRAW_OFF]) {
        return;
    }

    palOffset = g_state.bgPalCodeOffset[bgnm];

    if (g_state.ending_flag == 0) {
        if (bgnm == 3) {
            ppgSetupCurrentDataList(&ppgAkeList);
            bgAkebonoDraw();
            return;
        }

        global_index = (bgnm * 64) + 100;
        ppgSetupCurrentDataList(&ppgBgList[bgnm]);
        curDataList = &ppgBgList[bgnm];
    } else {
        global_index = (bgnm * 64) + 100;
        ppgSetupCurrentDataList(&ppgBgList[bgnm]);
        curDataList = &ppgBgList[bgnm];
    }

    switch (g_state.special_stage) {
    case 1:
        for (y = yy[0]; y < yy[1]; y += 128) {
            for (x = xx[0]; x < xx[1]; x += 128) {
                global_index_real = global_index + (((y >> 7) << 3) + (x >> 7));
                vtxColor = 0xFFFFFFFF;

                if (bgnm == 0) {
                    for (i = 0; i < 4; i++) {
                        if (global_index_real == g_state.rw_dat[i + 1].rwgbix) {
                            global_index_real = g_state.rw_dat[i + 1].gbix;
                            if (ppgCheckTextureNumber(0, global_index_real) == 0) {
                                ppgSetupCurrentDataList(&ppgRwBgList);
                            }
                            break;
                        }
                    }
                } else {
                    for (i = 0; i < 13; i++) {
                        if (global_index_real == g_state.rw_gbix[i]) {
                            global_index_real = *(g_state.rw_dat[0].rwd_ptr + i + 1);
                            vtxColor = *g_state.rw3col_ptr;

                            if (ppgCheckTextureNumber(0, global_index_real) == 0) {
                                ppgSetupCurrentDataList(&ppgRwBgList);
                            }
                            break;
                        }
                    }
                }

                bgDrawOneChip(x, y, 128, 128, global_index_real, vtxColor, palOffset);
                ppgSetupCurrentDataList(curDataList);
            }
        }

        if (g_state.EXE_flag != 0 || g_state.Game_pause != 0) {
            return;
        }

        if (bgnm == 0) {
            for (i = 0; i < 4; i = i + 1) {
                g_state.rw_dat[i + 1].rw_cnt--;

                if (g_state.rw_dat[i + 1].rw_cnt == 0) {
                    if (g_state.rw_dat[i + 1].rwd_ptr[0] == -1) {
                        g_state.rw_dat[i + 1].rwd_ptr = g_state.rw_dat[i + 1].brw_ptr;
                        g_state.rw_dat[i + 1].rw_cnt = *g_state.rw_dat[i + 1].rwd_ptr++;
                        g_state.rw_dat[i + 1].gbix = *g_state.rw_dat[i + 1].rwd_ptr++;
                    } else {
                        g_state.rw_dat[i + 1].rw_cnt = *g_state.rw_dat[i + 1].rwd_ptr++;
                        g_state.rw_dat[i + 1].gbix = *g_state.rw_dat[i + 1].rwd_ptr++;
                    }
                }
            }

            break;
        }

        g_state.rw_dat[0].rw_cnt--;

        if (g_state.rw_dat[0].rw_cnt != 0) {
            break;
        }

        if (g_state.stage_flash == 0) {
            g_state.rw_dat[0].rwd_ptr += 14;
            g_state.rw3col_ptr++;
            g_state.rw_dat[0].rw_cnt = g_state.rw_dat[0].rwd_ptr[0];

            if (g_state.rw_dat[0].rw_cnt != -1) {
                break;
            }

            g_state.stage_flash = random_16_bg();
            g_state.stage_flash = stage03_flash_tbl[g_state.stage_flash];

            if (g_state.stage_flash == 0) {
                g_state.rw_dat[0].rwd_ptr = g_state.rw_dat[0].brw_ptr;
                g_state.rw_dat[0].rw_cnt = g_state.rw_dat[0].rwd_ptr[0];
                g_state.rw3col_ptr = &rw30col[0];
            } else {
                g_state.rw_dat[0].rwd_ptr = &rw31[0];
                g_state.rw_dat[0].rw_cnt = 2;
                g_state.stage_ftimer = g_state.stage_flash;
                g_state.rw3col_ptr = rw31col;
            }

            break;
        }

        g_state.rw_dat[0].rwd_ptr += 14;
        g_state.rw3col_ptr++;
        g_state.rw_dat[0].rw_cnt = g_state.rw_dat[0].rwd_ptr[0];

        if (g_state.rw_dat[0].rw_cnt != -1) {
            break;
        }

        g_state.stage_ftimer--;

        if (g_state.stage_ftimer < 1) {
            g_state.stage_flash = 0;
            g_state.rw_dat[0].rwd_ptr = g_state.rw_dat[0].brw_ptr;
            g_state.rw_dat[0].rw_cnt = 2;
            g_state.rw3col_ptr = rw30col;
        } else {
            g_state.rw_dat[0].rwd_ptr = rw31;
            g_state.rw_dat[0].rw_cnt = 2;
            g_state.rw3col_ptr = rw31col;
        }

        break;

    case 2:
        if (g_state.judge_flag == 1 && bgnm == 1) {
            vtxColor = 0xFFA0A0A0;
        } else {
            vtxColor = 0xFFFFFFFF;
        }

        for (y = yy[0]; y < yy[1]; y += 128) {
            for (x = xx[0]; x < xx[1]; x += 128) {
                global_index_real = global_index + (((y >> 7) << 3) + (x >> 7));

                if (bgnm == 1) {
                    global_index_real += g_state.yang_ix_plus;
                }

                if (ppgCheckTextureNumber(0, global_index_real) == 0) {
                    ppgSetupCurrentDataList(&ppgRwBgList);
                }
                bgDrawOneChip(x, y, 128, 128, global_index_real, vtxColor, palOffset);
                ppgSetupCurrentDataList(curDataList);
            }
        }

        if (g_state.EXE_flag != 0 || g_state.Game_pause != 0) {
            return;
        }

        if (bgnm != 1) {
            break;
        }

        g_state.yang_timer--;

        if (g_state.yang_timer != 0) {
            break;
        }

        g_state.yang_timer = 4;
        g_state.yang_ix++;

        if (g_state.yang_ix == 4) {
            g_state.yang_ix = 0;
        }

        g_state.yang_ix_plus = g_state.yang_ix << 5;
        break;

    case 3:
        for (y = yy[0]; y < yy[1]; y += 128) {
            for (x = xx[0]; x < xx[1]; x += 128) {
                global_index_real = global_index + (((y >> 7) << 3) + (x >> 7));

                if (bgnm == 1) {
                    if (g_state.rw_dat[1].rwgbix == global_index_real) {
                        global_index_real = g_state.rw_dat[1].gbix;

                        if (!ppgCheckTextureNumber(0, global_index_real)) {
                            ppgSetupCurrentDataList(&ppgRwBgList);
                        }
                    } else {
                        for (i = 0; i < 4; i++) {
                            if (global_index_real == g_state.rw_gbix[i]) {
                                global_index_real = *(g_state.rw_dat[0].rwd_ptr + i + 1);

                                if (!ppgCheckTextureNumber(0, global_index_real)) {
                                    ppgSetupCurrentDataList(&ppgRwBgList);
                                }

                                break;
                            }
                        }
                    }
                }

                bgDrawOneChip(x, y, 128, 128, global_index_real, -1, palOffset);
                ppgSetupCurrentDataList(curDataList);
            }
        }

        if (g_state.EXE_flag != 0 || g_state.Game_pause != 0) {
            return;
        }

        if (bgnm != 1) {
            break;
        }

        g_state.rw_dat[0].rw_cnt--;

        if (g_state.rw_dat[0].rw_cnt == 0) {
            g_state.rw_dat[0].rwd_ptr += 5;
            g_state.rw_dat[0].rw_cnt = g_state.rw_dat[0].rwd_ptr[0];

            if (g_state.rw_dat[0].rw_cnt == -1) {
                g_state.stage_ftimer--;

                if (g_state.stage_ftimer == 0) {
                    g_state.stage_flash = random_16_bg();
                    g_state.stage_ftimer = random_16_bg();

                    switch (g_state.stage_flash) {
                    case 0:
                    case 1:
                        g_state.rw_dat[0].rwd_ptr = g_state.rw_dat[0].brw_ptr = rw191;
                        g_state.rw_dat[0].rw_cnt = 1;
                        g_state.stage_ftimer = stage19_loop_tbl2[g_state.stage_ftimer];
                        break;

                    case 2:
                    case 3:
                        g_state.rw_dat[0].rwd_ptr = g_state.rw_dat[0].brw_ptr = rw192;
                        g_state.rw_dat[0].rw_cnt = 1;
                        g_state.stage_ftimer = stage19_loop_tbl2[g_state.stage_ftimer];
                        break;

                    default:
                        g_state.rw_dat[0].rwd_ptr = g_state.rw_dat[0].brw_ptr = rw190;
                        g_state.rw_dat[0].rw_cnt = 2;
                        g_state.stage_ftimer = stage19_loop_tbl1[g_state.stage_ftimer];
                        break;
                    }
                } else {
                    g_state.rw_dat[0].rwd_ptr = g_state.rw_dat[0].brw_ptr;
                    g_state.rw_dat[0].rw_cnt = g_state.rw_dat[0].rwd_ptr[0];
                }
            }
        }

        g_state.rw_dat[1].rw_cnt--;

        if (g_state.rw_dat[1].rw_cnt != 0) {
            break;
        }

        if (g_state.rw_dat[1].rwd_ptr[0] == -1) {
            g_state.rw_dat[1].rwd_ptr = g_state.rw_dat[1].brw_ptr;
            g_state.rw_dat[1].rw_cnt = *g_state.rw_dat[1].rwd_ptr++;
            g_state.rw_dat[1].gbix = *g_state.rw_dat[1].rwd_ptr++;
        } else {
            g_state.rw_dat[1].rw_cnt = *g_state.rw_dat[1].rwd_ptr++;
            g_state.rw_dat[1].gbix = *g_state.rw_dat[1].rwd_ptr++;
        }

        break;

    case 5:
        for (y = yy[0]; y < yy[1]; y += 128) {
            for (x = xx[0]; x < xx[1]; x += 128) {
                global_index_real = global_index + (((y >> 7) << 3) + (x >> 7));

                if (g_state.palette_swap != 0) {
                    for (i = 0; i < 16; i++) {
                        if (g_state.gouki_end_gbix[i] == global_index_real) {
                            global_index_real = gouki_end_nosekae[g_state.palette_swap - 1][i];

                            if (ppgCheckTextureNumber(0, global_index_real) == 0) {
                                if (ppgCheckTextureNumber(&ppgRwBgTex, global_index_real)) {
                                    ppgSetupCurrentDataList(&ppgRwBgList);
                                } else {
                                    ppgSetupCurrentDataList(&ppgAkeList);
                                }
                            }

                            break;
                        }
                    }
                }

                if (bgnm == 0) {
                    if (g_state.bg_rewrite[0]) {
                        for (i = 0; i < 12; i++) {
                            if (global_index_real == g_state.rw_dat[i].rwgbix) {
                                global_index_real = g_state.rw_dat[i].rwd_ptr[g_state.g_number[0]];

                                if (ppgCheckTextureNumber(0, global_index_real) == 0) {
                                    if (ppgCheckTextureNumber(&ppgRwBgTex, global_index_real)) {
                                        ppgSetupCurrentDataList(&ppgRwBgList);
                                    } else {
                                        ppgSetupCurrentDataList(&ppgAkeList);
                                    }
                                }

                                break;
                            }
                        }
                    }

                    if (g_state.bg_rewrite[1]) {
                        for (i = 12; i < 20; i++) {
                            if (global_index_real == g_state.rw_dat[i].rwgbix) {
                                global_index_real = g_state.rw_dat[i].rwd_ptr[g_state.g_number[1]];

                                if (!ppgCheckTextureNumber(0, global_index_real)) {
                                    if (ppgCheckTextureNumber(&ppgRwBgTex, global_index_real)) {
                                        ppgSetupCurrentDataList(&ppgRwBgList);
                                    } else {
                                        ppgSetupCurrentDataList(&ppgAkeList);
                                    }
                                }

                                break;
                            }
                        }
                    }
                }

                bgDrawOneChip(x, y, 128, 128, global_index_real, -1, palOffset);
                ppgSetupCurrentDataList(curDataList);
            }
        }

        scr_calc2(bgnm);
        break;

    case 6:
        for (y = yy[0]; y < yy[1]; y += 128) {
            for (x = xx[0]; x < xx[1]; x += 128) {
                global_index_real = global_index + (((y >> 7) << 3) + (x >> 7));

                if (bgnm == 0) {
                    switch (g_state.char_rewrite) {
                    case 1:
                        for (i = 0; i < 8; i++) {
                            if (global_index_real == g_state.rw_dat[i].rwgbix) {
                                global_index_real = g_state.rw_dat[i].rwd_ptr[g_state.c_number];

                                if (!ppgCheckTextureNumber(0, global_index_real)) {
                                    ppgSetupCurrentDataList(&ppgRwBgList);
                                }

                                break;
                            }
                        }

                        break;

                    case 2:
                        for (i = 8; i < 16; i++) {
                            if (global_index_real == g_state.rw_dat[i].rwgbix) {
                                global_index_real = g_state.rw_dat[i].rwd_ptr[g_state.c_number];

                                if (!ppgCheckTextureNumber(0, global_index_real)) {
                                    ppgSetupCurrentDataList(&ppgRwBgList);
                                }

                                break;
                            }
                        }
                    }
                }

                bgDrawOneChip(x, y, 128, 128, global_index_real, -1, palOffset);
                ppgSetupCurrentDataList(curDataList);
            }
        }

        scr_calc2(bgnm);
        break;

    case 7:
        bgDrawOneScreen(bgnm, global_index, &xx[0], &yy[0], -1, palOffset, curDataList);

        if (g_state.EXE_flag != 0) {
            break;
        }

        if (g_state.Game_pause != 0) {
            break;
        }

        if (g_state.rw_bg_flag[bgnm] && g_state.rw_num) {
            bgRWWorkUpdate();
        }

        scr_calc2(bgnm);
        break;

    case 4:
        if (bgnm == 2) {
            bg_strip_pos = g_state.bg_pos[2].scr_x_buff.word_pos.h - 320;
            bg_strip_pos = bg_strip_pos * -0.5f;
            ppgSetupCurrentDataList(&ppgAkaneList);

            for (x = 0; x < 3; x = x + 1) {
                scr_trans_sub2(x * 256 + 128, 128, bg_strip_pos);

                if (No_Trans == 0) {
                    ppgSetupCurrentPaletteNumber(0, x);
                    // njDrawTexture(bgpoly, 4, x, 0);
                    u16 tex = ppgGetUsingTextureHandle(NULL, x);
                    u16 pal = ppgGetCurrentPaletteHandle();
                    Renderer_SetTexture(tex | (pal << 16));
                    Renderer_DrawTexturedQuadVtx((const RendererVertex*)bgpoly, 4);
                }
            }
        }

        /* fallthrough */

    default:
        bgDrawOneScreen(bgnm, global_index, &xx[0], &yy[0], -1, palOffset, curDataList);

        if (g_state.EXE_flag == 0 && g_state.Game_pause == 0 && g_state.rw_bg_flag[bgnm] && g_state.rw_num) {
            bgRWWorkUpdate();
        }

        break;
    }
}

/** @brief Update read/write work buffers for animated background tiles. */
void bgRWWorkUpdate() {
    s32 i;

    for (i = 0; i < g_state.rw_num; i++) {
        g_state.rw_dat[i].rw_cnt--;

        if (g_state.rw_dat[i].rw_cnt == 0) {
            if (*g_state.rw_dat[i].rwd_ptr == -1) {
                g_state.rw_dat[i].rwd_ptr = g_state.rw_dat[i].brw_ptr;
                g_state.rw_dat[i].rw_cnt = *g_state.rw_dat[i].rwd_ptr++;
                g_state.rw_dat[i].gbix = *g_state.rw_dat[i].rwd_ptr++;
            } else {
                g_state.rw_dat[i].rw_cnt = *g_state.rw_dat[i].rwd_ptr++;
                g_state.rw_dat[i].gbix = *g_state.rw_dat[i].rwd_ptr++;
            }
        }
    }
}

/** @brief Draw all visible chips for a single background screen. */
void bgDrawOneScreen(s32 bgnum, s32 gixbase, s32* xx, s32* yy, s32 /* unused */, s32 ofsPal, PPGDataList* curDataList) {
    s32 i, x, y, gbix;

    for (y = yy[0]; y < yy[1]; y += 128) {
        for (x = xx[0]; x < xx[1]; x += 128) {
            gbix = ((y >> 7) << 3) + (x >> 7) + gixbase;

            if (g_state.rw_bg_flag[bgnum] && g_state.rw_num) {
                for (i = 0; i < g_state.rw_num; i++) {
                    if (bgnum == g_state.rw_dat[i].bg_num && gbix == g_state.rw_dat[i].rwgbix) {
                        gbix = g_state.rw_dat[i].gbix;
                        if (!(ppgCheckTextureNumber(0, gbix))) {
                            ppgSetupCurrentDataList(&ppgRwBgList);
                        }
                        break;
                    }
                }
            }

            bgDrawOneChip(x, y, 128, 128, gbix, -1, ofsPal);
            ppgSetupCurrentDataList(curDataList);
        }
    }
}

/** @brief Draw a single background tile chip at the given position. */
void bgDrawOneChip(s32 x, s32 y, s32 xs, s32 ys, s32 gbix, u32 vtxCol, s32 ofsPal) {
    if ((No_Trans == 0) && ppgCheckTextureNumber(0, gbix)) {
        ppgCalScrPosition(x, y, xs, ys);

        if ((scrDrawPos->x >= 384.0f) || (scrDrawPos[3].x < 0.0f) || (scrDrawPos->y >= 224.0f) ||
            (scrDrawPos[3].y < 0.0f)) {
            return;
        }

        if (RENDERER_HAS_PLUGIN()) {
            void* hd_tex_plugin = g_renderer_plugin->LoadBGTileOverride(bg_texture_type, g_state.bg_w.stage, gbix);
            if (hd_tex_plugin != NULL) {
                float dx = scrDrawPos[0].x;
                float dy = scrDrawPos[0].y;
                float dw = scrDrawPos[3].x - scrDrawPos[0].x;
                float dh = scrDrawPos[3].y - scrDrawPos[0].y;
                float dz = flPS2ConvScreenFZ(scrDrawPos[0].z);
                g_renderer_plugin->DrawBGTile(hd_tex_plugin, dx, dy, dw, dh, dz, vtxCol);
                return;
            }
        }

        ppgWriteQuadUseTrans(scrDrawPos, vtxCol, 0, gbix, 0, 0, ofsPal);
    }
}

/** @brief Draw the Akebono (dawn sky) background layer. */
void bgAkebonoDraw() {
    s32 i;

    scrDrawPos->x = 0.0f;
    scrDrawPos->y = 0.0f;
    scrDrawPos[3].x = 128.0f;
    scrDrawPos[3].y = 224.0f;
    scrDrawPos->z = scrDrawPos[3].z = PrioBase[bg_priority[3]];
    scrDrawPos->s = scrDrawPos->t = 0.0f;
    scrDrawPos[3].s = 1.0f;
    scrDrawPos[3].t = 0.875f;

    for (i = 0; i < 3; i++) {
        ppgWriteQuadUseTrans(scrDrawPos, 0xFFFFFFFF, NULL, i, i, 0, 0);
        scrDrawPos->x += 128.0f;
        scrDrawPos[3].x += 128.0f;
    }
}

/** @brief Calculate scroll position for PPG background rendering. */
void ppgCalScrPosition(s32 x, s32 y, s32 xs, s32 ys) {
    Vec3 point[2];

    point[0].x = (f32)x;
    point[0].y = (f32)y;
    point[1].x = (f32)(x + xs);
    point[1].y = (f32)(y + ys);
    point[0].z = point[1].z = 0;
    njCalcPoints(0, point, point, 2);
    scrDrawPos[0].x = scrDrawPos[2].x = point[0].x;
    scrDrawPos[0].y = scrDrawPos[1].y = point[0].y;
    scrDrawPos[1].x = scrDrawPos[3].x = point[1].x;
    scrDrawPos[2].y = scrDrawPos[3].y = point[1].y;
    scrDrawPos[0].z = scrDrawPos[1].z = scrDrawPos[2].z = scrDrawPos[3].z = point[0].z;

    scrDrawPos[0].s = (f32)(x & 0x7F) / 128.0f;
    scrDrawPos[0].t = (f32)(y & 0x7F) / 128.0f;
    scrDrawPos[3].s = (f32)((x & 0x7F) + xs) / 128.0f;
    scrDrawPos[3].t = (f32)((y & 0x7F) + ys) / 128.0f;
    scrDrawPos[1].s = scrDrawPos[3].s;
    scrDrawPos[2].s = scrDrawPos[0].s;
    scrDrawPos[1].t = scrDrawPos[0].t;
    scrDrawPos[2].t = scrDrawPos[3].t;
}

/** @brief Sub-routine for background tile rendering with suzi offset. */
void scr_trans_sub2(s32 x, s32 y, s32 suzi) {
    Vec3 point[2];
    Vec3 spoint[2];

    point[0].x = (f32)x;
    spoint[0].x = (f32)(x + suzi);
    point[0].y = spoint[0].y = (f32)(y + 0x200);
    point[1].x = (f32)(x + 0x100);
    spoint[1].x = (f32)(x + suzi + 0x100);
    point[1].y = spoint[1].y = (f32)(y + 0x300);
    point[0].z = point[1].z = spoint[0].z = spoint[1].z = 0;
    njCalcPoints(NULL, &point[0], &point[0], 2);
    njCalcPoints(NULL, &spoint[0], &spoint[0], 2);
    bgpoly[0].x = spoint[0].x;
    bgpoly[0].y = point[0].y;
    bgpoly[0].z = point[0].z;
    bgpoly[0].u = 0.0f;
    bgpoly[0].v = 0.0f;
    bgpoly[0].color = 0xFFFFFFFF;
    bgpoly[1].x = spoint[1].x;
    bgpoly[1].y = point[0].y;
    bgpoly[1].z = point[0].z;
    bgpoly[1].u = 1.0f;
    bgpoly[1].v = 0.0f;
    bgpoly[1].color = 0xFFFFFFFF;
    bgpoly[2].x = point[0].x;
    bgpoly[2].y = point[1].y;
    bgpoly[2].z = point[1].z;
    bgpoly[2].u = 0.0f;
    bgpoly[2].v = 1.0f;
    bgpoly[2].color = 0xFFFFFFFF;
    bgpoly[3].x = point[1].x;
    bgpoly[3].y = point[1].y;
    bgpoly[3].z = point[1].z;
    bgpoly[3].u = 1.0f;
    bgpoly[3].v = 1.0f;
    bgpoly[3].color = 0xFFFFFFFF;
}

/** @brief Calculate scroll offset for a background layer (horizontal). */
void scr_calc(u8 bgnm) {
    njUnitMatrix(NULL);
    njScale(NULL, g_state.scr_sc, g_state.scr_sc, 1.0f);
    njTranslate(NULL, 0.0f, 224.0f, 0.0f);
    njScale(NULL, 1.0f, -1.0f, 1.0f);
    njTranslate(NULL, (s16)-g_state.bg_prm[bgnm].bg_h_shift, (s16)-g_state.bg_prm[bgnm].bg_v_shift, 0.0f);
    njGetMatrix(&BgMATRIX[bgnm + 1]);
}

/** @brief Calculate scroll offset for a background layer (vertical). */
void scr_calc2(u8 bgnm) {
    njUnitMatrix(NULL);
    njScale(NULL, g_state.scr_sc, g_state.scr_sc, 1.0f);
    njTranslate(NULL, 0.0f, 224.0f, 0.0f);
    njScale(NULL, 1.0f, -1.0f, 1.0f);
    njTranslate(NULL, (s16)-g_state.end_prm[bgnm + 1].bg_h_shift, (s16)-g_state.end_prm[bgnm + 1].bg_v_shift, 0.0f);
    njGetMatrix(&BgMATRIX[bgnm + 1]);
}

/** @brief Restore parallax family positions after pause. */
void Pause_Family_On() {
    njUnitMatrix(0);
    njTranslate(0, 0, 224, 0);
    njScale(0, 1, -1, 1);
    njGetMatrix(&BgMATRIX[8]);
}

/** @brief Initialize the zoom frame system. */
void Zoomf_Init() {
    g_state.zoom_add = 64;
    g_state.scr_sc = 1.0f;
    g_state.screen_adjust_x = 0;
    g_state.screen_adjust_y = 0;
}

/** @brief Set the zoom value for the stage frame. */
void Zoom_Value_Set(u16 zadd) {
    f32 work;
    u16 add;

    if (zadd < 0x40) {
        g_state.scr_sc = 64.0f / zadd;
        return;
    }

    if (zadd == 0x40) {
        g_state.scr_sc = 1.0f;
        return;
    }

    add = zadd & 0x3F;
    work = 1.0f / (64.0f / add);
    add = zadd & 0xFFC0;
    add >>= 6;
    g_state.scr_sc = 1.0f / (add + work);
}

/** @brief Expand the visible stage frame outward. */
void Frame_Up(u16 x, u16 y, u16 add) {
    if (g_state.zoom_add < 2) {
        g_state.scr_sc = 64.0f;
        return;
    }

    g_state.zoom_add -= add;
    Zoom_Value_Set(g_state.zoom_add);
    Frame_Adgjust(x, y);
}

/** @brief Shrink the visible stage frame inward. */
void Frame_Down(u16 x, u16 y, u16 add) {
    if (g_state.zoom_add >= 0xFFC0) {
        g_state.scr_sc = 0.0009775171f;
        return;
    }

    g_state.zoom_add += add;
    Zoom_Value_Set(g_state.zoom_add);
    Frame_Adgjust(x, y);
}

/** @brief Adjust the stage frame to match the current camera position. */
void Frame_Adgjust(u16 pos_x, u16 pos_y) {
    u16 buff;

    if (g_state.zoom_add >= 0x40) {
        buff = g_state.zoom_add;
        buff -= 0x40;
        buff *= pos_x;
        buff >>= 6;
        buff &= 0x1FF;
        g_state.screen_adjust_x = -buff;
    } else {
        buff = 0x40;
        buff -= g_state.zoom_add;
        buff *= pos_x;
        buff >>= 6;
        buff &= 0x1FF;
        g_state.screen_adjust_x = buff;
    }

    if (g_state.zoom_add >= 0x40) {
        buff = g_state.zoom_add;
        buff -= 0x40;
        buff *= pos_y + 0x15;
        buff >>= 6;
        buff &= 0x1FF;
        g_state.screen_adjust_y = -buff;

        if (g_state.screen_adjust_y == -0x14) {
            g_state.screen_adjust_y += 1;
        }
    } else {
        buff = 0x40;
        buff -= g_state.zoom_add;
        buff *= pos_y + 0x15;
        buff >>= 6;
        buff &= 0x1FF;
        g_state.screen_adjust_y = buff;

        if (g_state.screen_adjust_y == -0x14) {
            g_state.screen_adjust_y += 1;
        }
    }
}

/** @brief Initialize all screen scroll positions to default. */
void Scrn_Pos_Init() {
    u8 i;

    for (i = 0; i < 8; i++) {
        g_state.bg_pos[i].scr_x.long_pos = 0;
        g_state.bg_pos[i].scr_x_buff.long_pos = 0;
        g_state.bg_pos[i].scr_y.long_pos = 0;
        g_state.bg_pos[i].scr_y_buff.long_pos = 0;
        g_state.bg_prm[i].bg_h_shift = 0;
        g_state.bg_prm[i].bg_v_shift = 0;
        g_state.end_prm[i].bg_h_shift = 0;
        g_state.end_prm[i].bg_v_shift = 0;
    }
}

/** @brief Set scroll delta for a background layer. */
void Scrn_Move_Set(s8 bgnm, s16 x, s16 y) {
    g_state.bg_pos[bgnm].scr_x.word_pos.h = x;
    g_state.bg_pos[bgnm].scr_y.word_pos.h = y + 16;
}

/** @brief Initialize parallax family speed tables. */
void Family_Init() {
    u8 i;

    for (i = 0; i < 8; i++) {
        g_state.fm_pos[i].family_x.long_pos = 0;
        g_state.fm_pos[i].family_y.long_pos = 0;
        g_state.fm_pos[i].family_x_buff.long_pos = 0;
        g_state.fm_pos[i].family_y_buff.long_pos = 0;
    }
}

/** @brief Set parallax read position for a family layer. */
void Family_Set_R(s8 fmnm, s16 x, s16 y) {
    g_state.fm_pos[fmnm].family_x.word_pos.h = x;
    g_state.fm_pos[fmnm].family_y.word_pos.h = y;
    g_state.fm_pos[fmnm].family_x_buff.word_pos.h = x;
    g_state.fm_pos[fmnm].family_y_buff.word_pos.h = y;
}

/** @brief Set parallax write position for a family layer. */
void Family_Set_W(s8 fmnm, s16 x, s16 y) {
    g_state.fm_pos[fmnm].family_x.word_pos.h = x;
    g_state.fm_pos[fmnm].family_y.word_pos.h = y;
    g_state.fm_pos[fmnm].family_x_buff.word_pos.h = x;
    g_state.fm_pos[fmnm].family_y_buff.word_pos.h = y;
}

/** @brief Enable a background layer for reading. */
void Bg_On_R(u16 s_prm) {
    g_state.Screen_Switch |= s_prm;
    g_state.Screen_Switch_Buffer = g_state.Screen_Switch;
}

/** @brief Enable a background layer for writing. */
void Bg_On_W(u16 s_prm) {
    g_state.Screen_Switch |= s_prm;
    g_state.Screen_Switch_Buffer = g_state.Screen_Switch;
}

/** @brief Disable a background layer for reading. */
void Bg_Off_R(u16 s_prm) {
    s_prm = ~s_prm;
    g_state.Screen_Switch &= s_prm;
    g_state.Screen_Switch_Buffer = g_state.Screen_Switch;
}

/** @brief Disable a background layer for writing. */
void Bg_Off_W(u16 s_prm) {
    s_prm = ~s_prm;
    g_state.Screen_Switch &= s_prm;
    g_state.Screen_Switch_Buffer = g_state.Screen_Switch;
}

/** @brief Commit pending scroll positions for all layers. */
void Scrn_Renew() {
    g_state.Screen_Switch_Buffer = g_state.Screen_Switch;
}

/** @brief Apply parallax family offsets to all layers. */
void Irl_Family() {
    u8 i;

    for (i = 0; i < 8; i++) {
        g_state.fm_pos[i].family_x_buff.long_pos = g_state.fm_pos[i].family_x.long_pos;
        g_state.fm_pos[i].family_y_buff.long_pos = g_state.fm_pos[i].family_y.long_pos;
        g_state.bg_pos[i].scr_x_buff.long_pos = g_state.bg_pos[i].scr_x.long_pos;
        g_state.bg_pos[i].scr_y_buff.long_pos = g_state.bg_pos[i].scr_y.long_pos;
    }
}

/** @brief Apply final screen scroll positions. */
void Irl_Scrn() {
    s8 i;

    for (i = 0; i < 8; i++) {
        g_state.bg_prm[i].bg_h_shift = g_state.screen_adjust_x + g_state.bg_pos[i].scr_x_buff.word_pos.h;
        g_state.end_prm[i].bg_h_shift = g_state.screen_adjust_x + g_state.fm_pos[i].family_x_buff.word_pos.h;
        g_state.bg_prm[i].bg_v_shift = g_state.bg_pos[i].scr_y_buff.word_pos.h - g_state.screen_adjust_y;
        g_state.end_prm[i].bg_v_shift = g_state.fm_pos[i].family_y_buff.word_pos.h - g_state.screen_adjust_y;
    }
}

/** @brief Update parallax family positions each frame. */
void Family_Move() {
    u8 assign;
    u8 fam_ix;
    u8 i;
    u8 mask;

    fam_ix = use_family[g_state.bg_w.stage];
    mask = 0x80;

    for (i = 0; i < 8; i++, assign = mask >>= 1) {
        if (fam_ix & mask) {
            scr_calc(i);
        }

        (void)assign;
    }

    (void)assign;
}

/** @brief Update parallax family positions for ending sequences. */
void Ending_Family_Move() {
    u8 mask_val = ending_use_family[g_state.end_w.type];
    u8 assign;
    u8 i;
    u8 mask = 0x80;

    for (i = 0; i < 8; i++, assign = mask >>= 1) {
        if (mask_val & mask) {
            scr_calc2(i);
        }
    }

    (void)assign;
    scr_calc(3);
}

/** @brief Toggle background display on or off. */
void Bg_Disp_Switch(u8 on_off) {
    g_state.bg_disp_off = on_off;
}

/** @brief Toggle the Shin Gouki palette on stage 14 by XORing the diff against ColorRAM.
 *  Calling this an even number of times restores the original palette. */
void Bg_ToggleShinGoukiPalette(void) {
    s32 i;
    for (i = 0; i < GOUKI_XOR_PAL_SLOTS * 64; i++) {
        ColorRAM[300 + (i / 64)][i % 64] ^= s_gouki_xor_diff[i];
    }
    palUpdateGhostCP3(300, GOUKI_XOR_PAL_SLOTS);
    s_gouki_pal_xored = !s_gouki_pal_xored;
}

/** @brief Return whether the Shin Gouki palette is currently applied. */
u8 Bg_IsShinGoukiPalActive(void) {
    return s_gouki_pal_xored;
}
