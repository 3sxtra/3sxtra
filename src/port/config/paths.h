/**
 * @file paths.h
 * @brief SDL path helpers with portable mode support.
 */
#ifndef PORT_PATHS_H
#define PORT_PATHS_H

#ifdef __cplusplus
extern "C" {
#endif

/// Get app directory path
///
/// This value shouldn't be freed after use
const char* Paths_GetPrefPath();

const char* Paths_GetBasePath();

/// Resolve an asset relative path. Checks base_path first, then pref_path (AppData).
/// Returns a static buffer (not thread-safe, single-caller pattern).
const char* Paths_ResolveAsset(const char* relative_path);
/// Returns 1 if running in portable mode (config/ folder next to exe)
int Paths_IsPortable();

#ifdef __cplusplus
}
#endif

#endif
