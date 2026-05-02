/**
 * @file sdl_game_renderer.c
 * @brief Game renderer backend dispatch via vtable.
 *
 * Each SDLGameRenderer_*() function is now a one-liner that
 * forwards to g_game_renderer->Xxx().  The if/else chain has been
 * replaced by four static const vtable instances.
 */
#include "port/sdl/renderer/sdl_game_renderer.h"
#include "port/game_renderer_vtable.h"
#include "port/renderer_caps.h"
#include "port/texture_util_vtable.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/renderer/sdl_game_renderer_internal.h"
#include "port/sdl/renderer/sprite_override.h"
#include "port/sdl/renderer/gl_compat.h"
#include "port/render_pass.h"
#include "port/render_job.h"
#include <assert.h>
#include <string.h>

/* ================================================================
 *  RendererCaps — runtime GPU capability detection
 * ================================================================ */

RendererCaps g_renderer_caps;
RenderPassTable g_render_passes;

static TransientTexture s_transient_textures[TRANSIENT_TEXTURE_COUNT];

void RenderGraph_Compile(void) {
    // 1. Initial pass culling (geometry / force execute check)
    for (int i = 0; i < g_render_passes.count; i++) {
        if (!g_render_passes.passes[i].has_geometry && !g_render_passes.passes[i].force_execute) {
            g_render_passes.passes[i].skip_this_frame = true;
        } else {
            g_render_passes.passes[i].skip_this_frame = false;
        }
    }

    // 2. Automated Pass Culling based on Lifespans (Dead-Code Elimination)
    // Keep track of which transient textures are needed by active subsequent passes
    bool transient_needed[TRANSIENT_TEXTURE_COUNT] = {false};

    // Iterate backwards to propagate dependencies from end of frame to beginning
    for (int i = g_render_passes.count - 1; i >= 0; i--) {
        if (g_render_passes.passes[i].skip_this_frame) {
            continue; // Skip already culled passes
        }

        bool is_needed = true; // Assume needed unless proven otherwise

        // If this pass outputs to a transient texture, check if any later pass needs it
        if (g_render_passes.passes[i].transient_output >= 0 && 
            g_render_passes.passes[i].transient_output < TRANSIENT_TEXTURE_COUNT) {
            
            if (!transient_needed[g_render_passes.passes[i].transient_output]) {
                // No later pass needs this output! We can cull this pass safely.
                is_needed = false;
            }
        }

        if (is_needed) {
            // Since this pass is active and needed, mark all of its inputs as "needed"
            for (int j = 0; j < 4; j++) {
                int input_id = g_render_passes.passes[i].transient_inputs[j];
                if (input_id >= 0 && input_id < TRANSIENT_TEXTURE_COUNT) {
                    transient_needed[input_id] = true;
                }
            }
        } else {
            // Cull it!
            g_render_passes.passes[i].skip_this_frame = true;
        }
    }
}

void RenderGraph_Execute(int vp_x, int vp_y, int vp_w, int vp_h) {
    for (int i = 0; i < g_render_passes.count; i++) {
        if (g_render_passes.passes[i].skip_this_frame) {
            continue;
        }

        if (g_render_passes.passes[i].execute_callback) {
            g_render_passes.passes[i].execute_callback(i, g_render_passes.passes[i].user_data, vp_x, vp_y, vp_w, vp_h);
        } else {
            SDLGameRenderer_ExecutePass(i, vp_x, vp_y, vp_w, vp_h);
        }
    }
}

