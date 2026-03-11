/**
 * @file sdl_app_shader_config.c
 * @brief Shader preset discovery, loading, and cycling.
 *
 * Manages librashader preset scanning, loading, and runtime switching.
 * Supports both built-in and libretro-format shader presets with
 * recursive directory scanning. Split from sdl_app.c for modularity.
 */
#include "port/sdl/app/sdl_app_shader_config.h"
#include "port/config/config.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/app/sdl_app_config.h"
#include "port/sdl/app/sdl_app_internal.h"
#include "shaders/glslp_parser.h"
#include "librashader.h"
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
static char* g_base_path = NULL;
static bool s_shader_initialized = false;

// ── Chain state ────────────────────────────────────────────────
static GLSLP_Preset s_chain_preset;  // The composed chain
static bool s_chain_active = false;  // True when chain is in use (vs single preset)
static bool s_chain_needs_apply = false;

// ── Standalone parameter cache (reads from preset file, no GL state needed) ──
static struct libra_preset_param_list_t s_param_cache = {0};
static bool s_param_cache_valid = false;

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
                        *list = (char**)SDL_realloc(*list, *capacity * sizeof(char*));
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
    // Defer heavy work (directory scanning, preset loading) to first ProcessPendingLoad.
    // Just capture the base path here so boot stays fast (~59ms saved).
    g_base_path = SDL_strdup(base_path);
    s_shader_initialized = false;
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
        SDL_free(available_presets);
        available_presets = NULL;
    }
    if (s_param_cache_valid) {
        libra_preset_free_runtime_params(s_param_cache);
        s_param_cache_valid = false;
        s_param_cache.parameters = NULL;
        s_param_cache.length = 0;
    }
    if (g_base_path) {
        SDL_free(g_base_path);
        g_base_path = NULL;
    }
}

static void ensure_shader_initialized(void) {
    if (s_shader_initialized || !g_base_path)
        return;
    s_shader_initialized = true;

    shader_mode_libretro = Config_GetBool(CFG_KEY_SHADER_MODE_LIBRETRO);

    char shaders_path[1024];
    snprintf(shaders_path, sizeof(shaders_path), "%s%s", g_base_path, "shaders/libretro");

    int capacity = 64;
    available_presets = (char**)SDL_malloc(capacity * sizeof(char*));
    available_preset_count = 0;

    SDL_Log("Scanning shader presets in: %s", shaders_path);
    scan_presets_recursive(shaders_path, "", &available_presets, &available_preset_count, &capacity);

    if (available_preset_count > 0) {
        qsort(available_presets, available_preset_count, sizeof(char*), compare_strings);
    }
    SDL_Log("Found %d shader presets.", available_preset_count);

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
        s_pending_preset_index = current_preset_index;
    }
}

void SDLAppShader_ProcessPendingLoad() {
    ensure_shader_initialized();
    if (s_pending_preset_index >= 0) {
        load_preset_internal(s_pending_preset_index);
        s_pending_preset_index = -1;
    }
    if (s_chain_needs_apply) {
        // Defer chain apply while the shader menu is visible — LibrashaderManager_Init
        // corrupts GL state that RmlUI's GL3 renderer depends on.  The chain data model
        // (pass list) updates immediately via the per-frame dirty check; the actual
        // librashader reload happens once the menu closes.
        extern bool rmlui_wrapper_is_document_visible(const char* name);
        if (!rmlui_wrapper_is_document_visible("shaders")) {
            SDLAppShader_ChainApply();
        }
    }
}

LibrashaderManager* SDLAppShader_GetManager() {
    return libretro_manager;
}

bool SDLAppShader_IsLibretroMode() {
    return shader_mode_libretro;
}

void SDLAppShader_ToggleMode() {
    ensure_shader_initialized();
    shader_mode_libretro = !shader_mode_libretro;
    Config_SetBool(CFG_KEY_SHADER_MODE_LIBRETRO, shader_mode_libretro);
    SDL_Log("Shader Mode: %s", shader_mode_libretro ? "Libretro" : "Internal");
    if (shader_mode_libretro && !libretro_manager && available_preset_count > 0) {
        s_pending_preset_index = current_preset_index;
    }
}

void SDLAppShader_CyclePreset() {
    ensure_shader_initialized();
    if (available_preset_count == 0)
        return;
    current_preset_index = (current_preset_index + 1) % available_preset_count;
    s_pending_preset_index = current_preset_index;
}

void SDLAppShader_LoadPreset(int index) {
    s_pending_preset_index = index;
}

int SDLAppShader_GetAvailableCount() {
    ensure_shader_initialized();
    return available_preset_count;
}

