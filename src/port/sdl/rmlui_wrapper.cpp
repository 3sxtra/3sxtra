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

// GL header for RmlUi GL3 backend (glEnable, glBlendFunc, etc.)
#if !defined(__APPLE__)
#include <glad/gl.h>
#endif

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
// GPU viewport adapter — overrides virtual SetTransform/SetScissorRegion
// to bake viewport offset + scale into the rendering pipeline without
// modifying the third-party RenderInterface_SDL_GPU class.
// -------------------------------------------------------------------
class GameViewportGPU : public RenderInterface_SDL_GPU {
  public:
    using RenderInterface_SDL_GPU::RenderInterface_SDL_GPU;

    /// Activate viewport correction for game-context rendering.
    /// correction = Translate(off_x, off_y) * Scale(sx, sy)
    /// Maps logical coords (0..ctx_w, 0..ctx_h) → window pixels at letterbox offset.
    void ActivateGameViewport(int ctx_w, int ctx_h, int phys_w, int phys_h, int off_x, int off_y) {
        m_active = true;
        m_sx = (float)phys_w / (float)ctx_w;
        m_sy = (float)phys_h / (float)ctx_h;
        m_off_x = off_x;
        m_off_y = off_y;
        m_correction = Rml::Matrix4f::Translate((float)off_x, (float)off_y, 0) * Rml::Matrix4f::Scale(m_sx, m_sy, 1.0f);
        SetTransform(nullptr); // Force transform update with correction
    }

    void DeactivateGameViewport() {
        m_active = false;
        SetTransform(nullptr); // Restore to identity
    }

    void SetTransform(const Rml::Matrix4f* transform) override {
        if (!m_active) {
            RenderInterface_SDL_GPU::SetTransform(transform);
            return;
        }
        if (transform) {
            Rml::Matrix4f modified = m_correction * (*transform);
            RenderInterface_SDL_GPU::SetTransform(&modified);
        } else {
            RenderInterface_SDL_GPU::SetTransform(&m_correction);
        }
    }

    void SetScissorRegion(Rml::Rectanglei region) override {
        if (!m_active) {
            RenderInterface_SDL_GPU::SetScissorRegion(region);
            return;
        }
        Rml::Rectanglei adjusted = Rml::Rectanglei::FromPositionSize(
            { (int)(region.Left() * m_sx + m_off_x), (int)(region.Top() * m_sy + m_off_y) },
            { (int)(region.Width() * m_sx + 0.5f), (int)(region.Height() * m_sy + 0.5f) });
        RenderInterface_SDL_GPU::SetScissorRegion(adjusted);
    }

  private:
    bool m_active = false;
    float m_sx = 1.0f, m_sy = 1.0f;
    int m_off_x = 0, m_off_y = 0;
    Rml::Matrix4f m_correction;
};

// -------------------------------------------------------------------
// State
// -------------------------------------------------------------------

// Window context — Phase 2 overlay/debug menus (renders to window)
static Rml::Context* s_window_context = nullptr;
static std::unordered_map<std::string, Rml::ElementDocument*> s_window_documents;

// Game context — Phase 3 game-replacement screens (renders to 384×224 canvas FBO)
static Rml::Context* s_game_context = nullptr;
static std::unordered_map<std::string, Rml::ElementDocument*> s_game_documents;

static SystemInterface_SDL* s_system_interface = nullptr;

// Polymorphic base — used by Rml::SetRenderInterface() and context render
static Rml::RenderInterface* s_render_interface = nullptr;

// Typed pointers for backend-specific init/shutdown/render calls.
// Exactly one of these is non-null at a time.
static RenderInterface_GL3* s_render_gl3 = nullptr;
static RenderInterface_SDL* s_render_sdl = nullptr;
static GameViewportGPU* s_render_gpu = nullptr;

static RendererBackend s_active_backend;

static SDL_Window* s_window = nullptr;
static int s_window_w = 0;
static int s_window_h = 0;

