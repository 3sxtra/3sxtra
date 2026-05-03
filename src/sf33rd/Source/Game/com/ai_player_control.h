#ifndef AI_PLAYER_CONTROL_H
#define AI_PLAYER_CONTROL_H

#include "structs.h"

extern const u16 Correct_Lv_Data[16];

u16 cpu_algorithm(PlayerEntity* wk);
void Next_Be_Free(PlayerEntity* wk);
s32 Check_Passive(PlayerEntity* wk);
void Check_Guard_Type(PlayerEntity* wk, State* em);
s32 Check_Flip_GO(PlayerEntity* wk, s16 xx);
s32 SetShellFlipLever(PlayerEntity* wk);
void Be_Catch(PlayerEntity* wk);

#endif
