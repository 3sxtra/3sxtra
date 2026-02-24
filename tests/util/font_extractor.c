/**
 * Font Atlas Extractor - Type 8 format (28x28)
 * File 81 size matches Type 8, not Type 7!
 * 
 * Type 8 from kanji_tbl:
 * - fontw/h = 0x1C (28)
 * - one_size = 0x70 (112 bytes)
 * - file_size = 0x5D30 (23856 bytes) - MATCHES file 81!
 * - uni_table = 0x3
 * - font_max = 0xC5 (197 glyphs)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_FILE_INDEX 81
#define GLYPH_COUNT 197
#define GLYPH_SIZE 112  // 0x70 for Type 8
#define GLYPH_W 28
#define GLYPH_H 28
#define COLS 14

// Offset = 0x100 + (uni_table << 9) = 0x100 + 0x600 = 0x700
#define GLYPH_OFFSET 0x700

static int write_tga(const char* filename, int w, int h, const uint8_t* rgba) {
    FILE* f = fopen(filename, "wb");
    if (!f) return 0;
    uint8_t header[18] = {0};
    header[2] = 2;
    header[12] = w & 0xFF; header[13] = (w >> 8) & 0xFF;
    header[14] = h & 0xFF; header[15] = (h >> 8) & 0xFF;
    header[16] = 32; header[17] = 0x28;
    fwrite(header, 1, 18, f);
    for (int i = 0; i < w * h; i++) {
        uint8_t bgra[4] = { rgba[i*4+2], rgba[i*4+1], rgba[i*4+0], rgba[i*4+3] };
        fwrite(bgra, 1, 4, f);
    }
    fclose(f);
    return 1;
}

// 1bpp expansion from get_uni_adrs
static void expand_1bpp(const uint8_t* src, uint8_t* dst, int fontw, int fonth) {
    int n1 = fontw / 8;
    int n2 = fontw % 8;
    
    for (int row = 0; row < fonth; row++) {
        for (int j = 0; j < n1; j++) {
            uint32_t d0 = *src++;
            uint32_t d1, d2;
            
            d1 = (d0 & 0x80) >> 7; d1 |= d1 << 1;
            d2 = (d0 & 0x40) >> 2; d2 |= d2 << 1;
            *dst++ = d1 | d2;
            
            d1 = (d0 & 0x20) >> 5; d1 |= d1 << 1;
            d2 = d0 & 0x10; d2 |= d2 << 1;
            *dst++ = d1 | d2;
            
            d1 = (d0 & 8) >> 3; d1 |= d1 << 1;
            d2 = (d0 & 4) << 2; d2 |= d2 << 1;
            *dst++ = d1 | d2;
            
            d1 = (d0 & 2) >> 1; d1 |= d1 << 1;
            d2 = (d0 & 1) << 4; d2 |= d2 << 1;
            *dst++ = d1 | d2;
        }
        if (n2) {
            uint32_t d0 = *src++;
            uint32_t d1, d2;
            d1 = (d0 & 0x80) >> 7; d1 |= d1 << 1;
            d2 = (d0 & 0x40) >> 2; d2 |= d2 << 1;
            *dst++ = d1 | d2;
            
            d1 = (d0 & 0x20) >> 5; d1 |= d1 << 1;
            d2 = d0 & 0x10; d2 |= d2 << 1;
            *dst++ = d1 | d2;
        }
    }
}

static void convert4bpp_rgba(const uint8_t* src, uint8_t* dst, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x += 2) {
            uint8_t byte = src[y * (w/2) + x/2];
            uint8_t idx0 = byte & 0x0F;
            uint8_t idx1 = (byte >> 4) & 0x0F;
            
            int i0 = (y * w + x) * 4;
            dst[i0+0] = dst[i0+1] = dst[i0+2] = (idx0 == 3) ? 255 : (idx0 * 60);
            dst[i0+3] = (idx0 > 0) ? 255 : 80;
            
            int i1 = (y * w + x + 1) * 4;
            dst[i1+0] = dst[i1+1] = dst[i1+2] = (idx1 == 3) ? 255 : (idx1 * 60);
            dst[i1+3] = (idx1 > 0) ? 255 : 80;
        }
    }
}

int main() {
    FILE* afs = fopen("C:\\Users\\Dov\\AppData\\Roaming\\3sxtra\\3SX\\resources\\SF33RD.AFS", "rb");
    if (!afs) return 1;
    
    fseek(afs, 8 + FONT_FILE_INDEX * 8, SEEK_SET);
    uint32_t offset, size;
    fread(&offset, 4, 1, afs);
    fread(&size, 4, 1, afs);
    
    uint8_t* data = malloc(size);
    fseek(afs, offset, SEEK_SET);
    fread(data, 1, size, afs);
    fclose(afs);
    
    printf("File 81: %u bytes (0x%X)\n", size, size);
    printf("Using Type 8 params: %dx%d, glyph_size=%d, offset=0x%X\n", 
           GLYPH_W, GLYPH_H, GLYPH_SIZE, GLYPH_OFFSET);
    
    int rows = (GLYPH_COUNT + COLS - 1) / COLS;
    int atlas_w = COLS * GLYPH_W;
    int atlas_h = rows * GLYPH_H;
    
    uint8_t* atlas = malloc(atlas_w * atlas_h * 4);
    for (int i = 0; i < atlas_w * atlas_h; i++) {
        atlas[i*4] = atlas[i*4+1] = atlas[i*4+2] = 40;
        atlas[i*4+3] = 255;
    }
    
    uint8_t expanded[512];
    uint8_t glyph_rgba[GLYPH_W * GLYPH_H * 4];
    
    for (int g = 0; g < GLYPH_COUNT; g++) {
        uint8_t* glyph = data + GLYPH_OFFSET + g * GLYPH_SIZE;
        int col = g % COLS;
        int row = g / COLS;
        
        memset(expanded, 0, sizeof(expanded));
        expand_1bpp(glyph, expanded, GLYPH_W, GLYPH_H);
        convert4bpp_rgba(expanded, glyph_rgba, GLYPH_W, GLYPH_H);
        
        for (int y = 0; y < GLYPH_H; y++) {
            for (int x = 0; x < GLYPH_W; x++) {
                int src_i = (y * GLYPH_W + x) * 4;
                int dst_i = ((row * GLYPH_H + y) * atlas_w + (col * GLYPH_W + x)) * 4;
                memcpy(&atlas[dst_i], &glyph_rgba[src_i], 4);
            }
        }
    }
    
    write_tga("font_type8.tga", atlas_w, atlas_h, atlas);
    printf("Wrote: font_type8.tga (%dx%d)\n", atlas_w, atlas_h);
    
    free(atlas);
    free(data);
    return 0;
}