static constexpr int GAME_W = 384;
static constexpr int GAME_H = 224;

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
        s_render_gpu = new GameViewportGPU(device, window);
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

    // --- Window context (Phase 2 overlays — window resolution) ---
    s_window_context = Rml::CreateContext("window", Rml::Vector2i(s_window_w, s_window_h));
    if (!s_window_context) {
        SDL_Log("[RmlUi] Failed to create window context");
        return;
    }
    float dp_ratio = SDL_GetWindowDisplayScale(window);
    if (dp_ratio > 0.0f) {
        s_window_context->SetDensityIndependentPixelRatio(dp_ratio);
    } else {
        dp_ratio = 1.0f;
    }

    // --- Game context (Phase 3 game screens — CPS3 resolution) ---
    s_game_context = Rml::CreateContext("game", Rml::Vector2i(GAME_W, GAME_H));
    if (!s_game_context) {
        SDL_Log("[RmlUi] Failed to create game context");
        return;
    }
    s_game_context->SetDensityIndependentPixelRatio(1.0f);

    // Backend-specific post-init
    if (s_render_gl3) {
        s_render_gl3->SetViewport(s_window_w, s_window_h);
    }

    // Initialize debugger plugin (debug builds only)
#ifdef DEBUG
    Rml::Debugger::Initialise(s_window_context);
    Rml::Debugger::SetContext(s_game_context);
    // Hide debugger documents on init so they don't render when the debugger
    // is not actively visible.  Debugger::SetVisible(false) only sets CSS
    // visibility:hidden which still paints body backgrounds.
    // - "rmlui-debug-menu"  in window context: toolbar with background:#888
    // - "rmlui-debug-hook"  in game context:   context hook with styled body
    if (Rml::ElementDocument* dbg_menu = s_window_context->GetDocument("rmlui-debug-menu")) {
        dbg_menu->Hide();
    }
    if (Rml::ElementDocument* dbg_hook = s_game_context->GetDocument("rmlui-debug-hook")) {
        dbg_hook->Hide();
    }
    SDL_Log("[RmlUi] Debugger plugin initialized (F12 to toggle, inspecting game context)");
#endif

    const char* backend_name = (s_active_backend == RENDERER_SDLGPU)  ? "SDL_GPU"
                               : (s_active_backend == RENDERER_SDL2D) ? "SDL2D"
                                                                      : "GL3";
    SDL_Log("[RmlUi] Initialized (%s renderer, %dx%d window + %dx%d game, dp-ratio=%.2fx)",
            backend_name,
            s_window_w,
            s_window_h,
            GAME_W,
            GAME_H,
            dp_ratio);
}

// -------------------------------------------------------------------
// Shutdown
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_shutdown(void) {
    s_window_documents.clear();
    s_game_documents.clear();

    if (s_game_context) {
        Rml::RemoveContext("game");
        s_game_context = nullptr;
    }
    if (s_window_context) {
        Rml::RemoveContext("window");
        s_window_context = nullptr;
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
    if (!s_window_context || !event)
        return;

    // Toggle debugger with F12 (debug builds only)
#ifdef DEBUG
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_F12) {
        bool new_vis = !Rml::Debugger::IsVisible();
        Rml::Debugger::SetVisible(new_vis);
        // Show/hide the debugger documents so their backgrounds don't
        // render when the debugger is closed.
        if (Rml::ElementDocument* dbg_menu = s_window_context->GetDocument("rmlui-debug-menu")) {
            if (new_vis)
                dbg_menu->Show();
            else
                dbg_menu->Hide();
        }
        if (Rml::ElementDocument* dbg_hook = s_game_context->GetDocument("rmlui-debug-hook")) {
            if (new_vis)
                dbg_hook->Show();
            else
                dbg_hook->Hide();
        }
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

    // Route SDL events to the window context only (Phase 2 overlays use mouse).
    // Game context (Phase 3) is driven entirely by the CPS3 input system
    // (gamepad → SDLPad → plsw → Check_Menu_Lever → MC_Move_Sub → IO_Result).
    // Do NOT feed events to s_game_context — it would cause RmlUi's spatial
    // navigation to fight with the CPS3 state machine on screens with <button>
    // elements, and mouse clicks would hit at wrong coordinates (window vs 384×224).
    RmlSDL::InputEventHandler(s_window_context, s_window, *event);

    // Handle window resize
    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        s_window_w = event->window.data1;
        s_window_h = event->window.data2;
        s_window_context->SetDimensions(Rml::Vector2i(s_window_w, s_window_h));
        if (s_render_gl3) {
            s_render_gl3->SetViewport(s_window_w, s_window_h);
        }
        // Game context stays at GAME_W×GAME_H — no resize needed
    }
}

// -------------------------------------------------------------------
// Frame update (window context — Phase 2)
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_new_frame(void) {
    if (!s_window_context)
        return;
    s_window_context->Update();
}

