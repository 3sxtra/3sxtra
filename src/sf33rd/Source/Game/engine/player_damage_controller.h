#ifndef PLPDM_H
#define PLPDM_H

#include "structs.h"
#include "types.h"

extern const u8 guard_kind[];
extern const s8 atsagct[];

void Player_damage(PlayerEntity* wk);
void subtract_dm_vital(PlayerEntity* wk);
void subtract_dm_vital_aiuchi(PlayerEntity* wk);
s32 setup_kuzureochi(PlayerEntity* wk);
void get_catch_off_data(PlayerEntity* wk, s16 ix);

#endif
