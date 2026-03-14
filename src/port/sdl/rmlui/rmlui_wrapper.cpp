/**
 * @file rmlui_wrapper.cpp
 * @brief RmlUi initialization, event routing, rendering, and document management.
 *
 * Wraps RmlUi's SDL3+GL3/GPU/SDLrenderer backends for the 3SX application.
 * Selects the appropriate RmlUi renderer based on SDLApp_GetRenderer().
 * Documents are loaded from assets/ui/ and managed by name.
 */
#include "port/sdl/rmlui/rmlui_wrapper.h"
#include "port/config/config.h"
#include "port/config/paths.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/renderer/sdl_game_renderer_internal.h"
#include "port/sdl/renderer/sdl_texture_util.h"
#include "port/sdl/rmlui/rmlui_casual_lobby.h"

// GL header for RmlUi GL3 backend (glEnable, glBlendFunc, etc.)
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <glad/gl.h>
#endif

#include "lua_engine_bridge.h"
#include "lua_trials_loader.h"
#include <RmlUi/Core.h>
#include <RmlUi/Lua.h>
#ifdef DEBUG
#include <RmlUi/Debugger.h>
#endif
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

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
// Viewport adapters — override virtual SetTransform/SetScissorRegion
// to bake viewport offset + scale into the rendering pipeline without
// modifying any third-party RenderInterface classes.
// -------------------------------------------------------------------

// GL3 viewport adapter — maps logical (ctx_w × ctx_h) coordinates
// to the physical viewport (phys_w × phys_h) at letterbox offset.
// SetViewport is called with physical dims; the subclass scales
// RmlUi's logical geometry and scissor rects to physical space.
class GameViewportGL3 : public RenderInterface_GL3 {
  public:
    using RenderInterface_GL3::RenderInterface_GL3;

    void ActivateGameViewport(int ctx_w, int ctx_h, int phys_w, int phys_h) {
        m_active = true;
        m_sx = (float)phys_w / (float)ctx_w;
        m_sy = (float)phys_h / (float)ctx_h;
        m_correction = Rml::Matrix4f::Scale(m_sx, m_sy, 1.0f);
        SetTransform(nullptr);
    }

    void DeactivateGameViewport() {
        m_active = false;
        SetTransform(nullptr);
    }

    void SetTransform(const Rml::Matrix4f* transform) override {
        if (!m_active) {
            RenderInterface_GL3::SetTransform(transform);
            return;
        }
        if (transform) {
            Rml::Matrix4f modified = m_correction * (*transform);
            RenderInterface_GL3::SetTransform(&modified);
        } else {
            RenderInterface_GL3::SetTransform(&m_correction);
        }
    }

    void SetScissorRegion(Rml::Rectanglei region) override {
        if (!m_active) {
            RenderInterface_GL3::SetScissorRegion(region);
            return;
        }
        Rml::Rectanglei adjusted = Rml::Rectanglei::FromPositionSize(
            { (int)(region.Left() * m_sx + 0.5f), (int)(region.Top() * m_sy + 0.5f) },
            { (int)(region.Width() * m_sx + 0.5f), (int)(region.Height() * m_sy + 0.5f) });
        RenderInterface_GL3::SetScissorRegion(adjusted);
    }

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override {
        // RmlUi GL3 backend natively only supports TGAs. We override this to use
        // SDL_image to load PNG/JPG files. RmlUi also expects pre-multiplied alpha,
        // so we must do the conversion here before forwarding to GenerateTexture.
        SDL_Surface* surface = IMG_Load(source.c_str());
#ifndef _WIN32
        // RmlUi's SystemInterface::JoinPath strips the leading '/' from
        // absolute paths (e.g. "/userdata/..." → "userdata/...").  Retry
        // with the root prefix restored so IMG_Load can find the file.
        if (!surface && !source.empty() && source[0] != '/') {
            std::string abs_source = "/" + source;
            surface = IMG_Load(abs_source.c_str());
        }
#endif
        if (!surface)
            return 0;

        if (surface->format != SDL_PIXELFORMAT_RGBA32) {
            SDL_Surface* converted_surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(surface);
            if (!converted_surface)
                return 0;
            surface = converted_surface;
        }

        // Convert colors to premultiplied alpha (RmlUi uses glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA))
        const size_t pixels_byte_size = surface->w * surface->h * 4;
        uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
        for (size_t i = 0; i < pixels_byte_size; i += 4) {
            const uint8_t alpha = pixels[i + 3];
            for (size_t j = 0; j < 3; ++j) {
                pixels[i + j] = (uint8_t)((int(pixels[i + j]) * int(alpha)) / 255);
            }
        }

        texture_dimensions.x = surface->w;
        texture_dimensions.y = surface->h;

        Rml::Span<const Rml::byte> data { static_cast<const Rml::byte*>(surface->pixels),
                                          static_cast<size_t>(surface->pitch * surface->h) };
        Rml::TextureHandle handle = RenderInterface_GL3::GenerateTexture(data, texture_dimensions);

        SDL_DestroySurface(surface);
        return handle;
    }

