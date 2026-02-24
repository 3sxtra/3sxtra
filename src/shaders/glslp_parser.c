#include "glslp_parser.h"
#include <SDL3/SDL.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

static char* trim_whitespace(char* str) {
    char* end;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    *(end + 1) = 0;
    return str;
}

static void get_parent_dir(const char* path, char* out_dir) {
    const char* last_slash = strrchr(path, '/');
    const char* last_backslash = strrchr(path, '\\');
    const char* last = NULL;

    if (last_slash && last_backslash) {
        last = (last_slash > last_backslash) ? last_slash : last_backslash;
    } else if (last_slash) {
        last = last_slash;
    } else {
        last = last_backslash;
    }

    if (last) {
        size_t len = last - path;
        if (len >= MAX_PATH)
            len = MAX_PATH - 1;
        strncpy(out_dir, path, len);
        out_dir[len] = '\0';
    } else {
        strncpy(out_dir, ".", MAX_PATH - 1);
        out_dir[MAX_PATH - 1] = '\0';
    }
}

static void resolve_path(char* out_path, const char* base_dir, const char* rel_path) {
    char temp_rel[MAX_PATH];
    strncpy(temp_rel, rel_path, MAX_PATH - 1);
    temp_rel[MAX_PATH - 1] = '\0';

    // Normalize slashes in relative path
    for (int i = 0; temp_rel[i]; i++) {
        if (temp_rel[i] == '/' || temp_rel[i] == '\\') {
            temp_rel[i] = PATH_SEPARATOR;
        }
    }

#ifdef _WIN32
    if ((isalpha(temp_rel[0]) && temp_rel[1] == ':') || temp_rel[0] == '\\') {
#else
    if (temp_rel[0] == '/') {
#endif
        strncpy(out_path, temp_rel, MAX_PATH - 1);
        out_path[MAX_PATH - 1] = '\0';
        return;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(out_path, MAX_PATH, "%s%c%s", base_dir, PATH_SEPARATOR, temp_rel);
#pragma GCC diagnostic pop
}

static GLSLP_ScaleType parse_scale_type(const char* value) {
    if (strcmp(value, "source") == 0)
        return GLSLP_SCALE_SOURCE;
    if (strcmp(value, "viewport") == 0)
        return GLSLP_SCALE_VIEWPORT;
    if (strcmp(value, "absolute") == 0)
        return GLSLP_SCALE_ABSOLUTE;
    return GLSLP_SCALE_SOURCE;
}

static bool parse_bool(const char* value) {
    return (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
}

static int find_parameter_index(GLSLP_Preset* preset, const char* name) {
    for (int i = 0; i < preset->parameter_count; i++) {
        if (strcmp(preset->parameters[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

GLSLP_Preset* GLSLP_Load(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "GLSLP_Load: Failed to open file '%s': %s", path, strerror(errno));
        return NULL;
    }

    GLSLP_Preset* preset = (GLSLP_Preset*)calloc(1, sizeof(GLSLP_Preset));
    if (!preset) {
        fclose(f);
        return NULL;
    }

    char base_dir[MAX_PATH];
    get_parent_dir(path, base_dir);

    char line[2048]; // Increased buffer size for long texture lists
    while (fgets(line, sizeof(line), f)) {
        char* trimmed = trim_whitespace(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0')
            continue;

        char* eq = strchr(trimmed, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char* key = trim_whitespace(trimmed);
        char* value = trim_whitespace(eq + 1);

        if (value[0] == '"') {
            value++;
            char* end_quote = strrchr(value, '"');
            if (end_quote)
                *end_quote = '\0';
        }

        if (strcmp(key, "shaders") == 0) {
            preset->pass_count = atoi(value);
            if (preset->pass_count > MAX_SHADERS)
                preset->pass_count = MAX_SHADERS;
            continue;
        }

        if (strcmp(key, "textures") == 0) {
            char* ctx = NULL;
            char* token = strtok_r(value, ";", &ctx);
            while (token) {
                char* tex_name = trim_whitespace(token);
                if (preset->texture_count < MAX_TEXTURES) {
                    strncpy(preset->textures[preset->texture_count].name, tex_name, 63);
                    preset->textures[preset->texture_count].linear =
                        true; // Default to linear usually? or nearest? Libretro defaults: linear=true, mipmap=false,
                              // wrap=clamp_to_edge
                    preset->textures[preset->texture_count].mipmap = false;
                    strncpy(preset->textures[preset->texture_count].wrap_mode, "clamp_to_edge", 31);
                    preset->texture_count++;
                }
                token = strtok_r(NULL, ";", &ctx);
            }
            continue;
        }

        // Try parsing as pass property
        bool is_pass_prop = false;
        int index = -1;
        char prop[64];

        size_t key_len = strlen(key);
        // Find the last digit sequence
        if (isdigit(key[key_len - 1])) {
            int digit_start = key_len - 1;
            while (digit_start > 0 && isdigit(key[digit_start - 1])) {
                digit_start--;
            }
            index = atoi(key + digit_start);
            if (digit_start < sizeof(prop)) {
                strncpy(prop, key, digit_start);
                prop[digit_start] = '\0';

                // List of known pass properties
                const char* known_props[] = {
                    "shader", "filter_linear", "scale_type", "scale_type_x",     "scale_type_y",
                    "scale",  "scale_x",       "scale_y",    "srgb_framebuffer", "float_framebuffer",
                    "alias",  "mipmap_input",  "wrap_mode",  "frame_count_mod"
                };

                for (size_t i = 0; i < sizeof(known_props) / sizeof(known_props[0]); i++) {
                    if (strcmp(prop, known_props[i]) == 0) {
                        is_pass_prop = true;
                        break;
                    }
                }
            }
        }

        if (is_pass_prop && index >= 0 && index < MAX_SHADERS) {
            GLSLP_ShaderPass* pass = &preset->passes[index];
            if (strcmp(prop, "shader") == 0)
                resolve_path(pass->path, base_dir, value);
            else if (strcmp(prop, "filter_linear") == 0)
                pass->filter_linear = parse_bool(value);
            else if (strcmp(prop, "scale_type") == 0) {
                pass->scale_type_x = parse_scale_type(value);
                pass->scale_type_y = pass->scale_type_x;
            } else if (strcmp(prop, "scale_type_x") == 0)
                pass->scale_type_x = parse_scale_type(value);
            else if (strcmp(prop, "scale_type_y") == 0)
                pass->scale_type_y = parse_scale_type(value);
            else if (strcmp(prop, "scale") == 0) {
                pass->scale_x = atof(value);
                pass->scale_y = pass->scale_x;
            } else if (strcmp(prop, "scale_x") == 0)
                pass->scale_x = atof(value);
            else if (strcmp(prop, "scale_y") == 0)
                pass->scale_y = atof(value);
            else if (strcmp(prop, "srgb_framebuffer") == 0)
                pass->srgb_framebuffer = parse_bool(value);
            else if (strcmp(prop, "float_framebuffer") == 0)
                pass->float_framebuffer = parse_bool(value);
            else if (strcmp(prop, "alias") == 0)
                strncpy(pass->alias, value, 63);
            else if (strcmp(prop, "mipmap_input") == 0)
                pass->mipmap_input = parse_bool(value);
            else if (strcmp(prop, "wrap_mode") == 0)
                strncpy(pass->wrap_mode, value, 31);
            else if (strcmp(prop, "frame_count_mod") == 0)
                pass->frame_count_mod = atoi(value);
            continue;
        }

        // Try parsing as texture property
        // Iterate over known textures to see if key starts with name + property
        bool is_texture_prop = false;
        for (int i = 0; i < preset->texture_count; i++) {
            char* tex_name = preset->textures[i].name;
            size_t name_len = strlen(tex_name);
            if (strncmp(key, tex_name, name_len) == 0) {
                // Exact match (path) or suffix match?
                // key could be "texname" = "path"
                // or "texname_linear" = "true"
                if (key[name_len] == '\0') {
                    // It is the path
                    resolve_path(preset->textures[i].path, base_dir, value);
                    is_texture_prop = true;
                    break;
                } else if (key[name_len] == '_') {
                    char* suffix = key + name_len + 1;
                    if (strcmp(suffix, "linear") == 0) {
                        preset->textures[i].linear = parse_bool(value);
                        is_texture_prop = true;
                        break;
                    } else if (strcmp(suffix, "mipmap") == 0) {
                        preset->textures[i].mipmap = parse_bool(value);
                        is_texture_prop = true;
                        break;
                    } else if (strcmp(suffix, "wrap_mode") == 0) {
                        strncpy(preset->textures[i].wrap_mode, value, 31);
                        is_texture_prop = true;
                        break;
                    }
                }
            }
        }
        if (is_texture_prop)
            continue;

        // If not pass prop and not texture prop, it's a parameter
        if (preset->parameter_count < MAX_PARAMETERS) {
            // Check if parameter already exists (update it), else add new
            int idx = find_parameter_index(preset, key);
            if (idx == -1) {
                idx = preset->parameter_count++;
                strncpy(preset->parameters[idx].name, key, 63);
            }
            preset->parameters[idx].value = atof(value);
        }
    }

    fclose(f);
    return preset;
}

void GLSLP_Free(GLSLP_Preset* preset) {
    free(preset);
}
