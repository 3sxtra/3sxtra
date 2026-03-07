#ifndef PORT_RESOURCES_H
#define PORT_RESOURCES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Get path to a file in resources folder.
/// @param file_path Relative path to a file in resources, or `NULL` for path to the root of resources folder.
char* Resources_GetPath(const char* file_path);

/// @brief Get path to a file in rom folder next to the game executable.
/// @param file_path Relative path to a file in rom, or `NULL` for path to the root of rom folder.
char* Resources_GetRomPath(const char* file_path);

bool Resources_CheckIfPresent();

/// @brief Run resource copying flow. Repeated calls of this function progress the flow.
/// @return `true` if resources have been copied, `false` otherwise.
bool Resources_RunResourceCopyingFlow();

#ifdef __cplusplus
}
#endif

#endif
