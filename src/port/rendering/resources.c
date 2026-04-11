/**
 * @file resources.c
 * @brief Game resource locator and copy flow.
 *
 * Checks whether required game assets (e.g. SF33RD.AFS) exist in the
 * user-writable resources directory. If missing, prompts the user to
 * select a ROM directory and copies the necessary files.
 */
#include "port/rendering/resources.h"
#include "port/config/paths.h"
#include "port/sdl/app/sdl_app.h"

#include <SDL3/SDL.h>

#if !defined(PLATFORM_RPI4) && !defined(PLATFORM_PS3)
typedef enum FlowState { INIT, DIALOG_OPENED, COPY_ERROR, COPY_SUCCESS } ResourceCopyingFlowState;

static ResourceCopyingFlowState flow_state = INIT;
#endif

/** @brief Check whether a file exists at the given path. */
static bool file_exists(const char* path) {
#if defined(PLATFORM_PS3)
    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
#else
    SDL_PathInfo path_info;
    if (!SDL_GetPathInfo(path, &path_info)) {
        return false;
    }
    return path_info.type == SDL_PATHTYPE_FILE;
#endif
}

/** @brief Check if a named file exists in the resources directory. */
static bool check_if_file_present(const char* filename) {
    char* file_path = Resources_GetPath(filename);
    bool result = file_exists(file_path);
    SDL_free(file_path);
    return result;
}

/** @brief Ensure the resources directory exists (create if missing). */
#if !defined(PLATFORM_RPI4) && !defined(PLATFORM_PS3)
static void create_resources_directory() {
    char* path = Resources_GetPath(NULL);
    SDL_CreateDirectory(path);
    SDL_free(path);
}

/** @brief Copy a single file from rom_path/src_name to resources/dst_name. */
static bool copy_file(const char* rom_path, const char* src_name, const char* dst_name) {
    char* src_path = NULL;
    char* dst_path = Resources_GetPath(dst_name);
    SDL_asprintf(&src_path, "%s/%s", rom_path, src_name);

    create_resources_directory();
    const bool success = SDL_CopyFile(src_path, dst_path);

    SDL_free(src_path);
    SDL_free(dst_path);

    return success;
}

/** @brief SDL folder-dialog callback — copies required game files when a folder is selected. */
static void open_folder_dialog_callback(void* userdata, const char* const* filelist, int filter) {
    /* filelist is NULL when the dialog is cancelled, closed, or unavailable
       (e.g. headless Linux / Batocera where no dialog backend exists). */
    if (!filelist || !filelist[0]) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Folder dialog returned no selection (cancelled or unavailable)");
        flow_state = COPY_ERROR;
        return;
    }

    const char* rom_path = filelist[0];
    bool success = true;
    success &= copy_file(rom_path, "THIRD/SF33RD.AFS", "SF33RD.AFS");
    flow_state = success ? COPY_SUCCESS : COPY_ERROR;
}

#ifdef __ANDROID__
/** @brief SDL file-dialog callback — copies the selected AFS file. */
static void open_file_dialog_callback(void* userdata, const char* const* filelist, int filter) {
    if (!filelist || !filelist[0]) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "File dialog returned no selection");
        flow_state = COPY_ERROR;
        return;
    }

    const char* src_path = filelist[0];
    const char* internal_dir = SDL_GetAndroidInternalStoragePath();
    if (!internal_dir) {
        flow_state = COPY_ERROR;
        return;
    }

    char* dst_path = NULL;
    SDL_asprintf(&dst_path, "%s/SF33RD.AFS", internal_dir);

    bool success = SDL_CopyFile(src_path, dst_path);
    SDL_free(dst_path);

    flow_state = success ? COPY_SUCCESS : COPY_ERROR;
}
#endif
#endif

/** @brief Build and return the full path to a file in the resources directory (caller frees). */
char* Resources_GetPath(const char* file_path) {
    const char* base = Paths_GetPrefPath();
    char* full_path = NULL;

    if (file_path == NULL) {
        SDL_asprintf(&full_path, "%sresources/", base);
    } else {
        SDL_asprintf(&full_path, "%sresources/%s", base, file_path);
    }

    return full_path;
}