void RendererCaps_Detect(void) {
    memset(&g_renderer_caps, 0, sizeof(g_renderer_caps));

    RendererBackend backend = SDLApp_GetRenderer();

    if (backend == RENDERER_OPENGL) {
#if GL_COMPAT_ES
        g_renderer_caps.is_gles = true;
#else
        g_renderer_caps.is_gles = false;
#endif
        g_renderer_caps.has_get_tex_level_param = !g_renderer_caps.is_gles;

#ifdef GL_ARB_buffer_storage
        g_renderer_caps.has_persistent_mapping = (bool)GLAD_GL_ARB_buffer_storage;
#else
        g_renderer_caps.has_persistent_mapping = false;
#endif

        g_renderer_caps.has_texture_arrays = true;

#if GL_COMPAT_ES
        g_renderer_caps.has_compute_shaders = false;
        {
            const char* version = (const char*)glGetString(GL_VERSION);
            if (version) {
                const char* es = strstr(version, "ES ");
                if (es) {
                    int major = 0, minor = 0;
                    if (SDL_sscanf(es + 3, "%d.%d", &major, &minor) == 2) {
                        g_renderer_caps.has_compute_shaders = (major > 3 || (major == 3 && minor >= 1));
                    }
                }
            }
        }
#else
        g_renderer_caps.has_compute_shaders = (bool)GLAD_GL_ARB_compute_shader;
#endif

        g_renderer_caps.has_pbo = true;

        GLint max_tex = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
        g_renderer_caps.max_texture_size = (uint32_t)max_tex;

        GLint max_layers = 0;
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
        g_renderer_caps.max_array_layers = (uint32_t)max_layers;

        SDL_Log("[RendererCaps] %s | tex_level_param=%d persistent_map=%d "
                "compute=%d max_tex=%u max_layers=%u",
                g_renderer_caps.is_gles ? "GLES" : "GL",
                g_renderer_caps.has_get_tex_level_param,
                g_renderer_caps.has_persistent_mapping,
                g_renderer_caps.has_compute_shaders,
                g_renderer_caps.max_texture_size,
                g_renderer_caps.max_array_layers);

    } else if (backend == RENDERER_SDLGPU) {
        g_renderer_caps.has_texture_arrays = true;
        g_renderer_caps.has_compute_shaders = true;
        g_renderer_caps.max_texture_size = 8192;
        g_renderer_caps.max_array_layers = 2048;
        SDL_Log("[RendererCaps] SDL_GPU backend");
    } else {
        g_renderer_caps.max_texture_size = 4096;
        SDL_Log("[RendererCaps] SDL2D backend");
    }
}

/* ================================================================
 *  Static const vtable instances — one per backend
 * ================================================================ */

static const GameRendererVtable s_vtable_gl = {
    .Init            = SDLGameRendererGL_Init,
    .Shutdown        = SDLGameRendererGL_Shutdown,
    .BeginFrame      = SDLGameRendererGL_BeginFrame,
    .RenderFrame     = SDLGameRendererGL_RenderFrame,
    .ExecutePass     = SDLGameRendererGL_ExecutePass,
    .EndFrame        = SDLGameRendererGL_EndFrame,
    .CreateTexture   = SDLGameRendererGL_CreateTexture,
    .DestroyTexture  = SDLGameRendererGL_DestroyTexture,
    .UnlockTexture   = SDLGameRendererGL_UnlockTexture,
    .CreatePalette   = SDLGameRendererGL_CreatePalette,
    .DestroyPalette  = SDLGameRendererGL_DestroyPalette,
    .UnlockPalette   = SDLGameRendererGL_UnlockPalette,
    .SetTexture      = SDLGameRendererGL_SetTexture,
    .CreateTransientRenderTarget = SDLGameRendererGL_CreateTransientRenderTarget,
    .DestroyTransientRenderTarget = SDLGameRendererGL_DestroyTransientRenderTarget,
    .BindTransientRenderTarget = SDLGameRendererGL_BindTransientRenderTarget,
    .SetBlendMode    = SDLGameRendererGL_SetBlendMode,
    .DrawTexturedQuad = SDLGameRendererGL_DrawTexturedQuad,
    .DrawSolidQuad   = SDLGameRendererGL_DrawSolidQuad,
    .DrawSprite      = SDLGameRendererGL_DrawSprite,
    .DrawSprite2     = SDLGameRendererGL_DrawSprite2,
    .FlushSprite2Batch = SDLGameRendererGL_FlushSprite2Batch,
    .GetCachedGLTexture = SDLGameRendererGL_GetCachedGLTexture,
    .DumpTextures    = SDLGameRendererGL_DumpTextures,
    .DrawOverlayQuad = SDLGameRendererGL_DrawOverlayQuad,
    .DrawOverlayQuadEx = SDLGameRendererGL_DrawOverlayQuadEx,
    .DrawOverlaySubQuadEx = SDLGameRendererGL_DrawOverlaySubQuadEx,
};

