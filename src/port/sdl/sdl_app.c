/**
 * @file sdl_app.c
 * @brief SDL3 application lifecycle: window, GL context, shaders, input, frame loop.
 *
 * Manages SDL3 initialization, window creation with OpenGL context,
 * shader program compilation (scene + passthrough + texture-array variants),
 * Libretro shader preset loading, scale-mode management, screenshot capture,
 * bezel overlay rendering, and the per-frame present/swap pipeline.
 */
#include "port/sdl/sdl_app.h"
#include "common.h"
#include "game_state.h"
#include "netplay/lobby_server.h"
#include "port/broadcast.h"
#include "port/config.h"
#include "port/modded_stage.h"
#include "port/sdl/control_mapping.h"
#include "port/sdl/frame_display.h"
#include "port/sdl/imgui_wrapper.h"
#include "port/sdl/rmlui_frame_display.h"
#include "port/sdl/rmlui_input_display.h"
#include "port/sdl/rmlui_mods_menu.h"
#include "port/sdl/rmlui_shader_menu.h"
#include "port/sdl/rmlui_stage_config.h"
#include "port/sdl/rmlui_training_menu.h"
#include "port/sdl/rmlui_wrapper.h"
/* Phase 3 — Fight HUD & Mode Menu */
#include "port/sdl/input_display.h"
#include "port/sdl/mods_menu.h"
#include "port/sdl/rmlui_attract_overlay.h"
#include "port/sdl/rmlui_button_config.h"
#include "port/sdl/rmlui_char_select.h"
#include "port/sdl/rmlui_continue.h"
#include "port/sdl/rmlui_control_mapping.h"
#include "port/sdl/rmlui_copyright.h"
#include "port/sdl/rmlui_exit_confirm.h"
#include "port/sdl/rmlui_extra_option.h"
#include "port/sdl/rmlui_game_hud.h"
#include "port/sdl/rmlui_game_option.h"
#include "port/sdl/rmlui_gameover.h"
#include "port/sdl/rmlui_memory_card.h"
#include "port/sdl/rmlui_mode_menu.h"
#include "port/sdl/rmlui_name_entry.h"
#include "port/sdl/rmlui_netplay_ui.h"
#include "port/sdl/rmlui_network_lobby.h"
#include "port/sdl/rmlui_option_menu.h"
#include "port/sdl/rmlui_pause_overlay.h"
#include "port/sdl/rmlui_replay_picker.h"
#include "port/sdl/rmlui_sound_menu.h"
#include "port/sdl/rmlui_sysdir.h"
#include "port/sdl/rmlui_title_screen.h"
#include "port/sdl/rmlui_training_menus.h"
#include "port/sdl/rmlui_trials_hud.h"
#include "port/sdl/rmlui_vs_result.h"
#include "port/sdl/rmlui_vs_screen.h"
#include "port/sdl/rmlui_win_screen.h"
#include "port/sdl/sdl_app_config.h"
#include "port/sdl/sdl_app_input.h"
#include "port/sdl/sdl_app_internal.h"
#include "port/sdl/sdl_app_shader_config.h"
#include "port/sdl/sdl_netplay_ui.h"
#include "port/sdl/sdl_texture_util.h"
#include "port/sdl/shader_menu.h"
#include "port/sdl/stage_config_menu.h"
#include "port/sdl/training_menu.h"
#include "port/sdl_bezel.h"
#include "port/tracy_gpu.h"
#include "port/tracy_zones.h"

int g_resolution_scale = 1;
#include "port/sdl/sdl_game_renderer.h"
#include "port/sdl/sdl_game_renderer_internal.h"
#include "port/sdl/sdl_pad.h"
#include "port/sdl/sdl_text_renderer.h"
#include "port/sound/adx.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Game/engine/workuser.h"

// clang-format off
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_shadercross/SDL_shadercross.h>
// clang-format on
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "shaders/librashader_manager.h"

// ... existing includes ...

// Intermediate render target for librashader GPU output (matches GL backend's approach)
static SDL_GPUTexture* s_librashader_intermediate = NULL;
static int s_librashader_intermediate_w = 0;
static int s_librashader_intermediate_h = 0;

// CLI override for window geometry (set before SDLApp_Init)
static int g_cli_window_x = INT_MIN;
static int g_cli_window_y = INT_MIN;
static int g_cli_window_width = 0;
static int g_cli_window_height = 0;

static GLuint passthru_shader_program;
static GLuint scene_shader_program;
static GLuint scene_array_shader_program; // ⚡ Bolt: Texture array variant (sampler2DArray)
static GLuint vao;
static GLuint vbo;
static GLuint bezel_vao;
static GLuint bezel_vbo;

// ⚡ Bolt: Composition FBO for full-scene shading (HD Stage + Sprites)
// Used when "Bypass Shaders on HD Stages" is OFF.
static GLuint s_composition_fbo = 0;
static GLuint s_composition_texture = 0;
static int s_composition_w = 0;
static int s_composition_h = 0;

// Broadcast FBO for capturing final composited output (shaders + bezels + UI)
static GLuint s_broadcast_fbo = 0;
static GLuint s_broadcast_texture = 0;
static int s_broadcast_w = 0;
static int s_broadcast_h = 0;

// ⚡ Bolt: Cached uniform locations for the passthru shader — avoids 7
// glGetUniformLocation string-hash lookups per frame. These are stable because
// passthru_shader_program is created once at init and never recompiled.
static GLint s_pt_loc_projection = -1;
static GLint s_pt_loc_source = -1;
static GLint s_pt_loc_source_size = -1;
static GLint s_pt_loc_filter_type = -1;

static const float display_target_ratio = 4.0 / 3.0;

SDL_FRect get_letterbox_rect(int win_w, int win_h);
/** @brief Read an entire shader source file into a heap-allocated string. */
static char* read_shader_source(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*)malloc(length + 1);
    size_t read_size = fread(buffer, 1, length, file);
    buffer[read_size] = '\0';

    fclose(file);
    return buffer;
}

/** @brief Compile vertex + fragment shaders and link into a GL program. */
GLuint create_shader_program(const char* base_path, const char* vertex_path, const char* fragment_path) {
    char full_vertex_path[1024];
    snprintf(full_vertex_path, sizeof(full_vertex_path), "%s%s", base_path, vertex_path);

    char full_fragment_path[1024];
    snprintf(full_fragment_path, sizeof(full_fragment_path), "%s%s", base_path, fragment_path);

    char* vertex_source = read_shader_source(full_vertex_path);
    if (vertex_source == NULL) {
        fatal_error("Failed to read vertex shader source from path: %s", full_vertex_path);
    }

    char* fragment_source = read_shader_source(full_fragment_path);
    if (fragment_source == NULL) {
        fatal_error("Failed to read fragment shader source from path: %s", full_fragment_path);
    }

    GLint success;
    char info_log[512];

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, (const char* const*)&vertex_source, NULL);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
        fatal_error("Vertex shader compilation failed: %s", info_log);
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, (const char* const*)&fragment_source, NULL);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
        fatal_error("Fragment shader compilation failed: %s", info_log);
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, info_log);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader linking failed: %s", info_log);
        exit(1);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    free(vertex_source);
    free(fragment_source);

    return program;
}

#define FRAME_END_TIMES_MAX 30

typedef enum ScaleMode {
    SCALEMODE_NEAREST,
    SCALEMODE_LINEAR,
    SCALEMODE_SOFT_LINEAR,
    SCALEMODE_SQUARE_PIXELS,
    SCALEMODE_INTEGER,
    SCALEMODE_PIXEL_ART,
    SCALEMODE_COUNT
} ScaleMode;

static const char* app_name = "Street Fighter III: 3rd Strike";
static const double target_fps = 59.59949;
static const Uint64 target_frame_time_ns = 1000000000.0 / target_fps;

SDL_Window* window = NULL;
static RendererBackend g_renderer_backend = RENDERER_OPENGL; // SDL_GPU opt-in via --renderer gpu
static SDL_GPUDevice* gpu_device = NULL;
static SDL_Renderer* sdl_renderer = NULL; // Only used in SDL2D mode

// GPU Bezel Resources
static SDL_GPUGraphicsPipeline* s_bezel_pipeline = NULL;
static SDL_GPUSampler* s_bezel_sampler = NULL;
static SDL_GPUBuffer* s_bezel_vertex_buffer = NULL;
static SDL_GPUTransferBuffer* s_bezel_transfer_buffer = NULL;

static ScaleMode scale_mode = SCALEMODE_NEAREST;

static Uint64 frame_deadline = 0;
static Uint64 frame_end_times[FRAME_END_TIMES_MAX];
static int frame_end_times_index = 0;
static bool frame_end_times_filled = false;
static double fps = 0;
static Uint64 frame_counter = 0;
static int last_p1_char = -1;
static int last_p2_char = -1;

static bool should_save_screenshot = false;
static Uint64 last_mouse_motion_time = 0;
static const int mouse_hide_delay_ms = 2000; // 2 seconds
static bool cursor_visible = true;           // Track cursor state to avoid redundant SDL calls
static bool show_menu = false;
static bool show_shader_menu = false;
static bool show_mods_menu = false;
bool mods_menu_input_display_enabled = false;
bool mods_menu_shader_bypass_enabled = false;
bool game_paused = false;
static bool frame_rate_uncapped = false;
static bool vsync_enabled = true;      // user preference, independent of frame_rate_uncapped
static bool present_only_mode = false; // when true, EndFrame re-blits canvas without re-rendering game
bool show_debug_hud = false;

// FPS history — unbounded, grows since game start
static float* fps_history = NULL;
static int fps_history_count = 0;
static int fps_history_capacity = 0;

// ⚡ Bolt: Bezel VBO dirty flag — skip redundant vertex uploads.
static bool bezel_vbo_dirty = true;

// UI mode flag — when true, RmlUi handles overlay menus instead of ImGui
bool use_rmlui = false;

static const /** @brief Return the display name for the current scale mode. */
    char*
    scale_mode_name() {
    switch (scale_mode) {
    case SCALEMODE_NEAREST:
        return "Nearest";
    case SCALEMODE_LINEAR:
        return "Linear";
    case SCALEMODE_SOFT_LINEAR:
        return "Soft Linear";
    case SCALEMODE_SQUARE_PIXELS:
        return "Square Pixels";
    case SCALEMODE_INTEGER:
        return "Integer";
    case SCALEMODE_PIXEL_ART:
        return "Pixel Art";
    case SCALEMODE_COUNT:
        return "Unknown";
    }
    return "Unknown";
}

static const /** @brief Convert a ScaleMode enum to its config-file string key. */
    char*
    scale_mode_to_config_string(ScaleMode mode) {
    switch (mode) {
    case SCALEMODE_NEAREST:
        return "nearest";
    case SCALEMODE_LINEAR:
        return "linear";
    case SCALEMODE_SOFT_LINEAR:
        return "soft-linear";
    case SCALEMODE_SQUARE_PIXELS:
        return "square-pixels";
    case SCALEMODE_INTEGER:
        return "integer";
    case SCALEMODE_PIXEL_ART:
        return "pixel-art";
    default:
        return "nearest";
    }
}

