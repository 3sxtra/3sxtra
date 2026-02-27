/**
 * @file config.c
 * @brief INI-style configuration system for application settings.
 *
 * Manages typed config entries (bool, int, string) with file persistence,
 * default values, and a printf/scanf-based serialization format. Entries
 * are stored in a flat array searched linearly by key.
 */
#include "port/config.h"
#include "port/broadcast.h"
#include "port/paths.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <SDL3/SDL.h>

#define CONFIG_ENTRIES_MAX 128

typedef enum ConfigType {
    CFG_BOOL,
    CFG_INT,
    CFG_STRING,
} ConfigType;

typedef union ConfigValue {
    bool b;
    int i;
    char* s;
} ConfigValue;

typedef struct ConfigEntry {
    const char* key;
    ConfigType type;
    ConfigValue value;
} ConfigEntry;

static const ConfigEntry default_entries[] = {
    { .key = CFG_KEY_FULLSCREEN, .type = CFG_BOOL, .value.b = true },
    { .key = CFG_KEY_FULLSCREEN_WIDTH, .type = CFG_INT, .value.i = 0 },
    { .key = CFG_KEY_FULLSCREEN_HEIGHT, .type = CFG_INT, .value.i = 0 },
    { .key = CFG_KEY_WINDOW_WIDTH, .type = CFG_INT, .value.i = 640 },
    { .key = CFG_KEY_WINDOW_HEIGHT, .type = CFG_INT, .value.i = 480 },
    { .key = CFG_KEY_SCALEMODE, .type = CFG_STRING, .value.s = "soft-linear" },
    { .key = CFG_KEY_DRAW_RECT_BORDERS, .type = CFG_BOOL, .value.b = false },
    { .key = CFG_KEY_DUMP_TEXTURES, .type = CFG_BOOL, .value.b = false },
    { .key = CFG_KEY_SHADER_PATH, .type = CFG_STRING, .value.s = "" },
    { .key = CFG_KEY_BROADCAST_ENABLED, .type = CFG_BOOL, .value.b = false },
    { .key = CFG_KEY_BROADCAST_SOURCE, .type = CFG_INT, .value.i = 0 },
    { .key = CFG_KEY_BROADCAST_SHOW_UI, .type = CFG_BOOL, .value.b = false },
    { .key = CFG_KEY_TRAINING_HITBOXES, .type = CFG_BOOL, .value.b = true },
    { .key = CFG_KEY_TRAINING_PUSHBOXES, .type = CFG_BOOL, .value.b = true },
    { .key = CFG_KEY_TRAINING_HURTBOXES, .type = CFG_BOOL, .value.b = true },
    { .key = CFG_KEY_TRAINING_ATTACKBOXES, .type = CFG_BOOL, .value.b = true },
    { .key = CFG_KEY_TRAINING_THROWBOXES, .type = CFG_BOOL, .value.b = true },
    { .key = CFG_KEY_TRAINING_ADVANTAGE, .type = CFG_BOOL, .value.b = false },
    { .key = CFG_KEY_TRAINING_STUN, .type = CFG_BOOL, .value.b = true },
    { .key = CFG_KEY_TRAINING_INPUTS, .type = CFG_BOOL, .value.b = true },
    { .key = CFG_KEY_TRAINING_FRAME_METER, .type = CFG_BOOL, .value.b = true },
    { .key = CFG_KEY_NETPLAY_AUTO_CONNECT, .type = CFG_BOOL, .value.b = true },
    { .key = CFG_KEY_LOBBY_AUTO_CONNECT, .type = CFG_BOOL, .value.b = true },
    { .key = CFG_KEY_LOBBY_AUTO_SEARCH, .type = CFG_BOOL, .value.b = true },
};

static ConfigEntry entries[CONFIG_ENTRIES_MAX] = { 0 };
static int entry_count = 0;

/** @brief Strip leading and trailing whitespace from a string in-place. */
static void trim(char* str) {
    char* p = str;

    // Trim leading
    while (SDL_isspace((unsigned char)*p)) {
        p++;
    }

    if (p != str) {
        SDL_memmove(str, p, SDL_strlen(p) + 1);
    }

    // Trim trailing
    char* end = str + SDL_strlen(str);

    while ((end > str) && SDL_isspace((unsigned char)end[-1])) {
        end--;
        *end = '\0';
    }
}

/** @brief Return true if the string is a valid integer (optional leading minus). */
static bool is_int(const char* str) {
    for (int i = 0; i < SDL_strlen(str); i++) {
        if (SDL_isdigit(str[i]) || ((i == 0) && (str[i] == '-'))) {
            continue;
        } else {
            return false;
        }
    }

    return true;
}

/** @brief Linear search for a config entry by key in a given array. */
static const ConfigEntry* find_entry_in_array(const char* key, const ConfigEntry* array, size_t size) {
    for (int i = 0; i < size; i++) {
        const ConfigEntry* entry = &array[i];

        if (SDL_strcmp(key, entry->key) == 0) {
            return entry;
        }
    }

    return NULL;
}

