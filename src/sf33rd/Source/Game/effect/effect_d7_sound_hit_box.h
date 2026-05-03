#ifndef EFFECT_D7_SOUND_HIT_BOX_H
#define EFFECT_D7_SOUND_HIT_BOX_H

#include "structs.h"
#include "types.h"

void effect_D7_move(State_Other* ewk);
void cal_speeds_effD7(State_Other* ewk, s16 tm, s16 tx, s16 ty, s16 ysp);
void ball_init_position_effD7(State_Other* ewk, PlayerEntity* mwk);
u8 screen_range_check_effD7(State* wk);
s32 effect_D7_init(PlayerEntity* wk);

#endif
