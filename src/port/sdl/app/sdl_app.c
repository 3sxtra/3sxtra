/**
 * @file sdl_app.c
 * @brief SDL3 application lifecycle: window, GL context, shaders, input, frame loop.
 *
 * Manages SDL3 initialization, window creation with OpenGL context,
 * shader program compilation (scene + passthrough + texture-array variants),
 * Libretro shader preset loading, scale-mode management, screenshot capture,
 * bezel overlay rendering, and the per-frame present/swap pipeline.
 */
#include "port/sdl/app/sdl_app.h"
#include "common.h"
#include "game_state.h"
#include "netplay/identity.h"
#include "netplay/lobby_server.h"
#include "port/broadcast.h"
#include "port/config/config.h"
#include "port/mods/modded_stage.h"
#include "port/sdl/input/control_mapping.h"
#include "port/sdl/rmlui/rmlui_dev_overlay.h"
#include "port/sdl/rmlui/rmlui_frame_display.h"
#include "port/sdl/rmlui/rmlui_input_display.h"
#include "port/sdl/rmlui/rmlui_mods_menu.h"
#include "port/sdl/rmlui/rmlui_shader_menu.h"
#include "port/sdl/rmlui/rmlui_stage_config.h"
#include "port/sdl/rmlui/rmlui_training_hud.h"
#include "port/sdl/rmlui/rmlui_training_menu.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"
/* Phase 3 — Fight HUD & Mode Menu */
#include "port/rendering/sdl_bezel.h"
#include "port/sdl/app/sdl_app_bezel.h"
#include "port/sdl/app/sdl_app_config.h"
#include "port/sdl/app/sdl_app_debug_hud.h"
#include "port/sdl/app/sdl_app_input.h"
#include "port/sdl/app/sdl_app_internal.h"
#include "port/sdl/app/sdl_app_scale.h"
#include "port/sdl/app/sdl_app_screenshot.h"
#include "port/sdl/app/sdl_app_shader_config.h"

#include "port/config/cli_parser.h"
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "port/sdl/rmlui/rmlui_attract_overlay.h"
#include "port/sdl/rmlui/rmlui_button_config.h"
#include "port/sdl/rmlui/rmlui_char_select.h"
#include "port/sdl/rmlui/rmlui_continue.h"
#include "port/sdl/rmlui/rmlui_control_mapping.h"
#include "port/sdl/rmlui/rmlui_copyright.h"
#include "port/sdl/rmlui/rmlui_exit_confirm.h"
#include "port/sdl/rmlui/rmlui_extra_option.h"
#include "port/sdl/rmlui/rmlui_game_hud.h"
#include "port/sdl/rmlui/rmlui_game_option.h"
#include "port/sdl/rmlui/rmlui_gameover.h"
#include "port/sdl/rmlui/rmlui_memory_card.h"
#include "port/sdl/rmlui/rmlui_mode_menu.h"
#include "port/sdl/rmlui/rmlui_name_entry.h"
#include "port/sdl/rmlui/rmlui_netplay_ui.h"
#include "port/sdl/rmlui/rmlui_network_lobby.h"
#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include "port/sdl/rmlui/rmlui_leaderboard.h"
#include "port/sdl/rmlui/rmlui_option_menu.h"
#include "port/sdl/rmlui/rmlui_pause_overlay.h"
#include "port/sdl/rmlui/rmlui_replay_picker.h"
#include "port/sdl/rmlui/rmlui_sound_menu.h"
#include "port/sdl/rmlui/rmlui_sysdir.h"
#include "port/sdl/rmlui/rmlui_title_screen.h"
#include "port/sdl/rmlui/rmlui_training_menus.h"
#include "port/sdl/rmlui/rmlui_trials_hud.h"
#include "port/sdl/rmlui/rmlui_vs_result.h"
#include "port/sdl/rmlui/rmlui_vs_screen.h"
#include "port/sdl/rmlui/rmlui_win_screen.h"
#include "port/tracy_gpu.h"
#include "port/tracy_zones.h"
#include "port/training_menu.h"

int g_resolution_scale = 1;
#include "port/sdl/input/sdl_pad.h"
#include "port/sdl/renderer/sdl_game_renderer.h"
#include "port/sdl/renderer/sdl_game_renderer_internal.h"
#include "port/sdl/renderer/sdl_text_renderer.h"
#include "port/sound/adx.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Game/engine/workuser.h"

