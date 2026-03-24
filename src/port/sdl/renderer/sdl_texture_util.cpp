/**
 * @file sdl_texture_util.cpp
 * @brief Standalone texture loading, sizing, and cleanup.
 *
 * Standalone texture loading, sizing, and cleanup for all renderer backends.
 * Supports OpenGL, SDL_GPU, and SDL2D.
 */
#include "port/sdl/renderer/sdl_texture_util.h"
#include "port/config/paths.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/renderer/sdl_game_renderer_internal.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <glad/gl.h>
#include <string.h>
#include <unordered_map>

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

static void premultiply_surface_alpha(SDL_Surface* surface) {
    if (!surface || surface->format != SDL_PIXELFORMAT_RGBA32) return;
    Uint8* pixels = (Uint8*)surface->pixels;
    int pitch = surface->pitch;
    for (int y = 0; y < surface->h; y++) {
        Uint8* row = pixels + y * pitch;
        for (int x = 0; x < surface->w; x++) {
            Uint8* p = row + x * 4;
            unsigned int a = p[3];
            p[0] = (Uint8)((p[0] * a) / 255);
            p[1] = (Uint8)((p[1] * a) / 255);
            p[2] = (Uint8)((p[2] * a) / 255);
        }
    }
}

extern "C" void* TextureUtil_Load(const char* filename) {
    const char* resolved = Paths_ResolveAsset(filename);
    SDL_Surface* surface = IMG_Load(resolved);
    if (surface == NULL) {
        return NULL;
    }

    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        SDL_GPUDevice* device = SDLApp_GetGPUDevice();
        if (!device) {
            SDL_DestroySurface(surface);
            return NULL;
        }

        SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surface);
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
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create GPU texture: %s", SDL_GetError());
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
        /* Cache pixel data for staging upload overlay path.
         * NOTE: TextureUtil_Load is only called for overlay/UI textures (portraits,
         * sprite overrides), not the thousands of CPS3 game textures — so this
         * per-texture CPU copy has bounded memory overhead. */
        size_t px_size = (size_t)converted->w * converted->h * 4;
        meta.pixels = (uint32_t*)SDL_malloc(px_size);
        if (meta.pixels) {
            memcpy(meta.pixels, converted->pixels, px_size);
        }
        s_gpu_textures[(void*)texture] = meta;

        SDL_DestroySurface(converted);
        return (void*)texture;

    } else if (is_sdl2d_backend(SDLApp_GetRenderer())) {
        SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
        if (!renderer) {
            SDL_DestroySurface(surface);
            return NULL;
        }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
        }
        SDL_DestroySurface(surface);
        return (void*)texture;
    } else {
        GLuint texture_id;
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);

        SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (converted) {
            premultiply_surface_alpha(converted);
            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);
            SDL_DestroySurface(converted);
        } else {
            SDL_Log("Failed to convert surface: %s", SDL_GetError());
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        SDL_DestroySurface(surface);
        return (void*)(intptr_t)texture_id;
    }
}

static void* upload_surface_to_texture(SDL_Surface* surface) {
    if (!surface)
        return NULL;

    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
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

    } else if (is_sdl2d_backend(SDLApp_GetRenderer())) {
        SDL_Renderer* renderer = SDLApp_GetSDLRenderer();
        if (!renderer)
            return NULL;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        return (void*)texture;
    } else {
        GLuint texture_id;
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);

        SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (converted) {
            premultiply_surface_alpha(converted);
            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);
            SDL_DestroySurface(converted);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        return (void*)(intptr_t)texture_id;
    }
}

extern "C" void* TextureUtil_LoadFromSurface(SDL_Surface* surface) {
    return upload_surface_to_texture(surface);
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

    void* tex = upload_surface_to_texture(converted);
    SDL_DestroySurface(converted);
    return tex;
}

extern "C" void TextureUtil_Free(void* texture_id) {
    if (!texture_id)
        return;

    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
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
    } else if (is_sdl2d_backend(SDLApp_GetRenderer())) {
        SDL_Texture* tex = (SDL_Texture*)texture_id;
        SDL_DestroyTexture(tex);
    } else {
        GLuint id = (GLuint)(intptr_t)texture_id;
        glDeleteTextures(1, &id);
    }
}

