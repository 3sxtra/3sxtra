/**
 * @file font_test.c
 * @brief Font debug visualization screen — multi-page with D-pad navigation.
 *
 * Activated via --font-test CLI flag. Replaces the normal game boot
 * with rotating screens showcasing every font type and UI element in the
 * CPS3 screen-font engine.
 *
 * Controls:
 *   LEFT/RIGHT  — Manual page navigation
 *   Auto-cycles every ~10 seconds if no input
 */

#include "sf33rd/Source/Game/debug/font_test.h"
#include "common.h"
#include "main.h"
#include "port/renderer.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
#include "sf33rd/AcrSDK/common/pad.h"

#define PAGE_COUNT      13
#define FRAMES_PER_PAGE 596  /* ~10 seconds at 59.6 FPS */

/* ════════════════════════════════════════════════════════════════
 *  Page 0: Fixed-Width 8x8 — Full Charset & All 16 Palettes
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page0(void) {
    u8 p;
    SSPutStr(1, 0, 4, "PAGE 1: FIXED 8x8 CHARSET");
    /* Full charset - 3 rows */
    SSPutStr(0, 2, 4, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    SSPutStr(0, 3, 4, "abcdefghijklmnopqrstuvwxyz");
    SSPutStr(0, 4, 4, "0123456789 .:;!?+-=()<>[]");
    /* Comma baseline trick */
    SSPutStr(0, 5, 1, "COMMA: A,B,C,D,E,F  vs ABCDEF");
    /* All 16 palettes - 2 columns to save space */
    SSPutStr(0, 7, 1, "--- ALL 16 PALETTES ---");
    for (p = 0; p < 8; p++) {
        SSPutDec(0, 8 + p, 1, p, 2);
        SSPutStr(3, 8 + p, p, "ABCDEF 0123");
        SSPutDec(17, 8 + p, 1, p + 8, 2);
        SSPutStr(20, 8 + p, p + 8, "ABCDEF 0123");
    }
    /* SSPutStr2 (texture page 3) comparison */
    SSPutStr(0, 17, 1, "--- SSPUTSTR2 (TEX PAGE 3) ---");
    SSPutStr2(0, 18, 4, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    SSPutStr2(0, 19, 4, "abcdefghijklmnopqrstuvwxyz");
    SSPutStr2(0, 20, 4, "0123456789 .:;!?+-=()<>[]");
    /* Side by side */
    SSPutStr(0, 22, 1, "--- PAGE 1 vs PAGE 3 ---");
    SSPutStr( 0, 23, 4, "PAGE1 ABCDEF 012345");
    SSPutStr2(0, 24, 4, "PAGE3 ABCDEF 012345");
    /* SSPutDec */
    SSPutStr(0, 26, 1, "DEC:");
    SSPutDec(5, 26, 4, 7, 1);
    SSPutStr(7, 26, 1, "|");
    SSPutDec(8, 26, 4, 42, 2);
    SSPutStr(11, 26, 1, "|");
    SSPutDec(12, 26, 4, 255, 3);
}

/* ════════════════════════════════════════════════════════════════
 *  Page 1: Fixed-Width — Palette Showcase & Alignment
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page1(void) {
    SSPutStr(1, 0, 4, "PAGE 2: FIXED 8x8 STYLES");
    /* Each line IS the style it demonstrates */
    SSPutStr(0, 2, 0, "PAL 0: THE QUICK BROWN FOX...");
    SSPutStr(0, 3, 1, "PAL 1: THE QUICK BROWN FOX...");
    SSPutStr(0, 4, 2, "PAL 2: THE QUICK BROWN FOX...");
    SSPutStr(0, 5, 3, "PAL 3: THE QUICK BROWN FOX...");
    SSPutStr(0, 6, 4, "PAL 4: THE QUICK BROWN FOX...");
    SSPutStr(0, 7, 5, "PAL 5: THE QUICK BROWN FOX...");
    SSPutStr(0, 8, 6, "PAL 6: THE QUICK BROWN FOX...");
    SSPutStr(0, 9, 7, "PAL 7: THE QUICK BROWN FOX...");
    SSPutStr(0, 10, 8, "PAL 8: THE QUICK BROWN FOX...");
    SSPutStr(0, 11, 9, "PAL 9: THE QUICK BROWN FOX...");
    SSPutStr(0, 12, 10, "PAL10: THE QUICK BROWN FOX...");
    SSPutStr(0, 13, 11, "PAL11: THE QUICK BROWN FOX...");
    SSPutStr(0, 14, 12, "PAL12: THE QUICK BROWN FOX...");
    SSPutStr(0, 15, 13, "PAL13: THE QUICK BROWN FOX...");
    SSPutStr(0, 16, 14, "PAL14: THE QUICK BROWN FOX...");
    SSPutStr(0, 17, 15, "PAL15: THE QUICK BROWN FOX...");
    /* SSPutStr2 palettes */
    SSPutStr(0, 19, 1, "--- SSPutStr2 PALETTES ---");
    SSPutStr2(0, 20, 0, "P2-PAL0 ABCDEF 0123");
    SSPutStr2(17, 20, 1, "P2-PAL1 ABCDEF");
    SSPutStr2(0, 21, 4, "P2-PAL4 ABCDEF 0123");
    SSPutStr2(17, 21, 8, "P2-PAL8 ABCDEF");
    /* Alignment demo */
    SSPutStr(0, 23, 1, "--- ALIGNMENT ---");
    SSPutStr(0, 24, 4, "X= 0 LEFT");
    SSPutStr(12, 25, 4, "X=12 MIDDLE");
    SSPutStr(22, 26, 4, "X=22 RIGHT");
}

