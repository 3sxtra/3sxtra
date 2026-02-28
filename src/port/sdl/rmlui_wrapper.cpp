/**
 * @file rmlui_wrapper.cpp
 * @brief RmlUi initialization, event routing, rendering, and document management.
 *
 * Wraps RmlUi's SDL3+GL3/GPU/SDLrenderer backends for the 3SX application.
 * Selects the appropriate RmlUi renderer based on SDLApp_GetRenderer().
 * Documents are loaded from assets/ui/ and managed by name.
 */
#include "port/sdl/rmlui_wrapper.h"
#include "port/config.h"
#include "port/paths.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_game_renderer_internal.h"

#include <RmlUi/Core.h>
#ifdef DEBUG
#include <RmlUi/Debugger.h>
#endif
#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

// -------------------------------------------------------------------
// RmlUi requires three interfaces: System, Render, and File.
// The SDL backends provide System + Render; we use the defaults for File.
//
// The GL3 renderer uses RMLUI_GL3_CUSTOM_LOADER (set in CMakeLists.txt)
// to load our project's glad/gl.h instead of its bundled glad, avoiding
// duplicate symbol conflicts with our glad_gl_core library.
// -------------------------------------------------------------------

// Platform (common to all SDL renderers)
#include <RmlUi_Platform_SDL.h>

// Renderers
#include <RmlUi_Renderer_GL3.h>
#include <RmlUi_Renderer_SDL.h>
#include <RmlUi_Renderer_SDL_GPU.h>

// -------------------------------------------------------------------
// State
// -------------------------------------------------------------------
static Rml::Context* s_context = nullptr;
static SystemInterface_SDL* s_system_interface = nullptr;

// Polymorphic base — used by Rml::SetRenderInterface() and context render
static Rml::RenderInterface* s_render_interface = nullptr;

// Typed pointers for backend-specific init/shutdown/render calls.
// Exactly one of these is non-null at a time.
static RenderInterface_GL3* s_render_gl3 = nullptr;
static RenderInterface_SDL* s_render_sdl = nullptr;
static RenderInterface_SDL_GPU* s_render_gpu = nullptr;

static RendererBackend s_active_backend;

static SDL_Window* s_window = nullptr;
static int s_window_w = 0;
static int s_window_h = 0;

static std::unordered_map<std::string, Rml::ElementDocument*> s_documents;
static std::string s_ui_base_path;

// -------------------------------------------------------------------
// Init
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_init(SDL_Window* window, void* gl_context) {
    (void)gl_context;
    s_window = window;
    s_active_backend = SDLApp_GetRenderer();

    // Determine assets/ui/ path
    const char* base_path = Paths_GetBasePath();
    if (base_path) {
        s_ui_base_path = std::string(base_path) + "assets/ui/";
    } else {
        s_ui_base_path = "assets/ui/";
    }

    // Get window dimensions
    SDL_GetWindowSize(window, &s_window_w, &s_window_h);

    // Create system interface (SDL platform — shared by all renderers)
    s_system_interface = new SystemInterface_SDL();
    s_system_interface->SetWindow(window);
    Rml::SetSystemInterface(s_system_interface);

    // Create render interface based on active renderer
    switch (s_active_backend) {
    case RENDERER_OPENGL: {
        Rml::String gl_message;
        if (!RmlGL3::Initialize(&gl_message)) {
            SDL_Log("[RmlUi] Failed to initialize GL3 backend: %s", gl_message.c_str());
            return;
        }
        s_render_gl3 = new RenderInterface_GL3();
        s_render_interface = s_render_gl3;
        break;
    }
    case RENDERER_SDLGPU: {
        SDL_GPUDevice* device = SDLApp_GetGPUDevice();
        s_render_gpu = new RenderInterface_SDL_GPU(device, window);
        s_render_interface = s_render_gpu;
        break;
    }
    case RENDERER_SDL2D: {
        SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
        s_render_sdl = new RenderInterface_SDL(renderer);
        s_render_interface = s_render_sdl;
        break;
    }
    }

    Rml::SetRenderInterface(s_render_interface);

    // Initialize RmlUi core
    if (!Rml::Initialise()) {
        SDL_Log("[RmlUi] Failed to initialize RmlUi core");
        return;
    }

    // Load fonts
    std::string font_path = s_ui_base_path + "../NotoSansJP-Regular.ttf";
    if (!Rml::LoadFontFace(font_path.c_str(), true)) {
        SDL_Log("[RmlUi] Failed to load font: %s", font_path.c_str());
        // Try fallback
        std::string fallback = s_ui_base_path + "../BoldPixels.ttf";
        Rml::LoadFontFace(fallback.c_str(), true);
    }

    // Create the context with the window dimensions
    s_context = Rml::CreateContext("main", Rml::Vector2i(s_window_w, s_window_h));
    if (!s_context) {
        SDL_Log("[RmlUi] Failed to create RmlUi context");
        return;
    }

    // Set initial DPI scaling from the platform
    float dp_ratio = SDL_GetWindowDisplayScale(window);
    if (dp_ratio > 0.0f) {
        s_context->SetDensityIndependentPixelRatio(dp_ratio);
    } else {
        dp_ratio = 1.0f;
    }

    // Backend-specific post-init
    if (s_render_gl3) {
        s_render_gl3->SetViewport(s_window_w, s_window_h);
    }

    // Initialize debugger plugin (debug builds only)
#ifdef DEBUG
    Rml::Debugger::Initialise(s_context);
    SDL_Log("[RmlUi] Debugger plugin initialized (F12 to toggle)");
#endif

    const char* backend_name = (s_active_backend == RENDERER_SDLGPU)  ? "SDL_GPU"
                               : (s_active_backend == RENDERER_SDL2D) ? "SDL2D"
                                                                      : "GL3";
    SDL_Log("[RmlUi] Initialized (%s renderer, %dx%d, dp-ratio=%.2fx)", backend_name, s_window_w, s_window_h, dp_ratio);
}

