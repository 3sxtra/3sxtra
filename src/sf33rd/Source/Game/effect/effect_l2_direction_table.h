#ifndef EFFECT_L2_DIRECTION_TABLE_H
#define EFFECT_L2_DIRECTION_TABLE_H

#include "structs.h"
#include "types.h"

extern const s8 effl2_dir_tbl[2][16];

void effect_L2_move(State_Other* ewk);
void effl2_dir_check(State_Other* ewk);
s32 effect_L2_init();

#endif
