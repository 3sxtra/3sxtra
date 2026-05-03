#ifndef EFFI4_H
#define EFFI4_H

#include "structs.h"
#include "types.h"

void effect_I4_move(State_Other* ewk);
void effect_i4_hit_sub(State_Other* ewk);
void effi4_down_to_up(State_Other* ewk);
void effi4_up_to_down(State_Other* ewk);
s32 effect_I4_init();

#endif
