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

/// Returns 1 if running in portable mode (config/ folder next to exe)
int Paths_IsPortable();

#ifdef __cplusplus
}
#endif

#endif
