#ifndef AI_SUBROUTINES_H
#define AI_SUBROUTINES_H

#include "structs.h"
#include "types.h"

void End_Pattern(PlayerEntity* wk);
void Next_Be_Passive(PlayerEntity* wk, s32);
void Turn_Over_On(PlayerEntity* wk);
void Only_Shot(PlayerEntity* wk, s16 Lever_Data);
void Lever_On(PlayerEntity* wk, u16 LR_Lever, u16 UD_Lever);
void Lever_Off(PlayerEntity* wk);
void Enable_Overhead_Attack_Flag(PlayerEntity* wk);
void Setup_DENJIN_LEVEL(PlayerEntity* wk);
void Hold_Attack_Button(PlayerEntity* wk, s16 Power_Level);
void Keep_Away(PlayerEntity* wk, s16 Target_Pos, s16 Option);
void Check_Safe_Retreat_Space(PlayerEntity* wk, s16 Move_Value, s16 Next_Action, s16 Next_Menu);
void Approach_Walk(PlayerEntity* wk, s16 Target_Pos, s16 Option);
void Walk(PlayerEntity* wk, u16 Lever, s16 Time, s16 unused);
void Forced_Guard(PlayerEntity* wk, s16 pGuard_Type);
void Provoke(PlayerEntity* wk, s16 Lever);
void Normal_Attack(PlayerEntity* wk, s16 Reaction, u16 Lever_Data);
void Normal_Attack_SP(PlayerEntity* wk, s16 Reaction, u16 Lever_Data, s16 Time);
void Adjust_Attack(PlayerEntity* wk, s16 Reaction, u16 Lever_Data);
void Lever_Attack(PlayerEntity* wk, s16 Reaction, u16 Lever, u16 Lever_Data);
void Lever_Attack_SP(PlayerEntity* wk, s16 Reaction, u16 Lever, u16 Lever_Data, s16 Time);
s32 Setup_Guard_Lever(PlayerEntity* wk, u16 Lever);
s32 Check_Start_Lever_Attack(PlayerEntity* wk, u16 Lever, u16 Lever_Data);
void Check_Super_Art_Conditions(PlayerEntity* wk, u16 SA0, u16 SA1, u16 SA2, u16 pTerm_No);
void Check_SA(PlayerEntity* wk, s16 Next_Action, s16 Next_Menu);
void Check_EX(PlayerEntity* wk, s16 Next_Action, s16 Next_Menu);
void Check_SA_Full(PlayerEntity* wk, s16 Next_Action, s16 Next_Menu);
void Branch_By_Distance(PlayerEntity* wk, s16 Next_Action, s16 Menu, s16 Menu_01, s16 Menu_02, s16 Menu_03);
void AI_Random_Action_Select(PlayerEntity* wk, s16 Next_Action, s16 Menu_00, s16 Menu_01, s16 Menu_02, s16 Menu_03,
                             s16 Rnd_Type);
void Branch_Wait_Area(PlayerEntity* wk, s16 Time_00, s16 Time_01, s16 Time_02, s16 Time_03);
void Wait(PlayerEntity* wk, s16 Time);
void Look(PlayerEntity* wk, s16 Time);
void Keep_Status(PlayerEntity* wk, u16 Lever_Data, s16 Option_Data);
void VS_Jump_Guard(PlayerEntity* wk);
void Wait_Lie(PlayerEntity* wk, u16 Lever_Data);
void Wait_Get_Up(PlayerEntity* wk, u16 Lever_Data, s16 Option);
void Wait_Attack_Complete(PlayerEntity* wk, u16 Lever_Data, s16 Option);
void Short_Range_Attack(PlayerEntity* wk, s16 Reaction, u16 Lever_Data, s16 Next_Action, s16 Next_Menu);
void Check_Enemy_Distance(PlayerEntity* wk, s16 Range_X, s16 Range_Y, s16 Exit_Number, s16 Next_Action, s16 Next_Menu);
void Jump(PlayerEntity* wk, s16 Time);
void Hi_Jump(PlayerEntity* wk, s16 Pl_Number, s16 Jump_Dir);
void Jump_Attack(PlayerEntity* wk, s16 Reaction, s16 Time_Data, u16 Lever_Data, s16 Jump_Dir);
void Check_Jump_Attack_Conditions(PlayerEntity* wk, s16 Range_X, s16 Range_Y, s16 Reaction, u16 Lever_Data,
                                  s16 Jump_Dir, s16 Range_JX, s16 Range_JY, s16 J_Lever_Data);
