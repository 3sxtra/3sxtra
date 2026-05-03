/**
 * @file workuser_score.h
 * @brief Extern declarations for gameplay globals - Score and Ranking
 */
#ifndef STATE_SCORE_H
#define STATE_SCORE_H

#include "types.h"
#include "game_state.h"
#include "sf33rd/Source/Game/engine/cmd_data.h"
#include "sf33rd/Source/Game/select_timer.h"
#include <stdbool.h>

extern u32 Score[2][3];
extern u32 Complete_Bonus;
extern u32 Stock_Score[2];
extern u32 Vital_Bonus[2];
extern u32 Time_Bonus[2];
extern u32 Stage_Stock_Score[2];
extern u32 Bonus_Score;
extern u32 Final_Bonus_Score;
extern u32 WGJ_Score;
extern u32 Bonus_Score_Plus;
extern u32 Perfect_Bonus[2];
extern u32 Keep_Score[2];
extern u32 Disp_Score_Buff[2];
extern s8 Complete_Judgement;
extern u8 Present_Rank[2];
extern s8 Best_Grade[2];
extern s8 Rank_Type;
extern s8 Flash_Rank_Time;
extern s8 Flash_Rank_Interval;
extern s32 Ranking_X;
extern s8 Rank;
extern s8 Rank_X;
extern s8 EM_Rank;
extern s8 Lost_Round[2];
extern s8 Super_Arts_Finish[2];
extern s8 Stage_SA_Finish[2];
extern s8 Perfect_Finish[2];
extern s8 Cheap_Finish[2];
extern s8 Bonus_Game_Complete;
extern s8 Stage_Lost_Round[2];
extern s8 Stage_Perfect_Finish[2];
extern s8 Stage_Cheap_Finish[2];
extern u8 Stage_Time_Finish[2];
extern u8 Bonus_Type;
extern s8 Completion_Bonus[2][2];
extern s8 Rank_In[2][4];
extern s8 Request_Disp_Rank[2][4];
extern u8 Stop_Update_Score;
extern s16 Rank_Pos_X;
extern s16 Rank_Pos_Y;
extern s16 Bonus_Game_Flag;
extern s16 Bonus_Game_Work;
extern s16 Bonus_Game_result;
extern s16 Stock_Bonus_Game_Result;
extern s16 Bonus_Stage_RNO[4];
extern s16 Bonus_Stage_Level;
extern s16 Bonus_Stage_Tix;
extern s16 Bonus_Game_ex_result;
extern u16 Keep_Grade[2];
s8* Get_Ranking_Slot(int playerIdx, int slotIdx);

#endif // STATE_SCORE_H