// -------------------------------------------------------------------
// Render
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_render(void) {
    if (!s_window_context || !s_render_interface)
        return;

    if (s_render_gl3) {
        // GL3: simple begin/end frame
        s_render_gl3->BeginFrame();

        s_window_context->Render();
        s_render_gl3->EndFrame();
    } else if (s_render_gpu) {
        // SDL_GPU: needs command buffer + swapchain texture each frame
        SDL_GPUCommandBuffer* cb = SDLGameRendererGPU_GetCommandBuffer();
        SDL_GPUTexture* swapchain = SDLGameRendererGPU_GetSwapchainTexture();
        if (cb && swapchain) {
            int w, h;
            SDL_GetWindowSize(s_window, &w, &h);
            s_render_gpu->BeginFrame(cb, swapchain, (uint32_t)w, (uint32_t)h);
            s_window_context->Render();
            s_render_gpu->EndFrame();
        }
    } else if (s_render_sdl) {
        // SDL2D: do NOT call BeginFrame() — it calls SDL_RenderClear(),
        // which would wipe the game canvas already blitted to the backbuffer.
        // Just ensure render target is the window (not cps3_canvas).
        SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
        SDL_SetRenderTarget(renderer, NULL);
        s_window_context->Render();
        // EndFrame() is a no-op in the SDL backend, skip it.
    }
}

// -------------------------------------------------------------------
// Input capture queries
// -------------------------------------------------------------------
extern "C" bool rmlui_wrapper_want_capture_mouse(void) {
    if (!s_window_context)
        return false;
    Rml::Element* hover = s_window_context->GetHoverElement();
    if (!hover || hover == s_window_context->GetRootElement())
        return false;
    // Only capture if the hovered element's owning document is visible
    Rml::ElementDocument* doc = hover->GetOwnerDocument();
    return (doc != nullptr && doc->IsVisible());
}

extern "C" bool rmlui_wrapper_want_capture_keyboard(void) {
    if (!s_window_context)
        return false;
    // Only capture keyboard for text-input elements (input, textarea, select)
    Rml::Element* focus = s_window_context->GetFocusElement();
    if (!focus || focus == s_window_context->GetRootElement())
        return false;
    const Rml::String& tag = focus->GetTagName();
    return (tag == "input" || tag == "textarea" || tag == "select");
}

// -------------------------------------------------------------------
// Context accessors (for data model registration)
// -------------------------------------------------------------------
extern "C" void* rmlui_wrapper_get_context(void) {
    return static_cast<void*>(s_window_context);
}

extern "C" void* rmlui_wrapper_get_game_context(void) {
    return static_cast<void*>(s_game_context);
}

// -------------------------------------------------------------------
// Document management — Window context (Phase 2)
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_show_document(const char* name) {
    if (!s_window_context || !name)
        return;

    auto it = s_window_documents.find(name);
    if (it != s_window_documents.end()) {
        it->second->Show();
        return;
    }

    std::string path = s_ui_base_path + name + ".rml";
    Rml::ElementDocument* doc = s_window_context->LoadDocument(path.c_str());
    if (doc) {
        doc->Show();
        s_window_documents[name] = doc;
        SDL_Log("[RmlUi] Loaded window document: %s", path.c_str());
    } else {
        SDL_Log("[RmlUi] Failed to load window document: %s", path.c_str());
    }
}

extern "C" void rmlui_wrapper_hide_document(const char* name) {
    if (!name)
        return;
    auto it = s_window_documents.find(name);
    if (it != s_window_documents.end()) {
        it->second->Hide();
    }
}

extern "C" void rmlui_wrapper_hide_all_documents(void) {
    for (auto& [name, doc] : s_window_documents) {
        if (doc && doc->IsVisible()) {
            doc->Hide();
        }
    }
}

extern "C" bool rmlui_wrapper_is_document_visible(const char* name) {
    if (!name)
        return false;
    auto it = s_window_documents.find(name);
    if (it != s_window_documents.end()) {
        return it->second->IsVisible();
    }
    return false;
}

extern "C" void rmlui_wrapper_close_document(const char* name) {
    if (!name)
        return;
    auto it = s_window_documents.find(name);
    if (it != s_window_documents.end()) {
        it->second->Close();
        s_window_documents.erase(it);
        SDL_Log("[RmlUi] Closed window document: %s", name);
    }
}

// -------------------------------------------------------------------
// Document management — Game context (Phase 3)
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_show_game_document(const char* name) {
    if (!s_game_context || !name)
        return;

    auto it = s_game_documents.find(name);
    if (it != s_game_documents.end()) {
        it->second->Show();
        return;
    }

    std::string path = s_ui_base_path + name + ".rml";
    Rml::ElementDocument* doc = s_game_context->LoadDocument(path.c_str());
    if (doc) {
        doc->Show();
        s_game_documents[name] = doc;
        SDL_Log("[RmlUi] Loaded game document: %s", path.c_str());
    } else {
        SDL_Log("[RmlUi] Failed to load game document: %s", path.c_str());
    }
}