extern "C" void TextureUtil_GetSize(void* texture_id, int* w, int* h) {
    if (!texture_id) {
        if (w)
            *w = 0;
        if (h)
            *h = 0;
        return;
    }

    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        GPUTextureMetadata* meta = get_gpu_metadata(texture_id);
        if (meta) {
            if (w)
                *w = meta->w;
            if (h)
                *h = meta->h;
        } else {
            if (w)
                *w = 0;
            if (h)
                *h = 0;
        }
    } else if (is_sdl2d_backend(SDLApp_GetRenderer())) {
        SDL_Texture* tex = (SDL_Texture*)texture_id;
        if (w || h) {
            float fw, fh;
            SDL_GetTextureSize(tex, &fw, &fh);
            if (w)
                *w = (int)fw;
            if (h)
                *h = (int)fh;
        }
    } else {
        GLuint id = (GLuint)(intptr_t)texture_id;
        glBindTexture(GL_TEXTURE_2D, id);
        if (w)
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, w);
        if (h)
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, h);
    }
}

extern "C" void TextureUtil_Shutdown(void) {
    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        SDL_GPUDevice* device = SDLApp_GetGPUDevice();
        for (auto& pair : s_gpu_textures) {
            if (device)
                SDL_ReleaseGPUTexture(device, pair.second.texture);
        }
    }
    s_gpu_textures.clear();
    s_last_gpu_tex_id = nullptr;
    s_last_gpu_meta = nullptr;
}

extern "C" void TextureUtil_DrawQuad(void* texture_id, float x, float y, float w, float h, float z) {
    if (!texture_id)
        return;

    if (SDLApp_GetRenderer() == RENDERER_OPENGL) {
        /* GL path: push a render task into the batch renderer.
         * The overlay sprite uses the legacy texture path (array_layer = -1)
         * so it participates in the normal z-sorted batch draw.
         * z is already a converted depth value (from flPS2ConvScreenFZ or PrioBase). */
        SDLGameRendererGL_DrawOverlaySprite((unsigned int)(intptr_t)texture_id, x, y, w, h, z);

    } else if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        /* GPU path: use array layer for small textures, direct blit for oversized */
        GPUTextureMetadata* meta = get_gpu_metadata(texture_id);
        if (!meta || !meta->pixels)
            return;
        if (meta->w > 512 || meta->h > 512) {
            /* Oversized: queue a direct blit onto canvas */
            SDLGameRendererGPU_QueueDeferredBlit(meta->texture, meta->w, meta->h, x, y, w, h, z, 0, 0);
        } else {
            SDLGameRendererGPU_DrawOverlaySprite(meta->pixels, meta->w, meta->h, x, y, w, h, z);
        }

    } else if (SDLApp_GetRenderer() == RENDERER_SDL2D) {
        /* SDL2D path: enqueue into z-sorted batch */
        SDLGameRendererSDL_DrawOverlaySprite((SDL_Texture*)texture_id, x, y, w, h, z);

    } else if (SDLApp_GetRenderer() == RENDERER_SDL2D_CLASSIC) {
        /* Classic path: enqueue into AoS batch */
        SDLGameRendererClassic_DrawOverlaySprite((SDL_Texture*)texture_id, x, y, w, h, z);
    }
}

extern "C" void TextureUtil_DrawQuadEx(void* texture_id, float x, float y, float w, float h, float z, int flip_x,
                                       int flip_y) {
    if (!texture_id)
        return;

    if (SDLApp_GetRenderer() == RENDERER_OPENGL) {
        /* GL path */
        SDLGameRendererGL_DrawOverlaySpriteEx((unsigned int)(intptr_t)texture_id, x, y, w, h, z, flip_x, flip_y);

    } else if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        /* GPU path: use array layer for small textures, direct blit for oversized */
        GPUTextureMetadata* meta = get_gpu_metadata(texture_id);
        if (!meta || !meta->pixels)
            return;
        if (meta->w > 512 || meta->h > 512) {
            /* Oversized: queue a direct blit onto canvas (flip now supported via updated blits) */
            SDLGameRendererGPU_QueueDeferredBlit(meta->texture, meta->w, meta->h, x, y, w, h, z, flip_x, flip_y);
        } else {
            SDLGameRendererGPU_DrawOverlaySpriteEx(
                meta->pixels, meta->w, meta->h, x, y, w, h, z, flip_x, flip_y);
        }

    } else if (SDLApp_GetRenderer() == RENDERER_SDL2D) {
        /* SDL2D path: enqueue into z-sorted batch with flip */
        SDLGameRendererSDL_DrawOverlaySpriteEx((SDL_Texture*)texture_id, x, y, w, h, z, flip_x, flip_y);

    } else if (SDLApp_GetRenderer() == RENDERER_SDL2D_CLASSIC) {
        /* Classic path: enqueue into AoS batch with flip */
        SDLGameRendererClassic_DrawOverlaySpriteEx((SDL_Texture*)texture_id, x, y, w, h, z, flip_x, flip_y);
    }
}
