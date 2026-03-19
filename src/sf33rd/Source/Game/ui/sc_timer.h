/**
 * @file sc_timer.h
 * @brief Public API for HNC overlays, combo/score rendering, training display,
 *        button images, save/load title, and the SF3 logo animation.
 *
 * Part of the ui module. Split from sc_sub.c (task #21).
 */

#ifndef SC_TIMER_H
#define SC_TIMER_H

#include "types.h"

void hnc_set(u8 num, u8 atr);
void hnc_wipeinit(u8 atr);
s32 hnc_wipeout(u8 atr);
void ci_set(u8 type, u8 atr);
void nw_set(u8 PL_num, u8 atr);
void score8x16_put(u16 x, u16 y, u8 atr, u8 chr);
void score16x24_put(u16 x, u16 y, u8 atr, u8 chr);
void combo_message_set(u8 pl, u8 kind, u8 x, u8 num, u8 hi, u8 low);
void combo_pts_set(u8 pl, u8 x, u8 num, s8* pts, s8 digit);
void sc_ram_to_vram(s8 sc_num);
void sc_ram_to_vram_opc(s8 sc_num, s8 x, s8 y, u16 atr);
void sq_paint_chenge(u16 x, u16 y, u16 sx, u16 sy, u16 atr);
void SF3_logo(u8 step);
void Training_Disp_Work_Clear(void);
void Training_Damage_Set(s16 damage, s16 /* unused */, u8 kezuri);
void Training_Data_Disp(void);
void dispButtonImage(s32 px, s32 py, s32 pz, s32 sx, s32 sy, s32 cl, s32 ix);
void dispButtonImage2(s32 px, s32 py, s32 pz, s32 sx, s32 sy, s32 cl, s32 ix);
void dispSaveLoadTitle(void* ewk);

#endif