// clang-format off
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
// clang-format on
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "shaders/librashader_manager.h"

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

// ⚡ Track whether letterbox bars are visible — skip redundant backbuffer clears when not.
static bool last_had_letterbox_bars = true;
/** @brief Read an entire shader source file into a heap-allocated string. */
static char* read_shader_source(const char* path) {
    size_t length = 0;
    char* buffer = (char*)SDL_LoadFile(path, &length);
    return buffer; // caller frees with SDL_free
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
    SDL_free(vertex_source);
    SDL_free(fragment_source);

    return program;
}

static const char* app_name = "Street Fighter III: 3rd Strike";
static const double target_fps = 59.59949;
static const Uint64 target_frame_time_ns = 1000000000.0 / target_fps;

SDL_Window* window = NULL;
static RendererBackend g_renderer_backend = RENDERER_OPENGL; // SDL_GPU opt-in via --renderer gpu
static SDL_GPUDevice* gpu_device = NULL;
static SDL_Renderer* sdl_renderer = NULL; // Only used in SDL2D mode

static Uint64 frame_deadline = 0;
static Uint64 frame_counter = 0;

static Uint64 last_mouse_motion_time = 0;
static const int mouse_hide_delay_ms = 2000; // 2 seconds
static bool cursor_visible = true;           // Track cursor state to avoid redundant SDL calls
static bool show_menu = false;
static bool show_shader_menu = false;
static bool show_mods_menu = false;
bool show_stage_config_menu = false;
bool show_training_menu = false;
TrainingMenuSettings g_training_menu_settings = { 0 };
// Stubs for training_menu.h API — real logic is now in rmlui_training_menu.cpp
void training_menu_init(void) { /* no-op */ }
void training_menu_render(int w, int h) {
    (void)w;
    (void)h;
}
void training_menu_shutdown(void) { /* no-op */ }
bool mods_menu_input_display_enabled = false;
bool mods_menu_shader_bypass_enabled = false;
bool mods_menu_fast_pre_game = false;
bool game_paused = false;
static bool frame_rate_uncapped = false;
static bool vsync_enabled = false;     // VSync disabled — native frame pacing handles timing
static bool present_only_mode = false; // when true, EndFrame re-blits canvas without re-rendering game

// UI mode flag — when true, RmlUi handles overlay menus
bool use_rmlui = false;

/** @brief Initialize SDL3, create window + GL context, compile shaders, load config. */
int SDLApp_Init() {
    Config_Init();
    Identity_Init();
    LobbyServer_Init();

    const char* cfg_scale = Config_GetString(CFG_KEY_SCALEMODE);
    if (cfg_scale) {
        scale_mode = config_string_to_scale_mode(cfg_scale);
    }

    show_debug_hud = Config_GetBool(CFG_KEY_DEBUG_HUD); // defined in sdl_app_debug_hud.c

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

    const char* backend_name = (g_renderer_backend == RENDERER_SDLGPU)          ? "SDL_GPU"
                               : (g_renderer_backend == RENDERER_SDL2D)         ? "SDL2D"
                               : (g_renderer_backend == RENDERER_SDL2D_CLASSIC) ? "SDL2D-Classic"
                                                                                : "OpenGL";
    SDL_Log("SDLApp_Init: Creating window %dx%d, Fullscreen: %d, Backend: %s",
            width,
            height,
            (window_flags & SDL_WINDOW_FULLSCREEN) ? 1 : 0,
            backend_name);

    if (is_sdl2d_backend(g_renderer_backend)) {
        // SDL2D: use SDL_CreateWindowAndRenderer — no GL context, no GPU device
        if (!SDL_CreateWindowAndRenderer(app_name, width, height, window_flags, &window, &sdl_renderer)) {
            fatal_error("SDL2D: Couldn't create window/renderer: %s", SDL_GetError());
        }
        SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);
        SDL_Log("Renderer: SDL2D (SDL_Renderer)");

        // VSync OFF — native frame pacing handles timing for all backends.
        vsync_enabled = false;
        SDL_SetRenderVSync(sdl_renderer, 0);
        SDL_Log("VSync: OFF (SDL2D, native pacing)");
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
#ifdef PLATFORM_RPI4
        // V3D GPU: retry with reduced feature set if default creation failed
        if (!gpu_device) {
            SDL_Log("SDL_GPU: Retrying with reduced features for V3D...");
            SDL_PropertiesID props = SDL_CreateProperties();
            SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
            SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, gpu_debug);
            // Disable features V3D doesn't support
            SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN, false);
            SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN, false);
            SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN, false);
            SDL_SetBooleanProperty(
                props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN, false);
            // Disable 'near universal' features not supported by V3D (patched in SDL3)
            SDL_SetBooleanProperty(props, "SDL.gpu.device.create.feature.image_cube_array", false);
            SDL_SetBooleanProperty(props, "SDL.gpu.device.create.feature.independent_blend", false);
            SDL_SetBooleanProperty(props, "SDL.gpu.device.create.feature.sample_rate_shading", false);
            gpu_device = SDL_CreateGPUDeviceWithProperties(props);
            SDL_DestroyProperties(props);
        }
