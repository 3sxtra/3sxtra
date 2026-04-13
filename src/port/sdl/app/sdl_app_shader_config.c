/**
 * @file sdl_app_shader_config.c
 * @brief Shader preset discovery, loading, and cycling.
 *
 * Manages librashader preset scanning, loading, and runtime switching.
 * Supports both built-in and libretro-format shader presets with
 * recursive directory scanning. Split from sdl_app.c for modularity.
 */
#include "port/sdl/app/sdl_app_shader_config.h"
#include "librashader.h"
#include "port/config/config.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/app/sdl_app_config.h"
#include "port/sdl/app/sdl_app_internal.h"
#include "shaders/glslp_parser.h"
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
static GLSLP_Preset s_chain_preset; // The composed chain
static bool s_chain_active = false; // True when chain is in use (vs single preset)
static bool s_chain_needs_apply = false;

// ── Standalone parameter cache (reads from preset file, no GL state needed) ──
static struct libra_preset_param_list_t s_param_cache = { 0 };
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

#ifdef __ANDROID__
#include "port/config/paths.h"

// Extract a single APK asset to the filesystem.
// Returns true if the file already exists or was successfully extracted.
static bool extract_asset_to_filesystem(const char* asset_rel_path, const char* dest_dir) {
    char dest_path[1024];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, asset_rel_path);

    // Already extracted?
    SDL_PathInfo info;
    if (SDL_GetPathInfo(dest_path, &info))
        return true;

    // Ensure parent directories exist
    char dir_buf[1024];
    SDL_strlcpy(dir_buf, dest_path, sizeof(dir_buf));
    char* last_slash = SDL_strrchr(dir_buf, '/');
    if (last_slash) {
        *last_slash = '\0';
        SDL_CreateDirectory(dir_buf);
    }

    // Read from APK asset (AssetManager path = without "assets/" prefix)
    char asset_path[1024];
    snprintf(asset_path, sizeof(asset_path), "shaders/libretro/%s", asset_rel_path);
    SDL_IOStream* src = SDL_IOFromFile(asset_path, "rb");
    if (!src)
        return false;

    Sint64 size = SDL_GetIOSize(src);
    if (size <= 0) {
        SDL_CloseIO(src);
        return false;
    }

    void* data = SDL_malloc(size);
    SDL_ReadIO(src, data, size);
    SDL_CloseIO(src);

    SDL_IOStream* dst = SDL_IOFromFile(dest_path, "wb");
    if (!dst) {
        SDL_free(data);
        return false;
    }
    SDL_WriteIO(dst, data, size);
    SDL_CloseIO(dst);
    SDL_free(data);
    return true;
}