static const GameRendererVtable s_vtable_gpu = {
    .Init            = SDLGameRendererGPU_Init,
    .Shutdown        = SDLGameRendererGPU_Shutdown,
    .BeginFrame      = SDLGameRendererGPU_BeginFrame,
    .RenderFrame     = SDLGameRendererGPU_RenderFrame,
    .ExecutePass     = SDLGameRendererGPU_ExecutePass,
    .EndFrame        = SDLGameRendererGPU_EndFrame,
    .CreateTexture   = SDLGameRendererGPU_CreateTexture,
    .DestroyTexture  = SDLGameRendererGPU_DestroyTexture,
    .UnlockTexture   = SDLGameRendererGPU_UnlockTexture,
    .CreatePalette   = SDLGameRendererGPU_CreatePalette,
    .DestroyPalette  = SDLGameRendererGPU_DestroyPalette,
    .UnlockPalette   = SDLGameRendererGPU_UnlockPalette,
    .SetTexture      = SDLGameRendererGPU_SetTexture,
    .CreateTransientRenderTarget = SDLGameRendererGPU_CreateTransientRenderTarget,
    .DestroyTransientRenderTarget = SDLGameRendererGPU_DestroyTransientRenderTarget,
    .BindTransientRenderTarget = SDLGameRendererGPU_BindTransientRenderTarget,
    .SetBlendMode    = SDLGameRendererGPU_SetBlendMode,
    .DrawTexturedQuad = SDLGameRendererGPU_DrawTexturedQuad,
    .DrawSolidQuad   = SDLGameRendererGPU_DrawSolidQuad,
    .DrawSprite      = SDLGameRendererGPU_DrawSprite,
    .DrawSprite2     = SDLGameRendererGPU_DrawSprite2,
    .FlushSprite2Batch = SDLGameRendererGPU_FlushSprite2Batch,
    .GetCachedGLTexture = SDLGameRendererGPU_GetCachedGLTexture,
    .DumpTextures    = SDLGameRendererGPU_DumpTextures,
    .DrawOverlayQuad = SDLGameRendererGPU_DrawOverlayQuad,
    .DrawOverlayQuadEx = SDLGameRendererGPU_DrawOverlayQuadEx,
    .DrawOverlaySubQuadEx = SDLGameRendererGPU_DrawOverlaySubQuadEx,
};