#endif
        if (!gpu_device) {
            SDL_Log("Failed to create SDL_GPU device: %s — falling back to OpenGL", SDL_GetError());
            g_renderer_backend = RENDERER_OPENGL;
            // Re-create window with OpenGL flags
            if (window) {
                SDL_DestroyWindow(window);
                window = NULL;
            }
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
            window_flags |= SDL_WINDOW_OPENGL;
            window = SDL_CreateWindow(app_name, width, height, window_flags);
            if (!window) {
                SDL_Log("OpenGL fallback window creation failed: %s", SDL_GetError());
                return 1;
            }
            goto opengl_init;
        }
        if (!SDL_ClaimWindowForGPUDevice(gpu_device, window)) {
            SDL_Log("Failed to claim SDL_GPU window: %s", SDL_GetError());
            return 1;
        }
        SDL_Log("SDL_GPU Initialized Successfully.");

        // VSync OFF — native frame pacing handles timing.
        // Try IMMEDIATE (no vsync) first; fall back to MAILBOX then FIFO.
        // Pi4's v3dv Vulkan driver does not support IMMEDIATE, so without
        // this fallback the swapchain silently stays at FIFO (60fps cap).
        vsync_enabled = false;
        {
            SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;  // worst-case default
            if (SDL_WindowSupportsGPUPresentMode(gpu_device, window, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
                mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
            } else if (SDL_WindowSupportsGPUPresentMode(gpu_device, window, SDL_GPU_PRESENTMODE_MAILBOX)) {
                mode = SDL_GPU_PRESENTMODE_MAILBOX;
            }

            const char* mode_name = (mode == SDL_GPU_PRESENTMODE_IMMEDIATE)  ? "IMMEDIATE"
                                    : (mode == SDL_GPU_PRESENTMODE_MAILBOX)  ? "MAILBOX"
                                                                             : "VSYNC (FIFO)";
            if (SDL_SetGPUSwapchainParameters(
                    gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode)) {
                SDL_Log("VSync: OFF — present mode %s (SDL_GPU, native pacing)", mode_name);
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                            "Failed to set present mode %s: %s — falling back to default",
                            mode_name, SDL_GetError());
            }
        }
    } else if (g_renderer_backend == RENDERER_OPENGL) {
    opengl_init:
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

        // VSync OFF — native frame pacing handles timing.
        vsync_enabled = false;
        SDL_GL_SetSwapInterval(0);
        SDL_Log("VSync: OFF (OpenGL, native pacing)");
    }
    // else: SDL2D — window and renderer already created above

    SDL_Log("Initializing Game Renderer...");
    SDLGameRenderer_Init();
    SDL_Log("Game Renderer initialized.");

    // Initialize bezel GPU resources
    if (g_renderer_backend == RENDERER_SDLGPU) {
        SDLAppBezel_InitGPU(SDL_GetBasePath());
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

        // Create bezel resources (GL backend)
        SDLAppBezel_InitGL();
    }

    // RmlUi overlay: available on ALL backends (GL, GPU, SDL2D)
    rmlui_wrapper_init(window, gl_context);

    // Check if user wants RmlUi mode (set via --ui rmlui CLI flag, session-only)
    use_rmlui = g_ui_mode_rmlui;

    // Core RmlUi components — always initialized (replay picker always uses RmlUi)
    rmlui_replay_picker_init();
    rmlui_dev_overlay_init();

    // Fx-key overlay menus always use RmlUi (toggle_overlay is unconditional),
    // so their data models must be registered regardless of use_rmlui.
    rmlui_mods_menu_init();
    rmlui_shader_menu_init();
    rmlui_stage_config_init();
    rmlui_input_display_init();
    rmlui_frame_display_init();
    rmlui_netplay_ui_init();
    rmlui_training_menu_init();
    rmlui_training_hud_init();
    rmlui_control_mapping_init();
    rmlui_network_lobby_init(); // Always initialized for the Native -> RmlUI gateway
    rmlui_casual_lobby_init();
    rmlui_leaderboard_init();

    if (use_rmlui) {
        SDL_Log("UI mode: RmlUi (overlay menus via HTML/CSS)");

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
        rmlui_extra_option_init();
        rmlui_training_menus_init();
        rmlui_button_config_init();
        rmlui_char_select_init();
        rmlui_vs_screen_init();
        rmlui_pause_overlay_init();
        rmlui_trials_hud_init();
        rmlui_copyright_init();
        rmlui_name_entry_init();
        rmlui_exit_confirm_init();
        rmlui_attract_overlay_init();
    } else {
        SDL_Log("UI mode: Native (default)");
    }

    // Skip shaders and mod menus for SDL2D mode
    if (!is_sdl2d_backend(g_renderer_backend)) {
        SDLNetplayUI_Init();
        ModdedStage_Init();

        // Initialize Shader Config
        SDLAppShader_Init(base_path);
    }

    // Bezel system — available on all backends (GL, GPU, SDL2D)
    BezelSystem_Init();
    if (Config_HasKey(CFG_KEY_BEZEL_ENABLED)) {
        BezelSystem_SetVisible(Config_GetBool(CFG_KEY_BEZEL_ENABLED));
    }
    BezelSystem_LoadTextures();

    SDL_free(base_path);

    return 0;
}

