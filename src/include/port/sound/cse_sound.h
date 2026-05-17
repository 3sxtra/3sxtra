/**
 * @file cse_sound.h
 * @brief Consolidated Capcom Sound Engine (CSE) facade header.
 *
 * Game code should include this single header instead of reaching into
 * the deep sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/ directory tree.
 * Re-exports the TSB sequencer, memory map, and CSE system interfaces.
 */

#ifndef CSE_SOUND_H
#define CSE_SOUND_H

#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/cse.h"
#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/emlMemMap.h"
#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/emlTSB.h"

#endif /* CSE_SOUND_H */