void Hi_Jump_Attack(PlayerEntity* wk, s16 Reaction, s16 Time_Data, u16 Lever_Data, s16 Jump_Dir);
void Hi_Jump_Attack_Term(PlayerEntity* wk, s16 Range_X, s16 Range_Y, s16 Reaction, u16 Lever_Data, s16 Jump_Dir,
                         s16 Range_JX, s16 Range_JY, u16 J_Lever_Data);
void Oro_Check_Jump_Attack(PlayerEntity* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX, s16 RY,
                           u16 Lever_Data, s16 RJX, s16 RJY, u16 JLD);
void Oro_Check_High_Jump_Attack(PlayerEntity* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX, s16 RY,
                                u16 Lever_Data, s16 RJX, s16 RJY, u16 JLD);
void Command_Attack(PlayerEntity* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot);
void Jump_Command_Attack(PlayerEntity* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot);
void Rapid_Command_Attack(PlayerEntity* wk, s16 Reaction, u16 Tech_Number, s16 Shot, u16 Time);
void Oro_Check_Jump_Command_Attack(PlayerEntity* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX, s16 RY,
                                   u16 Tech_Number, s16 Power_Level, s16 Ex_Shot, s16 RJX, s16 RJY, u16 JLD);
void Oro_Check_High_Jump_Command_Attack(PlayerEntity* wk, s16 Reaction, s16 Jump_Dir, s16 JY, s16 Jump_Dir2, s16 RX,
                                        s16 RY, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot, s16 RJX, s16 RJY,
                                        u16 JLD);
void Jump_Command_Attack_Term(PlayerEntity* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot, s16 RX,
                              s16 RY, s16 Jump_Dir, s16 JRX, s16 JRY, u16 JLD);
void Hi_Jump_Command_Attack_Term(PlayerEntity* wk, s16 Reaction, u16 Tech_Number, s16 Power_Level, s16 Ex_Shot, s16 RX,
                                 s16 RY, s16 Jump_Dir, s16 JRX, s16 JRY, u16 JLD);
void Check_Store_Lever(PlayerEntity* wk, u16 Tech_Number, s16 Next_Action, s16 Next_Menu);
s32 Setup_Lv04(s16 xx);
s32 Setup_Lv08(s16 xx);
s32 Setup_Lv10(s16 xx);
s32 Setup_Lv18(s16 xx);
s32 Setup_VS_Catch_Data(PlayerEntity* wk);
void Next_Another_Menu(PlayerEntity* wk, s16 Next_Action, u16 Next_Menu);
s32 Select_Passive(PlayerEntity* wk);
void Check_BOSS(PlayerEntity* wk, u32 Next_Action, u16 Next_Menu);
void Check_BOSS_EX(PlayerEntity* wk, u32 Next_Action, u16 Next_Menu);
void Check_Miscellaneous_Conditions(PlayerEntity* wk, s16 pExit_No, u32 Next_Action, u16 Next_Menu);
void Check_Projectile_Impact_Time(PlayerEntity* wk, s16 Next_Command, s16 Exit_Number, s16 Next_Action, s16 Next_Menu,
                                  s16 unused); // unused arg
void Next_Be_Flip(PlayerEntity* wk, s16 xx);
s32 Check_Guard(PlayerEntity* wk);
s32 Check_Passive(PlayerEntity* wk);
void Check_Rapid(PlayerEntity* wk, u16 Tech_Number);
s32 Check_Shell_Another_in_Flip(PlayerEntity* wk);
s32 Ck_Area(PlayerEntity* wk);
void Ck_Distance(PlayerEntity* wk);
s32 Command_Type_00(PlayerEntity* wk, s16 Power_Level, u16 Tech_Number, s16 Ex_Shot);
s32 Command_Type_01(PlayerEntity* wk, s16 Power_Level, s16 Ex_Shot); // unused args
void Decide_Follow_Menu(PlayerEntity* wk);
s32 Flip_Term_Correct(PlayerEntity* wk);
void Rapid_Sub(PlayerEntity* wk);
void Select_Active(PlayerEntity* wk);
s32 Setup_EM_Rank_Index(PlayerEntity* wk);
s32 emLevelRemake(s32 now, s32 max, s32 exd);
s32 emGetMaxBlocking();
void Next_Be_Guard(PlayerEntity* wk, State* em, s16 Type_Of_Guard);

#endif
