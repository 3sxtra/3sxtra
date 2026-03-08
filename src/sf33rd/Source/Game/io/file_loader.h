#ifndef FILE_LOADER_H
#define FILE_LOADER_H

#include "structs.h"
#include "types.h"

/** @brief Load a file by number, allocating a key from any pool, returning adrs and key. */
s32 load_it_use_any_key2(u16 fnum, void** adrs, s16* key, u8 kokey, u8 group);

/** @brief Load a file by number, returning an allocated key. */
s16 load_it_use_any_key(u16 fnum, u8 kokey, u8 group);

/** @brief Load a file by number using a specific pre-allocated key. */
s32 load_it_use_this_key(u16 fnum, s16 key);

#endif
