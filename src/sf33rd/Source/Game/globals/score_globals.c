/**
 * @file score_globals.c
 * @brief Score and bonus global variable definitions.
 *
 * Order arrays, per-player scores, stage bonuses, and display buffers.
 * Split from game_globals.c for organizational clarity.
 */

#include "types.h"

/* === Score & Bonuses === */

u8 Order[148];
u8 Order_Timer[148];
u8 Order_Dir[148];
u32 Score[2][3];
u32 Complete_Bonus;
u32 Stock_Score[2];
u32 Vital_Bonus[2];
u32 Time_Bonus[2];
u32 Stage_Stock_Score[2];
u32 Bonus_Score;
u32 Final_Bonus_Score;
u32 WGJ_Score;
u32 Bonus_Score_Plus;
u32 Perfect_Bonus[2];
u32 Keep_Score[2];
u32 Disp_Score_Buff[2];
