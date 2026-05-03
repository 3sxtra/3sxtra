#ifndef EFFECT_78_QUAKE_CROW_H
#define EFFECT_78_QUAKE_CROW_H

#include "structs.h"
#include "types.h"

extern const s16 eff78_data_tbl[4];
extern const s16 crow_char_tbl[3][3];

void effect_78_move(State_Other* ewk);
s32 crow_fuss_check(State_Other* ewk);
void crow_fuss_move(State_Other* ewk);
s32 effect_78_init();

#endif
