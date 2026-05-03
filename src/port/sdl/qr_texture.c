/**
 * @file qr_texture.c
 * @brief QR code BMP image generation using qrcodegen.
 *
 * Uses Nayuki's qrcodegen (MIT) to encode text into a QR matrix,
 * then writes an uncompressed 24-bit BMP file. The BMP format
 * requires no external dependencies and is loadable by all RmlUi
 * render backends via SDL_image's IMG_Load().
 */

#include "port/sdl/qr_texture.h"
#include "third_party/qrcodegen/qrcodegen.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* BMP file header (14 bytes) + DIB header (40 bytes) = 54 bytes */
#pragma pack(push, 1)
typedef struct {
    /* BMP file header */
    uint8_t bm[2]; /* 'B','M' */
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixel_offset;
    /* DIB header (BITMAPINFOHEADER) */
    uint32_t dib_size; /* 40 */
    int32_t width;
    int32_t height;       /* Negative = top-down (no flip needed) */
    uint16_t planes;      /* 1 */
    uint16_t bpp;         /* 24 */
    uint32_t compression; /* 0 = BI_RGB */
    uint32_t image_size;  /* May be 0 for BI_RGB */
    int32_t x_ppm;
    int32_t y_ppm;
    uint32_t colors_used;
    uint32_t colors_important;
} BMPHeader;
#pragma pack(pop)

bool QRTexture_GenerateBMP(const char* text, const char* path, int scale) {
    if (!text || !path || scale < 1)
        return false;

    /* Encode text to QR code */
    uint8_t qr_buf[qrcodegen_BUFFER_LEN_MAX];
    uint8_t temp_buf[qrcodegen_BUFFER_LEN_MAX];

    bool can_activate = qrcodegen_encodeText(text,
                                   temp_buf,
                                   qr_buf,
                                   qrcodegen_Ecc_LOW, /* Low ECC for shorter URLs, denser QR */
                                   qrcodegen_VERSION_MIN,
                                   qrcodegen_VERSION_MAX,
                                   qrcodegen_Mask_AUTO,
                                   true /* Boost ECC if possible */
    );
    if (!can_activate) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[QR] Failed to encode: %s", text);
        return false;
    }

    int qr_size = qrcodegen_getSize(qr_buf);
    int quiet = 4; /* 4-module quiet zone (standard) for visual centering */
    int img_modules = qr_size + quiet * 2;
    int img_px = img_modules * scale;

    /* BMP row stride must be a multiple of 4 bytes */
    int row_bytes = img_px * 3;
    int row_stride = (row_bytes + 3) & ~3;

    uint32_t pixel_data_size = (uint32_t)(row_stride * img_px);
    uint32_t file_size = 54 + pixel_data_size;

    uint8_t* pixels = (uint8_t*)calloc(1, pixel_data_size);
    if (!pixels)
        return false;

    /* Fill pixels: white background, black modules
     * BMP with negative height = top-down scanline order */
    for (int py = 0; py < img_px; py++) {
        uint8_t* row = pixels + py * row_stride;
        for (int px = 0; px < img_px; px++) {
            int mx = px / scale - quiet; /* module x */
            int my = py / scale - quiet; /* module y */

            bool is_black = false;
            if (mx >= 0 && mx < qr_size && my >= 0 && my < qr_size) {
                is_black = qrcodegen_getModule(qr_buf, mx, my);
            }

            /* BMP pixel order is BGR */
            uint8_t val = is_black ? 0x00 : 0xFF;
            row[px * 3 + 0] = val; /* B */
            row[px * 3 + 1] = val; /* G */
            row[px * 3 + 2] = val; /* R */
        }
    }

    /* Write BMP file */
    BMPHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.bm[0] = 'B';
    hdr.bm[1] = 'M';
    hdr.file_size = file_size;
    hdr.pixel_offset = 54;
    hdr.dib_size = 40;
    hdr.width = img_px;
    hdr.height = -img_px; /* Negative = top-down */
    hdr.planes = 1;
    hdr.bpp = 24;
    hdr.image_size = pixel_data_size;

    FILE* f = fopen(path, "wb");
    if (!f) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[QR] Cannot open output file: %s", path);
        free(pixels);
        return false;
    }

    fwrite(&hdr, 1, sizeof(hdr), f);
    fwrite(pixels, 1, pixel_data_size, f);
    fclose(f);

    free(pixels);

    SDL_Log("[QR] Generated %dx%d BMP (%d modules, scale=%d): %s", img_px, img_px, qr_size, scale, path);
    return true;
}
