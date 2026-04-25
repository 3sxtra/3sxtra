/**
 * @file sdl_texture_util.cpp
 * @brief Standalone texture loading, sizing, and cleanup.
 *
 * Backend dispatch via TextureUtilVtable. Each public TextureUtil_*()
 * function is a one-liner forward to g_texture_util->Xxx().
 */
#include "port/sdl/renderer/sdl_texture_util.h"
#include "port/config/paths.h"
#include "port/renderer_caps.h"
#include "port/texture_util_vtable.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/renderer/sdl_game_renderer_internal.h"
#include "port/sdl/renderer/sdl_gpu_metadata.h"
#include "port/game_renderer_vtable.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "port/sdl/renderer/gl_compat.h"
#include <string.h>
#include <unordered_map>

/* ================================================================
 *  GPU backend — metadata tracking
 * ================================================================ */

struct GPUTextureMetadata {
    SDL_GPUTexture* texture;
    int w, h;
    uint32_t* pixels; /* cached RGBA8888 pixels for staging upload */
};

static std::unordered_map<void*, GPUTextureMetadata> s_gpu_textures;
static void* s_last_gpu_tex_id = nullptr;
static GPUTextureMetadata* s_last_gpu_meta = nullptr;

static GPUTextureMetadata* get_gpu_metadata(void* texture_id) {
    if (texture_id == s_last_gpu_tex_id) {
        return s_last_gpu_meta;
    }
    auto it = s_gpu_textures.find(texture_id);
    if (it != s_gpu_textures.end()) {
        s_last_gpu_tex_id = texture_id;
        s_last_gpu_meta = &it->second;
        return s_last_gpu_meta;
    }
    return nullptr;
}

/* Cached texture sizes for GLES (which lacks glGetTexLevelParameteriv).
 * Always compiled; only populated when !g_renderer_caps.has_get_tex_level_param. */
static std::unordered_map<GLuint, std::pair<int, int>> s_gles_texture_sizes;

/* ================================================================
 *  Shared helpers
 * ================================================================ */

static void premultiply_surface_alpha(SDL_Surface* surface) {
    if (!surface || surface->format != SDL_PIXELFORMAT_RGBA32)
        return;
    Uint8* pixels = (Uint8*)surface->pixels;
    int pitch = surface->pitch;
    for (int y = 0; y < surface->h; y++) {
        Uint8* row = pixels + y * pitch;
        for (int x = 0; x < surface->w; x++) {
            Uint8* p = row + x * 4;
            Uint8 a = p[3];
            if (a == 0) {
                p[0] = p[1] = p[2] = 0;
            } else if (a < 255) {
                p[0] = (Uint8)((p[0] * a + 127) / 255);
                p[1] = (Uint8)((p[1] * a + 127) / 255);
                p[2] = (Uint8)((p[2] * a + 127) / 255);
            }
        }
    }
}

/* ================================================================
 *  GPU backend implementation
 * ================================================================ */

static void* texutil_gpu_load_from_surface(SDL_Surface* surface) {
    if (!surface)
        return NULL;

    SDL_GPUDevice* device = SDLApp_GetGPUDevice();
    if (!device)
        return NULL;

    SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    if (!converted)
        return NULL;

    premultiply_surface_alpha(converted);

    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width = converted->w;
    tex_info.height = converted->h;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;

    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &tex_info);
    if (!texture) {
        SDL_DestroySurface(converted);
        return NULL;
    }

    SDL_GPUTransferBufferCreateInfo tb_info;
    SDL_zero(tb_info);
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size = converted->w * converted->h * 4;

    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(device, &tb_info);
    void* map = SDL_MapGPUTransferBuffer(device, tb, false);
    if (map) {
        memcpy(map, converted->pixels, converted->w * converted->h * 4);
        SDL_UnmapGPUTransferBuffer(device, tb);

        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cb);

        SDL_GPUTextureTransferInfo src;
        SDL_zero(src);
        src.transfer_buffer = tb;

        SDL_GPUTextureRegion dst;
        SDL_zero(dst);
        dst.texture = texture;
        dst.w = converted->w;
        dst.h = converted->h;
        dst.d = 1;

        SDL_UploadToGPUTexture(cp, &src, &dst, false);
        SDL_EndGPUCopyPass(cp);
        SDL_SubmitGPUCommandBuffer(cb);
    }

    SDL_ReleaseGPUTransferBuffer(device, tb);

    GPUTextureMetadata meta = { texture, converted->w, converted->h, nullptr };
    size_t px_size = (size_t)converted->w * converted->h * 4;
    meta.pixels = (uint32_t*)SDL_malloc(px_size);
    if (meta.pixels) {
        memcpy(meta.pixels, converted->pixels, px_size);
    }
    s_gpu_textures[(void*)texture] = meta;

    SDL_DestroySurface(converted);
    return (void*)texture;
}

