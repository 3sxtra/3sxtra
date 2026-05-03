#ifndef EFFECT_D6_FLOWER_HANA_H
#define EFFECT_D6_FLOWER_HANA_H

#include "structs.h"
#include "types.h"

void effect_D6_move(State_Other* ewk);
s32 effect_D6_init(State_Other* wk, s16 dr, s16 sp, s16 dl, s16 acc);
void setup_hana_extra(State* wk, s16 num, s16 acc);

#endif
