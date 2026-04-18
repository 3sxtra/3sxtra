#include "port/paths.h"

#include <SDL3/SDL.h>

static const char* pref_path = NULL;

const char* Paths_GetPrefPath() {
    if (pref_path == NULL) {
        pref_path = SDL_GetPrefPath("CrowdedStreet", "3SX");
    }

    return pref_path;
}

const char* Paths_GetBasePath() {
#ifdef __ANDROID__
    /* On Android, assets must be accessed via relative paths so that
     * SDL_IOFromFile routes through the AssetManager. Return empty. */
    return "";
#else
    return SDL_GetBasePath();
#endif
}
