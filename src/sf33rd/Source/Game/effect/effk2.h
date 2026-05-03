#ifndef EFFK2_H
#define EFFK2_H

#include "structs.h"
#include "types.h"

void effect_K2_move(State_Other* ewk);
void setup_effK2(State* wk);
void setup_effK2_sync_bomb(State* wk);
void illegal_setup_effK2(State* wk, s16 ix);

#endif
