#ifndef EFFECT_56_COLOR_BONUS_H
#define EFFECT_56_COLOR_BONUS_H

#include "structs.h"
#include "types.h"

// Serialized for netplay (fix desync #117)
extern const u8* ci_pointer;
extern u8 ci_col;
extern u8 ci_timer;

void effect_56_move(State_Other* ewk);
s32 effect_56_init(u8 type, u8 kill);

#endif