// Extract all shader files referenced by a preset's directory tree.
// The .slangp itself + all .slang, .inc, .h, .glsl files in its folder and subfolders.
static void extract_shader_preset_tree(const char* preset_rel, const char* dest_dir) {
    // Extract the preset file itself
    extract_asset_to_filesystem(preset_rel, dest_dir);

    // Determine the preset's parent directory
    char parent_dir[512] = "";
    SDL_strlcpy(parent_dir, preset_rel, sizeof(parent_dir));
    char* last_slash = SDL_strrchr(parent_dir, '/');
    if (last_slash)
        *last_slash = '\0';
    else
        parent_dir[0] = '\0';

    // Scan the manifest and extract all shader source files in this directory or subdirs
    // Re-read manifest to find related files
    SDL_IOStream* mio = SDL_IOFromFile("shaders/libretro/shader_manifest.txt", "rb");
    if (!mio)
        return;
    Sint64 msize = SDL_GetIOSize(mio);
    if (msize <= 0) {
        SDL_CloseIO(mio);
        return;
    }
    char* mdata = (char*)SDL_malloc(msize + 1);
    SDL_ReadIO(mio, mdata, msize);
    mdata[msize] = '\0';
    SDL_CloseIO(mio);

    // The manifest only has .slangp files. But .slang files aren't listed.
    // Extract ALL assets that share the same parent directory by probing common patterns.
    // Actually, let's just extract all .slangp files from the same directory so the chain works.
    char* line = strtok(mdata, "\r\n");
    while (line) {
        if (line[0] != '\0' && parent_dir[0] != '\0') {
            if (strncmp(line, parent_dir, strlen(parent_dir)) == 0) {
                extract_asset_to_filesystem(line, dest_dir);
            }
        }
        line = strtok(NULL, "\r\n");
    }
    SDL_free(mdata);

    // Now extract all the .slang source files referenced by this preset.
    // Parse the .slangp to find shader references and extract them.
    char preset_fs_path[1024];
    snprintf(preset_fs_path, sizeof(preset_fs_path), "%s/%s", dest_dir, preset_rel);
    SDL_IOStream* pio = SDL_IOFromFile(preset_fs_path, "rb");
    if (!pio)
        return;
    Sint64 psize = SDL_GetIOSize(pio);
    if (psize <= 0) {
        SDL_CloseIO(pio);
        return;
    }
    char* pdata = (char*)SDL_malloc(psize + 1);
    SDL_ReadIO(pio, pdata, psize);
    pdata[psize] = '\0';
    SDL_CloseIO(pio);

    // Find all shaderN = "path" references
    char* search = pdata;
    while ((search = strstr(search, "shader")) != NULL) {
        // Find the = sign
        char* eq = strchr(search, '=');
        if (!eq) {
            search++;
            continue;
        }
        // Skip whitespace after =
        char* val = eq + 1;
        while (*val == ' ' || *val == '\t')
            val++;
        // Strip quotes
        if (*val == '"')
            val++;
        char ref_path[512];
        int ri = 0;
        while (*val && *val != '"' && *val != '\n' && *val != '\r' && ri < 510)
            ref_path[ri++] = *val++;
        ref_path[ri] = '\0';
        // Trim trailing spaces
        while (ri > 0 && (ref_path[ri - 1] == ' ' || ref_path[ri - 1] == '\t'))
            ref_path[--ri] = '\0';

        if (ri > 0 && (strstr(ref_path, ".slang") || strstr(ref_path, ".glsl") || strstr(ref_path, ".inc") ||
                       strstr(ref_path, ".h"))) {
            // Resolve relative to preset directory
            char full_ref[1024];
            if (parent_dir[0])
                snprintf(full_ref, sizeof(full_ref), "%s/%s", parent_dir, ref_path);
            else
                SDL_strlcpy(full_ref, ref_path, sizeof(full_ref));
            extract_asset_to_filesystem(full_ref, dest_dir);
        }
        search = eq + 1;
    }
    SDL_free(pdata);
}

// Get the filesystem-backed shader directory
static const char* get_shader_fs_dir(void) {
    static char shader_dir[1024] = { 0 };
    if (shader_dir[0] == '\0') {
        const char* pref = Paths_GetPrefPath();
        snprintf(shader_dir, sizeof(shader_dir), "%sshaders/libretro", pref);
    }
    return shader_dir;
}
#endif // __ANDROID__

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
#ifdef __ANDROID__
    // On Android, extract shader files from APK assets to filesystem
    // because librashader uses standard file I/O (can't read APK assets).
    const char* fs_dir = get_shader_fs_dir();
    SDL_Log("Extracting shader preset to filesystem: %s -> %s", available_presets[index], fs_dir);
    extract_shader_preset_tree(available_presets[index], fs_dir);
    snprintf(full_path, sizeof(full_path), "%s/%s", fs_dir, available_presets[index]);
#else
    snprintf(full_path, sizeof(full_path), "%s%s/%s", g_base_path, "assets/shaders/libretro", available_presets[index]);