    void ReleaseTexture(Rml::TextureHandle texture_handle) override {
        RenderInterface_GL3::ReleaseTexture(texture_handle);
    }

  private:
    bool m_active = false;
    float m_sx = 1.0f, m_sy = 1.0f;
    Rml::Matrix4f m_correction;
};

// GPU viewport adapter — same pattern for SDL_GPU backend.
class GameViewportGPU : public RenderInterface_SDL_GPU {
  public:
    using RenderInterface_SDL_GPU::RenderInterface_SDL_GPU;

    void ActivateGameViewport(int ctx_w, int ctx_h, int phys_w, int phys_h, int off_x, int off_y) {
        m_active = true;
        m_sx = (float)phys_w / (float)ctx_w;
        m_sy = (float)phys_h / (float)ctx_h;
        m_off_x = off_x;
        m_off_y = off_y;
        m_correction = Rml::Matrix4f::Translate((float)off_x, (float)off_y, 0) * Rml::Matrix4f::Scale(m_sx, m_sy, 1.0f);
        SetTransform(nullptr);
    }

    void DeactivateGameViewport() {
        m_active = false;
        SetTransform(nullptr);
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
static GameViewportGL3* s_render_gl3 = nullptr;
static RenderInterface_SDL* s_render_sdl = nullptr;
static GameViewportGPU* s_render_gpu = nullptr;

static RendererBackend s_active_backend;

static SDL_Window* s_window = nullptr;
static int s_window_w = 0;
static int s_window_h = 0;
static float s_hidpi_ratio = 1.0f;

// Reference width for dp-ratio scaling: menus are designed for ~1280px windows.
// At smaller windows, dp values shrink proportionally; at larger, they grow.
static constexpr float REFERENCE_W = 1280.0f;

static float compute_window_dp_ratio(int window_w) {
    float size_scale = (float)window_w / REFERENCE_W;
    // Clamp to avoid extremes (too tiny or too huge)
    if (size_scale < 0.3f)
        size_scale = 0.3f;
    if (size_scale > 3.0f)
        size_scale = 3.0f;
    return s_hidpi_ratio * size_scale;
}

static constexpr int GAME_W = 384;
static constexpr int GAME_H = 224;

// PAR correction factor for portrait images: 1.0 / (m_sy / m_sx).
// = (GAME_H * view_w) / (GAME_W * view_h)
// In 4:3 modes this is 7/9 ≈ 0.778; in square-pixel mode it is 1.0.
static float s_par_correct_y = 1.0f;

static std::string s_ui_base_path;
static bool s_deferred_init_done = false;

// ⚡ Cached visibility flags — updated on show/hide/close, checked per-frame.
// Replaces per-frame unordered_map iteration with a single bool read.
static bool s_any_window_visible = false;
static bool s_any_game_visible = false;

// Recompute cached visibility flag by scanning the document map.
// Called only on show/hide/close — NOT on the per-frame hot path.
static bool recompute_visible(const std::unordered_map<std::string, Rml::ElementDocument*>& docs) {
    for (const auto& [name, doc] : docs) {
        if (doc && doc->IsVisible())
            return true;
    }
    return false;
}

// ⚡ Track whether RmlGL3::Initialize() has been called (loads GL function
// pointers — cheap, one-time).
static bool s_gl3_loader_initialized = false;

// ⚡ On-demand GL3 renderer lifecycle.
// The GameViewportGL3 object is kept alive for the entire RmlUi lifetime
// (RmlUi's RenderManager holds a reference to the RenderInterface and asserts
// on destruction).  Instead we toggle s_render_interface to enable/disable
// rendering, and track whether the GL3 object is "active" (viewport set,
// ready to render).
static bool s_gl3_active = false;

static void ensure_gl3_ready() {
    if (s_active_backend != RENDERER_OPENGL)
        return;
    if (s_gl3_active)
        return;
    if (!s_gl3_loader_initialized) {
        Rml::String msg;
        if (!RmlGL3::Initialize(&msg)) {
            SDL_Log("[RmlUi] GL3 init failed: %s", msg.c_str());
            return;
        }
        s_gl3_loader_initialized = true;
    }
    if (!s_render_gl3) {
        s_render_gl3 = new GameViewportGL3();
    }
    s_render_interface = s_render_gl3;
    Rml::SetRenderInterface(s_render_interface);
    s_render_gl3->SetViewport(s_window_w, s_window_h);
    s_gl3_active = true;
    SDL_Log("[RmlUi] GL3 renderer activated (on-demand)");
}

static void release_gl3_if_idle() {
    if (!s_gl3_active)
        return;
    // Keep active while any documents are visible
    if (s_any_window_visible || s_any_game_visible)
        return;
    // Don't delete — RmlUi's RenderManager still references the interface.
    // Just deactivate by clearing the pointer so render calls are skipped.
    s_render_interface = nullptr;
    Rml::SetRenderInterface(nullptr);
    s_gl3_active = false;
    SDL_Log("[RmlUi] GL3 renderer deactivated (idle)");
}

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
        // RmlGL3::Initialize loads GL function pointers — cheap, one-time.
        Rml::String gl_message;
        if (!RmlGL3::Initialize(&gl_message)) {
            SDL_Log("[RmlUi] Failed to initialize GL3 backend: %s", gl_message.c_str());
            return;
        }
        s_gl3_loader_initialized = true;
        // Create GL3 renderer for init (Rml::Initialise + font loading need it).
        // Destroyed after init completes to free V3D resources.
        s_render_gl3 = new GameViewportGL3();
        s_render_interface = s_render_gl3;
        break;
    }
    case RENDERER_SDLGPU: {
        SDL_GPUDevice* device = SDLApp_GetGPUDevice();
        s_render_gpu = new GameViewportGPU(device, window);
        s_render_interface = s_render_gpu;
        break;
    }
    case RENDERER_SDL2D:
    case RENDERER_SDL2D_CLASSIC: {
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

    // Initialize RmlUi Lua plugin — creates lua_State, registers all RmlUi
    // types (Element, Document, Context, Event, form controls), and replaces
    // the <body> instancer with LuaDocument for inline <script> support.
    Rml::Lua::Initialise();
    SDL_Log("[RmlUi Lua] Initialised");

    // Register engine API so Lua scripts can access game state
    lua_engine_bridge_init();
    SDL_Log("[RmlUi Lua] Engine bridge registered");

    // Set up Lua package.path so compat modules can be found
    // and pre-load FBNeo compatibility globals (joypad, emu, gui, memory)
    Rml::Lua::Interpreter::DoString("package.path = 'lua/?.lua;lua/?/init.lua;' .. package.path\n"
                                    "joypad = require('compat.joypad')\n"
                                    "emu    = require('compat.emu')\n"
                                    "gui    = require('compat.gui')\n"
                                    "memory = require('compat.memory')\n");
    SDL_Log("[RmlUi Lua] FBNeo compat modules loaded");

    // Trial definitions are loaded lazily on first access via
    // lua_trials_get_characters(), not at boot (saves ~100ms).

    // Training bootstrap and NotoSansJP font are deferred to first
    // rmlui_wrapper_new_frame() call (~130ms saved from boot).

    // NotoSansJP (~4MB) deferred to first frame — see rmlui_wrapper_new_frame()
    std::string font_bold = s_ui_base_path + "../BoldPixels.ttf";
    if (!Rml::LoadFontFace(font_bold.c_str())) {
        SDL_Log("[RmlUi] Failed to load font: %s", font_bold.c_str());
    }

    // --- Window context (Phase 2 overlays — window resolution) ---
    s_window_context = Rml::CreateContext("window", Rml::Vector2i(s_window_w, s_window_h));
    if (!s_window_context) {
        SDL_Log("[RmlUi] Failed to create window context");
        return;
    }
    s_hidpi_ratio = SDL_GetWindowDisplayScale(window);
    if (s_hidpi_ratio <= 0.0f)
        s_hidpi_ratio = 1.0f;
    float dp_ratio = compute_window_dp_ratio(s_window_w);
    s_window_context->SetDensityIndependentPixelRatio(dp_ratio);

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

    const char* backend_name = (s_active_backend == RENDERER_SDLGPU) ? "SDL_GPU"
                               : is_sdl2d_backend(s_active_backend)  ? "SDL2D"
                                                                     : "GL3";
    SDL_Log("[RmlUi] Initialized (%s renderer, %dx%d window + %dx%d game, dp-ratio=%.2fx)",
            backend_name,
            s_window_w,
            s_window_h,
            GAME_W,
            GAME_H,
            dp_ratio);

    // ⚡ Pi4: deactivate GL3 renderer now that init is done.
    // ensure_gl3_ready() will re-activate on first actual render need.
    // The object stays alive because RmlUi's RenderManager references it.
    if (s_render_gl3) {
        s_render_interface = nullptr;
        Rml::SetRenderInterface(nullptr);
        s_gl3_active = false;
        SDL_Log("[RmlUi] GL3 renderer deactivated after init (on-demand lifecycle)");
    }
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

    // F12 debugger toggle (debug builds only)
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
    //
    // ⚡ Pi4: skip input routing when no window documents are visible.
    // On Pi4 with gamepads, SDL generates many axis/button events per frame;
    // InputEventHandler does context hover/focus tracking for each one.
    // Window resize + display-scale events are handled below independently.
    if (s_any_window_visible)
        RmlSDL::InputEventHandler(s_window_context, s_window, *event);

    // Handle window resize
    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        s_window_w = event->window.data1;
        s_window_h = event->window.data2;
        s_window_context->SetDimensions(Rml::Vector2i(s_window_w, s_window_h));
        s_window_context->SetDensityIndependentPixelRatio(compute_window_dp_ratio(s_window_w));
        if (s_render_gl3) {
            s_render_gl3->SetViewport(s_window_w, s_window_h);
        }
        // Game context stays at GAME_W×GAME_H — no resize needed
    }

