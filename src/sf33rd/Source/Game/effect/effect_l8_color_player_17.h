#ifndef EFFECT_L8_COLOR_PLAYER_17_H
#define EFFECT_L8_COLOR_PLAYER_17_H

#include "structs.h"
#include "types.h"

void effect_L8_move(State_Other* ewk);
s32 effect_L8_init(PLW* wk);
void check_new_color_data_L8(State* wk);
void get_new_color_data_L8(State* /* unused */, s16* trom, s16* tram);
void save_old_color_data(s16* wram, s16* tram);
void load_old_color_data(s16* wram, s16* tram);

#endif
