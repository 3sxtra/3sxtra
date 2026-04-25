#ifndef SDL_GPU_METADATA_H
#define SDL_GPU_METADATA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief C-compatible version of GPUTextureMetadata.
 */
typedef struct GPUTextureMetadataC {
    void* texture;      // SDL_GPUTexture*
    int w, h;
    uint32_t* pixels;   // cached pixels for staging upload
} GPUTextureMetadataC;

/**
 * @brief Retrieve GPU metadata for a texture ID.
 * @return true if found, false otherwise.
 */
bool TextureUtil_GetGPUMetadata(void* texture_id, GPUTextureMetadataC* out_meta);

#ifdef __cplusplus
}
#endif

#endif