static /** @brief Parse a config-file string into a ScaleMode enum. */
    ScaleMode
    config_string_to_scale_mode(const char* string) {
    if (SDL_strcmp(string, "nearest") == 0) {
        return SCALEMODE_NEAREST;
    }
    if (SDL_strcmp(string, "linear") == 0) {
        return SCALEMODE_LINEAR;
    }
    if (SDL_strcmp(string, "soft-linear") == 0) {
        return SCALEMODE_SOFT_LINEAR;
    }
    if (SDL_strcmp(string, "square-pixels") == 0) {
        return SCALEMODE_SQUARE_PIXELS;
    }
    if (SDL_strcmp(string, "integer") == 0) {
        return SCALEMODE_INTEGER;
    }
    if (SDL_strcmp(string, "pixel-art") == 0) {
        return SCALEMODE_PIXEL_ART;
    }
    return SCALEMODE_NEAREST;
}

static /** @brief Advance to the next scale mode (wrapping). */
    void
    cycle_scale_mode() {
    scale_mode = (scale_mode + 1) % SCALEMODE_COUNT;
    Config_SetString(CFG_KEY_SCALEMODE, scale_mode_to_config_string(scale_mode));
    bezel_vbo_dirty = true; // Viewport changed, recalculate bezel positions
    SDL_Log("Scale mode: %s", scale_mode_name());
}

static SDL_GPUShader* CreateGPUShader(const char* filename, SDL_GPUShaderStage stage) {
    size_t size;
    void* code = SDL_LoadFile(filename, &size);
    if (!code) {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Failed to load shader: %s", filename);
        return NULL;
    }

    SDL_ShaderCross_SPIRV_Info info;
    SDL_zero(info);
    info.bytecode = (const Uint8*)code;
    info.bytecode_size = size;
    info.entrypoint = "main";
    info.shader_stage = (SDL_ShaderCross_ShaderStage)stage;

    SDL_ShaderCross_GraphicsShaderMetadata* metadata =
        SDL_ShaderCross_ReflectGraphicsSPIRV(info.bytecode, info.bytecode_size, 0);

    if (!metadata) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to reflect SPIRV: %s", filename);
        SDL_free(code);
        return NULL;
    }

    SDL_GPUShader* shader =
        SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(gpu_device, &info, &metadata->resource_info, 0);

    SDL_free(metadata);
    SDL_free(code);
    return shader;
}

static void InitBezelGPU(const char* base_path) {
    char vert_path[1024];
    char frag_path[1024];
    snprintf(vert_path, sizeof(vert_path), "%sshaders/blit.vert.spv", base_path);
    snprintf(frag_path, sizeof(frag_path), "%sshaders/blit.frag.spv", base_path);

    SDL_GPUShader* vert = CreateGPUShader(vert_path, SDL_GPU_SHADERSTAGE_VERTEX);
    SDL_GPUShader* frag = CreateGPUShader(frag_path, SDL_GPU_SHADERSTAGE_FRAGMENT);

    if (!vert || !frag)
        return;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info;
    SDL_zero(pipeline_info);
    pipeline_info.vertex_shader = vert;
    pipeline_info.fragment_shader = frag;

    // Attributes: Pos (vec2), UV (vec2)
    SDL_GPUVertexAttribute attrs[2];
    attrs[0].location = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[0].offset = 0;
    attrs[0].buffer_slot = 0;

    attrs[1].location = 1;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[1].offset = 2 * sizeof(float);
    attrs[1].buffer_slot = 0;

    pipeline_info.vertex_input_state.vertex_attributes = attrs;
    pipeline_info.vertex_input_state.num_vertex_attributes = 2;

    SDL_GPUVertexBufferDescription bindings[1];
    bindings[0].slot = 0;
    bindings[0].pitch = 4 * sizeof(float);
    bindings[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    pipeline_info.vertex_input_state.vertex_buffer_descriptions = bindings;
    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    SDL_GPUColorTargetDescription target_desc;
    SDL_zero(target_desc);
    target_desc.format = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
    target_desc.blend_state.enable_blend = true;
    target_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    pipeline_info.target_info.color_target_descriptions = &target_desc;
    pipeline_info.target_info.num_color_targets = 1;

    s_bezel_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);
    SDL_ReleaseGPUShader(gpu_device, vert);
    SDL_ReleaseGPUShader(gpu_device, frag);

    // Sampler
    SDL_GPUSamplerCreateInfo sampler_info;
    SDL_zero(sampler_info);
    sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    s_bezel_sampler = SDL_CreateGPUSampler(gpu_device, &sampler_info);

    // Buffers
    size_t buf_size = 48 * sizeof(float); // 2 quads * 6 verts * 4 floats

    SDL_GPUBufferCreateInfo b_info = { .usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = buf_size };
    s_bezel_vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &b_info);

    SDL_GPUTransferBufferCreateInfo tb_info = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = buf_size };
    s_bezel_transfer_buffer = SDL_CreateGPUTransferBuffer(gpu_device, &tb_info);
}