const char* SDLAppShader_GetPresetName(int index) {
    ensure_shader_initialized();
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

// ── Chain Management ──────────────────────────────────────────────

static void chain_load_and_merge(int preset_index, bool prepend) {
    ensure_shader_initialized();
    if (preset_index < 0 || preset_index >= available_preset_count)
        return;

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s/%s", g_base_path, "shaders/libretro", available_presets[preset_index]);
    for (int i = 0; full_path[i]; i++) {
        if (full_path[i] == '\\')
            full_path[i] = '/';
    }

    GLSLP_Preset* src = GLSLP_Load(full_path);
    if (!src) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ChainAppend: Failed to load preset '%s'", full_path);
        return;
    }

    if (prepend) {
        // Prepend: src goes first, current chain goes after
        GLSLP_Append(src, &s_chain_preset);
        s_chain_preset = *src;
    } else {
        // Append: current chain stays, src goes after
        GLSLP_Append(&s_chain_preset, src);
    }

    GLSLP_Free(src);
    s_chain_active = true;
    s_chain_needs_apply = true;

    // Refresh param cache immediately from the preset file
    // (ChainApply is deferred while menu is visible, but params must show now)
    {
        char temp_path[1024];
        if (g_base_path)
            snprintf(temp_path, sizeof(temp_path), "%s%s", g_base_path, "shaders/libretro/_3sx_chain.slangp");
        else
            snprintf(temp_path, sizeof(temp_path), "%s", "shaders/libretro/_3sx_chain.slangp");

        // Write the current chain composition so we can read its params
        if (GLSLP_Write(&s_chain_preset, temp_path)) {
            // Free old cache
            if (s_param_cache_valid) {
                libra_preset_free_runtime_params(s_param_cache);
                s_param_cache_valid = false;
            }

            // Create temp preset just to read params (no filter chain = no GL state)
            libra_shader_preset_t preset = {0};
            libra_error_t err = libra_preset_create_with_options(temp_path, NULL, NULL, &preset);
            if (err == 0) {
                err = libra_preset_get_runtime_params(&preset, &s_param_cache);
                if (err == 0) {
                    s_param_cache_valid = true;
                    SDL_Log("ParamCache: Refreshed %llu params from chain preset",
                            (unsigned long long)s_param_cache.length);
                } else {
                    libra_error_print(err);
                    s_param_cache.parameters = NULL;
                    s_param_cache.length = 0;
                }
                // preset is consumed/invalidated by get_runtime_params or we free it
                libra_preset_free(&preset);
            } else {
                libra_error_print(err);
            }
        }
    }
}

void SDLAppShader_ChainAppend(int preset_index) {
    chain_load_and_merge(preset_index, false);
}

void SDLAppShader_ChainPrepend(int preset_index) {
    chain_load_and_merge(preset_index, true);
}

void SDLAppShader_ChainRemovePass(int pass_index) {
    if (!s_chain_active)
        return;
    GLSLP_RemovePass(&s_chain_preset, pass_index);
    if (s_chain_preset.pass_count == 0) {
        s_chain_active = false;
    }
    s_chain_needs_apply = true;
}

void SDLAppShader_ChainMovePass(int from, int to) {
    if (!s_chain_active)
        return;
    GLSLP_MovePass(&s_chain_preset, from, to);
    s_chain_needs_apply = true;
}

void SDLAppShader_ChainClear(void) {
    memset(&s_chain_preset, 0, sizeof(GLSLP_Preset));
    s_chain_active = false;
    s_chain_needs_apply = true;

    // Clear param cache
    if (s_param_cache_valid) {
        libra_preset_free_runtime_params(s_param_cache);
        s_param_cache_valid = false;
        s_param_cache.parameters = NULL;
        s_param_cache.length = 0;
    }
}

