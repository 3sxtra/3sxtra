/**
 * @file sys_sub2.c
 * @brief Display configuration and screen-adjustment pack/unpack utilities.
 *
 * Encodes and decodes display size percentages (0–100) into a single byte
 * for compact storage in save data.
 *
 * Part of the system module.
 * Originally from the PS2 sys_sub2 module.
 */

#include "sf33rd/Source/Game/system/sys_sub2.h"
#include "common.h"
#include "sf33rd/Source/Game/system/work_sys.h"

/** @brief Pack two display-size percentages (X, Y) into a single byte. */
u8 dspwhPack(u8 xdsp, u8 ydsp) {
    u8 rnum = 100 - ydsp;
    rnum |= (100 - xdsp) * 16;
    return rnum;
}

/** @brief Unpack a display-size byte into two percentages (X, Y). */
void dspwhUnpack(u8 src, u8* xdsp, u8* ydsp) {
    *xdsp = 100 - ((src >> 4) & 0xF);
    *ydsp = 100 - (src & 0xF);
}