static void ShutdownBezelGPU() {
    if (s_bezel_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(gpu_device, s_bezel_pipeline);
    if (s_bezel_sampler)
        SDL_ReleaseGPUSampler(gpu_device, s_bezel_sampler);
    if (s_bezel_vertex_buffer)
        SDL_ReleaseGPUBuffer(gpu_device, s_bezel_vertex_buffer);
    if (s_bezel_transfer_buffer)
        SDL_ReleaseGPUTransferBuffer(gpu_device, s_bezel_transfer_buffer);
}

/** @brief Initialize SDL3, create window + GL context, compile shaders, load config. */
int SDLApp_Init() {
    Config_Init();
    LobbyServer_Init();

    const char* cfg_scale = Config_GetString(CFG_KEY_SCALEMODE);
    if (cfg_scale) {
        scale_mode = config_string_to_scale_mode(cfg_scale);
    }

    show_debug_hud = Config_GetBool(CFG_KEY_DEBUG_HUD);

    // shader_mode_libretro = Config_GetBool(CFG_KEY_SHADER_MODE_LIBRETRO); // Moved to SDLAppShader_Init

    SDL_SetAppMetadata(app_name, "0.1", NULL);
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR, "1");
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    if (g_renderer_backend == RENDERER_OPENGL) {
#ifdef PLATFORM_RPI4
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
#endif
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifndef PLATFORM_RPI4
        SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
#endif
    }

    // SDL2D path: skip GL attributes entirely

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (g_renderer_backend == RENDERER_OPENGL) {
        window_flags |= SDL_WINDOW_OPENGL;
    }
    // SDL2D: no extra window flags needed

    // CLI window geometry overrides fullscreen config
    bool cli_override = (g_cli_window_x != INT_MIN || g_cli_window_width > 0);
    if (Config_GetBool(CFG_KEY_FULLSCREEN) && !cli_override) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }

    // Apply user-defined fullscreen resolution (if any) before window creation
    // so SDL3 knows the desired mode when the window starts fullscreen.
    int fs_w = Config_GetInt(CFG_KEY_FULLSCREEN_WIDTH);
    int fs_h = Config_GetInt(CFG_KEY_FULLSCREEN_HEIGHT);
    bool has_fs_res = (fs_w > 0 && fs_h > 0);

    int width = (g_cli_window_width > 0) ? g_cli_window_width : Config_GetInt(CFG_KEY_WINDOW_WIDTH);
    int height = (g_cli_window_height > 0) ? g_cli_window_height : Config_GetInt(CFG_KEY_WINDOW_HEIGHT);
    if (width <= 0)
        width = 640;
    if (height <= 0)
        height = 480;

    const char* backend_name = (g_renderer_backend == RENDERER_SDLGPU)  ? "SDL_GPU"
                               : (g_renderer_backend == RENDERER_SDL2D) ? "SDL2D"
                                                                        : "OpenGL";
    SDL_Log("SDLApp_Init: Creating window %dx%d, Fullscreen: %d, Backend: %s",
            width,
            height,
            (window_flags & SDL_WINDOW_FULLSCREEN) ? 1 : 0,
            backend_name);

    if (g_renderer_backend == RENDERER_SDL2D) {
        // SDL2D: use SDL_CreateWindowAndRenderer — no GL context, no GPU device
        if (!SDL_CreateWindowAndRenderer(app_name, width, height, window_flags, &window, &sdl_renderer)) {
            fatal_error("SDL2D: Couldn't create window/renderer: %s", SDL_GetError());
        }
        SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);
        SDL_Log("Renderer: SDL2D (SDL_Renderer)");

        // Apply vsync from config (default on)
        vsync_enabled = !Config_HasKey(CFG_KEY_VSYNC) || Config_GetBool(CFG_KEY_VSYNC);
        SDL_SetRenderVSync(sdl_renderer, vsync_enabled ? 1 : 0);
        SDL_Log("VSync: %s (SDL2D)", vsync_enabled ? "ON" : "OFF");
    } else {
        window = SDL_CreateWindow(app_name, width, height, window_flags);
        if (!window) {
            SDL_Log("Couldn't create window: %s", SDL_GetError());
            return 1;
        }
    }

    // Set the fullscreen display mode on the window before it goes fullscreen.
    // When has_fs_res is true, SDL3 will enter exclusive fullscreen at the
    // requested resolution; otherwise it uses desktop borderless.
    if (has_fs_res) {
        SDL_DisplayMode mode = { 0 };
        mode.w = fs_w;
        mode.h = fs_h;
        if (!SDL_SetWindowFullscreenMode(window, &mode)) {
            SDL_Log("SDLApp_Init: Could not set fullscreen mode %dx%d: %s", fs_w, fs_h, SDL_GetError());
        } else {
            SDL_Log("SDLApp_Init: Fullscreen mode set to %dx%d", fs_w, fs_h);
        }
    }

    if (!(window_flags & SDL_WINDOW_FULLSCREEN)) {
        int x = (g_cli_window_x != INT_MIN) ? g_cli_window_x : Config_GetInt(CFG_KEY_WINDOW_X);
        int y = (g_cli_window_y != INT_MIN) ? g_cli_window_y : Config_GetInt(CFG_KEY_WINDOW_Y);
        if (x != 0 || y != 0 || g_cli_window_x != INT_MIN) {
            if (cli_override) {
                SDL_RestoreWindow(window);
                SDL_SetWindowSize(window, width, height);
            }
            SDL_SetWindowPosition(window, x, y);
        }
    }

    SDL_GLContext gl_context = NULL;

    if (g_renderer_backend == RENDERER_SDLGPU) {
        // GPU Backend Initialization
        bool gpu_debug = (SDL_getenv("SDL_GPU_DEBUG") != NULL);
        if (gpu_debug)
            SDL_Log("GPU debug mode ENABLED (Vulkan validation layers active).");
        gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, gpu_debug, NULL);
        if (!gpu_device) {
            SDL_Log("Failed to create SDL_GPU device: %s", SDL_GetError());
            return 1;
        }
        if (!SDL_ClaimWindowForGPUDevice(gpu_device, window)) {
            SDL_Log("Failed to claim SDL_GPU window: %s", SDL_GetError());
            return 1;
        }
        SDL_Log("SDL_GPU Initialized Successfully.");

        // Apply vsync from config (default on)
        vsync_enabled = !Config_HasKey(CFG_KEY_VSYNC) || Config_GetBool(CFG_KEY_VSYNC);
        SDL_SetGPUSwapchainParameters(gpu_device,
                                      window,
                                      SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                      vsync_enabled ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE);
        SDL_Log("VSync: %s (SDL_GPU)", vsync_enabled ? "ON" : "OFF");
    } else if (g_renderer_backend == RENDERER_OPENGL) {
        // OpenGL Backend Initialization
        gl_context = SDL_GL_CreateContext(window);
        if (!gl_context) {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                         "Failed to create OpenGL context: %s\n"
                         "This GPU may only support OpenGL ES, not desktop OpenGL.\n"
                         "Try: --renderer gpu (uses Vulkan via SDL_GPU)",
                         SDL_GetError());
            return 1;
        }

        if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
            SDL_Log("Failed to initialize GLAD");
            return 1;
        }

        // Set initial viewport
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        const SDL_FRect viewport = get_letterbox_rect(win_w, win_h);
        glViewport(viewport.x, viewport.y, viewport.w, viewport.h);

        // Initialize Tracy GPU profiling (after GL context + glad)
        SDL_Log("Initializing Tracy GPU Profiler...");
        TRACE_GPU_INIT();
        SDL_Log("Tracy GPU Profiler initialized.");

        // Apply vsync from config (default on)
        vsync_enabled = !Config_HasKey(CFG_KEY_VSYNC) || Config_GetBool(CFG_KEY_VSYNC);
        SDL_GL_SetSwapInterval(vsync_enabled ? 1 : 0);
        SDL_Log("VSync: %s (OpenGL)", vsync_enabled ? "ON" : "OFF");
    }
    // else: SDL2D — window and renderer already created above

    SDL_Log("Initializing Game Renderer...");
    SDLGameRenderer_Init();
    SDL_Log("Game Renderer initialized.");

    // Initialize bezel GPU resources
    if (g_renderer_backend == RENDERER_SDLGPU) {
        InitBezelGPU(SDL_GetBasePath());
        SDL_Log("Bezel GPU resources initialized.");
    }

    // Initialize pads
    SDLPad_Init();

    Broadcast_Initialize();

    char* base_path = SDL_GetBasePath();
    if (base_path == NULL) {
        fatal_error("Failed to get base path.");
    }

    // Text renderer init
    {
        char font_path[1024];
        snprintf(font_path, sizeof(font_path), "%s%s", base_path, "assets/BoldPixels.ttf");
        SDLTextRenderer_Init(base_path, font_path);
    }

    if (g_renderer_backend == RENDERER_OPENGL) {
        passthru_shader_program = create_shader_program(base_path, "shaders/blit.vert", "shaders/passthru.frag");
        scene_shader_program = create_shader_program(base_path, "shaders/scene.vert", "shaders/scene.frag");
        scene_array_shader_program = create_shader_program(base_path, "shaders/scene.vert", "shaders/scene_array.frag");

        // Create quad
        float vertices[] = { // positions        // texture coords
                             -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

                             -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Create bezel resources
        glGenVertexArrays(1, &bezel_vao);
        glGenBuffers(1, &bezel_vbo);

        glBindVertexArray(bezel_vao);
        glBindBuffer(GL_ARRAY_BUFFER, bezel_vbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    // RmlUi overlay: available on ALL backends (GL, GPU, SDL2D)
    rmlui_wrapper_init(window, gl_context);
    rmlui_mods_menu_init();
    rmlui_shader_menu_init();
    rmlui_stage_config_init();
    rmlui_input_display_init();
    rmlui_frame_display_init();
    rmlui_netplay_ui_init();
    rmlui_training_menu_init();
    rmlui_control_mapping_init();
    /* Phase 3 — Fight HUD & Mode Menu */
    rmlui_game_hud_init();
    rmlui_mode_menu_init();
    rmlui_option_menu_init();
    rmlui_game_option_init();
    rmlui_title_screen_init();
    rmlui_win_screen_init();
    rmlui_continue_init();
    rmlui_gameover_init();
    rmlui_vs_result_init();
    rmlui_memory_card_init();
    rmlui_sound_menu_init();
    rmlui_sysdir_init();
    rmlui_network_lobby_init();
    rmlui_extra_option_init();
    rmlui_training_menus_init();
    rmlui_button_config_init();
    rmlui_char_select_init();
    rmlui_vs_screen_init();
    rmlui_replay_picker_init();
    rmlui_pause_overlay_init();
    rmlui_trials_hud_init();
    rmlui_copyright_init();
    rmlui_name_entry_init();
    rmlui_exit_confirm_init();
    rmlui_attract_overlay_init();

    // Check if user wants RmlUi mode (set via --ui rmlui CLI flag, session-only)
    extern bool g_ui_mode_rmlui;
    use_rmlui = g_ui_mode_rmlui;
    if (use_rmlui) {
        SDL_Log("UI mode: RmlUi (overlay menus via HTML/CSS)");
    } else {
        SDL_Log("UI mode: ImGui (default)");
    }

    // Skip ImGui, bezels, shaders, and mod menus for SDL2D mode
    if (g_renderer_backend != RENDERER_SDL2D) {
        imgui_wrapper_init(window, gl_context);

        input_display_init();
        frame_display_init();
        SDLNetplayUI_Init();
        BezelSystem_Init();
        ModdedStage_Init();
        mods_menu_init();
        stage_config_menu_init();
        training_menu_init();

        // Load bezel visibility
        if (Config_HasKey(CFG_KEY_BEZEL_ENABLED)) {
            BezelSystem_SetVisible(Config_GetBool(CFG_KEY_BEZEL_ENABLED));
        }

        BezelSystem_LoadTextures();

        // Initialize Shader Config
        SDLAppShader_Init(base_path);
    }

    SDL_free(base_path);

    return 0;
}

extern BroadcastConfig broadcast_config;

/** @brief Shut down SDL, release shaders, destroy window. */
void SDLApp_Quit() {
    Broadcast_Shutdown();
    SDLGameRenderer_Shutdown();
    SDLTextRenderer_Shutdown();

    if (g_renderer_backend == RENDERER_SDL2D) {
        // SDL2D cleanup
        rmlui_wrapper_shutdown();
        if (sdl_renderer) {
            SDL_DestroyRenderer(sdl_renderer);
            sdl_renderer = NULL;
        }
    } else {
        SDLAppShader_Shutdown();

        if (g_renderer_backend == RENDERER_SDLGPU) {
            ShutdownBezelGPU();
        }

        // Shared UI shutdown (GL + GPU)
        input_display_shutdown();
        frame_display_shutdown();
        SDLNetplayUI_Shutdown();
        BezelSystem_Shutdown();
        ModdedStage_Shutdown();
        mods_menu_shutdown();
        stage_config_menu_shutdown();
        training_menu_shutdown();
        imgui_wrapper_shutdown();
        rmlui_wrapper_shutdown();

        if (g_renderer_backend == RENDERER_OPENGL) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteVertexArrays(1, &bezel_vao);
            glDeleteBuffers(1, &bezel_vbo);
            glDeleteProgram(passthru_shader_program);

            glDeleteProgram(scene_shader_program);
            glDeleteProgram(scene_array_shader_program);

            if (s_composition_fbo)
                glDeleteFramebuffers(1, &s_composition_fbo);
            if (s_composition_texture)
                glDeleteTextures(1, &s_composition_texture);
            if (s_broadcast_fbo)
                glDeleteFramebuffers(1, &s_broadcast_fbo);
            if (s_broadcast_texture)
                glDeleteTextures(1, &s_broadcast_texture);
        } else {
            // RENDERER_SDLGPU
            if (gpu_device) {
                if (s_librashader_intermediate) {
                    SDL_ReleaseGPUTexture(gpu_device, s_librashader_intermediate);
                    s_librashader_intermediate = NULL;
                }
                SDL_DestroyGPUDevice(gpu_device);
            }
        }
    }

    // Sync vsync config
    Config_SetBool(CFG_KEY_VSYNC, vsync_enabled);
    Config_SetBool(CFG_KEY_DEBUG_HUD, show_debug_hud);

    // Sync broadcast config
    Config_SetBool(CFG_KEY_BROADCAST_ENABLED, broadcast_config.enabled);
    Config_SetInt(CFG_KEY_BROADCAST_SOURCE, broadcast_config.source);
    Config_SetBool(CFG_KEY_BROADCAST_SHOW_UI, broadcast_config.show_ui);

    Config_Save();
    Config_Destroy();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

static /** @brief Hide the cursor after 2 seconds of inactivity. */
    void
    hide_cursor_if_needed() {
    if (show_menu || show_shader_menu || show_stage_config_menu || show_training_menu) {
        return;
    }
    const Uint64 now = SDL_GetTicks();

    if (cursor_visible && (last_mouse_motion_time > 0) && ((now - last_mouse_motion_time) > mouse_hide_delay_ms)) {
        SDL_HideCursor();
        cursor_visible = false;
    }
}

/** @brief Process all pending SDL events (input, window, quit). Returns false on quit. */
bool SDLApp_PollEvents() {
    SDL_Event event;
    bool continue_running = true;

    while (SDL_PollEvent(&event)) {
        bool request_quit = SDLAppInput_HandleEvent(&event);
        if (request_quit) {
            continue_running = false;
        }
    }

    control_mapping_update();

    return continue_running;
}

/** @brief Begin a new frame — start ImGui frame and clear the GL viewport. */
void SDLApp_BeginFrame() {
    if (g_renderer_backend != RENDERER_SDL2D) {
        // Process any deferred preset switch
        SDLAppShader_ProcessPendingLoad();
        if (!use_rmlui) {
            imgui_wrapper_new_frame();
        }
    }

    // RmlUi frame update runs on all backends
    if (use_rmlui) {
        rmlui_wrapper_new_frame();
    }

    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);

    // Render menus
    if (show_menu) {
        // ... (existing menu logic if any)
    }

    // SDL2D: clear the window backbuffer
    if (g_renderer_backend == RENDERER_SDL2D) {
        SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_SetRenderTarget(sdl_renderer, NULL);
        SDL_RenderClear(sdl_renderer);
    }

    if (!present_only_mode) {
        SDLGameRenderer_BeginFrame();
    }
}

static /** @brief Center an SDL_FRect within the window. */
    void
    center_rect(SDL_FRect* rect, int win_w, int win_h) {
    rect->x = (win_w - rect->w) / 2;
    rect->y = (win_h - rect->h) / 2;
}

static /** @brief Compute the largest 4:3 rectangle that fits the window (letterboxed). */
    SDL_FRect
    fit_4_by_3_rect(int win_w, int win_h) {
    SDL_FRect rect;
    rect.w = win_w;
    rect.h = win_w / display_target_ratio;

    if (rect.h > win_h) {
        rect.h = win_h;
        rect.w = win_h * display_target_ratio;
    }

    center_rect(&rect, win_w, win_h);
    return rect;
}

static /** @brief Compute the largest integer-scaled rectangle for pixel-perfect display. */
    SDL_FRect
    fit_integer_rect(int win_w, int win_h, int pixel_w, int pixel_h) {
    SDL_FRect rect;

    // Try pixel-ratio-aware integer scale first (true pixel-perfect at target ratio)
    int scale_w = win_w / (384 * pixel_w);
    int scale_h = win_h / (224 * pixel_h);
    int scale = (scale_h < scale_w) ? scale_h : scale_w;

    if (scale >= 1) {
        // Pixel-perfect at the target pixel ratio
        rect.w = (float)(scale * 384 * pixel_w);
        rect.h = (float)(scale * 224 * pixel_h);
    } else {
        // Window too small for full pixel-ratio scaling.
        // Fall back to integer-scaling the base 384x224 resolution,
        // then apply the pixel aspect ratio via the output rect dimensions.
        scale_w = win_w / 384;
        scale_h = win_h / 224;
        scale = (scale_h < scale_w) ? scale_h : scale_w;
        if (scale < 1)
            scale = 1;

        int base_w = scale * 384;
        int base_h = scale * 224;

        // Apply pixel aspect ratio and fit within window
        float aspect = (float)(base_w * pixel_w) / (float)(base_h * pixel_h);
        if ((float)base_w / (float)win_w > (float)base_h / (float)win_h) {
            // Width-constrained
            rect.w = (float)base_w;
            rect.h = (float)base_w / aspect;
        } else {
            // Height-constrained
            rect.h = (float)base_h;
            rect.w = (float)base_h * aspect;
        }

        // Clamp to window bounds
        if (rect.w > win_w) {
            float s = (float)win_w / rect.w;
            rect.w = (float)win_w;
            rect.h *= s;
        }
        if (rect.h > win_h) {
            float s = (float)win_h / rect.h;
            rect.h = (float)win_h;
            rect.w *= s;
        }
    }

    center_rect(&rect, win_w, win_h);
    return rect;
}

/** @brief Get the current letterbox/viewport rectangle based on scale mode. */
SDL_FRect get_letterbox_rect(int win_w, int win_h) {
    switch (scale_mode) {
    case SCALEMODE_NEAREST:
    case SCALEMODE_LINEAR:
    case SCALEMODE_SOFT_LINEAR:
        return fit_4_by_3_rect(win_w, win_h);

    case SCALEMODE_INTEGER:
        // In order to scale a 384x224 buffer to 4:3 we need to stretch the image vertically by 9 / 7
        return fit_integer_rect(win_w, win_h, 7, 9);

    case SCALEMODE_PIXEL_ART:
        return fit_4_by_3_rect(win_w, win_h);

    case SCALEMODE_SQUARE_PIXELS:
        return fit_integer_rect(win_w, win_h, 1, 1);

    case SCALEMODE_COUNT:
        return fit_4_by_3_rect(win_w, win_h);
    }
    return fit_4_by_3_rect(win_w, win_h);
}

/** @brief Record the current time for FPS calculation. */
void note_frame_end_time() {
    frame_end_times[frame_end_times_index] = SDL_GetTicksNS();
    frame_end_times_index += 1;
    frame_end_times_index %= FRAME_END_TIMES_MAX;

    if (frame_end_times_index == 0) {
        frame_end_times_filled = true;
    }
}

static /** @brief Compute and display FPS from the rolling frame-time buffer. */
    void
    update_fps() {
    if (!frame_end_times_filled) {
        return;
    }

    double total_frame_time_ms = 0;

    for (int i = 0; i < FRAME_END_TIMES_MAX - 1; i++) {
        const int cur = (frame_end_times_index + i) % FRAME_END_TIMES_MAX;
        const int next = (cur + 1) % FRAME_END_TIMES_MAX;
        total_frame_time_ms += (double)(frame_end_times[next] - frame_end_times[cur]) / 1e6;
    }

    double average_frame_time_ms = total_frame_time_ms / (FRAME_END_TIMES_MAX - 1);
    fps = 1000 / average_frame_time_ms;

    // Push into FPS history (grows dynamically)
    if (fps_history_count >= fps_history_capacity) {
        fps_history_capacity = fps_history_capacity ? fps_history_capacity * 2 : 1024;
        fps_history = (float*)realloc(fps_history, fps_history_capacity * sizeof(float));
    }
    fps_history[fps_history_count++] = (float)fps;
}

static /** @brief Capture the current framebuffer to a PNG file. */
    void
    save_screenshot(const char* filename) {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_Surface* surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGB24);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, surface->pixels);
    SDL_SaveBMP(surface, filename);
    SDL_DestroySurface(surface);
}