static const GameRendererVtable s_vtable_sdl = {
    .Init            = SDLGameRendererSDL_Init,
    .Shutdown        = SDLGameRendererSDL_Shutdown,
    .BeginFrame      = SDLGameRendererSDL_BeginFrame,
    .RenderFrame     = SDLGameRendererSDL_RenderFrame,
    .ExecutePass     = SDLGameRendererSDL_ExecutePass,
    .EndFrame        = SDLGameRendererSDL_EndFrame,
    .CreateTexture   = SDLGameRendererSDL_CreateTexture,
    .DestroyTexture  = SDLGameRendererSDL_DestroyTexture,
    .UnlockTexture   = SDLGameRendererSDL_UnlockTexture,
    .CreatePalette   = SDLGameRendererSDL_CreatePalette,
    .DestroyPalette  = SDLGameRendererSDL_DestroyPalette,
    .UnlockPalette   = SDLGameRendererSDL_UnlockPalette,
    .SetTexture      = SDLGameRendererSDL_SetTexture,
    .CreateTransientRenderTarget = SDLGameRendererSDL_CreateTransientRenderTarget,
    .DestroyTransientRenderTarget = SDLGameRendererSDL_DestroyTransientRenderTarget,
    .BindTransientRenderTarget = SDLGameRendererSDL_BindTransientRenderTarget,
    .SetBlendMode    = SDLGameRendererSDL_SetBlendMode,
    .DrawTexturedQuad = SDLGameRendererSDL_DrawTexturedQuad,
    .DrawSolidQuad   = SDLGameRendererSDL_DrawSolidQuad,
    .DrawSprite      = SDLGameRendererSDL_DrawSprite,
    .DrawSprite2     = SDLGameRendererSDL_DrawSprite2,
    .FlushSprite2Batch = SDLGameRendererSDL_FlushSprite2Batch,
    .GetCachedGLTexture = SDLGameRendererSDL_GetCachedGLTexture,
    .DumpTextures    = SDLGameRendererSDL_DumpTextures,
    .DrawOverlayQuad = SDLGameRendererSDL_DrawOverlayQuad,
    .DrawOverlayQuadEx = SDLGameRendererSDL_DrawOverlayQuadEx,
    .DrawOverlaySubQuadEx = SDLGameRendererSDL_DrawOverlaySubQuadEx,
};


static const GameRendererVtable s_vtable_classic = {
    .Init            = SDLGameRendererClassic_Init,
    .Shutdown        = SDLGameRendererClassic_Shutdown,
    .BeginFrame      = SDLGameRendererClassic_BeginFrame,
    .RenderFrame     = SDLGameRendererClassic_RenderFrame,
    .ExecutePass     = NULL,
    .EndFrame        = SDLGameRendererClassic_EndFrame,
    .CreateTexture   = SDLGameRendererClassic_CreateTexture,
    .DestroyTexture  = SDLGameRendererClassic_DestroyTexture,
    .UnlockTexture   = SDLGameRendererClassic_UnlockTexture,
    .CreatePalette   = SDLGameRendererClassic_CreatePalette,
    .DestroyPalette  = SDLGameRendererClassic_DestroyPalette,
    .UnlockPalette   = SDLGameRendererClassic_UnlockPalette,
    .SetTexture      = SDLGameRendererClassic_SetTexture,
    .SetBlendMode    = SDLGameRendererClassic_SetBlendMode,
    .DrawTexturedQuad = SDLGameRendererClassic_DrawTexturedQuad,
    .DrawSolidQuad   = SDLGameRendererClassic_DrawSolidQuad,
    .DrawSprite      = SDLGameRendererClassic_DrawSprite,
    .DrawSprite2     = SDLGameRendererClassic_DrawSprite2,
    .FlushSprite2Batch = SDLGameRendererClassic_FlushSprite2Batch,
    .GetCachedGLTexture = SDLGameRendererClassic_GetCachedGLTexture,
    .DumpTextures    = SDLGameRendererClassic_DumpTextures,
    .DrawOverlayQuad = SDLGameRendererClassic_DrawOverlayQuad,
    .DrawOverlayQuadEx = SDLGameRendererClassic_DrawOverlayQuadEx,
    .DrawOverlaySubQuadEx = SDLGameRendererClassic_DrawOverlaySubQuadEx,
};

/* ================================================================
 *  Global vtable pointer
 * ================================================================ */

const GameRendererVtable* g_game_renderer = NULL;

void GameRendererVtable_Init(void) {
    RendererBackend r = SDLApp_GetRenderer();
    switch (r) {
    case RENDERER_SDLGPU:       g_game_renderer = &s_vtable_gpu;     break;
    case RENDERER_SDL2D:        g_game_renderer = &s_vtable_sdl;     break;
    case RENDERER_SDL2D_CLASSIC: g_game_renderer = &s_vtable_classic; break;
    case RENDERER_OPENGL:       /* FALLTHROUGH */
    default:                    g_game_renderer = &s_vtable_gl;      break;
    }
    assert(g_game_renderer && "GameRendererVtable_Init: vtable not set");
}