// -------------------------------------------------------------------
// Shutdown
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_shutdown(void) {
    s_documents.clear();

    if (s_context) {
        Rml::RemoveContext("main");
        s_context = nullptr;
    }

    Rml::Shutdown();

    // Backend-specific cleanup
    if (s_render_gl3) {
        RmlGL3::Shutdown(); // Must shutdown GL state before deleting the interface
        delete s_render_gl3;
        s_render_gl3 = nullptr;
    }
    if (s_render_gpu) {
        s_render_gpu->Shutdown();
        delete s_render_gpu;
        s_render_gpu = nullptr;
    }
    if (s_render_sdl) {
        delete s_render_sdl;
        s_render_sdl = nullptr;
    }
    s_render_interface = nullptr;

    delete s_system_interface;
    s_system_interface = nullptr;

    SDL_Log("[RmlUi] Shut down");
}

// -------------------------------------------------------------------
// Event processing
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_process_event(union SDL_Event* event) {
    if (!s_context || !event)
        return;

    // Toggle debugger with F12 (debug builds only)
#ifdef DEBUG
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_F12) {
        Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
        return;
    }
#endif

    // Hot reload keybinds (Ctrl+F5 = stylesheets, Ctrl+Shift+F5 = all documents)
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_F5 && (event->key.mod & SDL_KMOD_CTRL) &&
        !event->key.repeat) {
        if (event->key.mod & SDL_KMOD_SHIFT) {
            rmlui_wrapper_reload_all_documents();
        } else {
            rmlui_wrapper_reload_stylesheets();
        }
        return;
    }

    // Let the SDL platform module handle the event
    RmlSDL::InputEventHandler(s_context, s_window, *event);

    // Handle window resize
    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        s_window_w = event->window.data1;
        s_window_h = event->window.data2;
        s_context->SetDimensions(Rml::Vector2i(s_window_w, s_window_h));
        if (s_render_gl3) {
            s_render_gl3->SetViewport(s_window_w, s_window_h);
        }
        // SDL_GPU and SDL2D handle dimensions per-frame, no resize callback needed
    }
}

// -------------------------------------------------------------------
// Frame update
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_new_frame(void) {
    if (!s_context)
        return;
    s_context->Update();
}

// -------------------------------------------------------------------
// Render
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_render(void) {
    if (!s_context || !s_render_interface)
        return;

    if (s_render_gl3) {
        // GL3: simple begin/end frame
        s_render_gl3->BeginFrame();
        s_context->Render();
        s_render_gl3->EndFrame();
    } else if (s_render_gpu) {
        // SDL_GPU: needs command buffer + swapchain texture each frame
        SDL_GPUCommandBuffer* cb = SDLGameRendererGPU_GetCommandBuffer();
        SDL_GPUTexture* swapchain = SDLGameRendererGPU_GetSwapchainTexture();
        if (cb && swapchain) {
            int w, h;
            SDL_GetWindowSize(s_window, &w, &h);
            s_render_gpu->BeginFrame(cb, swapchain, (uint32_t)w, (uint32_t)h);
            s_context->Render();
            s_render_gpu->EndFrame();
        }
    } else if (s_render_sdl) {
        // SDL2D: do NOT call BeginFrame() — it calls SDL_RenderClear(),
        // which would wipe the game canvas already blitted to the backbuffer.
        // Just ensure render target is the window (not cps3_canvas).
        SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
        SDL_SetRenderTarget(renderer, NULL);
        s_context->Render();
        // EndFrame() is a no-op in the SDL backend, skip it.
    }
}

// -------------------------------------------------------------------
// Input capture queries
// -------------------------------------------------------------------
extern "C" bool rmlui_wrapper_want_capture_mouse(void) {
    if (!s_context)
        return false;
    Rml::Element* hover = s_context->GetHoverElement();
    if (!hover || hover == s_context->GetRootElement())
        return false;
    // Only capture if the hovered element's owning document is visible
    Rml::ElementDocument* doc = hover->GetOwnerDocument();
    return (doc != nullptr && doc->IsVisible());
}