    // Override dp_ratio after display-scale changes.  RmlSDL::InputEventHandler
    // handles SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED by calling
    // SetDensityIndependentPixelRatio(display_scale), which sets dp to the raw
    // OS display scale (e.g. 1.25).  We need dp = display_scale × size_scale
    // (e.g. 2.50), so re-apply our custom computation after the handler runs.
    if (event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        s_hidpi_ratio = SDL_GetWindowDisplayScale(s_window);
        if (s_hidpi_ratio <= 0.0f)
            s_hidpi_ratio = 1.0f;
        s_window_context->SetDensityIndependentPixelRatio(compute_window_dp_ratio(s_window_w));
    }
}

// -------------------------------------------------------------------
// Frame update (window context — Phase 2)
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_new_frame(void) {
    if (!s_window_context)
        return;

    // ⚡ Cached bool — zero-cost check, no map iteration
    if (!s_any_window_visible)
        return;
    ensure_gl3_ready(); // ⚡ Context::Update may trigger texture loads
    s_window_context->Update();
}

// -------------------------------------------------------------------
// Render
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_render(void) {
    if (!s_window_context)
        return;
    // ⚡ On-demand GL3: create renderer if needed
    ensure_gl3_ready();
    if (!s_render_interface)
        return;
    // ⚡ Cached bool — zero-cost check, no map iteration
    if (!s_any_window_visible)
        return;

    // Ensure layout is up-to-date before rendering. Documents lazily shown
    // during render_overlays() (e.g. control_mapping on first F1 press) will
    // have missed the earlier Update() call in rmlui_wrapper_new_frame() since
    // no docs were visible at that point.
    s_window_context->Update();

    if (s_render_gl3) {
#ifdef PLATFORM_RPI4
        // ⚡ Pi4 fast path — skip glGet* state backup + FBO blit
        s_render_gl3->BeginFrameDirect();
        // Ensure the backbuffer covers the full window for UI overlays
        glViewport(0, 0, s_window_w, s_window_h);
        s_window_context->Render();
        s_render_gl3->EndFrameDirect();
        // Restore minimal GL state for game renderer
        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);
#else
        s_render_gl3->BeginFrame();
        s_window_context->Render();
        s_render_gl3->EndFrame();
#endif
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

/** @brief Lazy-load NotoSansJP (~4MB) on first document show.
 *  Deferred from init/new_frame to avoid V3D CMA pressure on Pi4
 *  when no RmlUi documents are actually displayed. */
static void ensure_fonts_loaded(void) {
    if (s_deferred_init_done)
        return;
    s_deferred_init_done = true;

    std::string font_noto = s_ui_base_path + "../NotoSansJP-Regular.ttf";
    if (!Rml::LoadFontFace(font_noto.c_str(), true)) {
        SDL_Log("[RmlUi] Failed to load font: %s", font_noto.c_str());
    }
}

extern "C" void rmlui_wrapper_show_document(const char* name) {
    if (!s_window_context || !name)
        return;
    ensure_gl3_ready(); // ⚡ LoadDocument needs render interface for textures
    ensure_fonts_loaded();

    auto it = s_window_documents.find(name);
    if (it != s_window_documents.end()) {
        it->second->Show();
        s_any_window_visible = true;
        return;
    }

    std::string path = s_ui_base_path + name + ".rml";
    Rml::ElementDocument* doc = s_window_context->LoadDocument(path.c_str());
    if (doc) {
        doc->Show();
        s_window_documents[name] = doc;
        s_any_window_visible = true;
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
        s_any_window_visible = recompute_visible(s_window_documents);
        release_gl3_if_idle(); // ⚡ Free GPU resources when idle
    }
}

extern "C" void rmlui_wrapper_hide_all_documents(void) {
    for (auto& [name, doc] : s_window_documents) {
        if (doc && doc->IsVisible()) {
            doc->Hide();
        }
    }
    s_any_window_visible = false;
    release_gl3_if_idle(); // ⚡ Free GPU resources when idle
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
        s_any_window_visible = recompute_visible(s_window_documents);
        SDL_Log("[RmlUi] Closed window document: %s", name);
    }
}