/* ================================================================
 *  Public API — thin one-liner dispatch
 * ================================================================ */

void SDLGameRenderer_Init() {
    GameRendererVtable_Init();
    RendererCaps_Detect();
    TextureUtilVtable_Init();
    
    // Initialize RenderGraph passes
    g_render_passes.count = 5;
    for(int i = 0; i < g_render_passes.count; i++) {
        g_render_passes.passes[i].transient_output = -1;
        for(int j = 0; j < 4; j++) g_render_passes.passes[i].transient_inputs[j] = -1;
        g_render_passes.passes[i].execute_callback = NULL;
        g_render_passes.passes[i].user_data = NULL;
    }

    g_render_passes.passes[0].name = "CPS3 Canvas";
    g_render_passes.passes[0].force_execute = true;
    
    g_render_passes.passes[1].name = "HD Backgrounds";
    g_render_passes.passes[1].force_execute = false;
    g_render_passes.passes[1].transient_output = TRANSIENT_TEXTURE_COMPOSITION;
    
    g_render_passes.passes[2].name = "HD Foregrounds";
    g_render_passes.passes[2].force_execute = false;

    g_render_passes.passes[3].name = "Composition";
    g_render_passes.passes[3].force_execute = true;
    g_render_passes.passes[3].transient_inputs[0] = TRANSIENT_TEXTURE_COMPOSITION;
    g_render_passes.passes[3].transient_output = TRANSIENT_TEXTURE_LIBRASHADER;

    g_render_passes.passes[4].name = "Librashader Post-Processing";
    g_render_passes.passes[4].force_execute = true;
    g_render_passes.passes[4].transient_inputs[0] = TRANSIENT_TEXTURE_LIBRASHADER;

    g_game_renderer->Init();

    // Initialize render job queue (Phase 3: threading infrastructure)
    // GPU backend: 2 workers for concurrent pass recording
    // GL/SDL2D: 0 workers (synchronous — GL requires context exclusivity)
    int worker_count = (SDLApp_GetRenderer() == RENDERER_SDLGPU) ? 2 : 0;
    RenderJobQueue_Init(worker_count);
}

void SDLGameRenderer_Shutdown() {
    RenderJobQueue_Shutdown();
    SpriteOverride_Shutdown();
    g_game_renderer->Shutdown();
}

void SDLGameRenderer_BeginFrame() {
    g_game_renderer->BeginFrame();
}

TransientTexture* SDLGameRenderer_GetTransientTexture(TransientTextureID id, int width, int height) {
    if (id < 0 || id >= TRANSIENT_TEXTURE_COUNT) return NULL;
    if (!g_game_renderer || !g_game_renderer->CreateTransientRenderTarget) return NULL;

    TransientTexture* tex = &s_transient_textures[id];

    // If size changed or not created yet
    if (tex->backend_handle == NULL || tex->width != width || tex->height != height) {
        if (tex->backend_handle != NULL) {
            g_game_renderer->DestroyTransientRenderTarget(tex->backend_handle);
            tex->backend_handle = NULL;
        }
        tex->backend_handle = g_game_renderer->CreateTransientRenderTarget(width, height);
        tex->width = width;
        tex->height = height;
    }

    return tex;
}

void SDLGameRenderer_DestroyTransientTextures(void) {
    if (!g_game_renderer || !g_game_renderer->DestroyTransientRenderTarget) return;
    for (int i = 0; i < TRANSIENT_TEXTURE_COUNT; i++) {
        if (s_transient_textures[i].backend_handle != NULL) {
            g_game_renderer->DestroyTransientRenderTarget(s_transient_textures[i].backend_handle);
            s_transient_textures[i].backend_handle = NULL;
        }
    }
}

