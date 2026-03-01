#include "shader_manager.h"
#include <SDL3/SDL.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

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
        if (len >= 1024)
            len = 1023;
        strncpy(out_dir, path, len);
        out_dir[len] = '\0';
    } else {
        strcpy(out_dir, ".");
    }
}

static void resolve_path(char* out_path, const char* base_dir, const char* rel_path) {
    char temp_rel[1024];
    strncpy(temp_rel, rel_path, 1023);
    temp_rel[1023] = '\0';

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
        strncpy(out_path, temp_rel, 1024 - 1);
        out_path[1024 - 1] = '\0';
        return;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(out_path, 1024, "%s%c%s", base_dir, PATH_SEPARATOR, temp_rel);
#pragma GCC diagnostic pop
}

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} StringBuilder;

static void sb_init(StringBuilder* sb) {
    sb->cap = 4096;
    sb->len = 0;
    sb->data = (char*)malloc(sb->cap);
    sb->data[0] = '\0';
}

static void sb_append(StringBuilder* sb, const char* str) {
    size_t l = strlen(str);
    if (sb->len + l + 1 >= sb->cap) {
        while (sb->len + l + 1 >= sb->cap)
            sb->cap *= 2;
        sb->data = (char*)realloc(sb->data, sb->cap);
    }
    strcpy(sb->data + sb->len, str);
    sb->len += l;
}

// Helper to find parameter index
static int find_parameter_index(ShaderManager* manager, const char* name) {
    for (int i = 0; i < manager->parameter_count; i++) {
        if (strcmp(manager->parameters[i].name, name) == 0)
            return i;
    }
    return -1;
}

// Helper to scan shader for parameters and directives
static void scan_shader_info(ShaderManager* manager, GLSLP_ShaderPass* pass, const char* source) {
    const char* ptr = source;
    while ((ptr = strstr(ptr, "#pragma"))) {
        char line[512];
        const char* end = strchr(ptr, '\n');
        if (!end)
            end = ptr + strlen(ptr);
        size_t len = end - ptr;
        if (len >= sizeof(line))
            len = sizeof(line) - 1;
        strncpy(line, ptr, len);
        line[len] = '\0';

        if (strncmp(line, "#pragma parameter", 17) == 0) {
            char name[64];
            float default_val;

            char* p = line + 17;
            while (*p && isspace((unsigned char)*p))
                p++;

            char* name_start = p;
            while (*p && !isspace((unsigned char)*p))
                p++;
            if (p == name_start) {
                ptr = end;
                continue;
            }

            size_t name_len = p - name_start;
            if (name_len >= 64)
                name_len = 63;
            strncpy(name, name_start, name_len);
            name[name_len] = '\0';

            while (*p && isspace((unsigned char)*p))
                p++;

            if (*p == '"') {
                p++;
                while (*p && *p != '"')
                    p++;
                if (*p == '"')
                    p++;
            }

            while (*p && isspace((unsigned char)*p))
                p++;

            default_val = (float)atof(p);

            if (find_parameter_index(manager, name) == -1) {
                if (manager->parameter_count < MAX_PARAMETERS) {
                    strncpy(manager->parameters[manager->parameter_count].name, name, 63);
                    manager->parameters[manager->parameter_count].value = default_val;
                    manager->parameter_count++;
                    SDL_Log("Parsed parameter: %s = %f", name, default_val);
                }
            }
        } else if (strncmp(line, "#pragma format", 14) == 0) {
            if (strstr(line, "R8G8B8A8_SRGB")) {
                pass->srgb_framebuffer = true;
            } else if (strstr(line, "R32G32B32A32_FLOAT")) {
                pass->float_framebuffer = true;
            }
        }

        ptr = end;
    }
}

