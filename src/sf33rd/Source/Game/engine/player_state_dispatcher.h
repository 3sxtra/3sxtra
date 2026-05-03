#ifndef PLS00_H
#define PLS00_H

#include "structs.h"
#include "types.h"

void check_lever_data(PlayerEntity* wk);
void check_jump_rl_dir(PlayerEntity* wk);
void set_new_jump_direction(PlayerEntity* wk);
void jumping_guard_type_check(PlayerEntity* wk);

#endif
