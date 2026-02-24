#ifndef VITAL_H
#define VITAL_H

#include "structs.h"
#include "types.h"

// Serialized for netplay
extern VIT vit[2];

void vital_cont_init();
void vital_cont_main();
void vital_control(u8 pl);
void vital_parts_allwrite(u8 Pl_Num);

#endif