static void read_file_recursive(const char* path, StringBuilder* sb, int depth) {
    if (depth > 16)
        return;

    FILE* file = fopen(path, "r");
    if (!file) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open file: %s", path);
        return;
    }

    char base_dir[1024];
    get_parent_dir(path, base_dir);

    char line[2048];
    while (fgets(line, sizeof(line), file)) {
        char* p = line;
        while (*p && isspace((unsigned char)*p))
            p++;

        if (strncmp(p, "#include", 8) == 0) {
            p += 8;
            while (*p && isspace((unsigned char)*p))
                p++;
            if (*p == '"') {
                p++;
                char* end_quote = strchr(p, '"');
                if (end_quote) {
                    *end_quote = '\0';
                    char inc_path[1024];
                    resolve_path(inc_path, base_dir, p);
                    read_file_recursive(inc_path, sb, depth + 1);
                    sb_append(sb, "\n");
                    continue;
                }
            }
        }

        sb_append(sb, line);
    }
    fclose(file);
}

// Helper to read file with #include support
static char* read_file(const char* path) {
    StringBuilder sb;
    sb_init(&sb);
    read_file_recursive(path, &sb, 0);
    return sb.data;
}

static GLuint load_texture(const char* path, bool linear, bool mipmap, const char* wrap_mode) {
    int w, h, c;
    unsigned char* data = stbi_load(path, &w, &h, &c, 4);
    if (!data) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load texture: %s", path);
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLint min_filter =
        linear ? (mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR) : (mipmap ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
    GLint mag_filter = linear ? GL_LINEAR : GL_NEAREST;
    GLint wrap = GL_CLAMP_TO_EDGE;
    if (strcmp(wrap_mode, "repeat") == 0)
        wrap = GL_REPEAT;
    else if (strcmp(wrap_mode, "clamp_to_border") == 0)
        wrap = GL_CLAMP_TO_BORDER;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

    if (wrap == GL_CLAMP_TO_BORDER) {
        float border_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
    }

    // ⚡ Bolt: Immutable texture storage — driver skips reallocation checks on upload.
    int mip_levels = mipmap ? (int)(log2(w > h ? w : h)) + 1 : 1;
    glTexStorage2D(GL_TEXTURE_2D, mip_levels, GL_RGBA8, w, h);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);

    if (mipmap)
        glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return tex;
}

static GLuint compile_shader(const char* source, GLenum type, const char* pass_name) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[2048];
        glGetShaderInfoLog(shader, 2048, NULL, info_log);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Shader compilation failed for %s (%s):\n%s",
                     pass_name,
                     type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT",
                     info_log);
        // SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader source (first 500 chars):\n%.500s", source);
        return 0;
    }
    SDL_Log("Successfully compiled %s shader for %s", type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT", pass_name);
    return shader;
}

