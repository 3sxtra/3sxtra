#ifndef EFFECT_61_MENU_OPTIONS_H
#define EFFECT_61_MENU_OPTIONS_H

#include "structs.h"
#include "types.h"

void effect_61_move(EffectMultiSprite* ewk);
s32 Check_Die_61(State_Other* ewk);
s32 effect_61_init(s16 master, u8 dir_old, s16 sync_bg, s16 master_player, s16 char_ix, s16 cursor_index,
                   u16 letter_type);
void Menu_UpdateNetworkLabel(void);

#endif
