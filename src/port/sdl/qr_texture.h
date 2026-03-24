/**
 * @file qr_texture.h
 * @brief QR code image generation for the lobby QR-join feature.
 *
 * Generates a QR code from a URL string and writes it as a BMP file
 * on disk. All three RmlUi render backends (GL3, SDL_GPU, SDL2D) can
 * load BMP files through their LoadTexture paths.
 */
#ifndef QR_TEXTURE_H
#define QR_TEXTURE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate a QR code for the given text and save it as a BMP image.
 *
 * @param text   The data to encode (e.g. a URL).
 * @param path   Output file path (will be overwritten if it exists).
 * @param scale  Pixel size of each QR module (e.g. 4 = 4x4 pixels per module).
 *               Minimum 1.
 * @return true on success, false on error (encoding failure or I/O error).
 */
bool QRTexture_GenerateBMP(const char* text, const char* path, int scale);

#ifdef __cplusplus
}
#endif

#endif /* QR_TEXTURE_H */
