/**
 * @file sc_sub.c
 * @brief Core HUD rendering: screen-font engine, text rendering,
 *        screen transitions (wipe/fade/tone), and initialization.
 *
 * The main UI rendering core. Handles screen-font initialization,
 * glyph rendering (8×8, 16×16, scalable proportional), and screen
 * transitions (wipe/fade/tone/panel overlay).
 *
 * Cockpit gauges are in sc_cockpit.c, score/combo/training display
 * in sc_timer.c, and player name/face rendering in sc_names.c.
 *
 * Part of the ui module.
 */

#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "common.h"

#include "port/rendering/renderer.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/ramcnt.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_data.h"
#include "structs.h"
#include <stdbool.h>

#define TO_UV_256(val) ((val) / 256.0f)
#define TO_UV_256_NEG(val) (TO_UV_256(val))
#define TO_UV_128(val) ((val) / 128.0f)

/* === Named constants for array dimensions === */
#define SA_FRAME_ROWS 3
#define SA_FRAME_COLS 48
#define SA_FRAME_Y_OFFSET 25
#define SA_PLAYER_HALF 24
#define FADE_DATA_COUNT 10

/* === Named constants for UI rendering magic numbers === */
#define FADE_ALPHA_MAX 255 /**< Full alpha for fade transitions */

typedef struct {
    s16 fade;
    s16 fade_kind;
    u8 fade_prio;
} FadeData;

// sdata
u8 ascProData[128] = { 0, 18, 0, 0, 0,  0, 0,  0,  0,  0, 0,  0, 0, 0,  0,  0,  0, 0, 0,  0,  0,  0,  0, 0,  0, 0,
                       0, 0,  0, 0, 0,  0, 34, 19, 18, 0, 0,  0, 0, 34, 34, 34, 1, 1, 34, 1,  34, 0,  0, 18, 0, 0,
                       0, 0,  0, 0, 0,  0, 34, 34, 17, 0, 17, 0, 0, 0,  0,  0,  0, 0, 0,  0,  0,  34, 0, 0,  0, 0,
                       0, 0,  0, 0, 0,  0, 0,  0,  0,  0, 0,  0, 0, 17, 0,  17, 1, 0, 34, 0,  0,  0,  0, 0,  0, 0,
                       0, 34, 2, 0, 34, 0, 0,  0,  0,  0, 16, 0, 0, 0,  0,  0,  0, 0, 0,  18, 35, 33, 0, 33 };

// bss
SAFrame sa_frame[SA_FRAME_ROWS][SA_FRAME_COLS];

// sbss
RendererVertex scrscrntex[4];
u8 WipeLimit;
u8 FadeLimit;
s16 Hnc_Num;
FadeData fd_dat;

// forward decls
static s32 SSGetDrawSizePro(const s8* str);
static s16 SSPutStrTexInputPro(u16 x, u16 y, u16 ix);
static f32 SSPutStrTexInputProScale(f32 x, f32 y, u16 ix, f32 sc);

/* ═══════════════════════════════════════════════════════════════ */
/*  Initialization                                                */
/* ═══════════════════════════════════════════════════════════════ */

/** @brief Initialize the screen-font rendering system and palette data. */
void Scrscreen_Init() {
    void* loadAdrs;
    u32 loadSize;
    s16 i;
    s16 key;

    ppgScrList.tex = ppgScrListFace.tex = ppgScrListShot.tex = ppgScrListOpt.tex = &ppgScrTex;
    ppgScrList.pal = &ppgScrPal;
    ppgScrListFace.pal = &ppgScrPalFace;
    ppgScrListShot.pal = &ppgScrPalShot;
    ppgScrListOpt.pal = &ppgScrPalOpt;
    ppgSetupCurrentDataList(&ppgScrList);
    loadSize = load_it_use_any_key2(10, &loadAdrs, &key, 2, 0); // scrscrn.ppg

    if (loadSize == 0) {
        // Could not load texture for score screen.\n
        flLogOut("スコアスクリーン用のテクスチャが読み込めませんでした。\n");
        while (1) {
            // Do nothing
        }
    }

    ppgSetupPalChunk(&ppgScrPalOpt, (u8*)loadAdrs, loadSize, 0, 3, 1);
    ppgSetupPalChunk(&ppgScrPalShot, (u8*)loadAdrs, loadSize, 0, 2, 1);
    ppgSetupPalChunk(&ppgScrPalFace, (u8*)loadAdrs, loadSize, 0, 1, 1);
    ppgSetupPalChunk(NULL, (u8*)loadAdrs, loadSize, 0, 0, 1);
    ppgSetupTexChunk_1st(NULL, (u8*)loadAdrs, loadSize, 0, 6, 0, 0);

    for (i = 0; i < 3; i++) {
        ppgSetupTexChunk_2nd(NULL, i);
        ppgSetupTexChunk_3rd(NULL, i, 1);
    }

    for (i = 3; i < ppgScrTex.textures; i++) {
        ppgSetupTexChunk_2nd(NULL, i);
        ppgSetupTexChunk_3rd(NULL, i, 1);
    }

    Push_ramcnt_key(key);
    ppgSourceDataReleased(NULL);
    Sa_frame_Clear();
}