/** @brief Shut down SDL, release shaders, destroy window. */
void SDLApp_Quit() {
    Broadcast_Shutdown();
    SDLGameRenderer_Shutdown();
    SDLTextRenderer_Shutdown();

    // Shut down Phase 5 / Phase 3 RmlUi data models before rmlui_wrapper_shutdown()
    rmlui_casual_lobby_shutdown();
    rmlui_leaderboard_shutdown();

    if (is_sdl2d_backend(g_renderer_backend)) {
        // SDL2D cleanup
        BezelSystem_Shutdown();
        rmlui_wrapper_shutdown();
        if (sdl_renderer) {
            SDL_DestroyRenderer(sdl_renderer);
            sdl_renderer = NULL;
        }
    } else {
        SDLAppShader_Shutdown();

        SDLAppBezel_Shutdown();

        // Shared UI shutdown (GL + GPU)
        SDLNetplayUI_Shutdown();
        BezelSystem_Shutdown();
        ModdedStage_Shutdown();
        rmlui_wrapper_shutdown();

        if (g_renderer_backend == RENDERER_OPENGL) {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
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
    Config_SetBool(CFG_KEY_DEBUG_HUD, SDLAppDebugHud_IsVisible());

    // Sync broadcast config
    Config_SetBool(CFG_KEY_BROADCAST_ENABLED, broadcast_config.enabled);
    Config_SetInt(CFG_KEY_BROADCAST_SOURCE, broadcast_config.source);
    Config_SetBool(CFG_KEY_BROADCAST_SHOW_UI, broadcast_config.show_ui);

    Config_Save();
    Config_Destroy();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

/** @brief Check if a quit event is pending without consuming events. */
static bool has_pending_quit(void) {
    SDL_Event peek;
    return SDL_PeepEvents(&peek, 1, SDL_PEEKEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT) > 0;
}

static /** @brief Hide the cursor after 2 seconds of inactivity. */
    void
    hide_cursor_if_needed() {
    if (show_menu || show_shader_menu || show_stage_config_menu || show_training_menu || show_dev_overlay) {
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

    TRACE_SUB_BEGIN("SDLEventPump");
    while (SDL_PollEvent(&event)) {
        bool request_quit = SDLAppInput_HandleEvent(&event);
        if (request_quit) {
            continue_running = false;
        }
    }
    TRACE_SUB_END();

    control_mapping_update();

    return continue_running;
}

/** @brief Begin a new frame — clear the GL viewport. */
void SDLApp_BeginFrame() {
    if (!is_sdl2d_backend(g_renderer_backend)) {
        // Process any deferred preset switch
        SDLAppShader_ProcessPendingLoad();
    }

    // RmlUi per-frame calls are skipped entirely when no UI is active.
    // This avoids ~13ms of implicit glFinish() overhead in eglSwapBuffers on Pi4.
    // ⚡ Pi4: only activate when window-context overlays are visible.
    // Game documents have their own update/render path (rmlui_wrapper_update_game
    // / rmlui_wrapper_render_game) — do NOT include rmlui_wrapper_any_game_visible()
    // here, as it would trigger the GL3 renderer for zero window documents.
    bool rmlui_active = use_rmlui || show_menu || show_shader_menu ||
                        show_mods_menu || show_stage_config_menu || show_training_menu || show_dev_overlay;
    if (rmlui_active) {
        rmlui_wrapper_new_frame();
    }

    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);

    // Render menus
    if (show_menu) {
        // ... (existing menu logic if any)
    }

    // SDL2D: clear the window backbuffer
    // ⚡ Skip clear when no letterbox bars are visible — the canvas blit will overwrite everything
    if (is_sdl2d_backend(g_renderer_backend) && last_had_letterbox_bars) {
        SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_SetRenderTarget(sdl_renderer, NULL);
        SDL_RenderClear(sdl_renderer);
    }

    if (!present_only_mode) {
        SDLGameRenderer_BeginFrame();
    }
}

/** @brief Dispatch menu/overlay rendering and flush the UI framework (RmlUi).
 *  Handles input/frame display updates, all menu overlays, netplay UI, and the
 *  final rmlui_wrapper_render() flush.
 *  @param win_w  Window width (logical) for overlay sizing.
 *  @param win_h  Window height (logical) for overlay sizing. */
static void render_overlays(int win_w, int win_h) {
    /* Input / frame-data overlays — always RmlUi */
    rmlui_input_display_update();
    rmlui_frame_display_update();

    /* Menu overlays — always use RmlUi for Fx-key menus */
    if (show_menu)
        rmlui_control_mapping_update();
    if (show_mods_menu)
        rmlui_mods_menu_update();
    if (show_shader_menu)
        rmlui_shader_menu_update();
    if (show_stage_config_menu)
        rmlui_stage_config_update();
    if (show_dev_overlay)
        rmlui_dev_overlay_update();
    if (show_training_menu)
        rmlui_training_menu_update();
    rmlui_training_hud_update();

    /* Netplay overlay — SDLNetplayUI_Render is not initialized on SDL2D */
    int hud_fps_count = 0;
    const float* hud_fps_history = SDLAppDebugHud_GetFPSHistory(&hud_fps_count);
    SDLNetplayUI_SetFPSHistory(hud_fps_history, hud_fps_count, (float)SDLAppDebugHud_GetFPS());
    if (!is_sdl2d_backend(g_renderer_backend)) {
        SDLNetplayUI_Render(win_w, win_h);
    }
    rmlui_netplay_ui_update();

    /* Flush UI framework — only when window-context overlays are active.
     * ⚡ Pi4: do NOT include rmlui_wrapper_any_game_visible() here — game
     * documents use their own separate render path. Including it would
     * activate the GL3 renderer every frame for zero visible windows. */
    bool rmlui_active = use_rmlui || show_menu || show_shader_menu ||
                        show_mods_menu || show_stage_config_menu || show_training_menu || show_dev_overlay;
    if (rmlui_active) {
        rmlui_wrapper_render();
    }
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
    // ⚡ Pi4: skip all 25 Phase 3 data model updates when no game documents are
    // visible. Each update individually checks visibility via hash-map lookup;
    // this single cached-bool check avoids all of them (~50µs saved on V3D).
    if (use_rmlui && rmlui_wrapper_any_game_visible()) {
        TRACE_SUB_BEGIN("RmlUiUpdates");
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
        rmlui_extra_option_update();
        rmlui_training_menus_update();
        rmlui_button_config_update();
        rmlui_char_select_update();
        rmlui_vs_screen_update();
        rmlui_pause_overlay_update();
        rmlui_trials_hud_update();
        rmlui_copyright_update();
        rmlui_name_entry_update();
        rmlui_exit_confirm_update();
        TRACE_SUB_END();
    }

    /* Replay picker and network lobby always use RmlUI — update outside use_rmlui gate */
    TRACE_SUB_BEGIN("LobbyUpdates");
    rmlui_replay_picker_update();
    rmlui_network_lobby_update();
    rmlui_casual_lobby_update();
    rmlui_leaderboard_update();
    TRACE_SUB_END();

    if (is_sdl2d_backend(g_renderer_backend)) {
        // --- SDL2D Backend ---
        if (!game_paused && !present_only_mode) {
            ADX_ProcessTracks();
        }

        SDL_SetRenderTarget(sdl_renderer, NULL);

        const SDL_FRect dst_rect = get_letterbox_rect(win_w, win_h);

        // ⚡ Track whether letterbox bars are visible for conditional clear in BeginFrame
        last_had_letterbox_bars =
            (dst_rect.x > 0.5f || dst_rect.y > 0.5f || dst_rect.w < (win_w - 0.5f) || dst_rect.h < (win_h - 0.5f));

        // Blit game canvas to window with letterboxing
        SDL_Texture* canvas = (g_renderer_backend == RENDERER_SDL2D_CLASSIC) ? SDLGameRendererClassic_GetCanvas()
                                                                             : SDLGameRendererSDL_GetCanvas();
        SDL_RenderTexture(sdl_renderer, canvas, NULL, &dst_rect);

        // Bezel rendering (SDL2D)
        SDLAppBezel_RenderSDL2D(sdl_renderer, win_w, win_h, &dst_rect);

        // Render RmlUi game context at window resolution (Phase 3 game screens)
        // ⚡ Pi4: allow explicitly visible documents to render even in Native mode
        if (use_rmlui || rmlui_wrapper_any_game_visible()) {
            rmlui_wrapper_update_game();
            rmlui_wrapper_render_game(win_w, win_h, dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h);
        }

        // Debug text
        SDLTextRenderer_DrawDebugBuffer((float)win_w, (float)win_h);
        if (SDLAppDebugHud_IsVisible()) {
            SDLAppDebugHud_RenderSDL2D(win_w, win_h, &dst_rect);
        }

        // Render overlays (menus, netplay, UI flush)
        render_overlays(win_w, win_h);

        TRACE_SUB_BEGIN("RenderPresent");
        if (!has_pending_quit()) {
            SDL_RenderPresent(sdl_renderer);
        }
        TRACE_SUB_END();

        SDLGameRenderer_EndFrame();
        hide_cursor_if_needed();

        // Frame pacing
        Uint64 now = SDL_GetTicksNS();
        TRACE_SUB_BEGIN("FramePacing");
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
                    if (has_pending_quit())
                        break;
                    SDL_CPUPauseInstruction();
                }
                now = SDL_GetTicksNS();
            }
            frame_deadline += target_frame_time_ns;
            if (now > frame_deadline + target_frame_time_ns) {
                frame_deadline = now + target_frame_time_ns;
            }
        }
        TRACE_SUB_END();

        frame_counter += 1;
        SDLAppDebugHud_NoteFrameEnd();
        SDLAppDebugHud_UpdateFPS();
        SDLPad_UpdatePreviousState();
        TRACE_ZONE_END();
        return;
    }

    if (g_renderer_backend == RENDERER_SDLGPU) {
        // --- GPU Backend ---

        // Update RmlUi game context (render happens after canvas blit below)
        // ⚡ Pi4: allow explicitly visible documents to update even in Native mode
        if (use_rmlui || rmlui_wrapper_any_game_visible()) {
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
            // rmlui_wrapper_render() gracefully handles NULL swapchain.
            rmlui_wrapper_render();
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

#if DEBUG
        // Debug Buffer (PS2)
        SDLTextRenderer_DrawDebugBuffer((float)win_w, (float)win_h);
#endif

        // Bezel Rendering (GPU)
        SDLAppBezel_RenderGPU(win_w, win_h);
        // Phase 3 game UI at window resolution (after canvas blit + bezels)
        // ⚡ Pi4: allow explicitly visible documents to render even in Native mode
        if (use_rmlui || rmlui_wrapper_any_game_visible()) {
            const SDL_FRect gp_vp = get_letterbox_rect(win_w, win_h);
            rmlui_wrapper_render_game(win_w, win_h, gp_vp.x, gp_vp.y, gp_vp.w, gp_vp.h);
        }

        // Render overlays (menus, netplay, UI flush)
        render_overlays(win_w, win_h);
        if (SDLAppDebugHud_IsVisible()) {
            const SDL_FRect viewport = get_letterbox_rect(win_w, win_h);
            SDLAppDebugHud_Render(win_w, win_h, &viewport);
        }

        // Flush Text Renderer (draws buffered text)
        SDLTextRenderer_Flush();

    gpu_end_frame_submit:; // empty statement after label for C compliance

    } else {
        // --- OpenGL Backend ---

        // Update RmlUi game context — allow explicitly visible documents even in Native mode
        TRACE_SUB_BEGIN("GL:RmlUiUpdateGame");
        if (use_rmlui || rmlui_wrapper_any_game_visible()) {
            rmlui_wrapper_update_game();
        }
        TRACE_SUB_END();

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
        TRACE_SUB_BEGIN("GL:Bezels");
        SDLAppBezel_RenderGL(win_w, win_h, &viewport, passthru_shader_program, (const float*)identity);
        TRACE_SUB_END();
        // Phase 3 game UI at window resolution (after canvas blit + bezels)
        // GL state reset is handled inside rmlui_wrapper_render_game (only when docs visible)
        TRACE_SUB_BEGIN("GL:RmlUiRenderGame");
        if (use_rmlui || rmlui_wrapper_any_game_visible()) {
            rmlui_wrapper_render_game(win_w, win_h, viewport.x, viewport.y, viewport.w, viewport.h);
        }
        TRACE_SUB_END();

        // Debug text buffer (game debug menu, effect overlay, etc.)
        // Must render independently of the FPS HUD toggle.
        // ⚡ Pi4: only reset GL state + render when there's actually text to draw.
        // The 5 GL state calls below cost ~50-100µs on V3D even with zero text.
        TRACE_SUB_BEGIN("GL:DebugText");
#if DEBUG
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);
            glUseProgram(0);
            glViewport(0, 0, win_w, win_h);

            SDLTextRenderer_DrawDebugBuffer((float)win_w, (float)win_h);
        }
#endif

        if (SDLAppDebugHud_IsVisible()) {
#if !DEBUG
            // Only reset GL state if we didn't already do it in the DEBUG block above
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);
            glUseProgram(0);
            glViewport(0, 0, win_w, win_h);
#endif
            SDLAppDebugHud_Render(win_w, win_h, &viewport);
        }
        TRACE_SUB_END();

        // Render overlays (menus, netplay, UI flush)
        TRACE_SUB_BEGIN("GL:Overlays");
        render_overlays(win_w, win_h);
        TRACE_SUB_END();

        // Final Output broadcast: capture the fully-composited frame
        if (broadcast_config.enabled && broadcast_config.source == BROADCAST_SOURCE_FINAL) {
            TRACE_SUB_BEGIN("GL:Broadcast");
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
            TRACE_SUB_END();
        }

        // Swap the window to display the final rendered frame
        TRACE_SUB_BEGIN("SwapWindow");
        if (!has_pending_quit()) {
            SDL_GL_SwapWindow(window);
        }
        TRACE_SUB_END();
    }
    TRACE_GPU_COLLECT();

    // Now that the frame is displayed, clean up resources for the next frame
    TRACE_SUB_BEGIN("GL:EndFrameCleanup");
    SDLGameRenderer_EndFrame();
    TRACE_SUB_END();

    // Run sound processing — after GPU submit so CPU audio decode
    // overlaps with GPU processing the submitted command buffer.
    TRACE_SUB_BEGIN("ADX");
    if (!game_paused && !present_only_mode) {
        ADX_ProcessTracks();
    }
    TRACE_SUB_END();

    SDLAppScreenshot_ProcessPending();

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
                if (has_pending_quit())
                    break;
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
    SDLAppDebugHud_NoteFrameEnd();
    SDLAppDebugHud_UpdateFPS();
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
    /* Fx menus always use RmlUi — sync document visibility unconditionally */
    if (!show_menu) {
        rmlui_wrapper_hide_document("control_mapping");
    }
}

