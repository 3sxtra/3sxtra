#ifndef EFFECT_B8_QUAKE_H
#define EFFECT_B8_QUAKE_H

#include "structs.h"
#include "types.h"

extern s16 old_mes_no2;
extern s16 old_mes_no3;
extern s16 old_mes_no_pl;
extern s16 test_pl_no;
extern s16 test_mes_no;
extern s16 test_in;
extern s16 mes_timer;

void effect_B8_move(EffectMultiSprite* ewk);
s32 effect_B8_init(s8 WIN_PL_NO, s16 timer);

#endif
