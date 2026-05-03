#ifndef CHARID_H
#define CHARID_H

#include "structs.h"

extern ParabolaData* parabora_own_table[20];
extern CharInitData char_init_data[23];

void set_char_base_data(WORK* wk);
void copy_char_base_data();

#endif
