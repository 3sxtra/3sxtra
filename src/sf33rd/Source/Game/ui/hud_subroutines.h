/**
 * @file sc_sub.h
 * @brief Public API for HUD rendering, screen transitions, and UI elements.
 *
 * Umbrella header — includes domain-specific headers for cockpit gauges,
 * timer/score/training display, and player name/face rendering.
 *
 * Part of the ui module.
 */

#ifndef HUD_SUBROUTINES_H
#define HUD_SUBROUTINES_H

#include "types.h"
#include "structs.h"

/* Domain headers (task #21 split) */
#include "sf33rd/Source/Game/ui/hud.h"
#include "sf33rd/Source/Game/ui/nameplates.h"
#include "sf33rd/Source/Game/ui/hud_timer.h"

/* ── Core: initialization ─────────────────────────────────────── */
void Scrscreen_Init(void);

/* ── HUD z-order priority (Draw Player Over HUD) ──────────────── */
extern int TopHUDPriority;
extern int TopHUDShadowPriority;
extern int TopHUDFacePriority;
extern int TopHUDVitalPriority;

void HUD_Shift_Init(void);

/* ── Core: shared state ───────────────────────────────────────── */
typedef struct {
    u8 atr;
    u8 page;
    u8 cx;
    u8 cy;
} SAFrame;

extern RendererVertex scrscrntex[4];
extern SAFrame sa_frame[3][48];
extern u8 FadeLimit;
extern u8 WipeLimit;
extern s16 Hnc_Num;

/* ── Core: super-art frame buffer ─────────────────────────────── */
void Sa_frame_Clear(void);
void Sa_frame_Clear2(u8 pl);
void Sa_frame_Write(void);

/* ── Core: screen-font text rendering ─────────────────────────── */
void SSPutStr(u16 x, u16 y, u8 atr, const s8* str);
s32 SSPutStrPro(u16 flag, u16 x, u16 y, u8 atr, u32 vtxcol, s8* str);
s32 SSPutStrPro_Scale(u16 flag, f32 x, f32 y, u8 atr, u32 vtxcol, s8* str, f32 sc);
void SSPutStr2(u16 x, u16 y, u8 atr, const s8* str);
void SSPutStr_Bigger(u16 x, u16 y, u8 atr, s8* str, f32 sc, u8 gr, u16 priority);
void SSPutDec(u16 x, u16 y, u8 atr, u8 dec, u8 size);

/* ── Core: screen-font glyph/tile rendering ───────────────────── */
void scfont_put(u16 x, u16 y, u8 atr, u8 page, u8 cx, u8 cy, u16 priority);
void scfont_put2(u16 x, u16 y, u8 atr, u8 page, u8 cx, u8 cy);
void scfont_sqput(u16 x, u16 y, u8 atr, u8 page, u8 cx1, u8 cy1, u8 cx2, u8 cy2, u16 priority);
void scfont_sqput2(u16 x, u16 y, u8 atr, u8 inverse, u8 page, u8 cx1, u8 cy1, u8 cx2, u8 cy2);
void sc_clear(u16 sposx, u16 sposy, u16 eposx, u16 eposy);

/* ── Core: screen transitions ─────────────────────────────────── */
void WipeInit(void);
s32 WipeOut(u8 type);
s32 WipeIn(u8 type);
void FadeInit(void);
s32 FadeOut(u8 type, u8 step, u8 priority);
s32 FadeIn(u8 type, u8 step, u8 priority);
void ToneDown(u8 tone, u8 priority);
void overwrite_panel(u32 color, u8 priority);
void fade_cont_init(void);
void fade_cont_main(void);

#endif
