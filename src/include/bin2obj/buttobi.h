/**
 * @file buttobi.h
 * @brief Knockback/launch timing and trajectory table declarations (binary-to-object data).
 *
 * Declares tables controlling knockback duration and vertical displacement
 * for airborne hit reactions. Generated from CPS3 ROM binary data.
 */
#ifndef BIN2OBJ_BUTTOBI_H
#define BIN2OBJ_BUTTOBI_H

#include "types.h"

extern s16 _buttobi_time_table[][4];
extern s16 _buttobi_add_y_table[][4];

#endif