// ⚡ Bolt: Pre-build NDC vertex data for a bezel quad into a caller-provided array.
// This separates vertex computation from GL upload so both bezels can be batched
// into a single VBO upload when the layout changes (resize/character switch).
static /** @brief Build NDC vertices for a bezel quad from an SDL_FRect. */
    void
    build_bezel_vertices(const SDL_FRect* rect, int win_w, int win_h, float* out) {
    if (rect->w <= 0 || rect->h <= 0) {
        memset(out, 0, 6 * 4 * sizeof(float));
        return;
    }

    // Convert to NDC
    float x1 = (rect->x / win_w) * 2.0f - 1.0f;
    float y1 = 1.0f - (rect->y / win_h) * 2.0f;
    float x2 = ((rect->x + rect->w) / win_w) * 2.0f - 1.0f;
    float y2 = 1.0f - ((rect->y + rect->h) / win_h) * 2.0f;

    // Triangle 1
    out[0] = x1;
    out[1] = y1;
    out[2] = 0.0f;
    out[3] = 0.0f;
    out[4] = x1;
    out[5] = y2;
    out[6] = 0.0f;
    out[7] = 1.0f;
    out[8] = x2;
    out[9] = y2;
    out[10] = 1.0f;
    out[11] = 1.0f;
    // Triangle 2
    out[12] = x1;
    out[13] = y1;
    out[14] = 0.0f;
    out[15] = 0.0f;
    out[16] = x2;
    out[17] = y2;
    out[18] = 1.0f;
    out[19] = 1.0f;
    out[20] = x2;
    out[21] = y1;
    out[22] = 1.0f;
    out[23] = 0.0f;
}

