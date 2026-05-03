#ifndef EFFECT_A3_CONTENT_CHECK_SYSTEM_H
#define EFFECT_A3_CONTENT_CHECK_SYSTEM_H

#include "structs.h"
#include "types.h"

void effect_A3_move(State_Other* ewk);
s32 effect_A3_init(s16 id, u8 Type, u8 Type_in_Type, u8 dir_step, u8 Death_Type, s16 pos_x, s16 pos_y, s16 Buff);

#endif
