#ifndef STUN_H
#define STUN_H

#include "structs.h"
#include "types.h"

// Serialized for netplay
extern SDAT sdat[2];

void stngauge_cont_init();
void stngauge_cont_main();
void stngauge_control(u8 pl);
void stngauge_work_clear();

#endif