/** @brief End the frame: render game to FBO, apply shaders, draw bezels/UI, swap buffers. */
void SDLApp_EndFrame() {
    TRACE_ZONE_N("EndFrame");
    Broadcast_Update();

    // Render all queued tasks to the FBO (skip in present-only mode — canvas already has last frame)
    if (!present_only_mode) {
        SDLGameRenderer_RenderFrame();
    }

    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);

    // Update Phase 3 data models immediately after game frame processing,
    // before the renderer backends run rmlui_wrapper_update_game() which processes them.
    if (use_rmlui) {
        rmlui_game_hud_update();
        rmlui_mode_menu_update();
        rmlui_option_menu_update();
        rmlui_game_option_update();
        rmlui_title_screen_update();
        rmlui_win_screen_update();
        rmlui_continue_update();
        rmlui_gameover_update();
        rmlui_vs_result_update();
        rmlui_memory_card_update();
        rmlui_sound_menu_update();
        rmlui_sysdir_update();
        rmlui_network_lobby_update();
        rmlui_extra_option_update();
        rmlui_training_menus_update();
        rmlui_button_config_update();
        rmlui_char_select_update();
        rmlui_vs_screen_update();
        rmlui_replay_picker_update();
        rmlui_pause_overlay_update();
        rmlui_trials_hud_update();
        rmlui_copyright_update();
        rmlui_name_entry_update();
        rmlui_exit_confirm_update();
    }

    if (g_renderer_backend == RENDERER_SDL2D) {
        // --- SDL2D Backend ---
        if (!game_paused && !present_only_mode) {
            ADX_ProcessTracks();
        }

        SDL_SetRenderTarget(sdl_renderer, NULL);

        const SDL_FRect dst_rect = get_letterbox_rect(win_w, win_h);

        // Blit game canvas to window with letterboxing
        SDL_Texture* canvas = SDLGameRendererSDL_GetCanvas();
        SDL_RenderTexture(sdl_renderer, canvas, NULL, &dst_rect);

        // Render RmlUi game context at window resolution (Phase 3 game screens)
        if (use_rmlui) {
            rmlui_wrapper_update_game();
            rmlui_wrapper_render_game(win_w, win_h, dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h);
        }

        // Debug text
        SDLTextRenderer_DrawDebugBuffer((float)win_w, (float)win_h);
        if (show_debug_hud) {
            char debug_text[64];
            snprintf(debug_text, sizeof(debug_text), "FPS: %.2f", fps);
            float overlay_scale = (float)win_h / 480.0f;
            float base_x = dst_rect.x + (10.0f * overlay_scale);
            float base_y = dst_rect.y + (2.0f * overlay_scale);

            SDLTextRenderer_DrawText(
                debug_text, base_x - 1, base_y - 1, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);
            SDLTextRenderer_DrawText(
                debug_text, base_x, base_y - 1, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);
            SDLTextRenderer_DrawText(
                debug_text, base_x + 1, base_y - 1, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);

            SDLTextRenderer_DrawText(
                debug_text, base_x - 1, base_y, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);
            SDLTextRenderer_DrawText(
                debug_text, base_x + 1, base_y, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);

            SDLTextRenderer_DrawText(
                debug_text, base_x - 1, base_y + 1, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);
            SDLTextRenderer_DrawText(
                debug_text, base_x, base_y + 1, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);
            SDLTextRenderer_DrawText(
                debug_text, base_x + 1, base_y + 1, overlay_scale, 0.0f, 0.0f, 0.0f, (float)win_w, (float)win_h);

            SDLTextRenderer_DrawText(
                debug_text, base_x, base_y, overlay_scale, 1.0f, 1.0f, 1.0f, (float)win_w, (float)win_h);
            SDLTextRenderer_Flush();
        }

        // Render RmlUi overlay (if active)
        if (use_rmlui) {
            rmlui_input_display_update();
            rmlui_frame_display_update();

            if (show_menu)
                rmlui_control_mapping_update();
            if (show_mods_menu)
                rmlui_mods_menu_update();
            if (show_shader_menu)
                rmlui_shader_menu_update();
            if (show_stage_config_menu)
                rmlui_stage_config_update();
            if (show_training_menu)
                rmlui_training_menu_update();

            SDLNetplayUI_SetFPSHistory(fps_history, fps_history_count, (float)fps);
            SDLNetplayUI_Render(win_w, win_h);
            rmlui_netplay_ui_update();

            rmlui_wrapper_render();
        }

        SDL_RenderPresent(sdl_renderer);

        SDLGameRenderer_EndFrame();
        hide_cursor_if_needed();

        // Frame pacing
        Uint64 now = SDL_GetTicksNS();
        if (!frame_rate_uncapped) {
            if (frame_deadline == 0) {
                frame_deadline = now + target_frame_time_ns;
            }
            if (now < frame_deadline) {
                Uint64 sleep_time = frame_deadline - now;
                const Uint64 spin_threshold_ns = 2000000;
                if (sleep_time > spin_threshold_ns) {
                    SDL_DelayNS(sleep_time - spin_threshold_ns);
                }
                while (SDL_GetTicksNS() < frame_deadline) {
                    SDL_CPUPauseInstruction();
                }
                now = SDL_GetTicksNS();
            }
            frame_deadline += target_frame_time_ns;
            if (now > frame_deadline + target_frame_time_ns) {
                frame_deadline = now + target_frame_time_ns;
            }
        }

        frame_counter += 1;
        note_frame_end_time();
        update_fps();
        SDLPad_UpdatePreviousState();
        TRACE_ZONE_END();
        return;
    }

    if (g_renderer_backend == RENDERER_SDLGPU) {
        // --- GPU Backend ---

        // Update RmlUi game context (render happens after canvas blit below)
        if (use_rmlui) {
            rmlui_wrapper_update_game();
        }

        // 1. Post-Process (Canvas -> Swapchain)
        SDL_GPUCommandBuffer* cb = SDLGameRendererGPU_GetCommandBuffer();
        SDL_GPUTexture* canvas = SDLGameRendererGPU_GetCanvasTexture();
        SDL_GPUTexture* swapchain = SDLGameRendererGPU_GetSwapchainTexture();

        // If swapchain is unavailable (window minimized/occluded), skip all rendering
        // and just submit the command buffer. Rendering to a NULL swapchain causes GPU device loss.
        if (!cb || !swapchain) {

            // Must still end the UI frame to balance the NewFrame() call.
            // imgui_wrapper_render() / rmlui_wrapper_render() gracefully handle NULL swapchain.
            if (use_rmlui) {
                rmlui_wrapper_render();
            } else {
                imgui_wrapper_render();
            }
            goto gpu_end_frame_submit;
        }

        if (cb && canvas && swapchain) {
            TRACE_SUB_BEGIN("GPU:PostProcess");
            const SDL_FRect viewport = get_letterbox_rect(win_w, win_h);

            // SDL_SKIP_LIBRASHADER=1 bypasses the librashader Vulkan render path
            // for debugging GPU crashes caused by raw Vulkan command injection.
            static bool skip_librashader = false;
            static bool skip_checked = false;
            if (!skip_checked) {
                const char* skip_env = SDL_getenv("SDL_SKIP_LIBRASHADER");
                skip_librashader = (skip_env != NULL && SDL_strcmp(skip_env, "1") == 0);
                if (skip_librashader)
                    SDL_Log("SKIP_LIBRASHADER: Using SDL_BlitGPUTexture fallback.");
                skip_checked = true;
            }

            // Using new accessor
            if (SDLAppShader_IsLibretroMode() && SDLAppShader_GetManager() && !skip_librashader) {
                int vp_w = (int)viewport.w;
                int vp_h = (int)viewport.h;

                // Clear swapchain to black for letterbox bars
                {
                    SDL_GPUColorTargetInfo clear_target;
                    SDL_zero(clear_target);
                    clear_target.texture = swapchain;
                    clear_target.load_op = SDL_GPU_LOADOP_CLEAR;
                    clear_target.store_op = SDL_GPU_STOREOP_STORE;
                    clear_target.clear_color = (SDL_FColor) { 0.0f, 0.0f, 0.0f, 1.0f };
                    SDL_GPURenderPass* clear_pass = SDL_BeginGPURenderPass(cb, &clear_target, 1, NULL);
                    if (clear_pass)
                        SDL_EndGPURenderPass(clear_pass);
                }

                // Ensure intermediate render target matches viewport size
                if (!s_librashader_intermediate || s_librashader_intermediate_w != vp_w ||
                    s_librashader_intermediate_h != vp_h) {
                    if (s_librashader_intermediate) {
                        SDL_ReleaseGPUTexture(gpu_device, s_librashader_intermediate);
                    }
                    SDL_GPUTextureCreateInfo tex_info;
                    SDL_zero(tex_info);
                    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
                    tex_info.format = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
                    tex_info.width = vp_w;
                    tex_info.height = vp_h;
                    tex_info.layer_count_or_depth = 1;
                    tex_info.num_levels = 1;
                    tex_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
                    s_librashader_intermediate = SDL_CreateGPUTexture(gpu_device, &tex_info);
                    s_librashader_intermediate_w = vp_w;
                    s_librashader_intermediate_h = vp_h;
                }

                // Two-stage render (matches GL backend):
                // 1. Librashader renders to intermediate at {0,0}
                // 2. Raw vkCmdBlitImage copies to swapchain at letterbox offset
                LibrashaderManager_Render_GPU_Wrapper(SDLAppShader_GetManager(),
                                                      cb,
                                                      canvas,
                                                      s_librashader_intermediate,
                                                      swapchain,
                                                      384,
                                                      224,
                                                      vp_w,
                                                      vp_h,
                                                      win_w,
                                                      win_h,
                                                      (int)viewport.x,
                                                      (int)viewport.y);
            } else {
                // Manual Blit with Scaling
                SDL_GPUBlitInfo blit_info;
                SDL_zero(blit_info);
                blit_info.source.texture = canvas;
                blit_info.source.w = 384;
                blit_info.source.h = 224;
                blit_info.destination.texture = swapchain;
                blit_info.destination.x = viewport.x;
                blit_info.destination.y = viewport.y;
                blit_info.destination.w = viewport.w;
                blit_info.destination.h = viewport.h;
                blit_info.load_op = SDL_GPU_LOADOP_CLEAR;
                blit_info.clear_color = (SDL_FColor) { 0.0f, 0.0f, 0.0f, 1.0f };
                blit_info.filter = (scale_mode == SCALEMODE_NEAREST || scale_mode == SCALEMODE_INTEGER ||
                                    scale_mode == SCALEMODE_SQUARE_PIXELS)
                                       ? SDL_GPU_FILTER_NEAREST
                                       : SDL_GPU_FILTER_LINEAR;

                SDL_BlitGPUTexture(cb, &blit_info);
            }
            TRACE_SUB_END();
        }

#if defined(DEBUG)
        // Debug Buffer (PS2)
        SDLTextRenderer_DrawDebugBuffer((float)win_w, (float)win_h);
#endif

        // Bezel Rendering
        TRACE_SUB_BEGIN("GPU:BezelRender");
        if (BezelSystem_IsVisible()) {
            int p1 = My_char[0];
            int p2 = My_char[1];

            if (!(G_No[0] == 2 && G_No[1] >= 2)) {
                p1 = -1;
                p2 = -1;
            }

            if (p1 != last_p1_char || p2 != last_p2_char) {
                last_p1_char = p1;
                last_p2_char = p2;
                BezelSystem_SetCharacters(last_p1_char, last_p2_char);
                bezel_vbo_dirty = true;
            }

            static SDL_FRect cached_left = { 0 }, cached_right = { 0 };
            static BezelTextures cached_bezels = { 0 };
            static float bezel_vertex_data[2 * 6 * 4] = { 0 };

            if (bezel_vbo_dirty) {
                const SDL_FRect viewport = get_letterbox_rect(win_w, win_h);
                BezelSystem_CalculateLayout(win_w, win_h, &viewport, &cached_left, &cached_right);
                BezelSystem_GetTextures(&cached_bezels);

                build_bezel_vertices(&cached_left, win_w, win_h, &bezel_vertex_data[0]);
                build_bezel_vertices(&cached_right, win_w, win_h, &bezel_vertex_data[24]);

                // Upload to GPU
                void* ptr = SDL_MapGPUTransferBuffer(gpu_device, s_bezel_transfer_buffer, true);
                if (ptr) {
                    memcpy(ptr, bezel_vertex_data, sizeof(bezel_vertex_data));
                    SDL_UnmapGPUTransferBuffer(gpu_device, s_bezel_transfer_buffer);

                    SDL_GPUCommandBuffer* cb = SDLGameRendererGPU_GetCommandBuffer();
                    if (cb) {
                        SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cb);
                        SDL_GPUTransferBufferLocation src = { .transfer_buffer = s_bezel_transfer_buffer, .offset = 0 };
                        SDL_GPUBufferRegion dst = { .buffer = s_bezel_vertex_buffer,
                                                    .offset = 0,
                                                    .size = sizeof(bezel_vertex_data) };
                        SDL_UploadToGPUBuffer(cp, &src, &dst, false);
                        SDL_EndGPUCopyPass(cp);
                    }
                }
                bezel_vbo_dirty = false;
            }

            SDL_GPUCommandBuffer* cb = SDLGameRendererGPU_GetCommandBuffer();
            if (cb && s_bezel_pipeline) {
                SDL_GPUTexture* swapchain = SDLGameRendererGPU_GetSwapchainTexture();
                if (swapchain) {
                    SDL_GPUColorTargetInfo target;
                    SDL_zero(target);
                    target.texture = swapchain;
                    target.load_op = SDL_GPU_LOADOP_LOAD;
                    target.store_op = SDL_GPU_STOREOP_STORE;

                    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, &target, 1, NULL);
                    if (pass) {
                        SDL_BindGPUGraphicsPipeline(pass, s_bezel_pipeline);

                        SDL_GPUBufferBinding vb = { .buffer = s_bezel_vertex_buffer, .offset = 0 };
                        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

                        // Draw Left
                        if (cached_bezels.left) {
                            SDL_GPUTextureSamplerBinding tb = { .texture = (SDL_GPUTexture*)cached_bezels.left,
                                                                .sampler = s_bezel_sampler };
                            SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
                            SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
                        }
                        // Draw Right
                        if (cached_bezels.right) {
                            SDL_GPUTextureSamplerBinding tb = { .texture = (SDL_GPUTexture*)cached_bezels.right,
                                                                .sampler = s_bezel_sampler };
                            SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
                            SDL_DrawGPUPrimitives(pass, 6, 1, 6, 0);
                        }
                        SDL_EndGPURenderPass(pass);
                    }
                }
            }
        }
        TRACE_SUB_END();

        // Phase 3 game UI at window resolution (after canvas blit + bezels)
        if (use_rmlui) {
            const SDL_FRect gp_vp = get_letterbox_rect(win_w, win_h);
            rmlui_wrapper_render_game(win_w, win_h, gp_vp.x, gp_vp.y, gp_vp.w, gp_vp.h);
        }

        // UI Overlays
        TRACE_SUB_BEGIN("GPU:UIOverlays");
        if (use_rmlui) {
            rmlui_input_display_update();
            rmlui_frame_display_update();
        } else {
            if (g_training_menu_settings.show_inputs) {
                input_display_render();
            }
            frame_display_render();
        }
        if (show_menu) {
            if (use_rmlui) {
                rmlui_control_mapping_update();
            } else {
                imgui_wrapper_show_control_mapping_window(win_w, win_h);
            }
        }
        if (show_mods_menu) {
            if (use_rmlui) {
                rmlui_mods_menu_update();
            } else {
                mods_menu_render(win_w, win_h);
            }
        }
        if (show_shader_menu) {
            if (use_rmlui) {
                rmlui_shader_menu_update();
            } else {
                shader_menu_render(win_w, win_h);
            }
        }
        if (use_rmlui) {
            if (show_stage_config_menu)
                rmlui_stage_config_update();
            if (show_training_menu)
                rmlui_training_menu_update();
        } else {
            stage_config_menu_render(win_w, win_h);
            training_menu_render(win_w, win_h);
        }

        SDLNetplayUI_SetFPSHistory(fps_history, fps_history_count, (float)fps);
        SDLNetplayUI_Render(win_w, win_h);
        if (use_rmlui) {
            rmlui_netplay_ui_update();
        }

        if (use_rmlui) {
            rmlui_wrapper_render();
        } else {
            imgui_wrapper_render();
        }
        TRACE_SUB_END();

        if (show_debug_hud) {
            // Debug Text Overlay
            char debug_text[512];
            char fps_text[64];
            char mode_text[128];
            char shader_text[128];
            const SDL_FRect viewport = get_letterbox_rect(win_w, win_h);
            float overlay_scale = ((float)win_h / 480.0f) * 0.8f;
            float base_x = viewport.x + (10.0f * overlay_scale);
            float base_y = 0.0f;

            snprintf(fps_text, sizeof(fps_text), "FPS: %.2f%s", fps, frame_rate_uncapped ? " UNCAPPED [F5]" : "");

            if (SDLAppShader_IsLibretroMode()) {
                if (SDLAppShader_GetAvailableCount() > 0) {
                    snprintf(mode_text,
                             sizeof(mode_text),
                             "Preset: %s [F9]",
                             SDLAppShader_GetPresetName(SDLAppShader_GetCurrentIndex()));
                } else {
                    snprintf(mode_text, sizeof(mode_text), "Preset: None found");
                }
            } else {
                snprintf(mode_text,
                         sizeof(mode_text),
                         "Scale: %s [F8]%s",
                         scale_mode_name(),
                         BezelSystem_IsVisible() ? " (Bezels On)" : "");
            }

            snprintf(shader_text,
                     sizeof(shader_text),
                     "Shader Mode: %s [F4]",
                     SDLAppShader_IsLibretroMode() ? "Libretro" : "Internal");

            snprintf(debug_text, sizeof(debug_text), "%s | %s | %s", fps_text, shader_text, mode_text);

            SDLTextRenderer_SetBackgroundEnabled(1);
            SDLTextRenderer_SetBackgroundColor(0.0f, 0.0f, 0.0f, 0.5f);

            SDLTextRenderer_DrawText(debug_text, base_x + 1, base_y + 1, overlay_scale, 0.0f, 0.0f, 0.0f, win_w, win_h);
            SDLTextRenderer_DrawText(debug_text, base_x, base_y, overlay_scale, 1.0f, 1.0f, 1.0f, win_w, win_h);

            SDLTextRenderer_SetBackgroundEnabled(0);
        }

        // Flush Text Renderer (draws buffered text)
        SDLTextRenderer_Flush();

    gpu_end_frame_submit:; // empty statement after label for C compliance

    } else {
        // --- OpenGL Backend ---

        // Update RmlUi game context (render happens after canvas blit below)
        if (use_rmlui) {
            rmlui_wrapper_update_game();
        }

        if (broadcast_config.enabled && broadcast_config.source == BROADCAST_SOURCE_NATIVE) {
            Broadcast_Send(cps3_canvas_texture, 384, 224, true);
        }

        // Get window dimensions and set viewport for final blit
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        const SDL_FRect viewport = get_letterbox_rect(win_w, win_h);

        // Common setup
        // ⚡ Bolt: Compile-time constant — avoids 64-byte stack init every frame.
        static const float identity[4][4] = { { 1.0f, 0.0f, 0.0f, 0.0f },
                                              { 0.0f, 1.0f, 0.0f, 0.0f },
                                              { 0.0f, 0.0f, 1.0f, 0.0f },
                                              { 0.0f, 0.0f, 0.0f, 1.0f } };

        // ⚡ Bolt: Canvas dimensions are always 384×224 (set in SDLGameRenderer_Init),
        // so use constants instead of querying the GPU every frame. Eliminates 2
        // glGetTexLevelParameteriv round-trips per frame (~20-100µs on RPi4 V3D).
        const int tex_w = 384;
        const int tex_h = 224;

        TRACE_SUB_BEGIN("SceneBlit");
        if (SDLAppShader_IsLibretroMode() && SDLAppShader_GetManager()) {
            bool modded_active_lr = ModdedStage_IsActiveForCurrentStage();
            bool bypass_shader = mods_menu_shader_bypass_enabled;

            // Standard Path: Bypass Enabled OR Modded Stage Inactive
            // Render transparency over HD background (if active)
            if (bypass_shader || !modded_active_lr) {
                if (modded_active_lr) {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glViewport(viewport.x, viewport.y, viewport.w, viewport.h);
                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
                    ModdedStage_Render(&bg_w);
                } else {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glViewport(viewport.x, viewport.y, viewport.w, viewport.h);
                    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
                }

                if (modded_active_lr) {
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }

                LibrashaderManager_Render(SDLAppShader_GetManager(),
                                          (void*)(intptr_t)cps3_canvas_texture,
                                          tex_w,
                                          tex_h,
                                          viewport.x,
                                          viewport.y,
                                          viewport.w,
                                          viewport.h);

                if (modded_active_lr) {
                    glDisable(GL_BLEND);
                }
            }
            // Composition Path: Bypass Disabled AND Modded Stage Active
            // Render HD Stage + Sprites to FBO, then Shade everything
            else {
                // Resize/Create Composition FBO if needed
                int vp_w = (int)viewport.w;
                int vp_h = (int)viewport.h;

                if (vp_w != s_composition_w || vp_h != s_composition_h || s_composition_fbo == 0) {
                    if (s_composition_texture)
                        glDeleteTextures(1, &s_composition_texture);
                    if (s_composition_fbo)
                        glDeleteFramebuffers(1, &s_composition_fbo);

                    glGenFramebuffers(1, &s_composition_fbo);
                    glBindFramebuffer(GL_FRAMEBUFFER, s_composition_fbo);

                    glGenTextures(1, &s_composition_texture);
                    glBindTexture(GL_TEXTURE_2D, s_composition_texture);
                    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, vp_w, vp_h);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                    glFramebufferTexture2D(
                        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_composition_texture, 0);

                    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Composition FBO incomplete!");
                    }

                    s_composition_w = vp_w;
                    s_composition_h = vp_h;
                }

                // 1. Bind Composition FBO
                glBindFramebuffer(GL_FRAMEBUFFER, s_composition_fbo);
                glViewport(0, 0, vp_w, vp_h);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                // 2. Render HD Stage
                // Note: ModdedStage_Render sets its own viewport if needed, but usually
                // relies on current viewport. It also sets its own shader/projection.
                // We need to ensure it draws to our FBO size.
                // ModdedStage usually draws to "screen", assumes native resolution or viewport.
                // Here we are drawing to an FBO of size 'viewport.w x viewport.h'.
                // So glViewport(0,0,w,h) is correct relative to FBO.

                // !!! ModdedStage_Render might change viewport! check/fix if needed
                ModdedStage_Render(&bg_w);

                // 3. Render Game Sprites (cps3_canvas_texture) on top
                // Use Passthru shader to blit transparency
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                glUseProgram(passthru_shader_program);
                if (s_pt_loc_projection == -1) {
                    s_pt_loc_projection = glGetUniformLocation(passthru_shader_program, "projection");
                    s_pt_loc_source = glGetUniformLocation(passthru_shader_program, "Source");
                    s_pt_loc_source_size = glGetUniformLocation(passthru_shader_program, "SourceSize");
                    s_pt_loc_filter_type = glGetUniformLocation(passthru_shader_program, "u_filter_type");
                }

                static const float identity[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                                    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };

                glUniformMatrix4fv(s_pt_loc_projection, 1, GL_FALSE, (const float*)identity);
                glUniform4f(s_pt_loc_source_size, (float)tex_w, (float)tex_h, 1.0f / (float)tex_w, 1.0f / (float)tex_h);
                glUniform1i(s_pt_loc_filter_type, (scale_mode == SCALEMODE_PIXEL_ART) ? 1 : 0);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, cps3_canvas_texture);
                glUniform1i(s_pt_loc_source, 0);

                glBindVertexArray(vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                glDisable(GL_BLEND);

                // 4. Feed Composed Texture to Librashader
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(viewport.x, viewport.y, viewport.w, viewport.h);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                LibrashaderManager_Render(SDLAppShader_GetManager(),
                                          (void*)(intptr_t)s_composition_texture,
                                          vp_w,
                                          vp_h,
                                          viewport.x,
                                          viewport.y,
                                          viewport.w,
                                          viewport.h);
            }
        } else {
            // Standard single-pass rendering (Passthru)
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(viewport.x, viewport.y, viewport.w, viewport.h);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // --- HD Modded Stage Background (native resolution) ---
            // Draw HD parallax layers BEFORE the canvas blit so they appear
            // behind all game sprites. The canvas FBO was cleared with alpha=0,
            // so we enable blending for the blit to composite correctly.
            bool modded_active = ModdedStage_IsActiveForCurrentStage();
            if (modded_active) {
                ModdedStage_Render(&bg_w);
            }

            GLuint current_shader = passthru_shader_program;
            glUseProgram(current_shader);

            // ⚡ Bolt: Cache uniform locations — resolve once, reuse every frame.
            if (s_pt_loc_projection == -1) {
                s_pt_loc_projection = glGetUniformLocation(current_shader, "projection");
                s_pt_loc_source = glGetUniformLocation(current_shader, "Source");
                s_pt_loc_source_size = glGetUniformLocation(current_shader, "SourceSize");
                s_pt_loc_filter_type = glGetUniformLocation(current_shader, "u_filter_type");
            }

            glUniformMatrix4fv(s_pt_loc_projection, 1, GL_FALSE, (const float*)identity);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, cps3_canvas_texture);
            glUniform1i(s_pt_loc_source, 0);

            glUniform4f(s_pt_loc_source_size, (float)tex_w, (float)tex_h, 1.0f / (float)tex_w, 1.0f / (float)tex_h);
            glUniform1i(s_pt_loc_filter_type, (scale_mode == SCALEMODE_PIXEL_ART) ? 1 : 0);

            // ⚡ Bolt: Only update texture filter params when scale_mode changes.
            // glTexParameteri triggers internal sampler state revalidation in the
            // driver; on V3D (RPi4) this adds ~5-15µs per call. Since scale_mode
            // only changes on F8 key press, we skip 2 GL calls/frame in the
            // common case (99.9% of frames).
            static int s_last_filter_scale_mode = -1;
            if (s_last_filter_scale_mode != scale_mode) {
                s_last_filter_scale_mode = scale_mode;
                if (scale_mode == SCALEMODE_NEAREST || scale_mode == SCALEMODE_SQUARE_PIXELS ||
                    scale_mode == SCALEMODE_INTEGER) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                } else {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                }
            }

            // When HD background is active, enable blending so the transparent
            // canvas pixels let the HD background show through.
            if (modded_active) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }

            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            if (modded_active) {
                glDisable(GL_BLEND);
            }
        }
        TRACE_SUB_END();

        // Bezel Rendering (OpenGL)
        TRACE_SUB_BEGIN("GL:BezelRender");
        if (BezelSystem_IsVisible()) {
            int p1 = My_char[0];
            int p2 = My_char[1];

            if (!(G_No[0] == 2 && G_No[1] >= 2)) {
                p1 = -1;
                p2 = -1;
            }

            if (p1 != last_p1_char || p2 != last_p2_char) {
                last_p1_char = p1;
                last_p2_char = p2;
                BezelSystem_SetCharacters(last_p1_char, last_p2_char);
                bezel_vbo_dirty = true;
            }

            static SDL_FRect cached_left = { 0 }, cached_right = { 0 };
            static BezelTextures cached_bezels = { 0 };
            static float bezel_vertex_data[2 * 6 * 4] = { 0 };

            if (bezel_vbo_dirty) {
                BezelSystem_CalculateLayout(win_w, win_h, &viewport, &cached_left, &cached_right);
                BezelSystem_GetTextures(&cached_bezels);

                build_bezel_vertices(&cached_left, win_w, win_h, &bezel_vertex_data[0]);
                build_bezel_vertices(&cached_right, win_w, win_h, &bezel_vertex_data[24]);

                glBindBuffer(GL_ARRAY_BUFFER, bezel_vbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(bezel_vertex_data), bezel_vertex_data, GL_DYNAMIC_DRAW);
                bezel_vbo_dirty = false;
            }

            if (cached_bezels.left || cached_bezels.right) {
                // Reset viewport to full window for NDC-based bezel quads
                glViewport(0, 0, win_w, win_h);

                glUseProgram(passthru_shader_program);
                glUniformMatrix4fv(s_pt_loc_projection, 1, GL_FALSE, (const float*)identity);
                glUniform1i(s_pt_loc_filter_type, 0); // nearest for bezels

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                glBindVertexArray(bezel_vao);

                // Draw Left
                if (cached_bezels.left) {
                    GLuint tex = (GLuint)(intptr_t)cached_bezels.left;
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glUniform1i(s_pt_loc_source, 0);

                    int tw = 0, th = 0;
                    TextureUtil_GetSize(cached_bezels.left, &tw, &th);
                    glUniform4f(s_pt_loc_source_size,
                                (float)tw,
                                (float)th,
                                tw > 0 ? 1.0f / (float)tw : 0.0f,
                                th > 0 ? 1.0f / (float)th : 0.0f);

                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                // Draw Right
                if (cached_bezels.right) {
                    GLuint tex = (GLuint)(intptr_t)cached_bezels.right;
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glUniform1i(s_pt_loc_source, 0);

                    int tw = 0, th = 0;
                    TextureUtil_GetSize(cached_bezels.right, &tw, &th);
                    glUniform4f(s_pt_loc_source_size,
                                (float)tw,
                                (float)th,
                                tw > 0 ? 1.0f / (float)tw : 0.0f,
                                th > 0 ? 1.0f / (float)th : 0.0f);

                    glDrawArrays(GL_TRIANGLES, 6, 6);
                }

                glDisable(GL_BLEND);
                glBindVertexArray(0);
            }
        }
        TRACE_SUB_END();

        // Phase 3 game UI at window resolution (after canvas blit + bezels)
        if (use_rmlui) {
            // Reset GL state to clean baseline before RmlUi rendering
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
            glBindVertexArray(0);
            glViewport(0, 0, win_w, win_h);

            rmlui_wrapper_render_game(win_w, win_h, viewport.x, viewport.y, viewport.w, viewport.h);
        }

        if (show_debug_hud) {
            // Render debug text in screen space (on top of everything)
            // Reset GL state that might have been changed by multi-pass shaders
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);
            glUseProgram(0);

            // Reset viewport to full window for text rendering
            glViewport(0, 0, win_w, win_h);

            SDLTextRenderer_DrawDebugBuffer((float)win_w, (float)win_h);

            char debug_text[512];
            char fps_text[64];
            char mode_text[128];
            char shader_text[128];

            snprintf(fps_text, sizeof(fps_text), "FPS: %.2f%s", fps, frame_rate_uncapped ? " UNCAPPED [F5]" : "");

            if (SDLAppShader_IsLibretroMode()) {
                if (SDLAppShader_GetAvailableCount() > 0) {
                    snprintf(mode_text,
                             sizeof(mode_text),
                             "Preset: %s [F9]",
                             SDLAppShader_GetPresetName(SDLAppShader_GetCurrentIndex()));
                } else {
                    snprintf(mode_text, sizeof(mode_text), "Preset: None found");
                }
            } else {
                snprintf(mode_text,
                         sizeof(mode_text),
                         "Scale: %s [F8]%s",
                         scale_mode_name(),
                         BezelSystem_IsVisible() ? " (Bezels On)" : "");
            }

            snprintf(shader_text,
                     sizeof(shader_text),
                     "Shader Mode: %s [F4]",
                     SDLAppShader_IsLibretroMode() ? "Libretro" : "Internal");

            snprintf(debug_text, sizeof(debug_text), "%s | %s | %s", fps_text, shader_text, mode_text);

            float overlay_scale = ((float)win_h / 480.0f) * 0.8f;
            float base_x = viewport.x + (10.0f * overlay_scale);
            float base_y = 0.0f;

            SDLTextRenderer_SetBackgroundEnabled(1);
            SDLTextRenderer_SetBackgroundColor(0.0f, 0.0f, 0.0f, 0.5f);

            SDLTextRenderer_DrawText(debug_text, base_x + 1, base_y + 1, overlay_scale, 0.0f, 0.0f, 0.0f, win_w, win_h);
            SDLTextRenderer_DrawText(debug_text, base_x, base_y, overlay_scale, 1.0f, 1.0f, 1.0f, win_w, win_h);

            SDLTextRenderer_SetBackgroundEnabled(0);
        }

        if (use_rmlui) {
            rmlui_input_display_update();
            rmlui_frame_display_update();
        } else {
            if (g_training_menu_settings.show_inputs) {
                input_display_render();
            }
            frame_display_render();
        }

        if (show_menu) {
            int w, h;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            if (use_rmlui) {
                rmlui_control_mapping_update();
            } else {
                imgui_wrapper_show_control_mapping_window(w, h);
            }
        }
        if (show_shader_menu) {
            if (use_rmlui) {
                rmlui_shader_menu_update();
            } else {
                int w, h;
                SDL_GetWindowSizeInPixels(window, &w, &h);
                shader_menu_render(w, h);
            }
        }
        if (show_mods_menu) {
            if (use_rmlui) {
                rmlui_mods_menu_update();
            } else {
                int w, h;
                SDL_GetWindowSizeInPixels(window, &w, &h);
                mods_menu_render(w, h);
            }
        }

        {
            int w, h;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            if (use_rmlui) {
                if (show_stage_config_menu)
                    rmlui_stage_config_update();
                if (show_training_menu)
                    rmlui_training_menu_update();
            } else {
                stage_config_menu_render(w, h);
                training_menu_render(w, h);
            }
        }

        SDLNetplayUI_SetFPSHistory(fps_history, fps_history_count, (float)fps);
        SDLNetplayUI_Render(win_w, win_h);
        if (use_rmlui) {
            rmlui_netplay_ui_update();
        }

        if (use_rmlui) {
            rmlui_wrapper_render();
        } else {
            imgui_wrapper_render();
        }

        // Final Output broadcast: capture the fully-composited frame
        if (broadcast_config.enabled && broadcast_config.source == BROADCAST_SOURCE_FINAL) {
            // Allocate/resize the broadcast FBO on demand
            if (win_w != s_broadcast_w || win_h != s_broadcast_h || s_broadcast_fbo == 0) {
                if (s_broadcast_texture)
                    glDeleteTextures(1, &s_broadcast_texture);
                if (s_broadcast_fbo)
                    glDeleteFramebuffers(1, &s_broadcast_fbo);

                glGenFramebuffers(1, &s_broadcast_fbo);
                glBindFramebuffer(GL_FRAMEBUFFER, s_broadcast_fbo);

                glGenTextures(1, &s_broadcast_texture);
                glBindTexture(GL_TEXTURE_2D, s_broadcast_texture);
                glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, win_w, win_h);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_broadcast_texture, 0);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                s_broadcast_w = win_w;
                s_broadcast_h = win_h;
            }

            // Blit the default framebuffer into the broadcast texture (GPU-to-GPU)
            // Flip Y during blit: OpenGL's default FB is bottom-up, but Spout
            // receivers (OBS etc.) expect top-down. Swapping dst Y does the flip.
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_broadcast_fbo);
            glBlitFramebuffer(0, 0, win_w, win_h, 0, win_h, win_w, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            Broadcast_Send(s_broadcast_texture, win_w, win_h, false);
        }

        // Swap the window to display the final rendered frame
        TRACE_SUB_BEGIN("SwapWindow");
        SDL_GL_SwapWindow(window);
        TRACE_SUB_END();
    }
    TRACE_GPU_COLLECT();

    // Now that the frame is displayed, clean up resources for the next frame
    SDLGameRenderer_EndFrame();

    // Run sound processing — after GPU submit so CPU audio decode
    // overlaps with GPU processing the submitted command buffer.
    TRACE_SUB_BEGIN("AudioProcess");
    if (!game_paused && !present_only_mode) {
        ADX_ProcessTracks();
    }
    TRACE_SUB_END();

    if (should_save_screenshot) {
        save_screenshot("screenshot.bmp");
        should_save_screenshot = false;
    }

    // Handle cursor hiding
    hide_cursor_if_needed();

    // Do frame pacing (skipped when uncapped for benchmarking)
    Uint64 now = SDL_GetTicksNS();
    TRACE_SUB_BEGIN("FramePacing");

    if (!frame_rate_uncapped) {
        if (frame_deadline == 0) {
            frame_deadline = now + target_frame_time_ns;
        }

        if (now < frame_deadline) {
            Uint64 sleep_time = frame_deadline - now;
            // ⚡ Bolt: Hybrid sleep+spin — kernel timer jitter on RPi4 is 1-4ms.
            // Sleep for the bulk, then spin-wait for the final 2ms to hit
            // the deadline precisely. Eliminates ~2ms frame-time jitter.
            const Uint64 spin_threshold_ns = 2000000; // 2ms
            if (sleep_time > spin_threshold_ns) {
                SDL_DelayNS(sleep_time - spin_threshold_ns);
            }
            // Spin-wait for remaining time — SDL_CPUPauseInstruction emits
            // 'yield' on ARM (reduces power/heat) or 'pause' on x86.
            while (SDL_GetTicksNS() < frame_deadline) {
                SDL_CPUPauseInstruction();
            }
            now = SDL_GetTicksNS();
        }

        frame_deadline += target_frame_time_ns;

        // If we fell behind by more than one frame, resync to avoid spiraling
        if (now > frame_deadline + target_frame_time_ns) {
            frame_deadline = now + target_frame_time_ns;
        }
    }
    TRACE_SUB_END();

    // Measure
    frame_counter += 1;
    note_frame_end_time();
    update_fps();
    SDLPad_UpdatePreviousState();
    TRACE_ZONE_END();
}

