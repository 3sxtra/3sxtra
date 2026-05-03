#ifndef EFFECT_13_QUAKE_SHADOW_HOMING_H
#define EFFECT_13_QUAKE_SHADOW_HOMING_H

#include "structs.h"
#include "types.h"

void effect_13_move(State_Other* ewk);
s32 screen_x_range_check(State* wk);
s32 effect_13_init(State* wk, const u8 data);

#endif