/** @brief Build and return the full path to a file in the rom directory (caller frees). */
char* Resources_GetRomPath(const char* file_path) {
    const char* base = Paths_GetBasePath();
    char* full_path = NULL;

    if (file_path == NULL) {
        SDL_asprintf(&full_path, "%srom/", base);
    } else {
        SDL_asprintf(&full_path, "%srom/%s", base, file_path);
    }

    return full_path;
}

/** @brief Check if required game resources (SF33RD.AFS) exist. */
bool Resources_CheckIfPresent() {
#ifdef __ANDROID__
    // On Android, check internal storage first, then APK assets.
    // SDL_IOFromFile with a relative path checks the Android AssetManager.
    const char* internal = SDL_GetAndroidInternalStoragePath();
    if (internal) {
        char* internal_path = NULL;
        SDL_asprintf(&internal_path, "%s/SF33RD.AFS", internal);
        bool found = file_exists(internal_path);
        SDL_free(internal_path);
        if (found)
            return true;
    }
    return false;
#else
    if (check_if_file_present("SF33RD.AFS")) {
        return true;
    }

    // Fallback: check rom/ folder next to the executable
    char* rom_path = Resources_GetRomPath("SF33RD.AFS");
    const bool found = file_exists(rom_path);
    SDL_free(rom_path);
    return found;
#endif
}

/** @brief Drive the resource-copying state machine (dialog → copy → done). */
bool Resources_RunResourceCopyingFlow() {
#if defined(PLATFORM_RPI4) || defined(PLATFORM_PS3)
    /* Batocera / headless Linux / PS3 has no desktop dialog backend.
       Log a clear error instead of spinning forever or crashing. */
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Resources not found. On RPi4/Batocera/PS3, place SF33RD.AFS in the rom/ folder "
                 "next to the executable (no file-picker available).");
    return false;
#else
    switch (flow_state) {
    case INIT:
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_INFORMATION,
            "Resources are missing",
#ifdef __ANDROID__
            "3SX needs the SF33RD.AFS ROM to run. Please select your SF33RD.AFS file in the next dialog.",
#else
            "3SX needs resources from a copy of \"Street Fighter III: 3rd Strike\" to run. Choose "
            "a location with the game files in the next dialog",
#endif
            SDLApp_GetWindow());
        flow_state = DIALOG_OPENED;
#ifdef __ANDROID__
        SDL_DialogFileFilter filter[1] = { { "AFS Files", "AFS;afs" } };
        SDL_ShowOpenFileDialog(open_file_dialog_callback, NULL, SDLApp_GetWindow(), filter, 1, NULL, false);
#else
        SDL_ShowOpenFolderDialog(open_folder_dialog_callback, NULL, SDLApp_GetWindow(), NULL, false);
#endif
        break;

    case DIALOG_OPENED:
        // Wait for the callback to be called
        break;

    case COPY_ERROR:
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Invalid directory",
#ifdef __ANDROID__
                                 "The file you provided couldn't be copied. Please select a valid SF33RD.AFS file.",
#else
                                 "The directory you provided doesn't contain the required files",
#endif
                                 SDLApp_GetWindow());
        flow_state = DIALOG_OPENED;
#ifdef __ANDROID__
        SDL_DialogFileFilter filter2[1] = { { "AFS Files", "AFS;afs" } };
        SDL_ShowOpenFileDialog(open_file_dialog_callback, NULL, SDLApp_GetWindow(), filter2, 1, NULL, false);
#else
        SDL_ShowOpenFolderDialog(open_folder_dialog_callback, NULL, SDLApp_GetWindow(), NULL, false);
#endif
        break;

    case COPY_SUCCESS:
        char* message = NULL;
#ifdef __ANDROID__
        SDL_asprintf(&message, "ROM copied successfully to internal storage!");
#else
        char* resources_path = Resources_GetPath(NULL);
        SDL_asprintf(&message, "You can find them at:\n%s", resources_path);
        SDL_free(resources_path);
#endif
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_INFORMATION, "Resources copied successfully", message, SDLApp_GetWindow());
        SDL_free(message);
        flow_state = INIT;
        return true;
    }

    return false;
#endif
}