void SDLGameRenderer_RenderFrame() {
    /* GPU backend populates has_geometry per-pass internally before calling
     * RenderGraph_Compile(). For GL/SDL2D/Classic, conservatively mark all
     * passes as having geometry — their own early-returns handle culling. */
    if (SDLApp_GetRenderer() != RENDERER_SDLGPU) {
        for (int i = 0; i < g_render_passes.count; i++) {
            g_render_passes.passes[i].has_geometry = true;
        }
        RenderGraph_Compile();
    }
    g_game_renderer->RenderFrame();
}

void SDLGameRenderer_ExecutePass(int pass_index, int viewport_x, int viewport_y, int viewport_w, int viewport_h) {
    if (g_game_renderer->ExecutePass) {
        g_game_renderer->ExecutePass(pass_index, viewport_x, viewport_y, viewport_w, viewport_h);
    }
}

void SDLGameRenderer_EndFrame() {
    g_game_renderer->EndFrame();
}

void SDLGameRenderer_CreateTexture(unsigned int th) {
    g_game_renderer->CreateTexture(th);
}

void SDLGameRenderer_DestroyTexture(unsigned int texture_handle) {
    g_game_renderer->DestroyTexture(texture_handle);
}

void SDLGameRenderer_UnlockTexture(unsigned int th) {
    g_game_renderer->UnlockTexture(th);
}

void SDLGameRenderer_CreatePalette(unsigned int ph) {
    g_game_renderer->CreatePalette(ph);
}

void SDLGameRenderer_DestroyPalette(unsigned int palette_handle) {
    g_game_renderer->DestroyPalette(palette_handle);
}

void SDLGameRenderer_UnlockPalette(unsigned int ph) {
    g_game_renderer->UnlockPalette(ph);
}

void SDLGameRenderer_SetTexture(unsigned int th) {
    g_game_renderer->SetTexture(th);
}

void SDLGameRenderer_SetBlendMode(RendererBlendMode mode) {
    g_game_renderer->SetBlendMode(mode);
}

void SDLGameRenderer_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    g_game_renderer->DrawTexturedQuad(sprite, color);
}

void SDLGameRenderer_DrawSolidQuad(const Quad* vertices, unsigned int color) {
    g_game_renderer->DrawSolidQuad(vertices, color);
}

void SDLGameRenderer_DrawSprite(const Sprite* sprite, unsigned int color) {
    g_game_renderer->DrawSprite(sprite, color);
}

void SDLGameRenderer_DrawSprite2(const Sprite2* sprite2) {
    g_game_renderer->DrawSprite2(sprite2);
}

unsigned int SDLGameRenderer_GetCachedGLTexture(unsigned int texture_handle, unsigned int palette_handle) {
    return g_game_renderer->GetCachedGLTexture(texture_handle, palette_handle);
}

void SDLGameRenderer_DumpTextures(void) {
    g_game_renderer->DumpTextures();
}

void SDLGameRenderer_FlushSprite2Batch(Sprite2* chips, const unsigned char* active_layers, int count) {
    g_game_renderer->FlushSprite2Batch(chips, active_layers, count);
}

/* ================================================================
 *  Standalone backend-specific functions (NOT in vtable)
 * ================================================================ */

// ⚡ Opt6: LZ77 GPU compute dispatch — only available on GPU backend
int Renderer_LZ77Available(void) {
    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        return SDLGameRendererGPU_LZ77Available();
    }
    return 0;
}

int Renderer_LZ77Enqueue(const u8* compressed, u32 comp_size, u32 decomp_size, int texture_handle, int palette_handle,
                         u32 code, u32 tile_dim) {
    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        return SDLGameRendererGPU_LZ77Enqueue(
            compressed, comp_size, decomp_size, texture_handle, palette_handle, code, tile_dim);
    }
    return 0;
}
