/**
 * @file sdl_app_shader_config.c
 * @brief Shader preset discovery, loading, and cycling.
 *
 * Manages librashader preset scanning, loading, and runtime switching.
 * Supports both built-in and libretro-format shader presets with
 * recursive directory scanning. Split from sdl_app.c for modularity.
 */
#include "port/sdl/sdl_app_shader_config.h"
#include "port/config.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_app_config.h"
#include "port/sdl/sdl_app_internal.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LibrashaderManager* libretro_manager = NULL;
static char** available_presets = NULL;
static int available_preset_count = 0;
static int current_preset_index = 0;
static int s_pending_preset_index = -1;
static bool shader_mode_libretro = false;
static char* g_base_path =
    NULL; // Owned reference (strdup'd or managed elsewhere? In sdl_app it was a pointer to SDL_GetBasePath result)

// Recursive scanner helper
static void scan_presets_recursive(const char* base_path, const char* relative_path, char*** list, int* count,
                                   int* capacity) {
    char current_path[2048];
    if (relative_path[0] == '\0') {
        snprintf(current_path, sizeof(current_path), "%s", base_path);
    } else {
        snprintf(current_path, sizeof(current_path), "%s/%s", base_path, relative_path);
    }

    int num_entries = 0;
    char** entries = SDL_GlobDirectory(current_path, "*", SDL_GLOB_CASEINSENSITIVE, &num_entries);

    if (!entries)
        return;

    for (int i = 0; i < num_entries; i++) {
        const char* entry = entries[i];
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0)
            continue;

        char entry_full_path[4096];
        snprintf(entry_full_path, sizeof(entry_full_path), "%s/%s", current_path, entry);

        SDL_PathInfo info;
        if (SDL_GetPathInfo(entry_full_path, &info)) {
            if (info.type == SDL_PATHTYPE_DIRECTORY) {
                // Recurse
                char new_relative[1024];
                if (relative_path[0] == '\0') {
                    snprintf(new_relative, sizeof(new_relative), "%s", entry);
                } else {
                    snprintf(new_relative, sizeof(new_relative), "%s/%s", relative_path, entry);
                }
                scan_presets_recursive(base_path, new_relative, list, count, capacity);
            } else if (info.type == SDL_PATHTYPE_FILE) {
                // Check extension
                size_t len = strlen(entry);
                bool is_slangp = (len > 7 && strcmp(entry + len - 7, ".slangp") == 0);

                if (is_slangp) {
                    // Found a valid preset/shader
                    if (*count >= *capacity) {
                        *capacity *= 2;
                        if (*capacity == 0)
                            *capacity = 16;
                        *list = (char**)realloc(*list, *capacity * sizeof(char*));
                    }

                    char preset_rel_path[1024];
                    if (relative_path[0] == '\0') {
                        snprintf(preset_rel_path, sizeof(preset_rel_path), "%s", entry);
                    } else {
                        snprintf(preset_rel_path, sizeof(preset_rel_path), "%s/%s", relative_path, entry);
                    }

                    (*list)[*count] = SDL_strdup(preset_rel_path);
                    (*count)++;
                }
            }
        }
    }
    SDL_free(entries);
}

static int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static void load_preset_internal(int index) {
    SDL_Log("load_preset called with index %d", index);
    if (index < 0 || index >= available_preset_count) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid preset index: %d", index);
        return;
    }

    if (available_presets == NULL || available_presets[index] == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "available_presets is invalid");
        return;
    }

    SDL_Log("Loading preset name: %s", available_presets[index]);

    if (libretro_manager) {
        SDL_Log("Freeing existing manager...");

        // Wait for idle if on GPU backend to avoid destroying in-flight resources
        if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
            SDL_GPUDevice* device = SDLApp_GetGPUDevice();
            if (device)
                SDL_WaitForGPUIdle(device);

            // Release the intermediate texture (implemented in sdl_app.c)
            SDLApp_ClearLibrashaderIntermediate();
        }

        LibrashaderManager_Free(libretro_manager);
        libretro_manager = NULL;
        SDL_Log("Manager freed.");
    }

    if (!g_base_path) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "g_base_path is NULL");
        return;
    }

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s/%s", g_base_path, "shaders/libretro", available_presets[index]);

    // Normalize path separators
    for (int i = 0; full_path[i]; i++) {
        if (full_path[i] == '\\') {
            full_path[i] = '/';
        }
    }

    libretro_manager = LibrashaderManager_Init(full_path);

    if (!libretro_manager) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize librashader manager.");
        return;
    }

    // Save configuration
    Config_SetString(CFG_KEY_SHADER_PATH, available_presets[index]);
}