/* ═══════════════════════════════════════════════════════════════ */
/*  Super-art frame buffer                                        */
/* ═══════════════════════════════════════════════════════════════ */

/** @brief Clear super-art frame state for both players. */
void Sa_frame_Clear() {
    u8 i;
    u8 j;

    for (j = 0; j < SA_FRAME_ROWS; j++) {
        for (i = 0; i < SA_FRAME_COLS; i++) {
            sa_frame[j][i].atr = 0;
            sa_frame[j][i].page = 0;
            sa_frame[j][i].cx = 0;
            sa_frame[j][i].cy = 0;
        }
    }
}

/** @brief Clear super-art frame state for a single player. */
void Sa_frame_Clear2(u8 pl) {
    u8 i;
    u8 j;

    for (j = 0; j < SA_FRAME_ROWS; j++) {
        for (i = pl * SA_PLAYER_HALF; i < (pl * SA_PLAYER_HALF) + SA_PLAYER_HALF; i++) {
            sa_frame[j][i].atr = 0;
            sa_frame[j][i].page = 0;
            sa_frame[j][i].cx = 0;
            sa_frame[j][i].cy = 0;
        }
    }
}

/** @brief Write queued super-art frame data to VRAM. */
void Sa_frame_Write() {
    u8 i;
    u8 j;

    if (omop_cockpit == 0) {
        return;
    }

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    if (omop_sa_bar_disp[0]) {
        for (j = 0; j < SA_FRAME_ROWS; j++) {
            for (i = 0; i < SA_PLAYER_HALF; i++) {
                if (sa_frame[j][i].atr != 0) {
                    scfont_put(
                        i, j + 25, sa_frame[j][i].atr, sa_frame[j][i].page, sa_frame[j][i].cx, sa_frame[j][i].cy, 2);
                }
            }
        }
    }

    if (omop_sa_bar_disp[1]) {
        for (j = 0; j < SA_FRAME_ROWS; j++) {
            for (i = SA_PLAYER_HALF; i < SA_FRAME_COLS; i++) {
                if (sa_frame[j][i].atr != 0) {
                    scfont_put(
                        i, j + 25, sa_frame[j][i].atr, sa_frame[j][i].page, sa_frame[j][i].cx, sa_frame[j][i].cy, 2);
                }
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════ */
/*  Screen-font text rendering (8×8 and proportional)             */
/* ═══════════════════════════════════════════════════════════════ */

/** @brief Render a string using screen-font texture input (8×8 glyphs). */
static void SSPutStrTexInput(u16 x, u16 y, const s8* str) {
    s32 u = ((*str & 0xF) * 8) + 0x80;
    s32 v = ((*str & 0xF0) >> 4) * 8;

    scrscrntex[0].u = TO_UV_256(u);
    scrscrntex[3].u = TO_UV_256(u + 8);
    scrscrntex[0].v = TO_UV_256(v);
    scrscrntex[3].v = TO_UV_256(v + 8);
    scrscrntex[0].x = x;
    scrscrntex[3].x = (x + 8);
    scrscrntex[0].y = y;
    scrscrntex[3].y = (y + 8);
}

/** @brief Render a single character using screen-font texture input. */
static void SSPutStrTexInput2(u16 x, u16 y, u8 str) {
    s32 u;

    u = (str * 8) + 128;

    scrscrntex[0].u = TO_UV_256(u);
    scrscrntex[3].u = TO_UV_256(u + 8);
    scrscrntex[0].v = TO_UV_256(0.0f);
    scrscrntex[3].v = TO_UV_256(8.0f);
    scrscrntex[0].x = x;
    scrscrntex[3].x = (x + 8);
    scrscrntex[0].y = y;
    scrscrntex[3].y = (y + 8);
}

/** @brief Shared implementation for SSPutStr/SSPutStr2.
 *  @param priority   PrioBase index for z-ordering.
 *  @param handle_comma  When true, comma glyphs are offset downward by 2px.
 */
static void SSPutStr_impl(u16 x, u16 y, u8 atr, const s8* str, u16 priority, u8 handle_comma) {
    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    scrscrntex[0].color = scrscrntex[3].color = 0xFFFFFFFF;
    scrscrntex[0].z = scrscrntex[3].z = PrioBase[priority];
    ppgSetupCurrentPaletteNumber(0, atr & 0x3F);
    x = x * 8;
    y = y * 8;

    while (*str != '\0') {
        if (handle_comma && *str == ',') {
            SSPutStrTexInput(x, y + 2, str);
        } else {
            SSPutStrTexInput(x, y, str);
        }

        Renderer_SetTexture(1);
        Renderer_DrawSprite(scrscrntex, 4);
        x += 8;
        str++;
    }
}

/** @brief Render a string using screen fonts with attribute coloring. */
void SSPutStr(u16 x, u16 y, u8 atr, const s8* str) {
    SSPutStr_impl(x, y, atr, str, 2, 1);
}

/** @brief Render a proportional string with custom vertex color. */
s32 SSPutStrPro(u16 flag, u16 x, u16 y, u8 atr, u32 vtxcol, s8* str) {
    s32 usex;
    s16 step;

    if (No_Trans) {
        return x;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    scrscrntex[0].color = scrscrntex[3].color = vtxcol;
    scrscrntex[0].z = scrscrntex[3].z = PrioBase[2];
    ppgSetupCurrentPaletteNumber(0, atr & 0x3F);

    if (flag) {
        x = (x - SSGetDrawSizePro(str)) / 2;
    }

    usex = x;

    while (*str != 0) {
        if (*str != ',') {
            step = SSPutStrTexInputPro(x, y, *str);
        } else {
            step = SSPutStrTexInputPro(x, y + 2, *str);
        }

        str += 1;
        x += step;
        Renderer_SetTexture(1);
        Renderer_DrawSprite(scrscrntex, 4);
    }

    return usex;
}

static f32 SSPutStrTexInputProScale(f32 x, f32 y, u16 ix, f32 sc) {
    s16 sideL;
    s16 sideR;
    s32 u;
    s32 v;
    f32 slide;

    if (ix >= sizeof(ascProData)) {
        return 0.0f;
    }

    u = (ix & 0xF) * 8 + 0x80;
    v = ((ix & 0xF0) >> 4) * 8;

    sideL = (ascProData[ix] >> 4) & 0xF;
    sideR = ascProData[ix] & 0xF;

    scrscrntex[0].u = scrscrntex[1].u = TO_UV_256(u + sideL);
    scrscrntex[2].u = scrscrntex[3].u = TO_UV_256(u + 8 - sideR);
    scrscrntex[0].v = scrscrntex[2].v = TO_UV_256(v);
    scrscrntex[1].v = scrscrntex[3].v = TO_UV_256(v + 8);

    slide = (f32)((8 - sideL) - sideR) * sc;

    scrscrntex[0].x = scrscrntex[1].x = x;
    scrscrntex[2].x = scrscrntex[3].x = x + slide;
    scrscrntex[0].y = scrscrntex[2].y = y;
    scrscrntex[1].y = scrscrntex[3].y = y + (8.0f * sc);

    return slide;
}

/** @brief Render a proportional string with custom vertex color and scale. */
s32 SSPutStrPro_Scale(u16 flag, f32 x, f32 y, u8 atr, u32 vtxcol, s8* str, f32 sc) {
    f32 usex;
    f32 step;

    if (No_Trans) {
        return (s32)x;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    scrscrntex[0].color = scrscrntex[1].color = scrscrntex[2].color = scrscrntex[3].color = vtxcol;
    scrscrntex[0].z = scrscrntex[1].z = scrscrntex[2].z = scrscrntex[3].z = PrioBase[2];
    ppgSetupCurrentPaletteNumber(0, atr & 0x3F);

    if (flag) {
        x = x - ((f32)SSGetDrawSizePro(str) * sc) / 2.0f;
    }

    usex = x;

    while (*str != 0) {
        if (*str != ',') {
            step = SSPutStrTexInputProScale(x, y, *str, sc);
        } else {
            step = SSPutStrTexInputProScale(x, y + (2.0f * sc), *str, sc);
        }

        str += 1;
        x += step;
        Renderer_SetTexture(1);
        Renderer_DrawTexturedQuad(scrscrntex, 4);
    }

    return (s32)usex;
}

/** @brief Render a single glyph from the proportional font sheet. */
static s16 SSPutStrTexInputPro(u16 x, u16 y, u16 ix) {
    s16 slide;
    s16 sideL;
    s16 sideR;
    s32 u;
    s32 v;

    if (ix >= sizeof(ascProData)) {
        return 0;
    }

    u = (ix & 0xF) * 8 + 0x80;
    v = ((ix & 0xF0) >> 4) * 8;

    sideL = (ascProData[ix] >> 4) & 0xF;
    sideR = ascProData[ix] & 0xF;
    scrscrntex[0].u = TO_UV_256(u + sideL);
    scrscrntex[3].u = TO_UV_256(u + 8 - sideR);
    scrscrntex[0].v = TO_UV_256(v);
    scrscrntex[3].v = TO_UV_256(v + 8);
    slide = (8 - sideL) - sideR;
    scrscrntex[0].x = x;
    scrscrntex[3].x = (x + slide);
    scrscrntex[0].y = y;
    scrscrntex[3].y = (y + 8);
    return slide;
}

/** @brief Calculate the pixel width of a proportional string. */
static s32 SSGetDrawSizePro(const s8* str) {
    s32 ix;
    s32 size = 0;

    while (*str != '\0') {
        ix = *str++;
        ix &= 0x7F;
        size += 8 - ((ascProData[ix] >> 4) & 0xF) - (ascProData[ix] & 0xF);
    }

    return size;
}

/** @brief Render a string using screen fonts variant 2 (different palette). */
void SSPutStr2(u16 x, u16 y, u8 atr, const s8* str) {
    SSPutStr_impl(x, y, atr, str, 1, 0);
}

/** @brief Render a scaled string using the bigger font sheet. */
static void SSPutStrTexInputB(f32 x, f32 y, s8* str, f32 sc) {
    s32 u = ((*str & 0xF) * 8) + 128;
    s32 v = ((*str & 0xF0) >> 4) * 8;

    scrscrntex[0].u = scrscrntex[1].u = TO_UV_256(u);
    scrscrntex[2].u = scrscrntex[3].u = TO_UV_256(u + 8);
    scrscrntex[0].v = scrscrntex[2].v = TO_UV_256(v);
    scrscrntex[1].v = scrscrntex[3].v = TO_UV_256(v + 8);
    scrscrntex[0].x = scrscrntex[1].x = x;
    scrscrntex[2].x = scrscrntex[3].x = (x + (8.0f * sc));
    scrscrntex[0].y = scrscrntex[2].y = y;
    scrscrntex[1].y = scrscrntex[3].y = (y + (8.0f * sc));
}

/** @brief Render a bigger/scaled string with custom color gradient and priority. */
void SSPutStr_Bigger(u16 x, u16 y, u8 atr, s8* str, f32 sc, u8 gr, u16 priority) {
    f32 xx;
    f32 yy;
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
    xx = x;
    yy = y;

    while (*str != '\0') {
        if (*str == '$') {
            str++;
            xx += 4.0f * sc;
            continue;
        }

        SSPutStrTexInputB(xx, yy, str, sc);
        Renderer_SetTexture(1);
        Renderer_DrawTexturedQuad(scrscrntex, 4);
        xx += 8.0f * sc;
        str++;
    }
}

/** @brief Render a decimal number using screen fonts. */
void SSPutDec(u16 x, u16 y, u8 atr, u8 dec, u8 size) {
    s8 str[3];
    u8 work;
    u8 num;
    u8 i;
    u8 zero_sw;

    if (No_Trans) {
        return;
    }

    if (size == 0) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    scrscrntex[0].color = scrscrntex[3].color = -1;
    scrscrntex[0].z = scrscrntex[3].z = PrioBase[2];
    ppgSetupCurrentPaletteNumber(0, atr & 0x3F);
    x = x * 8;
    y = y * 8;
    zero_sw = 0;
    work = 100;

    for (i = 0; i < 3; i++) {
        for (num = 0; dec + 1 > work; dec = dec - work, num++) {}

        str[i] = num;
        work = work / 10;
    }

    SSPutStrTexInput2(x, y, str[2]);
    Renderer_SetTexture(1);
    Renderer_DrawSprite(scrscrntex, 4);

    if (size == 0) {
        return;
    }

    x -= 16;

    if (size == 3 && str[0] != 0) {
        SSPutStrTexInput2(x, y, str[0]);
        Renderer_SetTexture(1);
        Renderer_DrawSprite(scrscrntex, 4);
        zero_sw = 1;
    }

    x += 8;

    if (zero_sw == 1) {
        SSPutStrTexInput2(x, y, str[1]);
        Renderer_SetTexture(1);
        Renderer_DrawSprite(scrscrntex, 4);
    } else if (size > 1 && str[1] != 0) {
        SSPutStrTexInput2(x, y, str[1]);
        Renderer_SetTexture(1);
        Renderer_DrawSprite(scrscrntex, 4);
    }
}

/* ═══════════════════════════════════════════════════════════════ */
/*  Screen-font glyph/tile rendering                              */
/* ═══════════════════════════════════════════════════════════════ */

/** @brief Render a single screen-font glyph at grid position.
 *  Delegates to scfont_sqput with a 1×1 cell region.
 */
void scfont_put(u16 x, u16 y, u8 atr, u8 page, u8 cx, u8 cy, u16 priority) {
    scfont_sqput(x, y, atr, page, cx, cy, 1, 1, priority);
}

/** @brief Render a screen-font glyph (variant 2 — fixed priority). */
void scfont_put2(u16 x, u16 y, u8 atr, u8 page, u8 cx, u8 cy) {
    if (y < SA_FRAME_Y_OFFSET || y - SA_FRAME_Y_OFFSET >= SA_FRAME_ROWS || x >= SA_FRAME_COLS) {
        return;
    }
    sa_frame[y - SA_FRAME_Y_OFFSET][x].atr = atr;
    sa_frame[y - SA_FRAME_Y_OFFSET][x].page = page;
    sa_frame[y - SA_FRAME_Y_OFFSET][x].cx = cx;
    sa_frame[y - SA_FRAME_Y_OFFSET][x].cy = cy;
}

/** @brief Render a rectangular screen-font region (multi-cell sprite). */
void scfont_sqput(u16 x, u16 y, u8 atr, u8 page, u8 cx1, u8 cy1, u8 cx2, u8 cy2, u16 priority) {
    s32 u1;
    s32 u2;
    s32 v1;
    s32 v2;

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    scrscrntex[0].color = scrscrntex[3].color = -1;
    scrscrntex[0].z = scrscrntex[3].z = PrioBase[priority];
    ppgSetupCurrentPaletteNumber(0, atr & 0x3F);
    x = x * 8;
    y = y * 8;
    u1 = cx1 * 8;
    u2 = u1 + (cx2 * 8);
    v1 = cy1 * 8;
    v2 = v1 + (cy2 * 8);

    if (atr & 0x80) {
        scrscrntex[3].u = TO_UV_256_NEG(u1);
        scrscrntex[0].u = TO_UV_256_NEG(u2);
    } else {
        scrscrntex[0].u = TO_UV_256(u1);
        scrscrntex[3].u = TO_UV_256(u2);
    }

    if (atr & 0x40) {
        scrscrntex[3].v = TO_UV_256_NEG(v1);
        scrscrntex[0].v = TO_UV_256_NEG(v2);
    } else {
        scrscrntex[0].v = TO_UV_256(v1);
        scrscrntex[3].v = TO_UV_256(v2);
    }

    scrscrntex[0].x = x;
    scrscrntex[3].x = (x + (u2 - u1));
    scrscrntex[0].y = y;
    scrscrntex[3].y = (y + (v2 - v1));
    Renderer_SetTexture(page);
    Renderer_DrawSprite(scrscrntex, 4);
}

/** @brief Render a rectangular font region with optional UV inversion. */
void scfont_sqput2(u16 x, u16 y, u8 atr, u8 inverse, u8 page, u8 cx1, u8 cy1, u8 cx2, u8 cy2) {
    u8 i;
    u8 j;

    for (j = 0; j < cy2; j++) {
        if (y - SA_FRAME_Y_OFFSET + j >= SA_FRAME_ROWS)
            break;
        for (i = 0; i < cx2; i++) {
            if (x + i >= SA_FRAME_COLS)
                break;
            sa_frame[y - SA_FRAME_Y_OFFSET + j][x + i].atr = atr;
            sa_frame[y - SA_FRAME_Y_OFFSET + j][x + i].page = page;
            sa_frame[y - SA_FRAME_Y_OFFSET + j][x + i].cx = inverse ? (cx1 + (cx2 - 1)) - i : cx1 + i;
            sa_frame[y - SA_FRAME_Y_OFFSET + j][x + i].cy = cy1 + j;
        }
    }
}

/** @brief Clear a rectangular region of the screen font buffer. */
void sc_clear(u16 sposx, u16 sposy, u16 eposx, u16 eposy) {
    u16 i;
    u16 j;

    for (j = 0; j < (eposy - sposy) + 1; j++) {
        if (sposy - SA_FRAME_Y_OFFSET + j >= SA_FRAME_ROWS)
            break;
        for (i = 0; i < (eposx - sposx) + 1; i++) {
            if (sposx + i >= SA_FRAME_COLS)
                break;
            sa_frame[sposy - SA_FRAME_Y_OFFSET + j][sposx + i].atr = 0;
            sa_frame[sposy - SA_FRAME_Y_OFFSET + j][sposx + i].page = 0;
            sa_frame[sposy - SA_FRAME_Y_OFFSET + j][sposx + i].cx = 0;
            sa_frame[sposy - SA_FRAME_Y_OFFSET + j][sposx + i].cy = 0;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════ */
/*  Screen transitions: wipe, fade, tone, panel overlay           */
/* ═══════════════════════════════════════════════════════════════ */

/** @brief Initialize the screen wipe effect state. */
void WipeInit() {
    WipeLimit = 0;
}

/** @brief Execute a screen wipe-out transition (screen goes black). */
s32 WipeOut(u8 type) {
    PAL_CURSOR wipe_pc;
    PAL_CURSOR_P wipe_p[4];
    PAL_CURSOR_COL wipe_col[4];
    s32 i;
    s32 dmylim;

    if (WipeLimit > 7) {
        overwrite_panel(0xFF000000, 0);
    }

    if (WipeLimit == 9) {
        overwrite_panel(0xFF000000, 0);
        return 1;
    }

    if (!No_Trans) {
        if (WipeLimit > 7) {
            dmylim = 7;
        } else {
            dmylim = WipeLimit;
        }

        wipe_pc.p = wipe_p;
        wipe_pc.col = wipe_col;
        wipe_pc.tex = 0;
        wipe_pc.num = 4;
        wipe_col[0].color = wipe_col[1].color = wipe_col[2].color = wipe_col[3].color = 0xFF000000;

        if (type == 0) {
            wipe_p[0].x = wipe_p[2].x = 0.0f;
            wipe_p[1].x = wipe_p[3].x = 384.0f;

            for (i = 224; i > 0; i -= 8) {
                wipe_p[0].y = wipe_p[1].y = i;
                wipe_p[2].y = wipe_p[3].y = (i - (dmylim + 1));
                Renderer_Queue2DPrimitive((f32*)wipe_p, PrioBase[0], (uintptr_t)wipe_col[0].color, 0);
            }
        } else if (WipeLimit != 8) {
            wipe_p[0].y = wipe_p[1].y = 0.0f;
            wipe_p[2].y = wipe_p[3].y = 224.0f;

            for (i = -224; i < 384; i += 8) {
                wipe_p[0].x = i;
                wipe_p[1].x = (i + dmylim + 1);
                wipe_p[2].x = 224.0f + wipe_p[0].x;
                wipe_p[3].x = 224.0f + wipe_p[1].x;
                Renderer_Queue2DPrimitive((f32*)wipe_p, PrioBase[0], (uintptr_t)wipe_col[0].color, 0);
            }
        }
    }

    WipeLimit += 1;
    return 0;
}

/** @brief Execute a screen wipe-in transition (screen reveals). */
s32 WipeIn(u8 type) {
    PAL_CURSOR wipe_pc;
    PAL_CURSOR_P wipe_p[4];
    PAL_CURSOR_COL wipe_col[4];
    s32 i;

    if (WipeLimit == 9) {
        return 1;
    }

    if ((WipeLimit != 8) && (!No_Trans)) {
        wipe_pc.p = &wipe_p[0];
        wipe_pc.col = &wipe_col[0];
        wipe_pc.tex = 0;
        wipe_pc.num = 4;
        wipe_col[0].color = wipe_col[1].color = wipe_col[2].color = wipe_col[3].color = 0xFF000000;

        if (type == 0) {
            wipe_p[0].x = wipe_p[2].x = 0.0f;
            wipe_p[1].x = wipe_p[3].x = 384.0f;

            for (i = 0; i < 224; i += 8) {
                wipe_p[0].y = wipe_p[1].y = i;
                wipe_p[2].y = wipe_p[3].y = ((i + 8) - (WipeLimit + 1));
                Renderer_Queue2DPrimitive((f32*)wipe_p, PrioBase[0], (uintptr_t)wipe_col[0].color, 0);
            }
        } else {
            wipe_p[0].y = wipe_p[1].y = 0.0f;
            wipe_p[2].y = wipe_p[3].y = 224.0f;

            for (i = -224; i < 384; i += 8) {
                wipe_p[0].x = i;
                wipe_p[1].x = ((i + 8) - (WipeLimit + 1));
                wipe_p[2].x = 224.0f + wipe_p[0].x;
                wipe_p[3].x = 224.0f + wipe_p[1].x;
                Renderer_Queue2DPrimitive((f32*)wipe_p, PrioBase[0], (uintptr_t)wipe_col[0].color, 0);
            }
        }
    }

    WipeLimit += 1;
    return 0;
}

/** @brief Initialize the screen fade effect state. */
void FadeInit() {
    FadeLimit = 1;
}

/** @brief Execute a screen fade-out transition. */
s32 FadeOut(u8 type, u8 step, u8 priority) {
    PAL_CURSOR fade_pc;
    PAL_CURSOR_P fade_p[4];
    PAL_CURSOR_COL fade_col[4];
    u32 Alpha;
    u8 i;
    u8 flag;

    Alpha = 0xFF000000;
    flag = 0;

    if (No_Trans) {
        return 0;
    }

    fade_pc.p = fade_p;
    fade_pc.col = fade_col;
    fade_pc.num = 4;

    if ((FadeLimit * step) < FADE_ALPHA_MAX) {
        Alpha = (FadeLimit * step) << 24;
    } else {
        flag = 1;
    }

    if (type == 0) {
        Alpha |= 0x00FFFFFF;
    }

    for (i = 0; i < 4; i++) {
        fade_p[i].x = Fade_Pos_tbl[i * 2];
        fade_p[i].y = Fade_Pos_tbl[i * 2 + 1];
        fade_col[i].color = Alpha;
    }

    Renderer_Queue2DPrimitive((f32*)fade_p, PrioBase[priority], (uintptr_t)fade_col[0].color, 0);

    if (flag) {
        return 1;
    }

    FadeLimit += 1;
    return 0;
}

/** @brief Execute a screen fade-in transition. */
s32 FadeIn(u8 type, u8 step, u8 priority) {
    PAL_CURSOR fade_pc;
    PAL_CURSOR_P fade_p[4];
    PAL_CURSOR_COL fade_col[4];
    u32 Alpha;
    u8 i;
    u8 flag;

    Alpha = 0;
    flag = 0;

    if (No_Trans) {
        return 0;
    }

    fade_pc.p = fade_p;
    fade_pc.col = fade_col;
    fade_pc.num = 4;

    if (FadeLimit * step < FADE_ALPHA_MAX) {
        Alpha = (FADE_ALPHA_MAX - FadeLimit * step) << 24;
    } else {
        flag = 1;
    }

    if (type == 0) {
        Alpha |= 0x00FFFFFF;
    }

    for (i = 0; i < 4; i++) {
        fade_p[i].x = Fade_Pos_tbl[i * 2];
        fade_p[i].y = Fade_Pos_tbl[i * 2 + 1];
        fade_col[i].color = Alpha;
    }

    Renderer_Queue2DPrimitive((f32*)fade_p, PrioBase[priority], (uintptr_t)fade_col[0].color, 0);

    if (flag) {
        return 1;
    }

    FadeLimit += 1;
    return 0;
}

/** @brief Apply a tone-down (darken) overlay to the screen. */
void ToneDown(u8 tone, u8 priority) {
    PAL_CURSOR tone_pc;
    PAL_CURSOR_P tone_p[4];
    PAL_CURSOR_COL tone_col[4];
    u8 i;

    if (No_Trans) {
        return;
    }

    tone_pc.p = tone_p;
    tone_pc.col = tone_col;
    tone_pc.num = 4;

    for (i = 0; i < 4; i++) {
        tone_p[i].x = Fade_Pos_tbl[i * 2];
        tone_p[i].y = Fade_Pos_tbl[i * 2 + 1];
        tone_col[i].color = tone << 24;
    }

    Renderer_Queue2DPrimitive((f32*)tone_p, PrioBase[priority], (uintptr_t)tone_col[0].color, 0);
}

/** @brief Draw a solid-color fullscreen panel overlay. */
void overwrite_panel(u32 color, u8 priority) {
    PAL_CURSOR panel_pc;
    PAL_CURSOR_P panel_p[4];
    PAL_CURSOR_COL panel_col[4];
    u8 i;

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);

    panel_pc.p = panel_p;
    panel_pc.col = panel_col;
    panel_pc.num = 4;

    for (i = 0; i < 4; i++) {
        panel_p[i].x = Fade_Pos_tbl[i * 2];
        panel_p[i].y = Fade_Pos_tbl[(i * 2) + 1];
        panel_col[i].color = color;
    }

    Renderer_Queue2DPrimitive((f32*)panel_p, PrioBase[priority], (uintptr_t)panel_col[0].color, 0);
}

/* ═══════════════════════════════════════════════════════════════ */
/*  Automatic fade controller                                     */
/* ═══════════════════════════════════════════════════════════════ */

/** @brief Initialize the automatic fade-transition controller. */
void fade_cont_init() {
    FadeInit();

    if (Fade_Number < 0 || Fade_Number >= FADE_DATA_COUNT) {
        return;
    }

    fd_dat.fade_kind = fade_data_tbl[Fade_Number][0];
    fd_dat.fade = fade_data_tbl[Fade_Number][1];
    fd_dat.fade_prio = fade_data_tbl[Fade_Number][2];
}

/** @brief Per-frame automatic fade-transition state machine. */
void fade_cont_main() {
    u8 flag = 0;

    switch (fd_dat.fade_kind) {
    case 0:
        flag = FadeIn(1, fd_dat.fade, fd_dat.fade_prio);
        break;

    case 1:
        flag = FadeOut(1, fd_dat.fade, fd_dat.fade_prio);
        break;

    case 2:
        flag = FadeIn(0, fd_dat.fade, fd_dat.fade_prio);
        break;

    case 3:
        flag = FadeOut(0, fd_dat.fade, fd_dat.fade_prio);
        break;
    }

    if (flag == 1) {
        Fade_Flag = 0;
    }
}
