#ifndef EFFECT_A0_POSITION_DATA_H
#define EFFECT_A0_POSITION_DATA_H

#include "structs.h"
#include "types.h"

void effect_A0_move(EffectMultiSprite* ewk);
s32 effect_A0_init(s16 type, u16 disp_target, s16 pos_index, s16 old_routine_no, s16 zero, s16 target_bg,
                   s16 master_player);

#endif
