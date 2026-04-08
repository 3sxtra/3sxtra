/**
 * @file palmod_storage.c
 * @brief Custom palette save/load using cJSON and SDL filesystem.
 *
 * Directory structure under Paths_GetPrefPath():
 *   palettes/char/<char_name>/<palette_name>.json
 *   palettes/stage/row_<N>/<palette_name>.json
 */
#include "port/sdl/rmlui/palmod_storage.h"
#include "port/config/paths.h"

#include <SDL3/SDL.h>
#include <cJSON.h>
#include <stdio.h>
#include <string.h>

/* ── Path construction ────────────────────────────────────────────── */

static void build_dir(char* buf, size_t size, const char* category, const char* sub_name) {
    snprintf(buf, size, "%spalettes/%s/%s", Paths_GetPrefPath(), category, sub_name);
}

static void build_path(char* buf, size_t size, const char* category, const char* sub_name, const char* pal_name) {
    snprintf(buf, size, "%spalettes/%s/%s/%s.json", Paths_GetPrefPath(), category, sub_name, pal_name);
}

/* Ensure directory tree exists (SDL3 mkdir is recursive if you go level-by-level) */
static bool ensure_dir(const char* path) {
    /* Build path level by level */
    char tmp[512];
    size_t len = strlen(path);
    if (len >= sizeof(tmp))
        return false;
    memcpy(tmp, path, len + 1);

    for (size_t i = 1; i <= len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\' || tmp[i] == '\0') {
            char saved = tmp[i];
            tmp[i] = '\0';
            /* SDL_CreateDirectory returns true if created or already exists */
            SDL_CreateDirectory(tmp);
            tmp[i] = saved;
            if (saved == '\0')
                break;
        }
    }
    return true;
}

/* ── Save ─────────────────────────────────────────────────────────── */

bool palmod_save(const char* category, const char* sub_name, const char* pal_name, const u16* colors) {
    if (!category || !sub_name || !pal_name || !colors)
        return false;

    char dir[512];
    build_dir(dir, sizeof(dir), category, sub_name);
    if (!ensure_dir(dir)) {
        SDL_Log("[PalMod Storage] Failed to create dir: %s", dir);
        return false;
    }

    char filepath[512];
    build_path(filepath, sizeof(filepath), category, sub_name, pal_name);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", pal_name);
    cJSON_AddStringToObject(root, "format", "cps3_u16");

    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < PALMOD_COLORS_PER_ROW; i++) {
        cJSON_AddItemToArray(arr, cJSON_CreateNumber((double)colors[i]));
    }
    cJSON_AddItemToObject(root, "colors", arr);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json_str)
        return false;

    bool ok = SDL_SaveFile(filepath, json_str, strlen(json_str));
    cJSON_free(json_str);

    if (ok) {
        SDL_Log("[PalMod Storage] Saved: %s", filepath);
    } else {
        SDL_Log("[PalMod Storage] Failed to save: %s (%s)", filepath, SDL_GetError());
    }
    return ok;
}

/* ── Load ─────────────────────────────────────────────────────────── */

bool palmod_load(const char* category, const char* sub_name, const char* pal_name, u16* out_colors) {
    if (!category || !sub_name || !pal_name || !out_colors)
        return false;

    char filepath[512];
    build_path(filepath, sizeof(filepath), category, sub_name, pal_name);

    size_t file_size = 0;
    char* data = (char*)SDL_LoadFile(filepath, &file_size);
    if (!data) {
        SDL_Log("[PalMod Storage] Failed to load: %s", filepath);
        return false;
    }

    cJSON* root = cJSON_ParseWithLength(data, file_size);
    SDL_free(data);
    if (!root) {
        SDL_Log("[PalMod Storage] JSON parse failed: %s", filepath);
        return false;
    }

    const cJSON* arr = cJSON_GetObjectItemCaseSensitive(root, "colors");
    if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) != PALMOD_COLORS_PER_ROW) {
        SDL_Log("[PalMod Storage] Invalid colors array in: %s", filepath);
        cJSON_Delete(root);
        return false;
    }

    int idx = 0;
    const cJSON* item = NULL;
    cJSON_ArrayForEach(item, arr) {
        if (cJSON_IsNumber(item) && idx < PALMOD_COLORS_PER_ROW) {
            out_colors[idx] = (u16)item->valuedouble;
        }
        idx++;
    }

    cJSON_Delete(root);
    SDL_Log("[PalMod Storage] Loaded: %s", filepath);
    return true;
}

/* ── List ─────────────────────────────────────────────────────────── */

int palmod_list(const char* category, const char* sub_name, PalmodEntry* out_entries) {
    if (!category || !sub_name || !out_entries)
        return 0;

    char dir[512];
    build_dir(dir, sizeof(dir), category, sub_name);

    int count = 0;
    int num_items = 0;
    char** items = SDL_GlobDirectory(dir, "*.json", SDL_GLOB_CASEINSENSITIVE, &num_items);
    if (!items)
        return 0;

    for (int i = 0; i < num_items && count < PALMOD_MAX_PALETTES; i++) {
        const char* fname = items[i];
        /* Strip .json extension for the display name */
        size_t len = strlen(fname);
        if (len > 5 && strcmp(fname + len - 5, ".json") == 0) {
            size_t name_len = len - 5;
            if (name_len >= PALMOD_NAME_MAX)
                name_len = PALMOD_NAME_MAX - 1;
            memcpy(out_entries[count].name, fname, name_len);
            out_entries[count].name[name_len] = '\0';
            count++;
        }
    }

    SDL_free(items);
    return count;
}

/* ── Delete ───────────────────────────────────────────────────────── */

bool palmod_delete(const char* category, const char* sub_name, const char* pal_name) {
    if (!category || !sub_name || !pal_name)
        return false;

    char filepath[512];
    build_path(filepath, sizeof(filepath), category, sub_name, pal_name);

    if (SDL_RemovePath(filepath)) {
        SDL_Log("[PalMod Storage] Deleted: %s", filepath);
        return true;
    }
    SDL_Log("[PalMod Storage] Failed to delete: %s", filepath);
    return false;
}
