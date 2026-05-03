/**
 * @file caldir_data.h
 * Extern declarations for direction / motion calculation lookup tables.
 */

#ifndef CALDIR_DATA_H
#define CALDIR_DATA_H

#include "types.h"

/** 256-entry fixed-point sin/cos rate table (16.16). */
extern const s32 rate_256_table[256][2];

/** 128x128 direction selection table (arctan approximation). */
extern const u8 dir_sel_table[128][128];

#endif
