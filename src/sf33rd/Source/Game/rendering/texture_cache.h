/**
 * @file texcash.h
 * @brief Texture cache manager declarations.
 */

#ifndef TEXTURE_CACHE_H
#define TEXTURE_CACHE_H

#include "structs.h"
#include "types.h"

extern TexturePoolUsed* tpu_free;
extern u8* texture_cache_melt_buffer;

void disp_texture_cache_free_area();
void Init_Texture_Cache_Primary();
void Init_Texture_Cache_Secondary(s16 ix);
void Init_Texture_Cache_Before_Process();
void Search_Texture_Cache_Free_Area(s16 ix);
void update_with_tpu_free(PatternState* mc16, PatternState* mc32);
void texture_cash_update();
void Allocate_Texture_Cache(s16 ix);
void Free_Texture_Cache(s16 ix);
void Clear_Texture_Cache();
s16 get_my_trans_mode(s16 curr);

#endif