/** @brief Toggle an overlay flag and sync RmlUi document visibility.
 *  @param flag        Pointer to the overlay's show_* bool.
 *  @param doc_name    RmlUi document name (e.g. "mods", "shaders").
 *  @param pauses_game If true, update game_paused based on the new flag state. */
static void toggle_overlay(bool* flag, const char* doc_name, bool pauses_game) {
    *flag = !*flag;
    if (pauses_game)
        game_paused = *flag || show_menu;
    if (*flag)
        SDL_ShowCursor();
    /* Fx menus always use RmlUi — sync document visibility unconditionally */
    if (*flag)
        rmlui_wrapper_show_document(doc_name);
    else
        rmlui_wrapper_hide_document(doc_name);
}

void SDLApp_ToggleModsMenu() {
    toggle_overlay(&show_mods_menu, "mods", true);
}

void SDLApp_ToggleShaderMenu() {
    toggle_overlay(&show_shader_menu, "shaders", true);
    /* Force reload: close the cached document so next F2 re-reads RML/RCSS
     * from disk.  This ensures layout changes take effect without restarting. */
    if (!show_shader_menu) {
        rmlui_wrapper_close_document("shaders");
    }
}

void SDLApp_ToggleStageConfigMenu() {
    toggle_overlay(&show_stage_config_menu, "stage_config", false);
}

