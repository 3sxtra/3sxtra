// #include "librashader_manager.h" // Do not include the public header to avoid struct redefinition conflicts
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <stdio.h>
#include <stdlib.h>

#define LIBRA_RUNTIME_OPENGL
#include "librashader.h"

// Forward declarations to match the dispatcher's expectation
typedef struct LibrashaderManagerGL LibrashaderManagerGL;

// Loader for librashader
static const void* gl_loader(const char* name) {
    return (const void*)SDL_GL_GetProcAddress(name);
}

struct LibrashaderManagerGL {
    libra_shader_preset_t preset;
    libra_gl_filter_chain_t filter_chain;
    uint64_t frame_count;

    // Output texture to render into before blitting to screen
    GLuint output_texture;
    GLuint output_fbo;
    int output_width;
    int output_height;

    // Simple blit shader to draw the result to screen
    GLuint blit_program;
    GLuint vao;
    GLuint vbo;
};

// Helper to compile internal blit shader
static GLuint compile_shader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader Blit Shader compilation failed: %s", info_log);
        return 0;
    }
    return shader;
}

static void init_blit_resources(LibrashaderManagerGL* manager) {
    const char* vs_src = "#version 330 core\n"
                         "layout(location = 0) in vec2 aPos;\n"
                         "layout(location = 1) in vec2 aTexCoord;\n"
                         "out vec2 TexCoord;\n"
                         "void main() {\n"
                         "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
                         "    TexCoord = aTexCoord;\n"
                         "}\n";

    const char* fs_src = "#version 330 core\n"
                         "in vec2 TexCoord;\n"
                         "out vec4 FragColor;\n"
                         "uniform sampler2D Source;\n"
                         "uniform sampler2D Original;\n"
                         "void main() {\n"
                         "    FragColor = vec4(texture(Source, TexCoord).rgb, texture(Original, TexCoord).a);\n"
                         "}\n";

    GLuint vs = compile_shader(vs_src, GL_VERTEX_SHADER);
    GLuint fs = compile_shader(fs_src, GL_FRAGMENT_SHADER);

    manager->blit_program = glCreateProgram();
    glAttachShader(manager->blit_program, vs);
    glAttachShader(manager->blit_program, fs);
    glLinkProgram(manager->blit_program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    float vertices[] = { // positions   // texCoords
                         -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

                         -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &manager->vao);
    glGenBuffers(1, &manager->vbo);

    glBindVertexArray(manager->vao);
    glBindBuffer(GL_ARRAY_BUFFER, manager->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

LibrashaderManagerGL* LibrashaderManager_Init_GL(const char* preset_path) {
    LibrashaderManagerGL* manager = (LibrashaderManagerGL*)calloc(1, sizeof(LibrashaderManagerGL));
    if (!manager)
        return NULL;

    SDL_Log("LibrashaderManagerGL: Loading preset %s", preset_path);

    libra_error_t err;

    // 1. Create Preset
    // librashader 0.9.0+ uses libra_preset_create_with_options for better compatibility
    err = libra_preset_create_with_options(preset_path, NULL, NULL, &manager->preset);
    if (err != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create preset");
        libra_error_print(err);
        free(manager);
        return NULL;
    }

    // 2. Create Filter Chain
    struct filter_chain_gl_opt_t opt;
    opt.version = LIBRASHADER_CURRENT_VERSION;
#ifdef PLATFORM_RPI4
    opt.glsl_version = 330;
#else
    opt.glsl_version = 460;
#endif
    opt.use_dsa = false; // Compatibility
    opt.force_no_mipmaps = false;
    opt.disable_cache = false;

    err = libra_gl_filter_chain_create(&manager->preset, gl_loader, &opt, &manager->filter_chain);
    if (err != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create filter chain");
        libra_error_print(err);
        libra_preset_free(&manager->preset); // Should check if this is needed, doc says preset invalidated?
        // Docs: "The shader preset is immediately invalidated and must be recreated after the filter chain is created."
        // So we don't need to free it if chain creation succeeded, but if it failed?
        // Usually ownership is consumed.
        free(manager);
        return NULL;
    }

    init_blit_resources(manager);

    return manager;
}

void LibrashaderManager_Render_GL(LibrashaderManagerGL* manager, GLuint input_texture, int input_w, int input_h,
                                  int viewport_x, int viewport_y, int viewport_w, int viewport_h) {
    if (!manager || !manager->filter_chain)
        return;

    // Resize output texture if needed
    if (manager->output_width != viewport_w || manager->output_height != viewport_h) {
        if (manager->output_texture)
            glDeleteTextures(1, &manager->output_texture);
        if (manager->output_fbo)
            glDeleteFramebuffers(1, &manager->output_fbo);

        glGenTextures(1, &manager->output_texture);
        glBindTexture(GL_TEXTURE_2D, manager->output_texture);
        // ⚡ Bolt: Immutable texture storage for output FBO
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, viewport_w, viewport_h);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        manager->output_width = viewport_w;
        manager->output_height = viewport_h;
    }

    // Ensure input texture is complete and has correct filtering
    glBindTexture(GL_TEXTURE_2D, input_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Re-enable mipmaps just in case, though unlikely to be the cause
    glGenerateMipmap(GL_TEXTURE_2D);

    // Prepare inputs
    struct libra_image_gl_t input_image = {
        .handle = input_texture, .format = GL_RGBA8, .width = input_w, .height = input_h
    };

    struct libra_image_gl_t output_image = {
        .handle = manager->output_texture, .format = GL_RGBA8, .width = viewport_w, .height = viewport_h
    };

    struct libra_viewport_t viewport = { .x = 0.0f, .y = 0.0f, .width = viewport_w, .height = viewport_h };

    // Frame options
    // MVP matrix to map [0, 1] to [-1, 1]
    // Scale x and y by 2, translate by -1
    float mvp[16] = {
        2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f
    };

    struct frame_gl_opt_t opt;
    opt.version = LIBRASHADER_CURRENT_VERSION;
    opt.clear_history = false;
    opt.frame_direction = 1;
    opt.rotation = 0;
    opt.total_subframes = 1;
    opt.current_subframe = 1;
    opt.aspect_ratio = 0.0f; // Auto
    opt.frames_per_second = 60.0f;
    opt.frametime_delta = 16; // ms

    libra_error_t err = libra_gl_filter_chain_frame(
        &manager->filter_chain, manager->frame_count++, input_image, output_image, &viewport, mvp, &opt);

    if (err != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Librashader frame failed");
        libra_error_print(err);
        return;
    }

    // Blit to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport_x, viewport_y, viewport_w, viewport_h);

    glUseProgram(manager->blit_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, manager->output_texture);
    glUniform1i(glGetUniformLocation(manager->blit_program, "Source"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, input_texture);
    glUniform1i(glGetUniformLocation(manager->blit_program, "Original"), 1);

    // Clear background to black before blitting shader output
    // REMOVED: glClearColor/glClear - Caller (SDLApp_EndFrame) is responsible for
    // clearing or drawing the background (HD stage) before this blit.
    // glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT);

    // Disable potential interfering states
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glBindVertexArray(manager->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void LibrashaderManager_Free_GL(LibrashaderManagerGL* manager) {
    if (!manager)
        return;

    if (manager->filter_chain) {
        libra_gl_filter_chain_free(&manager->filter_chain);
    }

    if (manager->output_texture)
        glDeleteTextures(1, &manager->output_texture);
    if (manager->blit_program)
        glDeleteProgram(manager->blit_program);
    if (manager->vao)
        glDeleteVertexArrays(1, &manager->vao);
    if (manager->vbo)
        glDeleteBuffers(1, &manager->vbo);

    free(manager);
}
