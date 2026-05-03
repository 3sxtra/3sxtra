#ifndef DAMAGE_CALCULATOR_H
#define DAMAGE_CALCULATOR_H

#include "structs.h"
#include "types.h"

void calculate_damage_vitality(PlayerEntity* as, PlayerEntity* ds);
void cal_damage_vitality_eff(State_Other* as, PlayerEntity* ds);
void additional_score_damage(State_Other* wk, u16 ix);

#endif