extern "C" bool rmlui_wrapper_want_capture_keyboard(void) {
    if (!s_context)
        return false;
    // Only capture keyboard for text-input elements (input, textarea, select)
    Rml::Element* focus = s_context->GetFocusElement();
    if (!focus || focus == s_context->GetRootElement())
        return false;
    const Rml::String& tag = focus->GetTagName();
    return (tag == "input" || tag == "textarea" || tag == "select");
}

// -------------------------------------------------------------------
// Context accessor (for data model registration)
// -------------------------------------------------------------------
extern "C" void* rmlui_wrapper_get_context(void) {
    return static_cast<void*>(s_context);
}

// -------------------------------------------------------------------
// Document management
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_show_document(const char* name) {
    if (!s_context || !name)
        return;

    // Check if already loaded
    auto it = s_documents.find(name);
    if (it != s_documents.end()) {
        it->second->Show();
        return;
    }

    // Load from assets/ui/<name>.rml
    std::string path = s_ui_base_path + name + ".rml";
    Rml::ElementDocument* doc = s_context->LoadDocument(path.c_str());
    if (doc) {
        doc->Show();
        s_documents[name] = doc;
        SDL_Log("[RmlUi] Loaded document: %s", path.c_str());
    } else {
        SDL_Log("[RmlUi] Failed to load document: %s", path.c_str());
    }
}

extern "C" void rmlui_wrapper_hide_document(const char* name) {
    if (!name)
        return;
    auto it = s_documents.find(name);
    if (it != s_documents.end()) {
        it->second->Hide();
    }
}

extern "C" bool rmlui_wrapper_is_document_visible(const char* name) {
    if (!name)
        return false;
    auto it = s_documents.find(name);
    if (it != s_documents.end()) {
        return it->second->IsVisible();
    }
    return false;
}

extern "C" void rmlui_wrapper_close_document(const char* name) {
    if (!name)
        return;
    auto it = s_documents.find(name);
    if (it != s_documents.end()) {
        it->second->Close();
        s_documents.erase(it);
        SDL_Log("[RmlUi] Closed document: %s", name);
    }
}

// -------------------------------------------------------------------
// Hot Reload
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_reload_stylesheets(void) {
    if (!s_context)
        return;
    int count = 0;
    for (auto& [name, doc] : s_documents) {
        if (doc) {
            doc->ReloadStyleSheet();
            count++;
        }
    }
    SDL_Log("[RmlUi] Reloaded stylesheets for %d document(s)", count);
}

extern "C" void rmlui_wrapper_reload_document(const char* name) {
    if (!s_context || !name)
        return;
    auto it = s_documents.find(name);
    if (it == s_documents.end())
        return;

    Rml::ElementDocument* old_doc = it->second;
    bool was_visible = old_doc->IsVisible();
    old_doc->Close();

    // Clear caches so the fresh file is picked up
    Rml::Factory::ClearStyleSheetCache();
    Rml::Factory::ClearTemplateCache();

    std::string path = s_ui_base_path + name + ".rml";
    Rml::ElementDocument* new_doc = s_context->LoadDocument(path.c_str());
    if (new_doc) {
        if (was_visible)
            new_doc->Show();
        it->second = new_doc;
        SDL_Log("[RmlUi] Reloaded document: %s", name);
    } else {
        s_documents.erase(it);
        SDL_Log("[RmlUi] Failed to reload document: %s", name);
    }
}

extern "C" void rmlui_wrapper_reload_all_documents(void) {
    if (!s_context)
        return;

    // Clear caches once before reloading
    Rml::Factory::ClearStyleSheetCache();
    Rml::Factory::ClearTemplateCache();

    // Snapshot names+visibility since we'll modify the map
    struct DocInfo {
        std::string name;
        bool visible;
    };
    std::vector<DocInfo> docs;
    docs.reserve(s_documents.size());
    for (auto& [name, doc] : s_documents) {
        docs.push_back({ name, doc && doc->IsVisible() });
    }

    // Close all
    for (auto& [name, doc] : s_documents) {
        if (doc)
            doc->Close();
    }
    s_documents.clear();

    // Reload all
    int count = 0;
    for (auto& info : docs) {
        std::string path = s_ui_base_path + info.name + ".rml";
        Rml::ElementDocument* new_doc = s_context->LoadDocument(path.c_str());
        if (new_doc) {
            if (info.visible)
                new_doc->Show();
            s_documents[info.name] = new_doc;
            count++;
        } else {
            SDL_Log("[RmlUi] Failed to reload document: %s", info.name.c_str());
        }
    }
    SDL_Log("[RmlUi] Reloaded %d/%zu document(s)", count, docs.size());
}

extern "C" void rmlui_wrapper_release_textures(void) {
    Rml::ReleaseTextures();
    SDL_Log("[RmlUi] Released all textures (will reload on next render)");
}
