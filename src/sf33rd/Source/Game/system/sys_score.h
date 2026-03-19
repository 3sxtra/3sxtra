/**
 * @file sys_score.h
 * @brief Score display, win records, digit rendering, and copyright API.
 *
 * Split from sys_sub.h — HUD score/win rendering and copyright text.
 */
#ifndef SYS_SCORE_H
#define SYS_SCORE_H

#include "types.h"

void Score_Sub(void);
void Disp_Win_Record(void);
void Disp_Win_Record_Sub(u16 win_record, s16 zz);
void Disp_Digit16x24(u32 Score_Buff, s16 Disp_X, s16 Disp_Y, s16 Color);
void Disp_Copyright(void);

#endif