// -------------------------------------------------------------------
// Document management — Game context (Phase 3)
// -------------------------------------------------------------------
extern "C" void rmlui_wrapper_show_game_document(const char* name) {
    if (!s_game_context || !name)
        return;
    ensure_gl3_ready(); // ⚡ LoadDocument needs render interface for textures
    ensure_fonts_loaded();

    auto it = s_game_documents.find(name);
    if (it != s_game_documents.end()) {
        it->second->Show();
        s_any_game_visible = true;
        return;
    }

    std::string path = s_ui_base_path + name + ".rml";
    Rml::ElementDocument* doc = s_game_context->LoadDocument(path.c_str());
    if (doc) {
        doc->Show();
        s_game_documents[name] = doc;
        s_any_game_visible = true;
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
        s_any_game_visible = recompute_visible(s_game_documents);
        release_gl3_if_idle(); // ⚡ Free GPU resources when idle
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
    s_any_game_visible = recompute_visible(s_game_documents);
    release_gl3_if_idle(); // ⚡ Free GPU resources when idle
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

extern "C" bool rmlui_wrapper_any_game_visible(void) {
    return s_any_game_visible;
}

extern "C" bool rmlui_wrapper_any_window_visible(void) {
    return s_any_window_visible;
}

static int s_window_popup_count = 0;

extern "C" void rmlui_wrapper_set_window_popup_visible(bool visible) {
    if (visible) {
        s_window_popup_count++;
        s_any_window_visible = true;
    } else {
        if (s_window_popup_count > 0) s_window_popup_count--;
        if (s_window_popup_count == 0) {
            s_any_window_visible = recompute_visible(s_window_documents);
        }
    }
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
    // ⚡ Cached bool — zero-cost check, no map iteration
    if (!s_any_game_visible)
        return;
    ensure_gl3_ready(); // ⚡ Context::Update may trigger texture loads
    s_game_context->Update();
}

extern "C" void rmlui_wrapper_render_game(int win_w, int win_h, float view_x, float view_y, float view_w,
                                          float view_h) {
    if (!s_game_context)
        return;
    // ⚡ Cached bool — zero-cost check, no map iteration
    if (!s_any_game_visible)
        return;

    // ⚡ On-demand GL3: create renderer if needed
    ensure_gl3_ready();

    // Reset GL state to clean baseline before RmlUi rendering (GL3 only).
    // Moved here from sdl_app.c so it only runs when docs are actually visible.
    if (s_render_gl3) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        glBindVertexArray(0);
        glViewport(0, 0, win_w, win_h);
    }

    // Use independent X/Y dp ratios so UI elements map 1:1 to physical pixels
    // without the CPS3 9/7 PAR stretch that the game canvas gets.
    // The smaller ratio ensures the UI fits within the viewport.
    const float dp_x = view_w / (float)GAME_W;
    const float dp_y = view_h / (float)GAME_H;
    const float dp_ratio = (dp_x < dp_y) ? dp_x : dp_y;
    s_game_context->SetDensityIndependentPixelRatio(dp_ratio);

    // PAR correction factor for portrait images (e.g. char select).
    // In 4:3 modes this is 7/9 ≈ 0.778; in square-pixel mode it is 1.0.
    if (view_h > 0.0f)
        s_par_correct_y = (view_w * (float)GAME_H) / (view_h * (float)GAME_W);
    else
        s_par_correct_y = 1.0f;

    // Context dimensions match the physical viewport so the viewport
    // adapter scales 1:1 (no PAR distortion on UI elements).
    const int ctx_w = (int)(view_w + 0.5f);
    const int ctx_h = (int)(view_h + 0.5f);
    s_game_context->SetDimensions(Rml::Vector2i(ctx_w, ctx_h));

    const int phys_w = (int)(view_w + 0.5f);
    const int phys_h = (int)(view_h + 0.5f);
    const int off_x = (int)(view_x + 0.5f);
    // OpenGL y=0 is bottom; convert from top-left origin
    const int off_y = win_h - (int)(view_y + 0.5f) - phys_h;

    if (s_render_gl3) {
        // GL3: Set physical viewport and use the GameViewportGL3 adapter
        // to scale RmlUi's logical coordinates to physical pixels.
        // The adapter bakes the ctx→phys scale into SetTransform/SetScissorRegion
        // (same pattern as GameViewportGPU), applying the CPS3 9/7 vertical stretch.
        s_render_gl3->SetViewport(phys_w, phys_h, off_x, off_y);
        s_render_gl3->ActivateGameViewport(ctx_w, ctx_h, phys_w, phys_h);

        // ⚡ Pi4 fast path — skip glGet* state backup + FBO blit
#ifdef PLATFORM_RPI4
        s_render_gl3->BeginFrameDirect();
        // Constrain direct backbuffer rendering to the letterbox viewport
        // (BeginFrameDirect skips FBOs so it relies on the active GL viewport)
        glViewport(off_x, off_y, phys_w, phys_h);
        s_game_context->Render();
        s_render_gl3->EndFrameDirect();
#else
        s_render_gl3->BeginFrame();
        s_game_context->Render();
        s_render_gl3->EndFrame();
#endif

        s_render_gl3->DeactivateGameViewport();

#ifdef PLATFORM_RPI4
        // Restore minimal GL state for game renderer
        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif

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
// PAR Correction Accessor
// -------------------------------------------------------------------
extern "C" float rmlui_wrapper_get_par_correct_y(void) {
    return s_par_correct_y;
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