/** @brief Request application exit. */
void SDLApp_Exit() {
    SDL_Event quit_event;
    quit_event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quit_event);
}

/** @brief Get the passthrough shader program handle. */
unsigned int SDLApp_GetPassthruShaderProgram() {
    return passthru_shader_program;
}

/** @brief Get the scene (2D) shader program handle. */
unsigned int SDLApp_GetSceneShaderProgram() {
    return scene_shader_program;
}

/** @brief Get the scene texture-array shader program handle. */
unsigned int SDLApp_GetSceneArrayShaderProgram() {
    return scene_array_shader_program;
}
// Scale Mode accessors
int SDLApp_GetScaleMode() {
    return scale_mode;
}

void SDLApp_SetScaleMode(int mode) {
    if (mode >= 0 && mode < SCALEMODE_COUNT) {
        scale_mode = mode;
    }
}

const char* SDLApp_GetScaleModeName(int mode) {
    ScaleMode old_mode = scale_mode;
    scale_mode = mode;
    const char* name = scale_mode_name();
    scale_mode = old_mode;
    return name;
}

// Shader Mode accessors
bool SDLApp_GetShaderModeLibretro() {
    return SDLAppShader_IsLibretroMode();
}

void SDLApp_SetShaderModeLibretro(bool libretro) {
    SDLAppShader_SetMode(libretro);
}

