#pragma once

#include <stdbool.h>

struct SDL_Surface;

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
 * @brief Load an image file and optionally downscale before GPU upload.
 * @param filename Path to the image file (PNG, etc.)
 * @param scale    Scale factor (0.0–1.0 downscales, >= 1.0 no change).
 * @return Opaque texture handle, or NULL on failure.
 */
void* TextureUtil_LoadScaled(const char* filename, float scale);

/**
 * @brief Create a GPU/GL texture from an SDL_Surface.
 * @param surface The surface to upload (caller retains ownership).
 * @return Opaque texture handle, or NULL on failure.
 */
void* TextureUtil_LoadFromSurface(struct SDL_Surface* surface);

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

/**
 * @brief Draw a TextureUtil texture at CPS3 canvas coordinates.
 *
 * Renders the texture as a full-UV quad at the given position and size.
 * Works with the active rendering backend (GL, GPU, SDL2D).
 * Must be called during the game frame (within BeginFrame/EndFrame).
 *
 * @param texture_id Handle returned by TextureUtil_Load/LoadFromSurface.
 * @param x, y       Top-left position in CPS3 pixels (384×224 canvas).
 * @param w, h       Width and height in CPS3 pixels.
 * @param z          Z-depth value (from PrioBase[]).
 */
void TextureUtil_DrawQuad(void* texture_id, float x, float y, float w, float h, float z);

/**
 * @brief Draw a TextureUtil texture with optional horizontal/vertical flip.
 *
 * Same as TextureUtil_DrawQuad but flips UV coordinates when flip_x/flip_y
 * are non-zero.  Used by the HD sprite override system.
 */
void TextureUtil_DrawQuadEx(void* texture_id, float x, float y, float w, float h, float z, int flip_x, int flip_y);

#ifdef __cplusplus
}
#endif
