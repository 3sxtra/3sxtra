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

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>

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

// Renderer
#include <RmlUi_Renderer_GL3.h>

// -------------------------------------------------------------------
// State
// -------------------------------------------------------------------
static Rml::Context* s_context = nullptr;
static SystemInterface_SDL* s_system_interface = nullptr;
static RenderInterface_GL3* s_render_interface = nullptr;

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

    // Determine assets/ui/ path
    const char* base_path = Paths_GetBasePath();
    if (base_path) {
        s_ui_base_path = std::string(base_path) + "assets/ui/";
    } else {
        s_ui_base_path = "assets/ui/";
    }

    // Get window dimensions
    SDL_GetWindowSize(window, &s_window_w, &s_window_h);

    // Create system interface (SDL platform)
    s_system_interface = new SystemInterface_SDL();
    s_system_interface->SetWindow(window);
    Rml::SetSystemInterface(s_system_interface);

    // Initialize the GL3 backend (loads GL functions via glad/SDL)
    Rml::String gl_message;
    if (!RmlGL3::Initialize(&gl_message)) {
        SDL_Log("[RmlUi] Failed to initialize GL3 backend: %s", gl_message.c_str());
        return;
    }

    // Create render interface (GL3)
    s_render_interface = new RenderInterface_GL3();
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

    // Initialize the GL3 render interface with the viewport
    s_render_interface->SetViewport(s_window_w, s_window_h);

    SDL_Log("[RmlUi] Initialized (GL3 renderer, %dx%d)", s_window_w, s_window_h);

    // Try loading the test document
    std::string test_path = s_ui_base_path + "test.rml";
    Rml::ElementDocument* doc = s_context->LoadDocument(test_path.c_str());
    if (doc) {
        doc->Show();
        s_documents["test"] = doc;
        SDL_Log("[RmlUi] Loaded test document: %s", test_path.c_str());
    } else {
        SDL_Log("[RmlUi] No test document found at: %s (OK for first run)", test_path.c_str());
    }
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

    delete s_render_interface;
    s_render_interface = nullptr;

    RmlGL3::Shutdown();

    delete s_system_interface;
    s_system_interface = nullptr;

    SDL_Log("[RmlUi] Shut down");
}

// -------------------------------------------------------------------
// Event processing
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_process_event(union SDL_Event* event) {
    if (!s_context || !event) return;

    // Let the SDL platform module handle the event
    RmlSDL::InputEventHandler(s_context, s_window, *event);

    // Handle window resize
    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        s_window_w = event->window.data1;
        s_window_h = event->window.data2;
        s_context->SetDimensions(Rml::Vector2i(s_window_w, s_window_h));
        if (s_render_interface) {
            s_render_interface->SetViewport(s_window_w, s_window_h);
        }
    }
}

// -------------------------------------------------------------------
// Frame update
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_new_frame(void) {
    if (!s_context) return;
    s_context->Update();
}

// -------------------------------------------------------------------
// Render
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_render(void) {
    if (!s_context || !s_render_interface) return;

    s_render_interface->BeginFrame();
    s_context->Render();
    s_render_interface->EndFrame();
}

// -------------------------------------------------------------------
// Input capture queries
// -------------------------------------------------------------------
extern "C" bool rmlui_wrapper_want_capture_mouse(void) {
    if (!s_context) return false;
    // RmlUi has hover detection — check if any element is under the mouse
    Rml::Element* hover = s_context->GetHoverElement();
    return (hover != nullptr && hover != s_context->GetRootElement());
}

extern "C" bool rmlui_wrapper_want_capture_keyboard(void) {
    if (!s_context) return false;
    Rml::Element* focus = s_context->GetFocusElement();
    return (focus != nullptr && focus != s_context->GetRootElement());
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
    if (!s_context || !name) return;

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
    if (!name) return;
    auto it = s_documents.find(name);
    if (it != s_documents.end()) {
        it->second->Hide();
    }
}

extern "C" bool rmlui_wrapper_is_document_visible(const char* name) {
    if (!name) return false;
    auto it = s_documents.find(name);
    if (it != s_documents.end()) {
        return it->second->IsVisible();
    }
    return false;
}
