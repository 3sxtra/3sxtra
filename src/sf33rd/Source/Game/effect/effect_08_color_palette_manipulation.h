#ifndef EFFECT_08_COLOR_PALETTE_MANIPULATION_H
#define EFFECT_08_COLOR_PALETTE_MANIPULATION_H

#include "structs.h"
#include "types.h"

extern const u8 Rewrite_Color_Data_08[2][13][2];

void effect_08_move(State_Other* ewk);
s32 effect_08_init(s8 sc_num, s8 x, s8 y, u16 atr, s16 color_type);

#endif
