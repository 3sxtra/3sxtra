/**
 * @file lua_trials_loader.h
 * @brief Runtime loader for trial definitions from Lua scripts.
 *
 * Loads sf3_3rd_trial_clean.lua at runtime and populates the same TrialDef
 * structures that trials_data.inc provides at compile time.
 */
#ifndef LUA_TRIALS_LOADER_H
#define LUA_TRIALS_LOADER_H

#include "sf33rd/Source/Game/training/trials.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load trial definitions from a Lua file at runtime.
 * Populates dynamically allocated TrialDef/TrialCharacterDef arrays.
 * Returns true on success, false on failure (falls back to static data).
 */
bool lua_trials_load(const char* lua_path);

/** Free all dynamically allocated trial data. */
void lua_trials_free(void);

/** Get the runtime-loaded character defs (NULL if not loaded). */
const TrialCharacterDef* lua_trials_get_characters(int* out_count);

#ifdef __cplusplus
}
#endif

#endif /* LUA_TRIALS_LOADER_H */