void SDLApp_ToggleDevOverlay() {
    toggle_overlay(&show_dev_overlay, "dev_overlay", false);
}

void SDLApp_ToggleTrainingMenu() {
    toggle_overlay(&show_training_menu, "training", true);
}

void SDLApp_CloseAllMenus() {
    bool any_open = show_menu || show_shader_menu || show_mods_menu || show_stage_config_menu || show_training_menu ||
                    show_dev_overlay || SDLNetplayUI_IsDiagnosticsVisible();
    if (!any_open)
        return;

    show_menu = false;
    show_shader_menu = false;
    show_mods_menu = false;
    show_stage_config_menu = false;
    show_training_menu = false;
    show_dev_overlay = false;
    game_paused = false;
    SDLNetplayUI_SetDiagnosticsVisible(false);

    /* Fx menus always use RmlUi — hide documents unconditionally */
    rmlui_wrapper_hide_document("control_mapping");
    rmlui_wrapper_hide_document("shaders");
    rmlui_wrapper_hide_document("mods");
    rmlui_wrapper_hide_document("stage_config");
    rmlui_wrapper_hide_document("training");
    rmlui_wrapper_hide_document("dev_overlay");
}

/** @brief Public wrapper for cycle_scale_mode(). */
void SDLApp_CycleScaleMode() {
    cycle_scale_mode();
}

