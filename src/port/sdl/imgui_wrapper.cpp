/**
 * @file imgui_wrapper.cpp
 * @brief ImGui initialization, rendering, and texture loading helpers.
 *
 * Wraps ImGui's SDL3+OpenGL3 backend for the 3SX application, providing
 * init/shutdown, per-frame event processing, and image-to-GL-texture
 * loading via SDL_image.
 */
#include "port/sdl/imgui_wrapper.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "port/imgui_font.h"
#include "port/paths.h"
#include "port/resources.h"
#include "port/sdl/control_mapping.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_game_renderer_internal.h"
#include "port/sdl/sdl_texture_util.h"
#include <SDL3/SDL.h>
#include <string>

static void* capcom_icons_texture = NULL;

extern "C" __attribute__((used)) void imgui_wrapper_init(SDL_Window* window, void* gl_context) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Set persistent path for imgui.ini
    static std::string imgui_ini_path;
    const char* pref_path = Paths_GetPrefPath();
    if (pref_path) {
        imgui_ini_path = std::string(pref_path) + "imgui.ini";
        io.IniFilename = imgui_ini_path.c_str();
        SDL_Log("ImGui config will be saved to: %s", imgui_ini_path.c_str());
    }

    ImGui::StyleColorsDark();

    const char* base_path = Paths_GetBasePath();
    char* font_path = NULL;

    // Try loading Japanese font first
    if (base_path) {
        SDL_asprintf(&font_path, "%sassets/NotoSansJP-Regular.ttf", base_path);
    }

    bool font_loaded = false;
    if (font_path) {
        if (ImGuiFont_LoadJapaneseFont(font_path, 18.0f)) {
            SDL_Log("Loaded font from: %s", font_path);
            font_loaded = true;
        } else {
            SDL_Log("Failed to load font from: %s", font_path);
        }
        SDL_free(font_path);
        font_path = NULL;
    }

    // Fallback to BoldPixels or default if Japanese font failed
    if (!font_loaded && base_path) {
        SDL_asprintf(&font_path, "%sassets/BoldPixels.ttf", base_path);
        if (font_path) {
            io.Fonts->AddFontFromFileTTF(font_path, 16.0f);
            SDL_free(font_path);
        }
    }

    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        ImGui_ImplSDL3_InitForSDLGPU(window);
        ImGui_ImplSDLGPU3_InitInfo info = {};
        info.Device = SDLApp_GetGPUDevice();
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(info.Device, window);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        ImGui_ImplSDLGPU3_Init(&info);
    } else {
        ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL3_Init("#version 130");
    }

    imgui_wrapper_load_capcom_icons();
    control_mapping_init();
}

extern "C" __attribute__((used)) void imgui_wrapper_shutdown() {
    control_mapping_shutdown();
    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        ImGui_ImplSDLGPU3_Shutdown();
    } else {
        ImGui_ImplOpenGL3_Shutdown();
    }
    TextureUtil_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

extern "C" __attribute__((used)) void imgui_wrapper_process_event(SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
}

extern "C" __attribute__((used)) void imgui_wrapper_new_frame() {
    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        ImGui_ImplSDLGPU3_NewFrame();
    } else {
        ImGui_ImplOpenGL3_NewFrame();
    }
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

extern "C" __attribute__((used)) void imgui_wrapper_render() {
    ImGui::Render();
    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        ImDrawData* draw_data = ImGui::GetDrawData();
        SDL_GPUCommandBuffer* cb = SDLGameRendererGPU_GetCommandBuffer();
        if (cb) {
            ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cb);

            SDL_GPUTexture* swapchain_texture = SDLGameRendererGPU_GetSwapchainTexture();
            if (swapchain_texture) {
                SDL_GPUColorTargetInfo color_target_info;
                SDL_zero(color_target_info);
                color_target_info.texture = swapchain_texture;
                color_target_info.load_op = SDL_GPU_LOADOP_LOAD;
                color_target_info.store_op = SDL_GPU_STOREOP_STORE;

                SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cb, &color_target_info, 1, NULL);
                if (render_pass) {
                    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cb, render_pass);
                    SDL_EndGPURenderPass(render_pass);
                }
            }
        }
    } else {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

extern "C" __attribute__((used)) void imgui_wrapper_show_demo_window() {
    ImGui::ShowDemoWindow();
}

extern "C" __attribute__((used)) void imgui_wrapper_show_control_mapping_window(int window_width, int window_height) {
    control_mapping_render(window_width, window_height);
}

extern "C" __attribute__((used)) bool imgui_wrapper_want_capture_mouse() {
    return ImGui::GetIO().WantCaptureMouse;
}

extern "C" __attribute__((used)) bool imgui_wrapper_want_capture_keyboard() {
    return ImGui::GetIO().WantCaptureKeyboard;
}

extern "C" __attribute__((used)) void imgui_wrapper_capture_input(bool control_mapping_active) {
    ImGuiIO& io = ImGui::GetIO();
    if (control_mapping_active) {
        io.WantCaptureKeyboard = false;
        io.WantCaptureMouse = false;
    }
}

extern "C" __attribute__((used)) void* imgui_wrapper_load_texture(const char* filename) {
    return TextureUtil_Load(filename);
}

extern "C" __attribute__((used)) void imgui_wrapper_free_texture(void* texture_id) {
    TextureUtil_Free(texture_id);
}

extern "C" __attribute__((used)) void imgui_wrapper_get_texture_size(void* texture_id, int* w, int* h) {
    TextureUtil_GetSize(texture_id, w, h);
}

extern "C" __attribute__((used)) void imgui_wrapper_load_capcom_icons() {
    const char* base_path = Paths_GetBasePath();
    char* icon_path = NULL;
    if (base_path) {
        SDL_asprintf(&icon_path, "%sassets/icons-capcom-32.png", base_path);
    }
    if (icon_path) {
        capcom_icons_texture = TextureUtil_Load(icon_path);
        SDL_free(icon_path);
    }
}

extern "C" __attribute__((used)) void* imgui_wrapper_get_capcom_icons_texture() {
    return capcom_icons_texture;
}
