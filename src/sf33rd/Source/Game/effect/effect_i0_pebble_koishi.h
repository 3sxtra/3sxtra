#ifndef EFFECT_I0_PEBBLE_KOISHI_H
#define EFFECT_I0_PEBBLE_KOISHI_H

#include "structs.h"
#include "types.h"

void effect_I0_move(State_Other* ewk);
s32 effect_I0_init(State* wk, s16 hsx, s16 hsy, s16 spx, s16 spy, s16 nxy);
s32 setup_koishi_extra(State* wk, u8 num);

#endif
