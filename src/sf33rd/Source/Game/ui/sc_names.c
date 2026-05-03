/**
 * @file sc_names.c
 * @brief Player name/face rendering, grade badge display, and naming entry
 *        for the ranking screen.
 *
 * Part of the ui module. Split from sc_sub.c (task #21).
 */

#include "sf33rd/Source/Game/ui/sc_names.h"
#include "game_state.h"
#include "sf33rd/Source/Game/effect/eff76.h" /* chkNameAkuma */
#include "common.h"

#include "port/rendering/renderer.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/ui/sc_data.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "structs.h"
#include <stdbool.h>

static PAL_CURSOR_COL hud_cursor_color;

#define TO_UV_256(val) ((val) / 256.0f)
#define TO_UV_256_NEG(val) (TO_UV_256(val))
#define GRADE_GOLD_THRESHOLD 0x18
#define GRADE_POS_COUNT 32

/* ── Player names ──────────────────────────────────────────────── */

/** @brief Draw player character names on the HUD. */
void player_name() {
    u8 pl1;
    u8 pl2;

    if (omop_cockpit == 0) {
        return;
    }

    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);
    pl1 = g_state.My_char[0];
    pl2 = g_state.My_char[1];
    pl1 += chkNameAkuma(pl1, 6);
    pl2 += chkNameAkuma(pl2, 6);
    scfont_sqput(6, 3, 1, 1, Player_Name_Pos_TBL[pl1][0], Player_Name_Pos_TBL[pl1][1], 5, 1, 2);
    scfont_sqput(37, 3, 1, 1, Player_Name_Pos_TBL[pl2][0], Player_Name_Pos_TBL[pl2][1], 5, 1, 2);
}

/* ── Player faces ──────────────────────────────────────────────── */

/** @brief Initialize player face portrait state. */
void player_face_init() {
    // Do nothing
}

/** @brief Render a face-portrait font region with custom priority. */
void scfont_sqput_face(u16 x, u16 y, u16 atr, u8 page, u8 cx1, u8 cy1, u8 cx2, u8 cy2, u16 priority) {
    s32 u1;
    s32 u2;
    s32 v1;
    s32 v2;

    scrscrntex[0].color = scrscrntex[3].color = -1;
    scrscrntex[0].z = scrscrntex[3].z = PrioBase[priority];
    ppgSetupCurrentPaletteNumber(0, atr & 0x3FFF);
    x = x * 8;
    y = y * 8;
    u1 = cx1 * 8;
    u2 = u1 + (cx2 * 8);
    v1 = cy1 * 8;
    v2 = v1 + (cy2 * 8);

    if (atr & 0x8000) {
        scrscrntex[3].u = TO_UV_256_NEG(u1);
        scrscrntex[0].u = TO_UV_256_NEG(u2);
    } else {
        scrscrntex[0].u = TO_UV_256(u1);
        scrscrntex[3].u = TO_UV_256(u2);
    }

    if (atr & 0x4000) {
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
    Renderer_DrawSpriteVtx(scrscrntex, 4);
}

/** @brief Draw the base frame behind player face portraits. */
static void face_base_put() {
    PAL_CURSOR vtx;
    PAL_CURSOR_P pos[4];

    if (No_Trans || g_state.SA_shadow_on) {
        return;
    }

    vtx.p = pos;
    vtx.col = &hud_cursor_color;
    hud_cursor_color.color = 0x50000000;
    pos[0].x = 5.6f;
    pos[3].x = 34.4f;
    pos[0].y = 25.0f;
    pos[3].y = 45.0f;
    pos[1].x = pos[3].x;
    pos[1].y = pos[0].y;
    pos[2].x = pos[0].x;
    pos[2].y = pos[3].y;
    Renderer_Queue2DPrimitive((f32*)pos, PrioBase[TopHUDFacePriority], (uintptr_t)hud_cursor_color.color, 0);
    pos[0].x = 348.8f;
    pos[3].x = 377.6f;
    pos[1].x = pos[3].x;
    pos[2].x = pos[0].x;
    Renderer_Queue2DPrimitive((f32*)pos, PrioBase[TopHUDFacePriority], (uintptr_t)hud_cursor_color.color, 0);
}

/** @brief Draw player face portraits on the HUD. */
void player_face() {
    u8 grade_tmp;

    if (omop_cockpit == 0) {
        return;
    }

    if (No_Trans) {
        return;
    }

    face_base_put();
    ppgSetupCurrentDataList(&ppgScrListFace);
    scfont_sqput_face(0,
                      3,
                      g_state.Player_Color[0] + (g_state.My_char[0] * 13),
                      0,
                      Face_Pos_TBL[g_state.My_char[0]][0],
                      Face_Pos_TBL[g_state.My_char[0]][1],
                      5,
                      3,
                      2);

    if (g_state.My_char[1] == 0) {
        scfont_sqput_face(0x2B,
                          3,
                          (g_state.Player_Color[1] + (g_state.My_char[1] * 13)) | 0x8000,
                          0,
                          Face_Pos_TBL[20][0],
                          Face_Pos_TBL[20][1],
                          5,
                          3,
                          2);
    } else {
        scfont_sqput_face(0x2B,
                          3,
                          (g_state.Player_Color[1] + (g_state.My_char[1] * 13)) | 0x8000,
                          0,
                          Face_Pos_TBL[g_state.My_char[1]][0],
                          Face_Pos_TBL[g_state.My_char[1]][1],
                          5,
                          3,
                          2);
    }

    ppgSetupCurrentDataList(&ppgScrList);
    scfont_put(5, 3, 1, 0, 0, 19, 2);
    scfont_put(5, 4, 1, 0, 0, 20, 2);
    scfont_put(42, 3, 129, 0, 0, 19, 2);
    scfont_put(42, 4, 129, 0, 0, 20, 2);

    if (g_state.Play_Type == 0) {
        return;
    }

    if (g_state.Keep_Grade[g_state.Champion] == 0) {
        return;
    }

    grade_tmp = g_state.Keep_Grade[g_state.Champion] - 1;

    if (grade_tmp >= GRADE_POS_COUNT) {
        return;
    }

    if (grade_tmp < GRADE_GOLD_THRESHOLD) {
        scfont_sqput(
            (g_state.Champion * 41) + 1, 1, 27, 2, Grade_Pos_TBL[grade_tmp][0], Grade_Pos_TBL[grade_tmp][1], 5, 1, 2);
    } else {
        scfont_sqput(
            (g_state.Champion * 41) + 1, 1, 28, 2, Grade_Pos_TBL[grade_tmp][0], Grade_Pos_TBL[grade_tmp][1], 5, 1, 2);
    }
}

/* ── Naming entry ──────────────────────────────────────────────── */

/** @brief Set a naming character on the ranking entry screen. */
void naming_set(u8 pl, s16 place, u16 atr, u16 chr) {
    if (No_Trans) {
        return;
    }

    ppgSetupCurrentDataList(&ppgScrList);
    scfont_put(place + 13 + (pl * 27), 0, atr, 0, rankname_pos_tbl[chr][0], rankname_pos_tbl[chr][1], 2);
}