static void texutil_gpu_free(void* texture_id) {
    auto it = s_gpu_textures.find(texture_id);
    if (it != s_gpu_textures.end()) {
        SDL_GPUDevice* device = SDLApp_GetGPUDevice();
        if (device)
            SDL_ReleaseGPUTexture(device, it->second.texture);
        if (it->second.pixels)
            SDL_free(it->second.pixels);
        s_gpu_textures.erase(it);
        if (texture_id == s_last_gpu_tex_id) {
            s_last_gpu_tex_id = nullptr;
            s_last_gpu_meta = nullptr;
        }
    }
}

static void texutil_gpu_get_size(void* texture_id, int* w, int* h) {
    GPUTextureMetadata* meta = get_gpu_metadata(texture_id);
    if (meta) {
        if (w) *w = meta->w;
        if (h) *h = meta->h;
    } else {
        if (w) *w = 0;
        if (h) *h = 0;
    }
}

static void texutil_gpu_shutdown(void) {
    SDL_GPUDevice* device = SDLApp_GetGPUDevice();
    for (auto& pair : s_gpu_textures) {
        if (device)
            SDL_ReleaseGPUTexture(device, pair.second.texture);
        if (pair.second.pixels)
            SDL_free(pair.second.pixels);
    }
    s_gpu_textures.clear();
    s_last_gpu_tex_id = nullptr;
    s_last_gpu_meta = nullptr;
}

/* ================================================================
 *  SDL2D backend implementation
 * ================================================================ */

static void* texutil_sdl2d_load_from_surface(SDL_Surface* surface) {
    if (!surface)
        return NULL;
    SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
    if (!renderer)
        return NULL;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
    }
    return (void*)texture;
}

static void texutil_sdl2d_free(void* texture_id) {
    SDL_Texture* tex = (SDL_Texture*)texture_id;
    SDL_DestroyTexture(tex);
}

static void texutil_sdl2d_get_size(void* texture_id, int* w, int* h) {
    SDL_Texture* tex = (SDL_Texture*)texture_id;
    if (w || h) {
        float fw, fh;
        SDL_GetTextureSize(tex, &fw, &fh);
        if (w) *w = (int)fw;
        if (h) *h = (int)fh;
    }
}

static void texutil_sdl2d_shutdown(void) {
    /* SDL2D textures are freed individually via Free(); nothing to batch-release */
}

/* ================================================================
 *  GL backend implementation
 * ================================================================ */

static void* texutil_gl_load_from_surface(SDL_Surface* surface) {
    if (!surface)
        return NULL;

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    if (converted) {
        premultiply_surface_alpha(converted);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);

        if (!g_renderer_caps.has_get_tex_level_param) {
            s_gles_texture_sizes[texture_id] = std::make_pair(converted->w, converted->h);
        }

        SDL_DestroySurface(converted);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return (void*)(intptr_t)texture_id;
}

static void texutil_gl_free(void* texture_id) {
    GLuint id = (GLuint)(intptr_t)texture_id;
    if (!g_renderer_caps.has_get_tex_level_param) {
        s_gles_texture_sizes.erase(id);
    }
    glDeleteTextures(1, &id);
}

static void texutil_gl_get_size(void* texture_id, int* w, int* h) {
    GLuint id = (GLuint)(intptr_t)texture_id;
    if (g_renderer_caps.has_get_tex_level_param) {
        glBindTexture(GL_TEXTURE_2D, id);
        if (w) glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, w);
        if (h) glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, h);
    } else {
        /* GLES doesn't have glGetTexLevelParameteriv — use cached sizes */
        auto it = s_gles_texture_sizes.find(id);
        if (it != s_gles_texture_sizes.end()) {
            if (w) *w = it->second.first;
            if (h) *h = it->second.second;
        } else {
            if (w) *w = 0;
            if (h) *h = 0;
        }
    }
}

static void texutil_gl_shutdown(void) {
    s_gles_texture_sizes.clear();
}

/* ================================================================
 *  Static const vtable instances
 * ================================================================ */

static const TextureUtilVtable s_vtable_gpu = {
    .LoadFromSurface = texutil_gpu_load_from_surface,
    .Free            = texutil_gpu_free,
    .GetSize         = texutil_gpu_get_size,
    .Shutdown        = texutil_gpu_shutdown,
};

