/**
 * @file gauge.h
 * @brief Super art gauge and stun gauge data table declarations (binary-to-object data).
 *
 * Declares per-character super art gauge increment tables and stun
 * (piyo) gauge data. Generated from CPS3 ROM binary data.
 */
#ifndef BIN2OBJ_GAUGE_H
#define BIN2OBJ_GAUGE_H

#include "types.h"

extern u8 _add_arts_gauge[20][16][4];
extern u8 _add_piyo_gauge[20][16];

#endif