void SDLAppShader_Init(const char* base_path) {
    g_base_path = SDL_strdup(base_path); // Copy it to own it (or just refer if guaranteed lifetime)

    // Config_Init must be called before this, which is true in SDLApp_Init
    shader_mode_libretro = Config_GetBool(CFG_KEY_SHADER_MODE_LIBRETRO);

    char shaders_path[1024];
    snprintf(shaders_path, sizeof(shaders_path), "%s%s", g_base_path, "shaders/libretro");

    int capacity = 64;
    available_presets = (char**)malloc(capacity * sizeof(char*));
    available_preset_count = 0;

    scan_presets_recursive(shaders_path, "", &available_presets, &available_preset_count, &capacity);

    if (available_preset_count > 0) {
        qsort(available_presets, available_preset_count, sizeof(char*), compare_strings);
        SDL_Log("Found %d shader presets.", available_preset_count);
    }

    const char* saved_shader = Config_GetString(CFG_KEY_SHADER_PATH);
    if (saved_shader && *saved_shader && available_preset_count > 0) {
        for (int i = 0; i < available_preset_count; i++) {
            if (SDL_strcmp(available_presets[i], saved_shader) == 0) {
                current_preset_index = i;
                break;
            }
        }
    }

    if (shader_mode_libretro && available_preset_count > 0) {
        // Immediate load on init
        load_preset_internal(current_preset_index);
    }
}

void SDLAppShader_Shutdown() {
    if (libretro_manager) {
        LibrashaderManager_Free(libretro_manager);
        libretro_manager = NULL;
    }
    if (available_presets) {
        for (int i = 0; i < available_preset_count; i++) {
            SDL_free(available_presets[i]);
        }
        free(available_presets);
        available_presets = NULL;
    }
    if (g_base_path) {
        SDL_free(g_base_path);
        g_base_path = NULL;
    }
}

void SDLAppShader_ProcessPendingLoad() {
    if (s_pending_preset_index >= 0) {
        load_preset_internal(s_pending_preset_index);
        s_pending_preset_index = -1;
    }
}

LibrashaderManager* SDLAppShader_GetManager() {
    return libretro_manager;
}

bool SDLAppShader_IsLibretroMode() {
    return shader_mode_libretro;
}

void SDLAppShader_ToggleMode() {
    shader_mode_libretro = !shader_mode_libretro;
    Config_SetBool(CFG_KEY_SHADER_MODE_LIBRETRO, shader_mode_libretro);
    SDL_Log("Shader Mode: %s", shader_mode_libretro ? "Libretro" : "Internal");
    if (shader_mode_libretro && !libretro_manager && available_preset_count > 0) {
        s_pending_preset_index = current_preset_index;
    }
}

void SDLAppShader_CyclePreset() {
    if (available_preset_count == 0)
        return;
    current_preset_index = (current_preset_index + 1) % available_preset_count;
    s_pending_preset_index = current_preset_index;
}

void SDLAppShader_LoadPreset(int index) {
    s_pending_preset_index = index;
}

int SDLAppShader_GetAvailableCount() {
    return available_preset_count;
}

const char* SDLAppShader_GetPresetName(int index) {
    if (index >= 0 && index < available_preset_count && available_presets) {
        return available_presets[index];
    }
    return NULL;
}

int SDLAppShader_GetCurrentIndex() {
    return current_preset_index;
}

void SDLAppShader_SetCurrentIndex(int index) {
    if (index >= 0 && index < available_preset_count) {
        current_preset_index = index;
    }
}

void SDLAppShader_SetMode(bool libretro) {
    if (shader_mode_libretro != libretro) {
        SDLAppShader_ToggleMode(); // Reusing toggle logic which handles config save
    }
}
