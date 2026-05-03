/**
 * @file opening_subroutines.h
 * @brief Opening subroutines — texture release, tile rendering, palette copy.
 *
 * Part of the opening module.
 */

#ifndef _OPENING_SUBROUTINES_H_
#define _OPENING_SUBROUTINES_H_

#include "structs.h"
#include "types.h"

void TexRelease(u32 G_Num);
void TexRelease_OP();
void put_chr2(OPTW* optw);
void opbg_trans(OPBW* opbw, s16 x, s16 y);
void COLOR_COPYn(s16 dst, s16 colcd, s16 n);

#endif
