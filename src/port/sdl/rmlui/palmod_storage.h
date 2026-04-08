/**
 * @file palmod_storage.h
 * @brief Custom palette save/load for the palette editor.
 *
 * Palettes are stored as JSON files under the preferences directory:
 *   <PrefPath>/palettes/char/<char_name>/<palette_name>.json
 *   <PrefPath>/palettes/stage/row_<N>/<palette_name>.json
 *
 * Each JSON file contains: { "name": "...", "colors": [64 u16 values] }
 */
#ifndef PALMOD_STORAGE_H
#define PALMOD_STORAGE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PALMOD_MAX_PALETTES 32
#define PALMOD_NAME_MAX 64
#define PALMOD_COLORS_PER_ROW 64

/** Entry in a palette list. */
typedef struct {
    char name[PALMOD_NAME_MAX];
} PalmodEntry;

/**
 * Save a palette to disk.
 * @param category "char" or "stage"
 * @param sub_name character name or "row_32" etc.
 * @param pal_name user-chosen palette name
 * @param colors   array of 64 u16 values (CPS3 format)
 * @return true on success
 */
bool palmod_save(const char* category, const char* sub_name, const char* pal_name, const u16* colors);

/**
 * Load a palette from disk.
 * @param category "char" or "stage"
 * @param sub_name character name or "row_32" etc.
 * @param pal_name palette name to load
 * @param out_colors output array of 64 u16 values
 * @return true on success
 */
bool palmod_load(const char* category, const char* sub_name, const char* pal_name, u16* out_colors);

/**
 * List saved palettes for a category/sub_name.
 * @param category "char" or "stage"
 * @param sub_name character name or "row_32"
 * @param out_entries output array (max PALMOD_MAX_PALETTES)
 * @return number of palettes found
 */
int palmod_list(const char* category, const char* sub_name, PalmodEntry* out_entries);

/**
 * Delete a saved palette.
 * @return true on success
 */
bool palmod_delete(const char* category, const char* sub_name, const char* pal_name);

#ifdef __cplusplus
}
#endif

#endif /* PALMOD_STORAGE_H */
