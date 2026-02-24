/**
 * @file sys_sub2.h
 * @brief Display-size pack/unpack utilities.
 */
#ifndef SYSSUB2_H
#define SYSSUB2_H

#include "types.h"

u8 dspwhPack(u8 xdsp, u8 ydsp);
void dspwhUnpack(u8 src, u8* xdsp, u8* ydsp);

#endif
