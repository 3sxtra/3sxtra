/**
 * @file sound_lookup.c
 * @brief CPS3 sound code lookup function.
 *
 * Provides Get_Sound_Lookup() which searches g_SoundLookupTable (defined in
 * sound_lookup_data.c) by logical SoundRequest ID.
 *
 * Part of the sound module.
 */

#include "sf33rd/Source/Game/sound/sound_ids.h"
#include "sf33rd/Source/Game/sound/sound_lookup_data.h"

const SoundLookupEntry* Get_Sound_Lookup(SoundRequest id) {
    const SoundLookupEntry* entry = g_SoundLookupTable;

    // The table starts with ID 0 (SND_NONE).
    if (id == 0) {
        return entry;
    }

    entry++; // Skip the first entry (ID 0)

    while (entry->logical_id != 0) {
        if (entry->logical_id == id) {
            return entry;
        }
        entry++;
    }

    return NULL;
}