extern "C" void rmlui_wrapper_hide_game_document(const char* name) {
    if (!name)
        return;
    auto it = s_game_documents.find(name);
    if (it != s_game_documents.end()) {
        it->second->Hide();
    }
}

extern "C" void rmlui_wrapper_hide_all_game_documents(void) {
    for (auto& [name, doc] : s_game_documents) {
        /* Never hide the attract overlay or copyright — they persist through
         * the attract-mode loop and are managed by their own show/hide calls. */
        if (name == "attract_overlay" || name == "copyright")
            continue;
        if (doc && doc->IsVisible()) {
            doc->Hide();
        }
    }
}

extern "C" bool rmlui_wrapper_is_game_document_visible(const char* name) {
    if (!name)
        return false;
    auto it = s_game_documents.find(name);
    if (it != s_game_documents.end()) {
        return it->second->IsVisible();
    }
    return false;
}

extern "C" void rmlui_wrapper_close_game_document(const char* name) {
    if (!name)
        return;
    auto it = s_game_documents.find(name);
    if (it != s_game_documents.end()) {
        it->second->Close();
        s_game_documents.erase(it);
        SDL_Log("[RmlUi] Closed game document: %s", name);
    }
}

// -------------------------------------------------------------------
// Game context update + render-to-canvas
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_update_game(void) {
    if (!s_game_context)
        return;
    s_game_context->Update();
}

extern "C" void rmlui_wrapper_render_game(int win_w, int win_h, float view_x, float view_y, float view_w,
                                          float view_h) {
    if (!s_game_context || !s_render_interface)
        return;

    // Width-based dp_ratio: fonts rasterize at high resolution.
    // Context dims are scaled by dp_ratio so dp-based RCSS fills the context.
    // SetViewportEx maps this logical space to the physical 4:3 viewport,
    // applying the CPS3 9/7 vertical PAR stretch automatically.
    const float dp_ratio = view_w / (float)GAME_W;
    s_game_context->SetDensityIndependentPixelRatio(dp_ratio);

    const int ctx_w = (int)(GAME_W * dp_ratio + 0.5f); // = view_w
    const int ctx_h = (int)(GAME_H * dp_ratio + 0.5f); // = 224 * view_w / 384
    s_game_context->SetDimensions(Rml::Vector2i(ctx_w, ctx_h));

    const int phys_w = (int)(view_w + 0.5f);
    const int phys_h = (int)(view_h + 0.5f);
    const int off_x = (int)(view_x + 0.5f);
    // OpenGL y=0 is bottom; convert from top-left origin
    const int off_y = win_h - (int)(view_y + 0.5f) - phys_h;

    if (s_render_gl3) {
        // GL3: SetViewportEx uses ctx_w×ctx_h for projection and
        // phys_w×phys_h for FBOs + glViewport. The viewport transform
        // applies the CPS3 9/7 vertical stretch.
        s_render_gl3->SetViewportEx(ctx_w, ctx_h, phys_w, phys_h, off_x, off_y);

        s_render_gl3->BeginFrame();
        s_game_context->Render();
        s_render_gl3->EndFrame();

        // Restore window viewport for subsequent rendering (bezels, overlays)
        s_render_gl3->SetViewport(s_window_w, s_window_h);
    } else if (s_render_gpu) {
        // SDL_GPU: render to the swapchain with viewport correction.
        // BeginFrame uses full window dims so the projection covers the entire
        // swapchain. The GameViewportGPU subclass then bakes a correction
        // matrix (translate + scale) into SetTransform so RmlUi logical
        // coordinates land at the correct letterbox position.
        SDL_GPUCommandBuffer* cb = SDLGameRendererGPU_GetCommandBuffer();
        SDL_GPUTexture* swapchain = SDLGameRendererGPU_GetSwapchainTexture();
        if (cb && swapchain) {
            const int top_y = (int)(view_y + 0.5f);
            s_render_gpu->BeginFrame(cb, swapchain, (uint32_t)win_w, (uint32_t)win_h);
            s_render_gpu->ActivateGameViewport(ctx_w, ctx_h, phys_w, phys_h, off_x, top_y);
            s_game_context->Render();
            s_render_gpu->DeactivateGameViewport();
            s_render_gpu->EndFrame();
        }
    } else if (s_render_sdl) {
        // SDL2D: set viewport to the letterbox rect and scale from logical to physical
        SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
        SDL_SetRenderTarget(renderer, NULL);

        // Confine rendering to the letterbox rectangle
        SDL_Rect vp_rect;
        vp_rect.x = (int)(view_x + 0.5f);
        vp_rect.y = (int)(view_y + 0.5f);
        vp_rect.w = phys_w;
        vp_rect.h = phys_h;
        SDL_SetRenderViewport(renderer, &vp_rect);

        // Scale from logical (ctx_w × ctx_h) to physical (phys_w × phys_h)
        float scale_x = (float)phys_w / (float)ctx_w;
        float scale_y = (float)phys_h / (float)ctx_h;
        SDL_SetRenderScale(renderer, scale_x, scale_y);

        s_game_context->Render();

        // Restore viewport and scale
        SDL_SetRenderViewport(renderer, NULL);
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    }
}

