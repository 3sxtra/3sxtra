/**
 * @file resources.c
 * @brief Game resource locator and copy flow.
 *
 * Checks whether required game assets (e.g. SF33RD.AFS) exist in the
 * user-writable resources directory. If missing, prompts the user to
 * select a ROM directory and copies the necessary files.
 */
#include "port/resources.h"
#include "port/paths.h"
#include "port/sdl/sdl_app.h"

#include <SDL3/SDL.h>

typedef enum FlowState { INIT, DIALOG_OPENED, COPY_ERROR, COPY_SUCCESS } ResourceCopyingFlowState;

static ResourceCopyingFlowState flow_state = INIT;

/** @brief Check whether a file exists at the given path. */
static bool file_exists(const char* path) {
    SDL_PathInfo path_info;
    SDL_GetPathInfo(path, &path_info);
    return path_info.type == SDL_PATHTYPE_FILE;
}

/** @brief Check if a named file exists in the resources directory. */
static bool check_if_file_present(const char* filename) {
    char* file_path = Resources_GetPath(filename);
    bool result = file_exists(file_path);
    SDL_free(file_path);
    return result;
}

/** @brief Ensure the resources directory exists (create if missing). */
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
    const char* rom_path = filelist[0];
    bool success = true;
    success &= copy_file(rom_path, "THIRD/SF33RD.AFS", "SF33RD.AFS");
    flow_state = success ? COPY_SUCCESS : COPY_ERROR;
}

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
    const char* base = SDL_GetBasePath();
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
    if (check_if_file_present("SF33RD.AFS")) {
        return true;
    }

    // Fallback: check rom/ folder next to the executable
    char* rom_path = Resources_GetRomPath("SF33RD.AFS");
    const bool found = file_exists(rom_path);
    SDL_free(rom_path);
    return found;
}

/** @brief Drive the resource-copying state machine (dialog → copy → done). */
bool Resources_RunResourceCopyingFlow() {
    switch (flow_state) {
    case INIT:
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                                 "Resources are missing",
                                 "3SX needs resources from a copy of \"Street Fighter III: 3rd Strike\" to run. Choose "
                                 "a location with the game files in the next dialog",
                                 SDLApp_GetWindow());
        flow_state = DIALOG_OPENED;
        SDL_ShowOpenFolderDialog(open_folder_dialog_callback, NULL, SDLApp_GetWindow(), NULL, false);
        break;

    case DIALOG_OPENED:
        // Wait for the callback to be called
        break;

    case COPY_ERROR:
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Invalid directory",
                                 "The directory you provided doesn't contain the required files",
                                 SDLApp_GetWindow());
        flow_state = DIALOG_OPENED;
        SDL_ShowOpenFolderDialog(open_folder_dialog_callback, NULL, SDLApp_GetWindow(), NULL, false);
        break;

    case COPY_SUCCESS:
        char* resources_path = Resources_GetPath(NULL);
        char* message = NULL;
        SDL_asprintf(&message, "You can find them at:\n%s", resources_path);
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_INFORMATION, "Resources copied successfully", message, SDLApp_GetWindow());
        SDL_free(resources_path);
        SDL_free(message);
        flow_state = INIT;
        return true;
    }

    return false;
}
