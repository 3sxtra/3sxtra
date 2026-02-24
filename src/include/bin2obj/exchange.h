/**
 * @file exchange.h
 * @brief Power exchange/palette swap table declarations (binary-to-object data).
 *
 * Declares power exchange and alternate color palette pointer tables
 * used during character transformations. Generated from CPS3 ROM binary data.
 */
#ifndef BIN2OBJ_EXCHANGE_H
#define BIN2OBJ_EXCHANGE_H

#include "types.h"

extern u32* _exchange_pow_pl03_sa3[];
extern u32* _exchange_pow[];
extern u32* _exchange_koa[];

#endif