// -------------------------------------------------------------------
// Hot Reload
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_reload_stylesheets(void) {
    int count = 0;
    for (auto& [name, doc] : s_window_documents) {
        if (doc) {
            doc->ReloadStyleSheet();
            count++;
        }
    }
    for (auto& [name, doc] : s_game_documents) {
        if (doc) {
            doc->ReloadStyleSheet();
            count++;
        }
    }
    SDL_Log("[RmlUi] Reloaded stylesheets for %d document(s)", count);
}

// Helper to reload a document in a given context + document map
static void reload_doc_in(Rml::Context* ctx, std::unordered_map<std::string, Rml::ElementDocument*>& docs,
                          const char* name) {
    auto it = docs.find(name);
    if (it == docs.end())
        return;

    Rml::ElementDocument* old_doc = it->second;
    bool was_visible = old_doc->IsVisible();
    old_doc->Close();

    Rml::Factory::ClearStyleSheetCache();
    Rml::Factory::ClearTemplateCache();

    std::string path = s_ui_base_path + std::string(name) + ".rml";
    Rml::ElementDocument* new_doc = ctx->LoadDocument(path.c_str());
    if (new_doc) {
        if (was_visible)
            new_doc->Show();
        it->second = new_doc;
        SDL_Log("[RmlUi] Reloaded document: %s", name);
    } else {
        docs.erase(it);
        SDL_Log("[RmlUi] Failed to reload document: %s", name);
    }
}

extern "C" void rmlui_wrapper_reload_document(const char* name) {
    if (!name)
        return;
    // Try window documents first, then game documents
    if (s_window_context && s_window_documents.count(name))
        reload_doc_in(s_window_context, s_window_documents, name);
    else if (s_game_context && s_game_documents.count(name))
        reload_doc_in(s_game_context, s_game_documents, name);
}

// Helper to reload all docs in a given context + map
static int reload_all_in(Rml::Context* ctx, std::unordered_map<std::string, Rml::ElementDocument*>& doc_map) {
    struct DocInfo {
        std::string name;
        bool visible;
    };
    std::vector<DocInfo> docs;
    docs.reserve(doc_map.size());
    for (auto& [name, doc] : doc_map)
        docs.push_back({ name, doc && doc->IsVisible() });
    for (auto& [name, doc] : doc_map)
        if (doc)
            doc->Close();
    doc_map.clear();

    int count = 0;
    for (auto& info : docs) {
        std::string path = s_ui_base_path + info.name + ".rml";
        Rml::ElementDocument* new_doc = ctx->LoadDocument(path.c_str());
        if (new_doc) {
            if (info.visible)
                new_doc->Show();
            doc_map[info.name] = new_doc;
            count++;
        } else {
            SDL_Log("[RmlUi] Failed to reload document: %s", info.name.c_str());
        }
    }
    return count;
}

extern "C" void rmlui_wrapper_reload_all_documents(void) {
    Rml::Factory::ClearStyleSheetCache();
    Rml::Factory::ClearTemplateCache();

    int total = 0;
    size_t total_docs = s_window_documents.size() + s_game_documents.size();
    if (s_window_context)
        total += reload_all_in(s_window_context, s_window_documents);
    if (s_game_context)
        total += reload_all_in(s_game_context, s_game_documents);
    SDL_Log("[RmlUi] Reloaded %d/%zu document(s)", total, total_docs);
}

extern "C" void rmlui_wrapper_release_textures(void) {
    Rml::ReleaseTextures();
    SDL_Log("[RmlUi] Released all textures (will reload on next render)");
}