ShaderManager* ShaderManager_Init(GLSLP_Preset* preset, const char* base_path) {
    SDL_Log("ShaderManager_Init called with preset %p", preset);
    ShaderManager* manager = (ShaderManager*)calloc(1, sizeof(ShaderManager));
    if (!manager)
        return NULL;

    manager->preset = preset;
    manager->pass_count = preset->pass_count;
    SDL_Log("ShaderManager_Init: pass_count = %d", manager->pass_count);

    if (manager->pass_count <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader preset has 0 passes.");
        free(manager);
        return NULL;
    }

    // Load LUT textures
    manager->texture_count = preset->texture_count;
    for (int i = 0; i < manager->texture_count; i++) {
        char tex_path[1024];
        if (preset->textures[i].path[0] != '\0') {
            // Parser calls resolve_path, so path should be valid absolute/relative.
            strncpy(tex_path, preset->textures[i].path, sizeof(tex_path) - 1);
            tex_path[sizeof(tex_path) - 1] = '\0';
        } else {
            // Skip
            continue;
        }

        SDL_Log("Loading texture %d: %s (%s)", i, preset->textures[i].name, tex_path);
        manager->textures[i].id = load_texture(
            tex_path, preset->textures[i].linear, preset->textures[i].mipmap, preset->textures[i].wrap_mode);
        strncpy(manager->textures[i].name, preset->textures[i].name, 63);

        // Retrieve width/height?
        if (manager->textures[i].id) {
            glBindTexture(GL_TEXTURE_2D, manager->textures[i].id);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &manager->textures[i].width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &manager->textures[i].height);
        }
    }

    // Copy parameters
    manager->parameter_count = preset->parameter_count;
    for (int i = 0; i < manager->parameter_count; i++) {
        manager->parameters[i] = *((ShaderParameter*)&preset->parameters[i]); // Struct layout matches
    }

    manager->passes = (ShaderPassRuntime*)calloc(manager->pass_count, sizeof(ShaderPassRuntime));
    if (!manager->passes) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate memory for shader passes.");
        free(manager);
        return NULL;
    }
    SDL_Log("Allocated memory for %d passes.", manager->pass_count);

    // Create Quad
    float vertices[] = { -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,
                         -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f };

    SDL_Log("Generating VAO/VBO...");
    glGenVertexArrays(1, &manager->vao);
    glGenBuffers(1, &manager->vbo);
    glBindVertexArray(manager->vao);
    glBindBuffer(GL_ARRAY_BUFFER, manager->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    SDL_Log("VAO/VBO generated and bound.");

    // Resolve path to default vertex shader
    char blit_vert_path[1024];
    if (base_path) {
        snprintf(blit_vert_path, sizeof(blit_vert_path), "%s%s", base_path, "shaders/blit.vert");
    } else {
        snprintf(blit_vert_path, sizeof(blit_vert_path), "%s", "shaders/blit.vert");
    }
    SDL_Log("Blit vertex shader path: %s", blit_vert_path);

    // Compile internal blit program
    const char* internal_blit_vs = "#version 330 core\n"
                                   "layout(location = 0) in vec2 aPos;\n"
                                   "layout(location = 1) in vec2 aTexCoord;\n"
                                   "out vec2 TexCoord;\n"
                                   "void main() {\n"
                                   "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
                                   "    TexCoord = aTexCoord;\n"
                                   "}\n";
    const char* internal_blit_fs = "#version 330 core\n"
                                   "in vec2 TexCoord;\n"
                                   "out vec4 FragColor;\n"
                                   "uniform sampler2D Source;\n"
                                   "void main() {\n"
                                   "    FragColor = texture(Source, TexCoord);\n"
                                   "}\n";

    GLuint ivs = compile_shader(internal_blit_vs, GL_VERTEX_SHADER, "Internal Blit");
    GLuint ifs = compile_shader(internal_blit_fs, GL_FRAGMENT_SHADER, "Internal Blit");
    manager->blit_program = glCreateProgram();
    glAttachShader(manager->blit_program, ivs);
    glAttachShader(manager->blit_program, ifs);
    glLinkProgram(manager->blit_program);
    manager->loc_blit_source = glGetUniformLocation(manager->blit_program, "Source");
    glDeleteShader(ivs);
    glDeleteShader(ifs);

    // Compile shaders
    for (int i = 0; i < manager->pass_count; i++) {
        GLSLP_ShaderPass* pass = &preset->passes[i];
        manager->passes[i].pass_info = pass;

        SDL_Log("Compiling pass %d: %s", i, pass->path);

        char* pass_source = read_file(pass->path);
        if (!pass_source) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read shader file: %s", pass->path);
            ShaderManager_Free(manager);
            return NULL;
        }

        // Parse info from source
        scan_shader_info(manager, pass, pass_source);

        // Comment out existing #version directives
        char* ver_ptr = pass_source;
        while ((ver_ptr = strstr(ver_ptr, "#version")) != NULL) {
            *ver_ptr = '/';
            *(ver_ptr + 1) = '/';
            ver_ptr += 2;
        }

        char* vs_source = NULL;
        char* fs_source = NULL;
        bool is_uber_shader =
            (strstr(pass_source, "#if defined(VERTEX)") != NULL || strstr(pass_source, "#ifdef VERTEX") != NULL);
        bool has_fragcolor_out =
            (strstr(pass_source, "vec4 FragColor") != NULL || strstr(pass_source, "vec4 fragColor") != NULL);

        if (is_uber_shader) {
            size_t src_len = strlen(pass_source);
            size_t header_len = 1024;
            size_t buf_size = src_len + header_len;

            vs_source = (char*)malloc(buf_size);
            fs_source = (char*)malloc(buf_size);

            // Only define PARAMETER_UNIFORM if we actually have parameters
            const char* param_def = (manager->parameter_count > 0)
                                        ? "#define PARAMETER_UNIFORM\n#define RUNTIME_SHADER_PARAMS_ENABLE\n"
                                        : "";

            snprintf(vs_source,
                     buf_size,
                     "#version 330 core\n"
                     "#define VERTEX\n"
                     "%s"
                     "#define varying out\n"
                     "#define attribute in\n"
                     "#define texture2D texture\n"
                     "%s",
                     param_def,
                     pass_source);

            if (has_fragcolor_out) {
                snprintf(fs_source,
                         buf_size,
                         "#version 330 core\n"
                         "#define FRAGMENT\n"
                         "%s"
                         "#define varying in\n"
                         "#define texture2D texture\n"
                         "#define gl_FragColor FragColor\n"
                         "%s",
                         param_def,
                         pass_source);
            } else {
                snprintf(fs_source,
                         buf_size,
                         "#version 330 core\n"
                         "#define FRAGMENT\n"
                         "%s"
                         "#define varying in\n"
                         "#define texture2D texture\n"
                         "#define gl_FragColor FragColor\n"
                         "out vec4 FragColor;\n"
                         "%s",
                         param_def,
                         pass_source);
            }
        } else {
            char* blit_source = read_file(blit_vert_path);
            if (!blit_source) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read blit vertex shader: %s", blit_vert_path);
                free(pass_source);
                ShaderManager_Free(manager);
                return NULL;
            }
            vs_source = blit_source;

            size_t src_len = strlen(pass_source);
            size_t buf_size = src_len + 1024;
            fs_source = (char*)malloc(buf_size);

            // Only define PARAMETER_UNIFORM if we actually have parameters
            const char* param_def = (manager->parameter_count > 0)
                                        ? "#define PARAMETER_UNIFORM\n#define RUNTIME_SHADER_PARAMS_ENABLE\n"
                                        : "";

            if (has_fragcolor_out) {
                snprintf(fs_source,
                         buf_size,
                         "#version 330 core\n"
                         "#define FRAGMENT\n"
                         "%s"
                         "#define texture2D texture\n"
                         "#define gl_FragColor FragColor\n"
                         "#define texCoord TexCoord\n"
                         "%s",
                         param_def,
                         pass_source);
            } else {
                snprintf(fs_source,
                         buf_size,
                         "#version 330 core\n"
                         "#define FRAGMENT\n"
                         "%s"
                         "#define texture2D texture\n"
                         "#define gl_FragColor FragColor\n"
                         "#define texCoord TexCoord\n"
                         "out vec4 FragColor;\n"
                         "%s",
                         param_def,
                         pass_source);
            }
        }

        free(pass_source);

        GLuint vs = compile_shader(vs_source, GL_VERTEX_SHADER, pass->path);
        GLuint fs = compile_shader(fs_source, GL_FRAGMENT_SHADER, pass->path);

        if (is_uber_shader)
            free(vs_source);
        else
            free(vs_source); // blit_source was allocated by read_file, but assigned to vs_source? No, vs_source =
                             // blit_source. So free(vs_source) is correct.
        free(fs_source);

        if (!vs || !fs) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to compile shader pass %d", i);
            if (vs)
                glDeleteShader(vs);
            if (fs)
                glDeleteShader(fs);
            ShaderManager_Free(manager);
            return NULL;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);

        glBindAttribLocation(program, 0, "VertexCoord");
        glBindAttribLocation(program, 1, "TexCoord");
        glBindAttribLocation(program, 0, "aPos");
        glBindAttribLocation(program, 1, "aTexCoord");

        glLinkProgram(program);
        manager->passes[i].loc_MVPMatrix = glGetUniformLocation(program, "MVPMatrix");
        manager->passes[i].loc_projection = glGetUniformLocation(program, "projection");
        manager->passes[i].loc_Source = glGetUniformLocation(program, "Source");
        manager->passes[i].loc_Texture = glGetUniformLocation(program, "Texture");
        manager->passes[i].loc_Original = glGetUniformLocation(program, "Original");
        manager->passes[i].loc_OriginalHistory0 = glGetUniformLocation(program, "OriginalHistory0");
        manager->passes[i].loc_SourceSize = glGetUniformLocation(program, "SourceSize");
        manager->passes[i].loc_OriginalSize = glGetUniformLocation(program, "OriginalSize");
        manager->passes[i].loc_OriginalHistorySize0 = glGetUniformLocation(program, "OriginalHistorySize0");
        manager->passes[i].loc_OutputSize = glGetUniformLocation(program, "OutputSize");
        manager->passes[i].loc_TextureSize = glGetUniformLocation(program, "TextureSize");
        manager->passes[i].loc_InputSize = glGetUniformLocation(program, "InputSize");
        manager->passes[i].loc_FrameCount = glGetUniformLocation(program, "FrameCount");
        manager->passes[i].loc_FrameDirection = glGetUniformLocation(program, "FrameDirection");
        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char info_log[512];
            glGetProgramInfoLog(program, 512, NULL, info_log);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader linking failed for pass %d: %s", i, info_log);
            glDeleteProgram(program);
            ShaderManager_Free(manager);
            return NULL;
        }

        manager->passes[i].program = program;
        SDL_Log("Shader pass %d compiled and linked successfully.", i);
    }

    return manager;
}