// Preset accessors
int SDLApp_GetCurrentPresetIndex() {
    return SDLAppShader_GetCurrentIndex();
}

void SDLApp_SetCurrentPresetIndex(int index) {
    SDLAppShader_SetCurrentIndex(index);
}

int SDLApp_GetAvailablePresetCount() {
    return SDLAppShader_GetAvailableCount();
}

const char* SDLApp_GetPresetName(int index) {
    return SDLAppShader_GetPresetName(index);
}

void SDLApp_LoadPreset(int index) {
    SDLAppShader_LoadPreset(index);
}

void SDLApp_SetWindowPosition(int x, int y) {
    g_cli_window_x = x;
    g_cli_window_y = y;
}

void SDLApp_SetWindowSize(int width, int height) {
    g_cli_window_width = width;
    g_cli_window_height = height;
}

void SDLApp_SetRenderer(RendererBackend backend) {
    g_renderer_backend = backend;
}

RendererBackend SDLApp_GetRenderer(void) {
    return g_renderer_backend;
}

SDL_GPUDevice* SDLApp_GetGPUDevice(void) {
    return gpu_device;
}

SDL_Renderer* SDLApp_GetSDLRenderer(void) {
    return sdl_renderer;
}

SDL_Window* SDLApp_GetWindow(void) {
    return window;
}

