/**
 * @file bg_load.h
 * Background texture loading and resource management.
 * Split from bg.c — see SYSTEM_MODERNIZATION.md #37.
 */

#ifndef BG_LOAD_H
#define BG_LOAD_H

#include "types.h"

void Bg_TexInit(void);
void Bg_Close(void);
void Bg_Texture_Load_EX(void);
void Bg_Texture_Load2(u8 type);
void Bg_Texture_Load_Ending(s16 type);

#endif