void ShaderManager_Render(ShaderManager* manager, GLuint input_texture, int input_w, int input_h, int viewport_w,
                          int viewport_h) {
    manager->frame_count++;

    // History Update
    manager->history_index = (manager->history_index + 1) % 8;
    int curr_idx = manager->history_index;

    // Ensure history FBO exists
    if (!manager->history_fbo)
        glGenFramebuffers(1, &manager->history_fbo);

    if (manager->history_width[curr_idx] != input_w || manager->history_height[curr_idx] != input_h) {
        if (manager->history_textures[curr_idx])
            glDeleteTextures(1, &manager->history_textures[curr_idx]);
        glGenTextures(1, &manager->history_textures[curr_idx]);
        glBindTexture(GL_TEXTURE_2D, manager->history_textures[curr_idx]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, input_w, input_h);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        manager->history_width[curr_idx] = input_w;
        manager->history_height[curr_idx] = input_h;
    }

    // Copy input to history using blit program
    glBindFramebuffer(GL_FRAMEBUFFER, manager->history_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, manager->history_textures[curr_idx], 0);
    glViewport(0, 0, input_w, input_h);

    glUseProgram(manager->blit_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, input_texture);
    glUniform1i(manager->loc_blit_source, 0);

    glBindVertexArray(manager->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    GLuint current_input = input_texture;
    int current_w = input_w;
    int current_h = input_h;

    for (int i = 0; i < manager->pass_count; i++) {
        ShaderPassRuntime* pass_runtime = &manager->passes[i];
        GLSLP_ShaderPass* pass_info = pass_runtime->pass_info;

        GLint old_min_filter = GL_NEAREST;
        if (pass_info->mipmap_input) {
            glBindTexture(GL_TEXTURE_2D, current_input);
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &old_min_filter);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        }

        // Calculate target size
        int target_w = current_w;
        int target_h = current_h;

        if (pass_info->scale_type_x == GLSLP_SCALE_VIEWPORT) {
            target_w = (int)(viewport_w * pass_info->scale_x);
        } else if (pass_info->scale_type_x == GLSLP_SCALE_ABSOLUTE) {
            target_w = (int)pass_info->scale_x;
        } else { // SOURCE
            target_w = (int)(current_w * pass_info->scale_x);
        }

        if (pass_info->scale_type_y == GLSLP_SCALE_VIEWPORT) {
            target_h = (int)(viewport_h * pass_info->scale_y);
        } else if (pass_info->scale_type_y == GLSLP_SCALE_ABSOLUTE) {
            target_h = (int)pass_info->scale_y;
        } else { // SOURCE
            target_h = (int)(current_h * pass_info->scale_y);
        }

        if (target_w <= 0)
            target_w = 1;
        if (target_h <= 0)
            target_h = 1;

        // Resize FBO if needed
        if (pass_runtime->width != target_w || pass_runtime->height != target_h || pass_runtime->fbo == 0) {
            if (pass_runtime->fbo)
                glDeleteFramebuffers(1, &pass_runtime->fbo);
            if (pass_runtime->texture)
                glDeleteTextures(1, &pass_runtime->texture);

            glGenFramebuffers(1, &pass_runtime->fbo);
            glGenTextures(1, &pass_runtime->texture);

            glBindTexture(GL_TEXTURE_2D, pass_runtime->texture);

            GLenum internal_fmt = GL_RGBA;
            if (pass_info->float_framebuffer) {
                internal_fmt = GL_RGBA16F;
            } else if (pass_info->srgb_framebuffer) {
                internal_fmt = GL_SRGB8_ALPHA8;
            }

            // ⚡ Bolt: Map unsized GL_RGBA to sized GL_RGBA8 for glTexStorage2D.
            // GL_RGBA16F and GL_SRGB8_ALPHA8 are already sized formats.
            GLenum sized_fmt = internal_fmt;
            if (internal_fmt == GL_RGBA)
                sized_fmt = GL_RGBA8;
            glTexStorage2D(GL_TEXTURE_2D, 1, sized_fmt, target_w, target_h);

            GLint filter = pass_info->filter_linear ? GL_LINEAR : GL_NEAREST;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

            // Wrap mode from preset
            GLint wrap = GL_CLAMP_TO_EDGE;
            if (strcmp(pass_info->wrap_mode, "repeat") == 0)
                wrap = GL_REPEAT;
            else if (strcmp(pass_info->wrap_mode, "clamp_to_border") == 0)
                wrap = GL_CLAMP_TO_BORDER;

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

            if (wrap == GL_CLAMP_TO_BORDER) {
                float border_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, pass_runtime->fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pass_runtime->texture, 0);

            pass_runtime->width = target_w;
            pass_runtime->height = target_h;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, pass_runtime->fbo);
        glViewport(0, 0, target_w, target_h);

        if (pass_info->srgb_framebuffer) {
            glEnable(GL_FRAMEBUFFER_SRGB);
        } else {
            glDisable(GL_FRAMEBUFFER_SRGB);
        }

        // Clear?
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(pass_runtime->program);

        // MVP (Flip Y)
        float mvp[4][4] = { { 1.0f, 0.0f, 0.0f, 0.0f },
                            { 0.0f, -1.0f, 0.0f, 0.0f },
                            { 0.0f, 0.0f, 1.0f, 0.0f },
                            { 0.0f, 0.0f, 0.0f, 1.0f } };
        glUniformMatrix4fv(pass_runtime->loc_MVPMatrix, 1, GL_FALSE, (const float*)mvp);
        glUniformMatrix4fv(pass_runtime->loc_projection, 1, GL_FALSE, (const float*)mvp);

        // Input Texture (Texture 0) - "Source"
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_input);
        glUniform1i(pass_runtime->loc_Source, 0);
        glUniform1i(pass_runtime->loc_Texture, 0);

        // Original Texture (Texture 1) - "Original"
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, input_texture);
        glUniform1i(pass_runtime->loc_Original, 1);
        glUniform1i(pass_runtime->loc_OriginalHistory0, 1);

        // Sizes
        glUniform4f(pass_runtime->loc_SourceSize,
                    (float)current_w,
                    (float)current_h,
                    1.0f / current_w,
                    1.0f / current_h);

        glUniform4f(pass_runtime->loc_OriginalSize,
                    (float)input_w,
                    (float)input_h,
                    1.0f / input_w,
                    1.0f / input_h);

        glUniform4f(pass_runtime->loc_OriginalHistorySize0,
                    (float)input_w,
                    (float)input_h,
                    1.0f / input_w,
                    1.0f / input_h);

        glUniform4f(pass_runtime->loc_OutputSize,
                    (float)target_w,
                    (float)target_h,
                    1.0f / target_w,
                    1.0f / target_h);

        glUniform4f(pass_runtime->loc_TextureSize,
                    (float)current_w,
                    (float)current_h,
                    1.0f / current_w,
                    1.0f / current_h);

        glUniform4f(pass_runtime->loc_InputSize,
                    (float)input_w,
                    (float)input_h,
                    1.0f / input_w,
                    1.0f / input_h);

        int effective_frame_count = manager->frame_count;
        if (pass_info->frame_count_mod > 0) {
            effective_frame_count = manager->frame_count % pass_info->frame_count_mod;
        }
        glUniform1i(pass_runtime->loc_FrameCount, effective_frame_count);
        glUniform1i(pass_runtime->loc_FrameDirection, 1);

        // Bind Parameters
        for (int p = 0; p < manager->parameter_count; p++) {
            GLint loc = glGetUniformLocation(pass_runtime->program, manager->parameters[p].name);
            if (loc != -1) {
                glUniform1f(loc, manager->parameters[p].value);
            }
        }

        int tex_unit = 2;

        // Bind OriginalHistory
        for (int h = 0; h < 8; h++) {
            int h_idx = (manager->history_index - h + 8) % 8;
            GLuint h_tex = manager->history_textures[h_idx];
            if (h_tex == 0)
                h_tex = input_texture;

            char name[64];
            snprintf(name, sizeof(name), "OriginalHistory%d", h);
            GLint loc = glGetUniformLocation(pass_runtime->program, name);
            if (loc != -1) {
                glActiveTexture(GL_TEXTURE0 + tex_unit);
                glBindTexture(GL_TEXTURE_2D, h_tex);
                glUniform1i(loc, tex_unit);

                snprintf(name, sizeof(name), "OriginalHistorySize%d", h);
                GLint locSize = glGetUniformLocation(pass_runtime->program, name);
                if (locSize != -1) {
                    float w = (float)manager->history_width[h_idx];
                    float h = (float)manager->history_height[h_idx];
                    if (w == 0.0f) {
                        w = (float)input_w;
                        h = (float)input_h;
                    }
                    glUniform4f(locSize, w, h, 1.0f / w, 1.0f / h);
                }
                tex_unit++;
            }
        }

        // Bind LUTs
        for (int t = 0; t < manager->texture_count; t++) {
            GLint loc = glGetUniformLocation(pass_runtime->program, manager->textures[t].name);
            if (loc != -1) {
                glActiveTexture(GL_TEXTURE0 + tex_unit);
                glBindTexture(GL_TEXTURE_2D, manager->textures[t].id);
                glUniform1i(loc, tex_unit);

                // Size uniform
                char size_name[128];
                snprintf(size_name, sizeof(size_name), "%sSize", manager->textures[t].name);
                GLint size_loc = glGetUniformLocation(pass_runtime->program, size_name);
                if (size_loc != -1) {
                    float w = (float)manager->textures[t].width;
                    float h = (float)manager->textures[t].height;
                    glUniform4f(size_loc, w, h, 1.0f / w, 1.0f / h);
                }

                tex_unit++;
            }
        }

        // Bind Aliases (Previous Passes)
        for (int prev = 0; prev < i; prev++) {
            if (manager->passes[prev].pass_info->alias[0] != '\0') {
                char* alias = manager->passes[prev].pass_info->alias;
                GLint loc = glGetUniformLocation(pass_runtime->program, alias);
                if (loc != -1) {
                    glActiveTexture(GL_TEXTURE0 + tex_unit);
                    glBindTexture(GL_TEXTURE_2D, manager->passes[prev].texture);
                    glUniform1i(loc, tex_unit);

                    // Size uniform
                    char size_name[128];
                    snprintf(size_name, sizeof(size_name), "%sSize", alias);
                    GLint size_loc = glGetUniformLocation(pass_runtime->program, size_name);
                    if (size_loc != -1) {
                        float w = (float)manager->passes[prev].width;
                        float h = (float)manager->passes[prev].height;
                        glUniform4f(size_loc, w, h, 1.0f / w, 1.0f / h);
                    }

                    tex_unit++;
                }
            }
        }

        // Bind PassOutputN (Absolute)
        for (int n = 0; n < i; n++) {
            char name[64];
            snprintf(name, sizeof(name), "PassOutput%d", n);
            GLint loc = glGetUniformLocation(pass_runtime->program, name);
            if (loc != -1) {
                glActiveTexture(GL_TEXTURE0 + tex_unit);
                glBindTexture(GL_TEXTURE_2D, manager->passes[n].texture);
                glUniform1i(loc, tex_unit);

                snprintf(name, sizeof(name), "PassOutputSize%d", n);
                GLint size_loc = glGetUniformLocation(pass_runtime->program, name);
                if (size_loc != -1) {
                    float w = (float)manager->passes[n].width;
                    float h = (float)manager->passes[n].height;
                    glUniform4f(size_loc, w, h, 1.0f / w, 1.0f / h);
                }

                tex_unit++;
            }
        }

        // Bind PassPrevNTexture (Relative Legacy)
        for (int n = 1; n <= i; n++) {
            char name[64];
            snprintf(name, sizeof(name), "PassPrev%dTexture", n);
            GLint loc = glGetUniformLocation(pass_runtime->program, name);
            if (loc != -1) {
                int target_pass = i - n;
                glActiveTexture(GL_TEXTURE0 + tex_unit);
                glBindTexture(GL_TEXTURE_2D, manager->passes[target_pass].texture);
                glUniform1i(loc, tex_unit);

                snprintf(name, sizeof(name), "PassPrev%dTextureSize", n);
                GLint size_loc = glGetUniformLocation(pass_runtime->program, name);
                if (size_loc != -1) {
                    float w = (float)manager->passes[target_pass].width;
                    float h = (float)manager->passes[target_pass].height;
                    glUniform4f(size_loc, w, h, 1.0f / w, 1.0f / h);
                }

                snprintf(name, sizeof(name), "PassPrev%dInputSize", n);
                GLint locInputSize = glGetUniformLocation(pass_runtime->program, name);
                if (locInputSize != -1) {
                    float w = (float)manager->passes[target_pass].width;
                    float h = (float)manager->passes[target_pass].height;
                    glUniform4f(locInputSize, w, h, 1.0f / w, 1.0f / h);
                }

                tex_unit++;
            }
        }

        // Legacy Prev binding
        const char* prev_names[] = { "PrevTexture",  "Prev1Texture", "Prev2Texture", "Prev3Texture",
                                     "Prev4Texture", "Prev5Texture", "Prev6Texture" };
        for (int k = 0; k < 7; k++) {
            GLint loc = glGetUniformLocation(pass_runtime->program, prev_names[k]);
            if (loc != -1) {
                if (i == 0) {
                    int h_idx = (manager->history_index - (k + 1) + 8) % 8;
                    GLuint h_tex = manager->history_textures[h_idx];
                    if (!h_tex)
                        h_tex = input_texture;
                    glActiveTexture(GL_TEXTURE0 + tex_unit);
                    glBindTexture(GL_TEXTURE_2D, h_tex);
                    glUniform1i(loc, tex_unit);
                    tex_unit++;
                } else {
                    int target_pass = i - (k + 1);
                    if (target_pass >= 0) {
                        glActiveTexture(GL_TEXTURE0 + tex_unit);
                        glBindTexture(GL_TEXTURE_2D, manager->passes[target_pass].texture);
                        glUniform1i(loc, tex_unit);
                        tex_unit++;
                    }
                }
            }
        }

        glBindVertexArray(manager->vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        if (pass_info->mipmap_input) {
            glBindTexture(GL_TEXTURE_2D, current_input);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, old_min_filter);
        }

        // Update input for next pass
        current_input = pass_runtime->texture;
        current_w = target_w;
        current_h = target_h;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_FRAMEBUFFER_SRGB);
}

void ShaderManager_Free(ShaderManager* manager) {
    if (!manager)
        return;

    if (manager->passes) {
        for (int i = 0; i < manager->pass_count; i++) {
            if (manager->passes[i].program)
                glDeleteProgram(manager->passes[i].program);
            if (manager->passes[i].fbo)
                glDeleteFramebuffers(1, &manager->passes[i].fbo);
            if (manager->passes[i].texture)
                glDeleteTextures(1, &manager->passes[i].texture);
        }
        free(manager->passes);
    }

    for (int i = 0; i < manager->texture_count; i++) {
        if (manager->textures[i].id)
            glDeleteTextures(1, &manager->textures[i].id);
    }

    if (manager->vao)
        glDeleteVertexArrays(1, &manager->vao);
    if (manager->vbo)
        glDeleteBuffers(1, &manager->vbo);

    if (manager->preset) {
        GLSLP_Free(manager->preset);
    }

    if (manager->history_fbo)
        glDeleteFramebuffers(1, &manager->history_fbo);
    for (int i = 0; i < 8; i++) {
        if (manager->history_textures[i])
            glDeleteTextures(1, &manager->history_textures[i]);
    }

    if (manager->blit_program)
        glDeleteProgram(manager->blit_program);

    free(manager);
}
