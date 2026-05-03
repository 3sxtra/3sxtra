#ifndef EFF94_H
#define EFF94_H

#include "structs.h"
#include "types.h"

extern const s16 eff94_data_tbl[5][10];
extern const s16 eff94_2000_tbl[8];
extern const s8 eff94_2000_1_tbl[16];
extern const s16 eff94_3000_tbl[4][3];

void effect_94_move(State_Other* ewk);
void eff94_0000(State_Other* ewk);
void eff94_1000(State_Other* ewk);
void eff94_2000(State_Other* ewk);
void eff94_2000_0(State_Other* ewk);
void eff94_2000_1(State_Other* ewk);
void eff94_2000_2(State_Other* ewk);
void eff94_2000_3(State_Other* ewk);
void eff94_2000_4(State_Other* ewk);
void eff94_3000(State_Other* ewk);
void eff94_3000_0(State_Other* ewk);
void eff94_3000_4(State_Other* ewk);
void eff94_4000(State_Other* ewk);
s32 effect_94_init(u8 type94);

#endif
