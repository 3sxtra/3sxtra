/**
 * @file paths.c
 * @brief SDL path helpers with portable mode support.
 *
 * Supports dual-path resolution:
 *   1. Portable mode: <exe_dir>/config/  (if the folder exists)
 *   2. Standard mode: AppData/CrowdedStreet/3SX/  (SDL_GetPrefPath)
 *
 * Portable mode is auto-detected at startup. To enable it, create a
 * "config" folder next to the game executable.
 */
#include "port/paths.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const char* pref_path = NULL;
static int portable_mode = -1; /* -1=unchecked, 0=standard, 1=portable */

/**
 * @brief Get the user preferences/save directory path (lazy-initialized).
 *
 * Checks for <exe_dir>/config/ first (portable mode).
 * Falls back to SDL_GetPrefPath("CrowdedStreet", "3SX") (AppData).
 */
const char* Paths_GetPrefPath() {
    if (pref_path != NULL) {
        return pref_path;
    }

    /* Check for portable mode: config/ folder next to executable */
    if (portable_mode == -1) {
        const char* base = SDL_GetBasePath();
        if (base) {
            static char portable_path[512];
            snprintf(portable_path, sizeof(portable_path), "%sconfig/", base);

            SDL_PathInfo info;
            if (SDL_GetPathInfo(portable_path, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
                portable_mode = 1;
                pref_path = portable_path;
                printf("[Paths] Portable mode: using %s\n", pref_path);
                return pref_path;
            }
        }
        portable_mode = 0;
    }

    /* Standard mode: AppData */
    pref_path = SDL_GetPrefPath("CrowdedStreet", "3SX");
    printf("[Paths] Standard mode: using %s\n", pref_path);
    return pref_path;
}

/** @brief Get the application base directory path (lazy-initialized, cached). */
const char* Paths_GetBasePath() {
    static const char* base_path = NULL;
    if (base_path == NULL) {
        const char* sdl_base = SDL_GetBasePath();
        if (sdl_base) {
            base_path = SDL_strdup(sdl_base);
        }
    }
    return base_path;
}

/** @brief Returns 1 if running in portable mode (config/ next to exe). */
int Paths_IsPortable() {
    if (portable_mode == -1) {
        Paths_GetPrefPath(); /* trigger detection */
    }
    return portable_mode == 1;
}
