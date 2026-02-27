/**
 * @file sdl_texture_util.cpp
 * @brief Standalone texture loading, sizing, and cleanup.
 *
 * Extracted from imgui_wrapper.cpp to decouple texture management from ImGui.
 * Supports both OpenGL and SDL_GPU backends.
 */
#include "port/sdl/sdl_texture_util.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_game_renderer_internal.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <glad/gl.h>
#include <map>
#include <string.h>

struct GPUTextureMetadata {
    SDL_GPUTexture* texture;
    int w, h;
};

static std::map<void*, GPUTextureMetadata> s_gpu_textures;

extern "C" void* TextureUtil_Load(const char* filename) {
    SDL_Surface* surface = IMG_Load(filename);
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

        GPUTextureMetadata meta = { texture, converted->w, converted->h };
        s_gpu_textures[(void*)texture] = meta;

        SDL_DestroySurface(converted);
        return (void*)texture;

    } else {
        GLuint texture_id;
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);

        SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (converted) {
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

extern "C" void TextureUtil_Free(void* texture_id) {
    if (!texture_id)
        return;

    if (SDLApp_GetRenderer() == RENDERER_SDLGPU) {
        auto it = s_gpu_textures.find(texture_id);
        if (it != s_gpu_textures.end()) {
            SDL_GPUDevice* device = SDLApp_GetGPUDevice();
            if (device)
                SDL_ReleaseGPUTexture(device, it->second.texture);
            s_gpu_textures.erase(it);
        }
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
        auto it = s_gpu_textures.find(texture_id);
        if (it != s_gpu_textures.end()) {
            if (w)
                *w = it->second.w;
            if (h)
                *h = it->second.h;
        } else {
            if (w)
                *w = 0;
            if (h)
                *h = 0;
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
}