// ==============================================================================================
// Implementation of Internal Public Functions for Input Handling (replacing static handlers)
// ==============================================================================================

void SDLApp_ToggleMenu() {
    show_menu = !show_menu;
    game_paused = show_menu;
    if (show_menu) {
        SDL_ShowCursor();
    }
    if (use_rmlui) {
        if (!show_menu) {
            rmlui_wrapper_hide_document("control_mapping");
        }
    }
}

void SDLApp_ToggleModsMenu() {
    show_mods_menu = !show_mods_menu;
    game_paused = show_mods_menu || show_menu;
    if (show_mods_menu) {
        SDL_ShowCursor();
    }
    if (use_rmlui) {
        if (show_mods_menu) {
            rmlui_wrapper_show_document("mods");
        } else {
            rmlui_wrapper_hide_document("mods");
        }
    }
}

void SDLApp_ToggleShaderMenu() {
    show_shader_menu = !show_shader_menu;
    game_paused = show_shader_menu || show_menu;
    if (show_shader_menu) {
        SDL_ShowCursor();
    }
    if (use_rmlui) {
        if (show_shader_menu) {
            rmlui_wrapper_show_document("shaders");
        } else {
            rmlui_wrapper_hide_document("shaders");
        }
    }
}

void SDLApp_ToggleStageConfigMenu() {
    show_stage_config_menu = !show_stage_config_menu;
    if (show_stage_config_menu)
        SDL_ShowCursor();
    if (use_rmlui) {
        if (show_stage_config_menu) {
            rmlui_wrapper_show_document("stage_config");
        } else {
            rmlui_wrapper_hide_document("stage_config");
        }
    }
}

void SDLApp_ToggleTrainingMenu() {
    show_training_menu = !show_training_menu;
    if (show_training_menu)
        SDL_ShowCursor();
    if (use_rmlui) {
        if (show_training_menu) {
            rmlui_wrapper_show_document("training");
        } else {
            rmlui_wrapper_hide_document("training");
        }
    }
}

void SDLApp_CycleScaleMode() {
    cycle_scale_mode();
}

void SDLApp_ToggleFullscreen() {
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);

    if (flags & SDL_WINDOW_FULLSCREEN) {
        SDL_SetWindowFullscreen(window, false);
        Config_SetBool(CFG_KEY_FULLSCREEN, false);
    } else {
        // Apply user-defined fullscreen resolution before entering fullscreen.
        // If both dimensions are set, SDL3 enters exclusive fullscreen at that
        // resolution; otherwise it falls back to desktop borderless.
        int fs_w = Config_GetInt(CFG_KEY_FULLSCREEN_WIDTH);
        int fs_h = Config_GetInt(CFG_KEY_FULLSCREEN_HEIGHT);
        if (fs_w > 0 && fs_h > 0) {
            SDL_DisplayMode mode = { 0 };
            mode.w = fs_w;
            mode.h = fs_h;
            if (!SDL_SetWindowFullscreenMode(window, &mode)) {
                SDL_Log("ToggleFullscreen: Could not set mode %dx%d: %s", fs_w, fs_h, SDL_GetError());
            }
        } else {
            // Ensure desktop borderless (NULL = use desktop mode)
            SDL_SetWindowFullscreenMode(window, NULL);
        }
        SDL_SetWindowFullscreen(window, true);
        Config_SetBool(CFG_KEY_FULLSCREEN, true);
    }
}

void SDLApp_HandleMouseMotion() {
    last_mouse_motion_time = SDL_GetTicks();
    if (!cursor_visible) {
        SDL_ShowCursor();
        cursor_visible = true;
    }
}

void SDLApp_SaveScreenshot() {
    should_save_screenshot = true;
}

void SDLApp_ToggleShaderMode() {
    SDLAppShader_ToggleMode();
}

void SDLApp_CyclePreset() {
    SDLAppShader_CyclePreset();
}

void SDLApp_ToggleBezel() {
    bool visible = !BezelSystem_IsVisible();
    BezelSystem_SetVisible(visible);
    Config_SetBool(CFG_KEY_BEZEL_ENABLED, visible);
    SDL_Log("Bezels toggled: %s", visible ? "ON" : "OFF");
}

void SDLApp_HandleWindowResize(int w, int h) {
    bezel_vbo_dirty = true; // ⚡ Bolt: Force bezel VBO re-upload

    // Check fullscreen using SDL3 API directly if needed, or rely on window
    // SDL_GetWindowFlags(window) should work
    if (window && !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)) {
        Config_SetInt(CFG_KEY_WINDOW_WIDTH, w);
        Config_SetInt(CFG_KEY_WINDOW_HEIGHT, h);
    }

    if (g_renderer_backend == RENDERER_OPENGL) {
        const SDL_FRect viewport = get_letterbox_rect(w, h);
        glViewport(viewport.x, viewport.y, viewport.w, viewport.h);
    }
}

void SDLApp_HandleWindowMove(int x, int y) {
    if (window && !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)) {
        Config_SetInt(CFG_KEY_WINDOW_X, x);
        Config_SetInt(CFG_KEY_WINDOW_Y, y);
    }
}

bool SDLApp_IsMenuVisible() {
    return show_menu;
}

/** @brief Set vsync on the active backend. Independent of the software frame pacer. */
void SDLApp_SetVSync(bool enabled) {
    vsync_enabled = enabled;

    // Always apply to the GPU — vsync is independent of frame rate uncap
    if (g_renderer_backend == RENDERER_OPENGL) {
        SDL_GL_SetSwapInterval(enabled ? 1 : 0);
    } else if (g_renderer_backend == RENDERER_SDLGPU && gpu_device && window) {
        SDL_SetGPUSwapchainParameters(gpu_device,
                                      window,
                                      SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                      enabled ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE);
    } else if (g_renderer_backend == RENDERER_SDL2D && sdl_renderer) {
        SDL_SetRenderVSync(sdl_renderer, enabled ? 1 : 0);
    }

    Config_SetBool(CFG_KEY_VSYNC, enabled);
    SDL_Log("VSync: %s", enabled ? "ON" : "OFF");
}

bool SDLApp_IsVSyncEnabled() {
    return vsync_enabled;
}

void SDLApp_ToggleFrameRateUncap() {
    frame_rate_uncapped = !frame_rate_uncapped;

    // VSync is never touched here — the VSync checkbox controls it independently.
    // When uncapped + VSync ON:  rendering is decoupled, game stays at 59.6 fps
    // When uncapped + VSync OFF: full speed benchmarking (no pacer, no vsync)

    // Reset frame deadline so the pacer doesn't spiral on re-enable
    frame_deadline = 0;

    SDL_Log("Frame rate %s", frame_rate_uncapped ? "UNCAPPED" : "capped (59.6 FPS)");
}

bool SDLApp_IsFrameRateUncapped() {
    return frame_rate_uncapped;
}

/** @brief Re-present the last rendered frame without running game logic.
 *  Used in decoupled mode to render at uncapped FPS while game ticks at 59.6. */
void SDLApp_PresentOnly(void) {
    present_only_mode = true;
    SDLApp_BeginFrame(); // skips SDLGameRenderer_BeginFrame (no FBO clear)
    SDLApp_EndFrame();   // skips SDLGameRenderer_RenderFrame + audio, re-blits existing canvas
    present_only_mode = false;
}

/** @brief Get the target frame time for the game logic tick rate (59.6 fps). */
Uint64 SDLApp_GetTargetFrameTimeNS(void) {
    return target_frame_time_ns;
}

void SDLApp_ClearLibrashaderIntermediate() {
    if (s_librashader_intermediate) {
        SDL_ReleaseGPUTexture(gpu_device, s_librashader_intermediate);
        s_librashader_intermediate = NULL;
        s_librashader_intermediate_w = 0;
        s_librashader_intermediate_h = 0;
    }
}

void SDLApp_ToggleDebugHUD() {
    show_debug_hud = !show_debug_hud;
    Config_SetBool(CFG_KEY_DEBUG_HUD, show_debug_hud);
    SDL_Log("Debug HUD %s", show_debug_hud ? "ON" : "OFF");
}