void SDLAppShader_ChainApply(void) {
    if (!s_chain_active || s_chain_preset.pass_count == 0) {
        // Clear chain — unload any active manager
        if (libretro_manager) {
            if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
                SDL_GPUDevice* device = SDLApp_GetGPUDevice();
                if (device)
                    SDL_WaitForGPUIdle(device);
                SDLApp_ClearLibrashaderIntermediate();
            }
            LibrashaderManager_Free(libretro_manager);
            libretro_manager = NULL;
        }
        s_chain_needs_apply = false;
        return;
    }

    // Write the merged chain to a temp file
    char temp_path[1024];
    if (g_base_path) {
        snprintf(temp_path, sizeof(temp_path), "%s%s", g_base_path, "shaders/libretro/_3sx_chain.slangp");
    } else {
        snprintf(temp_path, sizeof(temp_path), "%s", "shaders/libretro/_3sx_chain.slangp");
    }

    if (!GLSLP_Write(&s_chain_preset, temp_path)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ChainApply: Failed to write merged preset");
        s_chain_needs_apply = false;
        return;
    }

    // Reload via librashader
    if (libretro_manager) {
        if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
            SDL_GPUDevice* device = SDLApp_GetGPUDevice();
            if (device)
                SDL_WaitForGPUIdle(device);
            SDLApp_ClearLibrashaderIntermediate();
        }
        LibrashaderManager_Free(libretro_manager);
        libretro_manager = NULL;
    }

    libretro_manager = LibrashaderManager_Init(temp_path);
    if (!libretro_manager) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ChainApply: Failed to init manager from merged preset");
    } else {
        SDL_Log("ChainApply: Loaded merged chain (%d passes)", s_chain_preset.pass_count);
    }

    s_chain_needs_apply = false;
}

int SDLAppShader_ChainGetPassCount(void) {
    return s_chain_active ? s_chain_preset.pass_count : 0;
}

const char* SDLAppShader_ChainGetPassShaderPath(int pass_index) {
    if (!s_chain_active || pass_index < 0 || pass_index >= s_chain_preset.pass_count)
        return NULL;
    return s_chain_preset.passes[pass_index].path;
}

const char* SDLAppShader_ChainGetPassSourcePreset(int pass_index) {
    if (!s_chain_active || pass_index < 0 || pass_index >= s_chain_preset.pass_count)
        return NULL;
    const char* src = s_chain_preset.passes[pass_index].source_preset;
    return (src[0] != '\0') ? src : NULL;
}

bool SDLAppShader_ChainSaveAsPreset(const char* path) {
    if (!s_chain_active || s_chain_preset.pass_count == 0)
        return false;
    return GLSLP_Write(&s_chain_preset, path);
}

// ── Runtime Parameter API (reads from standalone cache) ──────────

int SDLAppShader_GetParamCount(void) {
    if (!s_param_cache_valid)
        return 0;
    return (int)s_param_cache.length;
}

const char* SDLAppShader_GetParamName(int index) {
    if (!s_param_cache_valid || index < 0 || index >= (int)s_param_cache.length)
        return "";
    return s_param_cache.parameters[index].name;
}

const char* SDLAppShader_GetParamDesc(int index) {
    if (!s_param_cache_valid || index < 0 || index >= (int)s_param_cache.length)
        return "";
    const char* desc = s_param_cache.parameters[index].description;
    return desc ? desc : "";
}

float SDLAppShader_GetParamValue(int index) {
    if (!s_param_cache_valid || index < 0 || index >= (int)s_param_cache.length)
        return 0.0f;
    // Try to get live value from the active manager first
    if (libretro_manager) {
        float val = 0;
        if (LibrashaderManager_GetParam(libretro_manager, s_param_cache.parameters[index].name, &val))
            return val;
    }
    // Fall back to initial value from the cached preset metadata
    return s_param_cache.parameters[index].initial;
}

float SDLAppShader_GetParamInitial(int index) {
    if (!s_param_cache_valid || index < 0 || index >= (int)s_param_cache.length)
        return 0.0f;
    return s_param_cache.parameters[index].initial;
}

float SDLAppShader_GetParamMin(int index) {
    if (!s_param_cache_valid || index < 0 || index >= (int)s_param_cache.length)
        return 0.0f;
    return s_param_cache.parameters[index].minimum;
}

float SDLAppShader_GetParamMax(int index) {
    if (!s_param_cache_valid || index < 0 || index >= (int)s_param_cache.length)
        return 1.0f;
    return s_param_cache.parameters[index].maximum;
}

float SDLAppShader_GetParamStep(int index) {
    if (!s_param_cache_valid || index < 0 || index >= (int)s_param_cache.length)
        return 0.1f;
    return s_param_cache.parameters[index].step;
}

void SDLAppShader_SetParamValue(int index, float value) {
    if (!s_param_cache_valid || index < 0 || index >= (int)s_param_cache.length)
        return;
    const char* name = s_param_cache.parameters[index].name;
    if (!name)
        return;
    // Apply to live filter chain if available
    if (libretro_manager)
        LibrashaderManager_SetParam(libretro_manager, name, value);
}

void SDLAppShader_ResetParam(int index) {
    if (!s_param_cache_valid || index < 0 || index >= (int)s_param_cache.length)
        return;
    SDLAppShader_SetParamValue(index, s_param_cache.parameters[index].initial);
}