static const TextureUtilVtable s_vtable_sdl2d = {
    .LoadFromSurface = texutil_sdl2d_load_from_surface,
    .Free            = texutil_sdl2d_free,
    .GetSize         = texutil_sdl2d_get_size,
    .Shutdown        = texutil_sdl2d_shutdown,
};

static const TextureUtilVtable s_vtable_gl = {
    .LoadFromSurface = texutil_gl_load_from_surface,
    .Free            = texutil_gl_free,
    .GetSize         = texutil_gl_get_size,
    .Shutdown        = texutil_gl_shutdown,
};

/* ================================================================
 *  Global vtable pointer
 * ================================================================ */

const TextureUtilVtable* g_texture_util = NULL;

void TextureUtilVtable_Init(void) {
    RendererBackend r = SDLApp_GetRenderer();
    switch (r) {
    case RENDERER_SDLGPU:        g_texture_util = &s_vtable_gpu;   break;
    case RENDERER_SDL2D:         /* FALLTHROUGH */
    case RENDERER_SDL2D_CLASSIC: g_texture_util = &s_vtable_sdl2d; break;
    case RENDERER_OPENGL:        /* FALLTHROUGH */
    default:                     g_texture_util = &s_vtable_gl;    break;
    }
}

/* ================================================================
 *  Public API — thin one-liner dispatch
 * ================================================================ */

extern "C" void* TextureUtil_Load(const char* filename) {
    const char* resolved = Paths_ResolveAsset(filename);
    SDL_Surface* surface = IMG_Load(resolved);
    if (surface == NULL)
        return NULL;
    void* tex = g_texture_util->LoadFromSurface(surface);
    SDL_DestroySurface(surface);
    return tex;
}

extern "C" void* TextureUtil_LoadFromSurface(SDL_Surface* surface) {
    return g_texture_util->LoadFromSurface(surface);
}

extern "C" void* TextureUtil_LoadScaled(const char* filename, float scale) {
    const char* resolved = Paths_ResolveAsset(filename);
    SDL_Surface* surface = IMG_Load(resolved);
    if (surface == NULL)
        return NULL;

    /* Convert to RGBA32 so we can safely blit/scale */
    SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (!converted)
        return NULL;

    premultiply_surface_alpha(converted);

    /* Downscale at the surface level if scale < 1.0 */
    if (scale < 1.0f && scale > 0.0f && converted->w > 1 && converted->h > 1) {
        int new_w = (int)(converted->w * scale + 0.5f);
        int new_h = (int)(converted->h * scale + 0.5f);
        if (new_w < 1) new_w = 1;
        if (new_h < 1) new_h = 1;

        SDL_Surface* scaled = SDL_CreateSurface(new_w, new_h, SDL_PIXELFORMAT_RGBA32);
        if (scaled != NULL) {
            SDL_BlitSurfaceScaled(converted, NULL, scaled, NULL, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(converted);
            converted = scaled;
        }
    }

    void* tex = g_texture_util->LoadFromSurface(converted);
    SDL_DestroySurface(converted);
    return tex;
}

extern "C" void TextureUtil_Free(void* texture_id) {
    if (!texture_id)
        return;
    g_texture_util->Free(texture_id);
}

extern "C" void TextureUtil_GetSize(void* texture_id, int* w, int* h) {
    if (!texture_id) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    g_texture_util->GetSize(texture_id, w, h);
}

extern "C" bool TextureUtil_GetGPUMetadata(void* texture_id, GPUTextureMetadataC* out_meta) {
    GPUTextureMetadata* meta = get_gpu_metadata(texture_id);
    if (!meta) return false;
    out_meta->texture = (void*)meta->texture;
    out_meta->w = meta->w;
    out_meta->h = meta->h;
    out_meta->pixels = meta->pixels;
    return true;
}

extern "C" void TextureUtil_Shutdown(void) {
    g_texture_util->Shutdown();
}

/* ================================================================
 *  Drawing — already uses g_game_renderer vtable
 * ================================================================ */

extern "C" void TextureUtil_DrawQuad(void* texture_id, float x, float y, float w, float h, float z) {
    g_game_renderer->DrawOverlayQuad(texture_id, x, y, w, h, z);
}

extern "C" void TextureUtil_DrawQuadEx(void* texture_id, float x, float y, float w, float h, float z, int flip_x,
                                       int flip_y) {
    g_game_renderer->DrawOverlayQuadEx(texture_id, x, y, w, h, z, flip_x, flip_y);
}

extern "C" void TextureUtil_DrawSubQuadEx(void* texture_id, float x, float y, float w, float h, float u0, float v0,
                                          float u1, float v1, float z) {
    g_game_renderer->DrawOverlaySubQuadEx(texture_id, x, y, w, h, u0, v0, u1, v1, z);
}
