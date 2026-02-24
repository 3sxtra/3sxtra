/**
 * @file libgraph.h
 * @brief PS2 GS (Graphics Synthesizer) pixel storage format constants.
 *
 * Shim header providing SCE_GS_PSM* defines for PS2 pixel formats
 * (PSMCT32, PSMT8, PSMT4, etc.) used by the texture/VRAM system.
 */
#ifndef _LIBGRAPH_H
#define _LIBGRAPH_H

#define SCE_GS_PSMCT32 (0)
#define SCE_GS_PSMCT24 (1)
#define SCE_GS_PSMCT16 (2)
#define SCE_GS_PSMCT16S (10)
#define SCE_GS_PSMT8 (19)
#define SCE_GS_PSMT4 (20)
#define SCE_GS_PSMT8H (27)
#define SCE_GS_PSMT4HL (36)
#define SCE_GS_PSMT4HH (44)
#define SCE_GS_PSMZ32 (48)
#define SCE_GS_PSMZ24 (49)
#define SCE_GS_PSMZ16 (50)
#define SCE_GS_PSMZ16S (58)

#endif
