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
#include "port/config/paths.h"

#include <SDL3/SDL.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "port/config/paths.h"
#include "port/config/config.h"

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
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Paths] Portable mode: using %s\n", pref_path);
                return pref_path;
            }
        }
        portable_mode = 0;
    }

    /* Standard mode: AppData */
#ifdef PLATFORM_PS3
    pref_path = "/dev_hdd0/game/3SX00001/USRDIR/";
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Paths] Standard mode (PS3): using %s\n", pref_path);
#else
    pref_path = SDL_GetPrefPath("CrowdedStreet", "3SX");
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Paths] Standard mode: using %s\n", pref_path);
#endif
    return pref_path;
}

static void Paths_Normalize(char* path) {
    if (!path)
        return;
    for (int i = 0; path[i]; i++) {
        if (path[i] == '\\')
            path[i] = '/';
    }
    /* Resolve /../ sequences (needed for Android asset paths) */
    char* p;
    while ((p = SDL_strstr(path, "/../")) != NULL) {
        char* prev = p - 1;
        while (prev >= path && *prev != '/')
            prev--;
        if (prev >= path) {
            SDL_memmove(prev, p + 3, SDL_strlen(p + 3) + 1);
        } else {
            SDL_memmove(path, p + 4, SDL_strlen(p + 4) + 1);
        }
    }
}
/** @brief Get the application base directory path (lazy-initialized, cached). */
const char* Paths_GetBasePath() {
    static char s_base_path[1024] = { 0 };
    if (s_base_path[0] == '\0') {
#ifdef _WIN32
        wchar_t w_path[MAX_PATH];
        if (GetModuleFileNameW(NULL, w_path, MAX_PATH) > 0) {
            // Convert to UTF-8
            char utf8_path[1024];
            int size = WideCharToMultiByte(CP_UTF8, 0, w_path, -1, utf8_path, sizeof(utf8_path), NULL, NULL);
            if (size > 0) {
                char current[1024];
                SDL_strlcpy(current, utf8_path, sizeof(current));
                Paths_Normalize(current);

                // Strip filename
                char* last_slash = strrchr(current, '/');
                if (last_slash)
                    *last_slash = '\0';

                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Paths] Searching for root from (Win32): %s", current);

                bool found_root = false;
                while (true) {
                    char check_path[1024];
                    snprintf(check_path, sizeof(check_path), "%s/assets/ASSET_VERSION", current);

                    SDL_IOStream* io = SDL_IOFromFile(check_path, "rb");
                    if (io) {
                        SDL_CloseIO(io);
                        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Paths] FOUND PROJECT ROOT AT: %s", current);
                        found_root = true;
                        break;
                    }

                    char* up = strrchr(current, '/');
                    if (!up || (up > current && *(up - 1) == ':'))
                        break;
                    *up = '\0';
                }

                if (found_root) {
                    snprintf(s_base_path, sizeof(s_base_path), "%s/", current);
                } else {
                    // Fallback to exe directory
                    SDL_strlcpy(s_base_path, utf8_path, sizeof(s_base_path));
                    last_slash = strrchr(s_base_path, '/');
                    if (last_slash)
                        *(last_slash + 1) = '\0';
                }
            }
        }
#endif
        // Fallback or multi-platform
        if (s_base_path[0] == '\0') {
#ifdef __ANDROID__
            /* On Android, assets must be accessed via relative paths to
             * utilize the AssetManager. Keep base path empty. */
            s_base_path[0] = '\0';
#elif defined(PLATFORM_PS3)
            /* Force PS3 base path to the standard app installation directory.
             * This prevents RPCS3 from attempting to resolve relative to /app_home/
             * when booting an ELF directly, ensuring reliable asset locators. */
            SDL_strlcpy(s_base_path, "/dev_hdd0/game/3SX00001/USRDIR/", sizeof(s_base_path));
#else
            const char* sdl_base = SDL_GetBasePath();
            if (sdl_base) {
                SDL_strlcpy(s_base_path, sdl_base, sizeof(s_base_path));
                Paths_Normalize(s_base_path);
            } else {
                SDL_strlcpy(s_base_path, "./", sizeof(s_base_path));
            }
#endif
        }
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Paths] Base path initialized to: %s", s_base_path);
    }
    return s_base_path;
}

/**
 * @brief Resolve an asset path by searching multiple project-relative locations.
 *
 * Search order for each base (Project Root, then Pref Path):
 *   1. base/relative_path
 *   2. base/assets/relative_path
 *   3. base/src/relative_path
 *   4. base/bin/relative_path      (to handle build artifacts)
 *
 * Returns a pointer to a rotating static buffer (supports ~4 concurrent callers
 * in a single statement).
 */
const char* Paths_ResolveAsset(const char* relative_path) {
    if (!relative_path || !relative_path[0])
        return relative_path;

    static char buffers[4][1024];
    static int next_idx = 0;
    char* resolved = buffers[next_idx++ % 4];
    memset(resolved, 0, 1024);

    const char* sub_dirs[] = { "", "assets/", "src/", "bin/" };
    const char* bases[] = { Paths_GetBasePath(), Paths_GetPrefPath() };

    for (int i = 0; i < 2; i++) {
        const char* base = bases[i];
        if (!base || !base[0])
            continue;

        for (int j = 0; j < 4; j++) {
            // Avoid double 'assets/' if the input path already contains it
            if (j == 1 && (strncmp(relative_path, "assets/", 7) == 0 || strncmp(relative_path, "assets\\", 7) == 0))
                continue;

            snprintf(resolved, 1024, "%s%s%s", base, sub_dirs[j], relative_path);
            Paths_Normalize(resolved);

            const char* final_path = resolved;
#ifdef __ANDROID__
            /* Strip assets/ prefix so SDL_IOFromFile routes through AssetManager */
            if (strncmp(final_path, "assets/", 7) == 0) {
                final_path += 7;
            }
#endif

            SDL_IOStream* io = SDL_IOFromFile(final_path, "rb");
            if (io) {
                SDL_CloseIO(io);
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Paths] Resolved '%s' -> '%s'", relative_path, final_path);
                /* Return exactly the working path that was successful */
                SDL_strlcpy(resolved, final_path, 1024);
                return resolved;
            }
        }
    }

#ifdef __ANDROID__
    /* On Android the base path is empty, so the search loop above may be
     * skipped. Normalize the path (resolve ../) and strip the "assets/"
     * prefix so that SDL_IOFromFile routes through the AssetManager. */
    SDL_strlcpy(resolved, relative_path, 1024);
    Paths_Normalize(resolved);
    if (strncmp(resolved, "assets/", 7) == 0) {
        memmove(resolved, resolved + 7, strlen(resolved + 7) + 1);
    }
    return resolved;
#endif

    /* Not found anywhere — return original (copy it into the buffer to be safe against caller mutation) */
    SDL_strlcpy(resolved, relative_path, 1024);
    return resolved;
}
/** @brief Returns 1 if running in portable mode (config/ next to exe). */
int Paths_IsPortable() {
    if (portable_mode == -1) {
        Paths_GetPrefPath(); /* trigger detection */
    }
    return portable_mode == 1;
}