/** @brief Mark the bezel VBO as dirty so it gets re-uploaded next frame. */
void SDLApp_MarkBezelDirty() {
    SDLAppBezel_MarkDirty();
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
    SDLAppScreenshot_RequestCapture();
}

void SDLApp_ToggleBezel() {
    bool visible = !BezelSystem_IsVisible();
    BezelSystem_SetVisible(visible);
    Config_SetBool(CFG_KEY_BEZEL_ENABLED, visible);
    SDL_Log("Bezels toggled: %s", visible ? "ON" : "OFF");
}

void SDLApp_HandleWindowResize(int w, int h) {
    SDLAppBezel_MarkDirty(); // Force bezel VBO re-upload

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

/** @brief VSync toggle (currently disabled — native pacing handles all timing). */
void SDLApp_SetVSync(bool enabled) {
    (void)enabled;
    // VSync is permanently disabled — native frame pacing handles timing.
    // All backends stay at swap-interval 0 / IMMEDIATE mode.
}

bool SDLApp_IsVSyncEnabled() {
    return vsync_enabled;
}

void SDLApp_ToggleFrameRateUncap() {
    frame_rate_uncapped = !frame_rate_uncapped;
    // VSync is permanently off — no need to re-apply.

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
    SDLAppDebugHud_Toggle();
}
