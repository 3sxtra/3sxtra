#ifndef EFFECT_85_QUAKE_CHARACTER_INDEX_H
#define EFFECT_85_QUAKE_CHARACTER_INDEX_H

#include "structs.h"
#include "types.h"

extern const s16 eff85_char_index_tbl[9];

void effect_85_move(State_Other* ewk);
void eff85_0000(State_Other* ewk);
void eff85_0100(State_Other* ewk);
void eff85_1000(State_Other* ewk);
void eff85_common();
void eff85_3000(State_Other* ewk);
void eff85_5000(State_Other* ewk);
void eff85_7000(State_Other* ewk);
void eff85_8000(State_Other* ewk);
void eff85_9000(State_Other* ewk);
s32 swallow_sprize_check();
void eff85_0200(State_Other* ewk);
s32 effect_85_init();

#endif