/* ════════════════════════════════════════════════════════════════
 *  Page 2: Proportional — Charset, Narrow/Wide, Centering
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page2(void) {
    s8 upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    s8 lower[] = "abcdefghijklmnopqrstuvwxyz";
    s8 digits[] = "0123456789 .:;!?+-=()";
    s8 narrow[] = "iIl1!.:;| NARROW GLYPHS";
    s8 wide[] = "MWmwQOGD@ WIDE GLYPHS";
    s8 ctr[] = "THIS TEXT IS CENTERED";
    s8 left[] = "THIS TEXT IS LEFT-ALIGNED";
    s8 fixed_vs[] = "iIl1MWmw COMPARE";

    SSPutStr(1, 0, 4, "PAGE 3: PROPORTIONAL FONT");
    /* Proportional charset */
    SSPutStrPro(0, 0, 2 * 8, 4, 0xFFFFFFFF, upper);
    SSPutStrPro(0, 0, 3 * 8, 4, 0xFFFFFFFF, lower);
    SSPutStrPro(0, 0, 4 * 8, 4, 0xFFFFFFFF, digits);
    /* Narrow vs wide */
    SSPutStrPro(0, 0, 6 * 8, 4, 0xFFFFFF00, narrow);
    SSPutStrPro(0, 0, 7 * 8, 4, 0xFFFF8800, wide);
    /* Centering */
    SSPutStr(0, 9, 1, "CENTERING FLAG=1 (CENTER=|):");
    SSPutStr(23, 9, 1, "|");
    SSPutStrPro(1, 192, 10 * 8, 4, 0xFFFFFFFF, ctr);
    SSPutStrPro(0, 0, 11 * 8, 8, 0xFFFFFFFF, left);
    /* Fixed vs proportional side-by-side */
    SSPutStr(0, 13, 1, "FIXED vs PROPORTIONAL:");
    SSPutStr(0, 14, 4, "iIl1MWmw COMPARE");
    SSPutStrPro(0, 0, 15 * 8, 8, 0xFFFFFFFF, fixed_vs);
    /* All proportional palettes */
    SSPutStr(0, 17, 1, "PROPORTIONAL PALETTES:");
    {
        s8 samp[] = "AaBb0123";
        SSPutStrPro(0,   0, 18 * 8, 0, 0xFFFFFFFF, samp);
        SSPutStrPro(0,  80, 18 * 8, 1, 0xFFFFFFFF, samp);
        SSPutStrPro(0, 160, 18 * 8, 2, 0xFFFFFFFF, samp);
        SSPutStrPro(0, 240, 18 * 8, 3, 0xFFFFFFFF, samp);
        SSPutStrPro(0,   0, 19 * 8, 4, 0xFFFFFFFF, samp);
        SSPutStrPro(0,  80, 19 * 8, 5, 0xFFFFFFFF, samp);
        SSPutStrPro(0, 160, 19 * 8, 6, 0xFFFFFFFF, samp);
        SSPutStrPro(0, 240, 19 * 8, 7, 0xFFFFFFFF, samp);
        SSPutStrPro(0,   0, 20 * 8, 8, 0xFFFFFFFF, samp);
        SSPutStrPro(0,  80, 20 * 8, 9, 0xFFFFFFFF, samp);
        SSPutStrPro(0, 160, 20 * 8, 10, 0xFFFFFFFF, samp);
        SSPutStrPro(0, 240, 20 * 8, 11, 0xFFFFFFFF, samp);
    }
    /* Vertex color previews */
    SSPutStr(0, 22, 1, "VERTEX COLORS:");
    {
        s8 r[] = "RED";   s8 g[] = "GREEN"; s8 b[] = "BLUE";
        s8 y[] = "YELLOW"; s8 c[] = "CYAN"; s8 m[] = "MAGENTA";
        SSPutStrPro(0,   0, 23 * 8, 4, 0xFFFF0000, r);
        SSPutStrPro(0,  48, 23 * 8, 4, 0xFF00FF00, g);
        SSPutStrPro(0, 112, 23 * 8, 4, 0xFF0000FF, b);
        SSPutStrPro(0, 176, 23 * 8, 4, 0xFFFFFF00, y);
        SSPutStrPro(0, 248, 23 * 8, 4, 0xFF00FFFF, c);
        SSPutStrPro(0, 312, 23 * 8, 4, 0xFFFF00FF, m);
    }
    /* Alpha */
    {
        s8 a1[] = "ALPHA FF (OPAQUE)";
        s8 a2[] = "ALPHA 80 (SEMI)";
        s8 a3[] = "ALPHA 40 (GHOST)";
        SSPutStrPro(0, 0, 25 * 8, 4, 0xFFFFFFFF, a1);
        SSPutStrPro(0, 0, 26 * 8, 4, 0x80FFFFFF, a2);
        SSPutStrPro(0, 160, 26 * 8, 4, 0x40FFFFFF, a3);
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Page 3: Proportional — In-Game Messages & Colors
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page3(void) {
    s8 r1[] = "ROUND 1";    s8 r2[] = "ROUND 2";    s8 r3[] = "ROUND 3";
    s8 fgt[] = "FIGHT!";    s8 ko[] = "K.O.";
    s8 win[] = "YOU WIN";   s8 perf[] = "PERFECT";
    s8 dko[] = "DOUBLE K.O."; s8 tov[] = "TIME OVER";
    s8 dr[] = "DRAW GAME";
    s8 rdx[] = "FINAL ROUND";
    s8 cont[] = "CONTINUE?";
    s8 gover[] = "GAME OVER";

    SSPutStr(1, 0, 4, "PAGE 4: IN-GAME MESSAGES");
    /* Centered messages like actual game */
    SSPutStrPro(1, 192, 2 * 8, 4, 0xFFFFFFFF, r1);
    SSPutStrPro(1, 192, 3 * 8, 4, 0xFFFFFFFF, r2);
    SSPutStrPro(1, 192, 4 * 8, 4, 0xFFFFFFFF, r3);
    SSPutStrPro(1, 192, 5 * 8, 4, 0xFFFFFFFF, rdx);
    SSPutStrPro(1, 192, 7 * 8, 4, 0xFFFF0000, fgt);
    SSPutStrPro(1, 192, 9 * 8, 4, 0xFFFFFF00, ko);
    SSPutStrPro(1, 192, 10 * 8, 4, 0xFFFF0000, dko);
    SSPutStrPro(1, 192, 12 * 8, 4, 0xFF00FF00, win);
    SSPutStrPro(0, 24 * 8, 12 * 8, 4, 0xFFFF00FF, perf);
    SSPutStrPro(1, 192, 14 * 8, 4, 0xFFFFFF00, tov);
    SSPutStrPro(1, 192, 15 * 8, 4, 0xFFFF8800, dr);
    SSPutStrPro(1, 192, 17 * 8, 4, 0xFF00FFFF, cont);
    SSPutStrPro(1, 192, 18 * 8, 4, 0xFFFF0000, gover);

    /* More color combos */
    SSPutStr(0, 20, 1, "COLOR + PALETTE COMBOS:");
    {
        s8 t1[] = "PAL0+RED"; s8 t2[] = "PAL1+GREEN"; s8 t3[] = "PAL4+BLUE";
        s8 t4[] = "PAL8+YELLOW"; s8 t5[] = "PAL9+CYAN"; s8 t6[] = "PAL1+ORANGE";
        SSPutStrPro(0,   0, 21 * 8, 0, 0xFFFF0000, t1);
        SSPutStrPro(0, 112, 21 * 8, 1, 0xFF00FF00, t2);
        SSPutStrPro(0, 248, 21 * 8, 4, 0xFF0000FF, t3);
        SSPutStrPro(0,   0, 22 * 8, 8, 0xFFFFFF00, t4);
        SSPutStrPro(0, 128, 22 * 8, 9, 0xFF00FFFF, t5);
        SSPutStrPro(0, 264, 22 * 8, 1, 0xFFFF8800, t6);
    }
    /* Scaled messages */
    SSPutStr(0, 24, 1, "SCALED MESSAGES:");
    {
        s8 s1[] = "FIGHT!"; s8 s2[] = "K.O."; s8 s3[] = "PERFECT";
        SSPutStrPro_Scale(0, 0, 25 * 8, 4, 0xFFFF0000, s1, 1.5f);
        SSPutStrPro_Scale(0, 112, 25 * 8, 4, 0xFFFFFF00, s2, 1.5f);
        SSPutStrPro_Scale(0, 192, 25 * 8, 4, 0xFFFF00FF, s3, 1.5f);
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Page 4: Proportional Scaled (SSPutStrPro_Scale)
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page4(void) {
    s8 s10[] = "Scale 1.0x (normal)";
    s8 s12[] = "Scale 1.2x";
    s8 s15[] = "Scale 1.5x";
    s8 s20[] = "Scale 2.0x";
    s8 s25[] = "Scale 2.5x";
    s8 s30[] = "Scale 3x";
    s8 abc[] = "ABCDEFGHIJKL";

    SSPutStr(1, 0, 4, "PAGE 5: PRO_SCALE SIZES");
    /* Each line IS the scale it shows */
    SSPutStrPro_Scale(0, 0, 2 * 8, 4, 0xFFFFFFFF, s10, 1.0f);
    SSPutStrPro_Scale(0, 0, 3.5f * 8, 4, 0xFFFFFFFF, s12, 1.2f);
    SSPutStrPro_Scale(0, 0, 5.0f * 8, 4, 0xFFFFFFFF, s15, 1.5f);
    SSPutStrPro_Scale(0, 0, 7.0f * 8, 4, 0xFFFFFFFF, s20, 2.0f);
    SSPutStrPro_Scale(0, 0, 9.5f * 8, 4, 0xFFFFFFFF, s25, 2.5f);
    SSPutStrPro_Scale(0, 0, 12.5f * 8, 4, 0xFFFFFFFF, s30, 3.0f);
    /* Scaled + color combos on same row */
    SSPutStrPro_Scale(0,   0, 16 * 8, 4, 0xFFFF0000, abc, 1.5f);
    SSPutStrPro_Scale(0, 192, 16 * 8, 4, 0xFF00FF00, abc, 1.5f);
    SSPutStrPro_Scale(0,   0, 18 * 8, 4, 0xFFFFFF00, abc, 1.5f);
    SSPutStrPro_Scale(0, 192, 18 * 8, 4, 0xFF00FFFF, abc, 1.5f);
    SSPutStrPro_Scale(0,   0, 20 * 8, 4, 0xFFFF00FF, abc, 1.5f);
    SSPutStrPro_Scale(0, 192, 20 * 8, 4, 0x80FFFFFF, abc, 1.5f);
    /* Centered scaled */
    {
        s8 ctr1[] = "CENTERED 1.5x";
        s8 ctr2[] = "CENTERED 2.0x";
        SSPutStrPro_Scale(1, 192, 22 * 8, 4, 0xFFFFFFFF, ctr1, 1.5f);
        SSPutStrPro_Scale(1, 192, 24 * 8, 4, 0xFFFFFF00, ctr2, 2.0f);
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Page 5: Bigger/Scaled Fonts — Sizes & Gradients
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page5(void) {
    s8 s10[] = "1.0X BIGGER FONT";
    s8 s15[] = "1.5X BIGGER";
    s8 s20[] = "2.0X BIGGER";
    s8 g0[] = "GRADIENT 0 GOLD";
    s8 g1[] = "GRADIENT 1 MULTI";
    s8 g2[] = "GRADIENT 2 WARM";

    SSPutStr(1, 0, 4, "PAGE 6: SSPUTSTR_BIGGER");
    /* Each rendered in its own scale - no separate labels */
    SSPutStr_Bigger(0, 2 * 8, 4, s10, 1.0f, 0, 2);
    SSPutStr_Bigger(0, 4 * 8, 4, s15, 1.5f, 0, 2);
    SSPutStr_Bigger(0, 7 * 8, 4, s20, 2.0f, 0, 2);
    /* Gradients - each rendered with its gradient */
    SSPutStr_Bigger(0, 11 * 8, 4, g0, 1.5f, 0, 2);
    SSPutStr_Bigger(0, 13 * 8, 4, g1, 1.5f, 1, 2);
    SSPutStr_Bigger(0, 15 * 8, 4, g2, 1.5f, 2, 2);
    /* Gradient at 2x */
    {
        s8 g2x0[] = "GRAD0 2X";
        s8 g2x1[] = "GRAD1 2X";
        s8 g2x2[] = "GRAD2 2X";
        SSPutStr_Bigger(0, 18 * 8, 4, g2x0, 2.0f, 0, 2);
        SSPutStr_Bigger(0, 21 * 8, 4, g2x1, 2.0f, 1, 2);
        SSPutStr_Bigger(0, 24 * 8, 4, g2x2, 2.0f, 2, 2);
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Page 6: Score Digits — All Sizes
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page6(void) {
    u8 d;
    SSPutStr(1, 0, 4, "PAGE 7: SCORE DIGITS");
    /* 8x16 - two palette rows side by side */
    SSPutStr(0, 2, 1, "SCORE 8x16:");
    for (d = 0; d < 10; d++) score8x16_put(1 + d, 3, 8, d);
    SSPutStr(14, 2, 1, "PAL4:");
    for (d = 0; d < 10; d++) score8x16_put(14 + 1 + d, 3, 4, d);
    /* 16x24 */
    SSPutStr(0, 6, 1, "SCORE 16x24:");
    for (d = 0; d < 10; d++) score16x24_put(d * 2, 7, 8, d);
    SSPutStr(0, 10, 1, "SCORE 16x24 PAL4:");
    for (d = 0; d < 10; d++) score16x24_put(d * 2, 11, 4, d);
    /* Bigger palette variations */
    SSPutStr(0, 14, 1, "SCORE 16x24 PAL1:");
    for (d = 0; d < 10; d++) score16x24_put(d * 2, 15, 1, d);
    SSPutStr(0, 18, 1, "SCORE 16x24 PAL9:");
    for (d = 0; d < 10; d++) score16x24_put(d * 2, 19, 9, d);
    /* SSPutDec comparison */
    SSPutStr(0, 22, 1, "SSPUTDEC:");
    SSPutStr(0, 23, 1, "1D:");  SSPutDec(4, 23, 4, 0, 1); SSPutDec(6, 23, 4, 5, 1); SSPutDec(8, 23, 4, 9, 1);
    SSPutStr(10, 23, 1, "2D:"); SSPutDec(14, 23, 4, 0, 2); SSPutDec(17, 23, 4, 42, 2); SSPutDec(20, 23, 4, 99, 2);
    SSPutStr(0, 24, 1, "3D:");  SSPutDec(4, 24, 4, 0, 3); SSPutDec(8, 24, 4, 100, 3); SSPutDec(12, 24, 4, 255, 3);
    SSPutStr(16, 24, 1, "4D:"); SSPutDec(20, 24, 4, 0, 4); SSPutDec(25, 24, 4, 128, 4);
}

/* ════════════════════════════════════════════════════════════════
 *  Page 7: Tile Blocks & ATR Flips
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page7(void) {
    u8 t;
    SSPutStr(1, 0, 4, "PAGE 8: TILES & ATR FLIPS");
    /* 4 rows tile page 0 */
    SSPutStr(0, 2, 1, "SCFONT PAGE0 R0-3:");
    for (t = 0; t < 20; t++) scfont_put(1 + t, 3, 4, 0, t, 0, 2);
    for (t = 0; t < 20; t++) scfont_put(1 + t, 4, 4, 0, t, 1, 2);
    for (t = 0; t < 20; t++) scfont_put(1 + t, 5, 4, 0, t, 2, 2);
    for (t = 0; t < 20; t++) scfont_put(1 + t, 6, 4, 0, t, 3, 2);
    /* 4 rows tile page 2 */
    SSPutStr(0, 8, 1, "SCFONT PAGE2 R0-3:");
    for (t = 0; t < 20; t++) scfont_put(1 + t, 9, 4, 2, t, 0, 2);
    for (t = 0; t < 20; t++) scfont_put(1 + t, 10, 4, 2, t, 1, 2);
    for (t = 0; t < 20; t++) scfont_put(1 + t, 11, 4, 2, t, 2, 2);
    for (t = 0; t < 20; t++) scfont_put(1 + t, 12, 4, 2, t, 3, 2);
    /* ATR flips - compact 2-column layout */
    SSPutStr(0, 14, 1, "ATR FLIPS:");
    SSPutStr(1, 15, 1, "NRM:");
    scfont_put(5, 15, 0x04, 0, 1, 0, 2); scfont_put(6, 15, 0x04, 0, 2, 0, 2); scfont_put(7, 15, 0x04, 0, 3, 0, 2);
    SSPutStr(10, 15, 1, "H:");
    scfont_put(12, 15, 0x84, 0, 1, 0, 2); scfont_put(13, 15, 0x84, 0, 2, 0, 2); scfont_put(14, 15, 0x84, 0, 3, 0, 2);
    SSPutStr(1, 16, 1, "V:");
    scfont_put(5, 16, 0x44, 0, 1, 0, 2); scfont_put(6, 16, 0x44, 0, 2, 0, 2); scfont_put(7, 16, 0x44, 0, 3, 0, 2);
    SSPutStr(10, 16, 1, "HV:");
    scfont_put(13, 16, 0xC4, 0, 1, 0, 2); scfont_put(14, 16, 0xC4, 0, 2, 0, 2); scfont_put(15, 16, 0xC4, 0, 3, 0, 2);
    /* SQPUT multi-cell */
    SSPutStr(0, 18, 1, "SQPUT 4x1:");
    scfont_sqput(0, 19, 4, 0, 0, 0, 4, 1, 2);
    scfont_sqput(5, 19, 8, 0, 4, 0, 4, 1, 2);
    scfont_sqput(10, 19, 1, 0, 8, 0, 4, 1, 2);
    scfont_sqput(15, 19, 9, 0, 12, 0, 4, 1, 2);
    SSPutStr(0, 21, 1, "SQPUT 8x2:");
    scfont_sqput(0, 22, 4, 0, 0, 0, 8, 2, 2);
    scfont_sqput(9, 22, 8, 2, 0, 0, 8, 2, 2);
    scfont_sqput(18, 22, 1, 0, 8, 0, 8, 2, 2);
    /* More palettes */
    SSPutStr(0, 25, 1, "SQPUT PALETTES:");
    scfont_sqput(0, 26, 0, 0, 0, 0, 4, 1, 2);
    scfont_sqput(5, 26, 1, 0, 0, 0, 4, 1, 2);
    scfont_sqput(10, 26, 4, 0, 0, 0, 4, 1, 2);
    scfont_sqput(15, 26, 8, 0, 0, 0, 4, 1, 2);
    scfont_sqput(20, 26, 9, 0, 0, 0, 4, 1, 2);
}

/* ════════════════════════════════════════════════════════════════
 *  Page 8: Health, Stun, & HUD Bars
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page8(void) {
    SSPutStr(1, 0, 4, "PAGE 9: HUD BARS & GAUGES");
    /* HP bars */
    SSPutStr(0, 2, 1, "VITAL_PUT HP=160 | HP=100:");
    vital_put(0, 8, 160, 0, 2);
    vital_put(1, 8, 100, 0, 2);
    SSPutStr(0, 4, 1, "SILVER_VITAL (RECOVERABLE):");
    silver_vital_put(0);
    silver_vital_put(1);
    SSPutStr(0, 6, 1, "VITAL_BASE (HP FRAME):");
    vital_base_put(0);
    vital_base_put(1);
    /* Stun */
    SSPutStr(0, 8, 1, "STUN=100 | STUN=60:");
    omop_st_bar_disp[0] = 1;
    omop_st_bar_disp[1] = 1;
    stun_put(0, 100);
    stun_put(1, 60);
    SSPutStr(0, 10, 1, "STUN_BASE + SPGAUGE_BASE:");
    stun_base_put(0, 160);
    stun_base_put(1, 160);
    spgauge_base_put(0, 160);
    spgauge_base_put(1, 160);
    /* Notes */
    SSPutStr(0, 13, 4, "HP: Y=16-24  STUN: Y=24-32");
    SSPutStr(0, 14, 4, "P1: X=8-168  P2: X=216-376");
    /* Tonedown */
    SSPutStr(0, 16, 1, "TONEDOWN (DARKEN OVERLAY):");
    ToneDown(48, 0);
    SSPutStr(0, 18, 4, "TONEDOWN DIMS ALL BELOW THIS");
    SSPutStr(0, 19, 4, "IT AFFECTS THE ENTIRE SCREEN");
    SSPutStr(0, 20, 4, "RENDERING STATE BELOW THE BAR");
}

/* ════════════════════════════════════════════════════════════════
 *  Page 9: Screen Transitions (animated)
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page9(void) {
    static u8 anim_phase = 0;

    SSPutStr(1, 0, 4, "PAGE 10: SCREEN TRANSITIONS");
    /* Dense background text to show transitions */
    SSPutStr(0, 2, 4, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123");
    SSPutStr(0, 3, 8, "THE QUICK BROWN FOX JUMPS OVER");
    SSPutStr(0, 4, 4, "THE LAZY DOG 0123456789 !?.,;:");
    SSPutStr(0, 5, 1, "abcdefghijklmnopqrstuvwxyz");
    SSPutStr(0, 7, 4, "ROUND 1    FIGHT!    K.O.");
    SSPutStr(0, 8, 4, "YOU WIN    PERFECT   TIME OVER");
    SSPutStr(0, 10, 1, "CYCLING: FADE+WIPE VARIANTS");

    switch (anim_phase) {
    case 0:
        SSPutStr(0, 12, 4, ">> FADEOUT (BLACK)");
        if (FadeOut(0, 8, 0)) { anim_phase = 1; FadeInit(); }
        break;
    case 1:
        SSPutStr(0, 12, 4, ">> FADEIN (BLACK)");
        if (FadeIn(0, 8, 0)) { anim_phase = 2; FadeInit(); }
        break;
    case 2:
        SSPutStr(0, 12, 4, ">> FADEOUT (WHITE)");
        if (FadeOut(1, 8, 0)) { anim_phase = 3; FadeInit(); }
        break;
    case 3:
        SSPutStr(0, 12, 4, ">> FADEIN (WHITE)");
        if (FadeIn(1, 8, 0)) { anim_phase = 4; WipeInit(); }
        break;
    case 4:
        SSPutStr(0, 12, 4, ">> WIPEOUT (HORIZ)");
        if (WipeOut(0)) { anim_phase = 5; WipeInit(); }
        break;
    case 5:
        SSPutStr(0, 12, 4, ">> WIPEIN (HORIZ)");
        if (WipeIn(0)) { anim_phase = 6; WipeInit(); }
        break;
    case 6:
        SSPutStr(0, 12, 4, ">> WIPEOUT (DIAG)");
        if (WipeOut(1)) { anim_phase = 7; WipeInit(); }
        break;
    case 7:
        SSPutStr(0, 12, 4, ">> WIPEIN (DIAG)");
        if (WipeIn(1)) { anim_phase = 0; FadeInit(); }
        break;
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Page 10: In-Game HUD Recreation
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page10(void) {
    s8 rnd[] = "ROUND 1";
    s8 fgt[] = "FIGHT!";

    SSPutStr(1, 0, 4, "PAGE 11: IN-GAME HUD");
    /* Full HUD setup */
    vital_base_put(0);
    vital_base_put(1);
    vital_put(0, 8, 120, 0, 2);
    vital_put(1, 8, 90, 0, 2);
    omop_st_bar_disp[0] = 1;
    omop_st_bar_disp[1] = 1;
    stun_base_put(0, 160);
    stun_base_put(1, 160);
    stun_put(0, 80);
    stun_put(1, 40);
    /* Timer digits */
    scfont_sqput(22, 0, 4, 2, 18, 2, 2, 4, 2);
    scfont_sqput(24, 0, 4, 2, 18, 2, 2, 4, 2);
    scfont_sqput(21, 1, 9, 0, 12, 6, 1, 4, 2);
    scfont_sqput(26, 1, 137, 0, 12, 6, 1, 4, 2);
    scfont_sqput(22, 4, 9, 0, 3, 18, 4, 1, 2);
    /* Round/Fight text */
    SSPutStr_Bigger(14 * 8, 8 * 8, 4, rnd, 2.0f, 0, 2);
    SSPutStr_Bigger(16 * 8, 11 * 8, 4, fgt, 2.0f, 1, 2);
    /* Combo + buttons */
    SSPutStr(0, 14, 1, "COMBO:");
    combo_message_set(0, 0, 2, 5, 1, 2);
    SSPutStr(0, 18, 1, "BUTTONS:");
    dispButtonImage(8, 160, 2, 16, 16, 0, 0);
    dispButtonImage(32, 160, 2, 16, 16, 0, 1);
    dispButtonImage(56, 160, 2, 16, 16, 0, 2);
    dispButtonImage(80, 160, 2, 16, 16, 0, 3);
}

/* ════════════════════════════════════════════════════════════════
 *  Page 11: Menu Letter Sprites — Mode & Game Options
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page11(void) {
    SSPutStr(1, 0, 4, "PAGE 12: MENU SPRITES (CG OBJ)");
    /* Mode menu - compact: index + name on same line */
    SSPutStr(0, 2, 1, "MODE MENU (CG 0x7047, 14px):");
    SSPutStr(0, 3, 4, " 0 ARCADE");       SSPutStr(16, 3, 4, " 1 VERSUS");
    SSPutStr(0, 4, 4, " 2 TRAINING");      SSPutStr(16, 4, 4, " 3 NETWORK");
    SSPutStr(0, 5, 4, " 4 REPLAY");        SSPutStr(16, 5, 4, " 5 OPTION");
    SSPutStr(0, 6, 4, " 6 EXIT GAME");
    /* Game option sub-menu */
    SSPutStr(0, 8, 1, "OPTION SUB (CG 0x7047):");
    SSPutStr(0, 9, 4, " 7 GAME OPTION");   SSPutStr(16, 9, 4, "10 SOUND");
    SSPutStr(0, 10, 4, " 8 BUTTON CONFIG"); SSPutStr(16, 10, 4, "11 SAVE/LOAD");
    SSPutStr(0, 11, 4, " 9 SYS DIRECTION"); SSPutStr(16, 11, 4, "12 EXTRA OPT");
    SSPutStr(0, 12, 4, "13 EXIT");
    /* Game options (smaller CG) */
    SSPutStr(0, 14, 1, "GAME OPTS (CG 0x70A7, 8px):");
    SSPutStr(0, 15, 8, "25 DIFFICULTY");    SSPutStr(16, 15, 8, "26 TIME LIMIT");
    SSPutStr(0, 16, 8, "27 ROUNDS(1P)");    SSPutStr(16, 16, 8, "28 ROUNDS(VS)");
    SSPutStr(0, 17, 8, "29 DAMAGE LVL");    SSPutStr(16, 17, 8, "30 GUARD JDG");
    SSPutStr(0, 18, 8, "31 ANALOG STK");    SSPutStr(16, 18, 8, "32 HANDICAP");
    SSPutStr(0, 19, 8, "33 PLAYER1(VS)");   SSPutStr(16, 19, 8, "34 PLAYER2(VS)");
    SSPutStr(0, 20, 8, "35 DEFAULT SET");   SSPutStr(16, 20, 8, "36 EXIT");
    /* Extra options */
    SSPutStr(0, 22, 1, "EXTRA OPTS:");
    SSPutStr(0, 23, 4, "14 X POSITION");    SSPutStr(16, 23, 4, "15 Y POSITION");
    SSPutStr(0, 24, 4, "16 X RANGE");       SSPutStr(16, 24, 4, "17 Y RANGE");
    SSPutStr(0, 25, 4, "18 FILTER");        SSPutStr(16, 25, 4, "19 DEFAULT SET");
    SSPutStr(0, 26, 4, "20 EXIT");
}

/* ════════════════════════════════════════════════════════════════
 *  Page 12: Menu Sprites — Sound, Training, Pause, Lobby, Save
 * ════════════════════════════════════════════════════════════════ */
static void FontTest_Page12(void) {
    SSPutStr(1, 0, 4, "PAGE 13: MENU SPRITES (CONT)");
    /* Save/Load */
    SSPutStr(0, 2, 1, "SAVE/LOAD (CG 0x7047):");
    SSPutStr(0, 3, 4, "21 SAVE DATA");     SSPutStr(16, 3, 4, "22 LOAD DATA");
    SSPutStr(0, 4, 4, "23 AUTO SAVE");     SSPutStr(16, 4, 4, "24 EXIT");
    /* Sound */
    SSPutStr(0, 6, 1, "SOUND (CG 0x7047):");
    SSPutStr(0, 7, 4, "58 AUDIO");         SSPutStr(16, 7, 4, "59 BGM LEVEL");
    SSPutStr(0, 8, 4, "60 SE LEVEL");      SSPutStr(16, 8, 4, "61 BGM SELECT");
    SSPutStr(0, 9, 4, "62 DEFAULT SET");   SSPutStr(16, 9, 4, "63 BGM TEST");
    SSPutStr(0, 10, 4, "64 EXIT");
    /* Training */
    SSPutStr(0, 12, 1, "TRAINING (CG 0x7047):");
    SSPutStr(0, 13, 4, "52 NORMAL TRAIN"); SSPutStr(16, 13, 4, "53 PARRY TRAIN");
    SSPutStr(0, 14, 4, "54 EXIT");         SSPutStr(16, 14, 4, "65 TRIALS");
    /* Pause menus */
    SSPutStr(0, 16, 1, "PAUSE (CG 0x70A7):");
    SSPutStr(0, 17, 8, "37 CONTINUE");     SSPutStr(16, 17, 8, "40 CONTINUE");
    SSPutStr(0, 18, 8, "38 REPLAY SAVE");  SSPutStr(16, 18, 8, "41 REPLAY SAVE");
    SSPutStr(0, 19, 8, "39 EXIT");         SSPutStr(16, 19, 8, "42 EXIT");
    /* In-game option */
    SSPutStr(0, 21, 1, "IN-GAME OPT (CG 0x7047):");
    SSPutStr(0, 22, 4, "43 DIRECTION");    SSPutStr(16, 22, 4, "44 SAVE");
    SSPutStr(0, 23, 4, "45 LOAD");         SSPutStr(16, 23, 4, "46 EXIT");
    /* Network lobby */
    SSPutStr(0, 25, 1, "LOBBY (CG 0x70A7):");
    SSPutStr(0, 26, 8, "66 NET LOBBY");    SSPutStr(16, 26, 8, "67-72 CONN/EXIT");
}

/* ════════════════════════════════════════════════════════════════
 *  Main Task Dispatcher with D-pad Navigation
 * ════════════════════════════════════════════════════════════════ */
void FontTest_Task(struct _TASK* task_ptr) {
    static s16 frame_counter = 0;
    static s16 current_page = 0;
    static u16 prev_input = 0;
    u16 new_press;
    s8 page_str[8];
    u8 i;

    No_Trans = 0;
    omop_cockpit = 1;
    ppgSetupCurrentDataList(&ppgScrList);

    /* ── D-pad Navigation ──────────────────────────────────── */
    new_press = p1sw_0 & ~prev_input;
    prev_input = p1sw_0;

    if (new_press & SWK_RIGHT) {
        current_page++;
        if (current_page >= PAGE_COUNT) current_page = 0;
        frame_counter = 0;
    }
    if (new_press & SWK_LEFT) {
        current_page--;
        if (current_page < 0) current_page = PAGE_COUNT - 1;
        frame_counter = 0;
    }

    /* ── Auto-cycle ────────────────────────────────────────── */
    frame_counter++;
    if (frame_counter >= FRAMES_PER_PAGE) {
        frame_counter = 0;
        current_page++;
        if (current_page >= PAGE_COUNT) current_page = 0;
    }

    /* ── Render current page ───────────────────────────────── */
    switch (current_page) {
    case 0:  FontTest_Page0();  break;
    case 1:  FontTest_Page1();  break;
    case 2:  FontTest_Page2();  break;
    case 3:  FontTest_Page3();  break;
    case 4:  FontTest_Page4();  break;
    case 5:  FontTest_Page5();  break;
    case 6:  FontTest_Page6();  break;
    case 7:  FontTest_Page7();  break;
    case 8:  FontTest_Page8();  break;
    case 9:  FontTest_Page9();  break;
    case 10: FontTest_Page10(); break;
    case 11: FontTest_Page11(); break;
    case 12: FontTest_Page12(); break;
    }

    /* ── Page indicator bar (bottom) ───────────────────────── */
    SSPutStr(12, 27, 1, "<");
    for (i = 0; i < PAGE_COUNT; i++) {
        if (i == current_page) {
            SSPutStr(13 + i, 27, 4, "#");
        } else {
            SSPutStr(13 + i, 27, 1, ".");
        }
    }
    SSPutStr(13 + PAGE_COUNT, 27, 1, ">");

    /* Page number (handles 2-digit) */
    {
        s16 pg = current_page + 1;
        s16 tot = PAGE_COUNT;
        s16 k = 0;
        if (pg >= 10) { page_str[k++] = '0' + (pg / 10); }
        page_str[k++] = '0' + (pg % 10);
        page_str[k++] = '/';
        if (tot >= 10) { page_str[k++] = '0' + (tot / 10); }
        page_str[k++] = '0' + (tot % 10);
        page_str[k] = '\0';
        SSPutStr(13 + PAGE_COUNT + 1, 27, 4, page_str);
    }
}
