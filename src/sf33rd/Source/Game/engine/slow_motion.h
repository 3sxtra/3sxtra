#ifndef SLOW_MOTION_H
#define SLOW_MOTION_H

#include "types.h"

extern s16 slowmo_timer;
extern s16 slowmo_flag;
extern s16 execute_flag;

void init_slowmo_flag();
void set_round_end_slowmo();
void set_execute_flag();

#endif // SLOW_MOTION_H
