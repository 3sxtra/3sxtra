#ifndef EFFECT_A8_POSITION_DATA_H
#define EFFECT_A8_POSITION_DATA_H

#include "structs.h"
#include "types.h"

void effect_A8_move(EffectMultiSprite* ewk);
s32 effect_A8_init(s16 id, u8 dir_old, s16 sync_bg, s16 master_player, s16 cursor_index, s16 char_ix, s16 pos_index);

#endif
