#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load an image file into a GPU/GL texture.
 * @param filename Path to the image file (PNG, etc.)
 * @return Opaque texture handle, or NULL on failure.
 */
void* TextureUtil_Load(const char* filename);

/**
 * @brief Free a previously loaded texture.
 * @param texture_id Handle returned by TextureUtil_Load.
 */
void TextureUtil_Free(void* texture_id);

/**
 * @brief Query the pixel dimensions of a loaded texture.
 * @param texture_id Handle returned by TextureUtil_Load.
 * @param w Output width (may be NULL).
 * @param h Output height (may be NULL).
 */
void TextureUtil_GetSize(void* texture_id, int* w, int* h);

/**
 * @brief Release all tracked GPU textures. Call during shutdown.
 */
void TextureUtil_Shutdown(void);

#ifdef __cplusplus
}
#endif