/** @brief Find a config entry by key, searching overrides first then defaults. */
static const ConfigEntry* find_entry(const char* key) {
    const ConfigEntry* default_entry = find_entry_in_array(key, default_entries, SDL_arraysize(default_entries));
    const ConfigEntry* read_entry = find_entry_in_array(key, entries, entry_count);

    if (read_entry != NULL) {
        if (default_entry != NULL && read_entry->type != default_entry->type) {
            // If we expect a certain type and the one we read from config is unexpected, let's use the default entry
            // instead
            return default_entry;
        } else {
            return read_entry;
        }
    } else if (default_entry != NULL) {
        return default_entry;
    } else {
        // SDL_assert(false);
        return NULL;
    }
}

/** @brief Find a mutable config entry by key (overrides only). */
static ConfigEntry* find_entry_mutable(const char* key) {
    for (int i = 0; i < entry_count; i++) {
        if (SDL_strcmp(key, entries[i].key) == 0) {
            return &entries[i];
        }
    }
    return NULL;
}

/** @brief Add a new config entry or update an existing one's value. */
static void add_or_update_entry(const char* key, ConfigType type, ConfigValue value) {
    ConfigEntry* entry = find_entry_mutable(key);
    if (entry) {
        // Update
        if (entry->type == CFG_STRING)
            SDL_free(entry->value.s);
        entry->type = type;
        if (type == CFG_STRING)
            entry->value.s = SDL_strdup(value.s);
        else
            entry->value = value;
    } else {
        // Add
        if (entry_count < CONFIG_ENTRIES_MAX) {
            entries[entry_count].key = SDL_strdup(key);
            entries[entry_count].type = type;
            if (type == CFG_STRING)
                entries[entry_count].value.s = SDL_strdup(value.s);
            else
                entries[entry_count].value = value;
            entry_count++;
        }
    }
}

/** @brief Writes a string to an SDL_IOStream. */
static void print_io(SDL_IOStream* io, const char* string) {
    SDL_WriteIO(io, string, SDL_strlen(string));
}

/** @brief Prints a config entry to an SDL_IOStream. */
static void print_config_entry_to_io(SDL_IOStream* io, const ConfigEntry* entry) {
    print_io(io, entry->key);
    print_io(io, " = ");

    switch (entry->type) {
    case CFG_BOOL:
        print_io(io, entry->value.b ? "true" : "false");
        break;

    case CFG_INT: {
        char str[32];
        SDL_itoa(entry->value.i, str, 10);
        print_io(io, str);
        break;
    }

    case CFG_STRING:
        print_io(io, entry->value.s);
        break;
    }
}

/** @brief Dumps default config entries to a file. */
static void dump_defaults(const char* dst_path) {
    SDL_IOStream* io = SDL_IOFromFile(dst_path, "w");
    if (!io)
        return;

    for (int i = 0; i < SDL_arraysize(default_entries); i++) {
        print_config_entry_to_io(io, &default_entries[i]);
        print_io(io, "\n");
    }

    print_io(io,
             "\n# To use a custom matchmaking server instead of the default Oracle VPS, uncomment and edit these:\n");
    print_io(io, "# lobby-server-url=http://your-server-ip:3000\n");
    print_io(io, "# lobby-server-key=your-secret-hmac-key\n");
    print_io(io, "\n# Set your online display name (shown to other players in the lobby):\n");
    print_io(io, "# lobby-display-name=YourName\n");

    SDL_CloseIO(io);
}

