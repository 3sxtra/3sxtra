#ifndef EFFECT_25_BACKGROUND_H
#define EFFECT_25_BACKGROUND_H

#include "structs.h"
#include "types.h"

extern const s16 eff25_data_0000[16];
extern const s16* scr_obj_data25[1];

void effect_25_move(State_Other* ewk);
void eff25_char_set(State_Other* ewk);
void piece_set(State_Other* ewk);
void eff25_00(State_Other* ewk);
void eff25_02(State_Other* ewk);
void eff25_04(State_Other* ewk);
void eff25_06(State_Other* ewk);
void eff25_08(State_Other* ewk);
s32 effect_25_init(s8 num);

#endif