#endif

    // Normalize path separators
    for (int i = 0; full_path[i]; i++) {
        if (full_path[i] == '\\') {
            full_path[i] = '/';
        }
    }

    // Populate chain state so UI reflects the loaded chain on boot
    if (SDL_strcmp(available_presets[index], "_3sx_chain.slangp") == 0) {
        GLSLP_Preset* src = GLSLP_Load(full_path);
        if (src) {
            s_chain_preset = *src;
            s_chain_active = true;
            GLSLP_Free(src);
        } else {
            memset(&s_chain_preset, 0, sizeof(GLSLP_Preset));
            s_chain_active = false;
        }
    } else {
        memset(&s_chain_preset, 0, sizeof(GLSLP_Preset));
        s_chain_active = false;
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
    snprintf(shaders_path, sizeof(shaders_path), "%s%s", g_base_path, "assets/shaders/libretro");

#ifdef __ANDROID__
    // On Android, APK assets can't be enumerated with SDL_GlobDirectory.
    // Read a pre-generated manifest file listing all .slangp preset paths.
    {
        char manifest_path[1024];
        snprintf(manifest_path, sizeof(manifest_path), "%s/shader_manifest.txt", shaders_path);
        // On Android, SDL_IOFromFile routes through AssetManager which uses
        // paths relative to the assets/ root — strip the prefix.
        const char* manifest_open_path = manifest_path;
#ifdef __ANDROID__
        if (strncmp(manifest_open_path, "assets/", 7) == 0)
            manifest_open_path += 7;
#endif
        SDL_IOStream* io = SDL_IOFromFile(manifest_open_path, "rb");
        if (io) {
            Sint64 size = SDL_GetIOSize(io);
            if (size > 0) {
                char* data = (char*)SDL_malloc(size + 1);
                SDL_ReadIO(io, data, size);
                data[size] = '\0';
                SDL_CloseIO(io);

                int capacity = 64;
                available_presets = (char**)SDL_malloc(capacity * sizeof(char*));
                available_preset_count = 0;

                char* line = strtok(data, "\r\n");
                while (line) {
                    if (line[0] != '\0') {
                        if (available_preset_count >= capacity) {
                            capacity *= 2;
                            available_presets = (char**)SDL_realloc(available_presets, capacity * sizeof(char*));
                        }
                        available_presets[available_preset_count] = SDL_strdup(line);
                        available_preset_count++;
                    }
                    line = strtok(NULL, "\r\n");
                }
                SDL_free(data);
            } else {
                SDL_CloseIO(io);
            }
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader manifest not found: %s", manifest_path);
        }
    }
#else
    int capacity = 64;
    available_presets = (char**)SDL_malloc(capacity * sizeof(char*));
    available_preset_count = 0;

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Scanning shader presets in: %s", shaders_path);
    scan_presets_recursive(shaders_path, "", &available_presets, &available_preset_count, &capacity);
#endif

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
#ifdef __ANDROID__
    // On Android, extract shader files from APK assets to filesystem
    // because GLSLP_Load uses standard file I/O (can't read APK assets).
    const char* fs_dir = get_shader_fs_dir();
    extract_shader_preset_tree(available_presets[preset_index], fs_dir);
    snprintf(full_path, sizeof(full_path), "%s/%s", fs_dir, available_presets[preset_index]);
#else
    snprintf(full_path,
             sizeof(full_path),
             "%s%s/%s",
             g_base_path,
             "assets/shaders/libretro",
             available_presets[preset_index]);
#endif
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
#ifdef __ANDROID__
        snprintf(temp_path, sizeof(temp_path), "%s/_3sx_chain.slangp", get_shader_fs_dir());
#else
        if (g_base_path)
            snprintf(temp_path, sizeof(temp_path), "%s%s", g_base_path, "assets/shaders/libretro/_3sx_chain.slangp");
        else
            snprintf(temp_path, sizeof(temp_path), "%s", "assets/shaders/libretro/_3sx_chain.slangp");
#endif

        // Write the current chain composition so we can read its params
        if (GLSLP_Write(&s_chain_preset, temp_path)) {
            // Free old cache
            if (s_param_cache_valid) {
                libra_preset_free_runtime_params(s_param_cache);
                s_param_cache_valid = false;
            }

            // Create temp preset just to read params (no filter chain = no GL state)
            libra_shader_preset_t preset = { 0 };
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
        Config_SetString(CFG_KEY_SHADER_PATH, "");
        return;
    }

    // Write the merged chain to a temp file
    char temp_path[1024];
#ifdef __ANDROID__
    snprintf(temp_path, sizeof(temp_path), "%s/_3sx_chain.slangp", get_shader_fs_dir());
#else
    if (g_base_path) {
        snprintf(temp_path, sizeof(temp_path), "%s%s", g_base_path, "assets/shaders/libretro/_3sx_chain.slangp");
    } else {
        snprintf(temp_path, sizeof(temp_path), "%s", "assets/shaders/libretro/_3sx_chain.slangp");
    }
#endif

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
        Config_SetString(CFG_KEY_SHADER_PATH, "_3sx_chain.slangp");
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