/** @brief Initializes the config system by loading settings from a file or creating defaults. */
void Config_Init() {
    const char* pref_path = Paths_GetPrefPath();
    char* config_path;
    SDL_asprintf(&config_path, "%sconfig", pref_path);

    SDL_Log("Config_Init: Loading config from %s", config_path);

    FILE* f = fopen(config_path, "r");

    if (f == NULL) {
        SDL_Log("Config_Init: File not found, creating defaults.");
        // Config doesn't exist. Dump defaults
        dump_defaults(config_path);
        // Re-open
        f = fopen(config_path, "r");
        if (f == NULL) {
            SDL_free(config_path);
            return;
        }
    }

    SDL_free(config_path);

    char line[256];

    while (fgets(line, sizeof(line), f)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = '\0';

        char* p = line;

        // Skip leading whitespace
        while (SDL_isspace((unsigned char)*p)) {
            p++;
        }

        // Skip empty/comment lines
        if (*p == '\0' || *p == '#') {
            continue;
        }

        char key[128];
        char value[128];

        if (sscanf(p, "%127[^=]=%127s", key, value) != 2) {
            continue;
        }

        trim(key);
        trim(value);

        if (SDL_strlen(key) == 0 || SDL_strlen(value) == 0) {
            continue;
        }

        if (entry_count == CONFIG_ENTRIES_MAX) {
            printf("⚠️ Reached max config entry count (%d), skipping the rest\n", CONFIG_ENTRIES_MAX);
            break;
        }

        ConfigEntry* entry = &entries[entry_count];
        entry->key = SDL_strdup(key);

        const bool is_true = SDL_strcmp(value, "true") == 0;
        const bool is_false = SDL_strcmp(value, "false") == 0;

        if (is_true || is_false) {
            entry->type = CFG_BOOL;
            entry->value.b = is_true;
        } else if (is_int(value)) {
            entry->type = CFG_INT;
            entry->value.i = SDL_atoi(value);
        } else {
            entry->type = CFG_STRING;
            entry->value.s = SDL_strdup(value);
        }

        entry_count += 1;
    }
    fclose(f);

    // Generate a unique client ID if one doesn't exist
    if (!Config_HasKey(CFG_KEY_LOBBY_CLIENT_ID) || SDL_strlen(Config_GetString(CFG_KEY_LOBBY_CLIENT_ID)) == 0) {
        srand((unsigned int)time(NULL) ^ (unsigned int)SDL_GetTicks());
        unsigned int r1 = (unsigned int)rand() ^ (unsigned int)SDL_GetTicks();
        unsigned int r2 = (unsigned int)rand() ^ (unsigned int)time(NULL);
        unsigned int r3 = (unsigned int)rand() ^ (unsigned int)SDL_GetPerformanceCounter();
        unsigned int r4 = (unsigned int)rand();
        char new_id[33];
        snprintf(new_id, sizeof(new_id), "%08x%08x%08x%08x", r1, r2, r3, r4);
        Config_SetString(CFG_KEY_LOBBY_CLIENT_ID, new_id);
        Config_Save();
    }
}

/** @brief Destroys the config system, freeing all allocated memory. */
void Config_Destroy() {
    for (int i = 0; i < entry_count; i++) {
        ConfigEntry* entry = &entries[i];
        SDL_free((void*)entry->key);

        if (entry->type == CFG_STRING) {
            SDL_free(entry->value.s);
        }
    }

    SDL_zeroa(entries);
    entry_count = 0;
}

/** @brief Saves the current config settings to a file. */
void Config_Save() {
    const char* pref_path = Paths_GetPrefPath();
    char* config_path;
    SDL_asprintf(&config_path, "%sconfig", pref_path);

    SDL_Log("Config_Save: Saving config to %s", config_path);

    SDL_IOStream* io = SDL_IOFromFile(config_path, "w");
    if (!io) {
        SDL_free(config_path);
        return;
    }

    for (int i = 0; i < entry_count; i++) {
        print_config_entry_to_io(io, &entries[i]);
        print_io(io, "\n");
    }

    print_io(io,
             "\n# To use a custom matchmaking server instead of the default Oracle VPS, uncomment and edit these:\n");
    print_io(io, "# lobby-server-url=http://your-server-ip:3000\n");
    print_io(io, "# lobby-server-key=your-secret-hmac-key\n");
    print_io(io, "\n# Set your online display name (shown to other players in the lobby):\n");
    print_io(io, "# lobby-display-name=YourName\n");

    SDL_CloseIO(io);
    SDL_free(config_path);
}

/** @brief Checks if a config key exists. */
bool Config_HasKey(const char* key) {
    return find_entry(key) != NULL;
}

/** @brief Get a boolean config value ("true"/"1" → true). */
bool Config_GetBool(const char* key) {
    const ConfigEntry* entry = find_entry(key);

    if (entry == NULL || entry->type != CFG_BOOL) {
        return false;
    }

    return entry->value.b;
}

/** @brief Get an integer config value (returns 0 if key not found). */
int Config_GetInt(const char* key) {
    const ConfigEntry* entry = find_entry(key);

    if (entry == NULL || entry->type != CFG_INT) {
        return 0;
    }

    return entry->value.i;
}

/** @brief Get a string config value (returns NULL if key not found). */
const char* Config_GetString(const char* key) {
    const ConfigEntry* entry = find_entry(key);

    if (entry == NULL || entry->type != CFG_STRING) {
        return NULL;
    }

    return entry->value.s;
}

/** @brief Set a boolean config value. */
void Config_SetBool(const char* key, bool value) {
    ConfigValue v;
    v.b = value;
    add_or_update_entry(key, CFG_BOOL, v);
}

/** @brief Set an integer config value. */
void Config_SetInt(const char* key, int value) {
    ConfigValue v;
    v.i = value;
    add_or_update_entry(key, CFG_INT, v);
}

/** @brief Set a string config value. */
void Config_SetString(const char* key, const char* value) {
    ConfigValue v;
    v.s = (char*)value;
    add_or_update_entry(key, CFG_STRING, v);
}
