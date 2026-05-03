#ifndef EFFM0_H
#define EFFM0_H

#include "structs.h"
#include "types.h"

void effect_M0_move(State_Other* ewk);
void cat_run_set2(State_Other* ewk);
void cat_walk_set(State_Other* ewk);
s32 effect_M0_init(u8 pl_rl, u8 animal_type);

#endif
