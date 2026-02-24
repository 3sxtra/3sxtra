/**
 * Font Rendering Analysis Test
 * 
 * This test extracts the font conversion logic from sdl_message_renderer.c
 * and tests it in isolation. It:
 * 1. Tests the INDEX4LSB to RGBA conversion algorithm
 * 2. Tests palette interpretation (only index 3 = opaque)
 * 3. Generates visual output files for manual inspection
 * 
 * Run: ctest -R font_rendering
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

// ============================================================================
// Font conversion algorithm (extracted from sdl_message_renderer.c)
// ============================================================================

/**
 * Convert PS2 4-bit indexed (INDEX4LSB) pixel data to RGBA.
 * 
 * @param width    Texture width in pixels
 * @param height   Texture height in pixels  
 * @param src      Source INDEX4LSB data (2 pixels per byte)
 * @param dst      Destination RGBA data (4 bytes per pixel)
 *
 * Palette interpretation (from kanji_tbl.pal_tbl = rgba_tbl4):
 * - Index 0: Transparent
 * - Index 1: Transparent (anti-alias level 1)
 * - Index 2: Transparent (anti-alias level 2)
 * - Index 3: Opaque white (glyph body)
 */
static void convert_index4lsb_to_rgba(int width, int height, 
                                       const uint8_t* src, uint8_t* dst) {
    int pitch = width / 2;  // 2 pixels per byte
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 2) {
            uint8_t byte = src[y * pitch + x / 2];
            uint8_t idx0 = byte & 0x0F;        // Low nibble = pixel x
            uint8_t idx1 = (byte >> 4) & 0x0F; // High nibble = pixel x+1
            
            // Pixel x
            int dst0 = (y * width + x) * 4;
            dst[dst0 + 0] = 255;  // R
            dst[dst0 + 1] = 255;  // G
            dst[dst0 + 2] = 255;  // B
            dst[dst0 + 3] = (idx0 == 3) ? 255 : 0;  // A: only index 3 is opaque
            
            // Pixel x+1
            if (x + 1 < width) {
                int dst1 = (y * width + x + 1) * 4;
                dst[dst1 + 0] = 255;
                dst[dst1 + 1] = 255;
                dst[dst1 + 2] = 255;
                dst[dst1 + 3] = (idx1 == 3) ? 255 : 0;
            }
        }
    }
}

/**
 * Write RGBA image data to TGA file for visual inspection.
 */
static int write_tga(const char* filename, int width, int height, const uint8_t* rgba) {
    FILE* f = fopen(filename, "wb");
    if (!f) return 0;
    
    uint8_t header[18] = {0};
    header[2] = 2;  // Uncompressed RGB
    header[12] = width & 0xFF;
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;
    header[15] = (height >> 8) & 0xFF;
    header[16] = 32;   // 32 bpp
    header[17] = 0x28; // Top-left origin + 8 alpha bits
    fwrite(header, 1, 18, f);
    
    // Write BGRA pixels (TGA uses BGRA)
    for (int i = 0; i < width * height; i++) {
        uint8_t bgra[4] = {
            rgba[i * 4 + 2],  // B
            rgba[i * 4 + 1],  // G
            rgba[i * 4 + 0],  // R
            rgba[i * 4 + 3],  // A
        };
        fwrite(bgra, 1, 4, f);
    }
    
    fclose(f);
    return 1;
}

// ============================================================================
// Unit Tests
// ============================================================================

/**
 * Test that only palette index 3 produces opaque pixels.
 */
static void test_palette_index3_only_opaque(void** state) {
    (void)state;
    
    // Create test data with all 16 possible index values
    // Each byte contains two indices: [high:4][low:4]
    uint8_t test_data[8] = {
        0x10, 0x32,  // indices: 0,1,2,3
        0x54, 0x76,  // indices: 4,5,6,7
        0x98, 0xBA,  // indices: 8,9,A,B
        0xDC, 0xFE,  // indices: C,D,E,F
    };
    
    uint8_t rgba[16 * 4];  // 16 pixels
    memset(rgba, 0xCD, sizeof(rgba));
    
    convert_index4lsb_to_rgba(16, 1, test_data, rgba);
    
    // Verify alpha values: only index 3 should be 255
    // Indices 0,1,2,3,4,5,6,7,8,9,A,B,C,D,E,F
    assert_int_equal(rgba[0*4 + 3], 0);    // idx 0 -> transparent
    assert_int_equal(rgba[1*4 + 3], 0);    // idx 1 -> transparent
    assert_int_equal(rgba[2*4 + 3], 0);    // idx 2 -> transparent
    assert_int_equal(rgba[3*4 + 3], 255);  // idx 3 -> OPAQUE
    assert_int_equal(rgba[4*4 + 3], 0);    // idx 4 -> transparent
    assert_int_equal(rgba[5*4 + 3], 0);    // idx 5 -> transparent
    assert_int_equal(rgba[6*4 + 3], 0);    // idx 6 -> transparent
    assert_int_equal(rgba[7*4 + 3], 0);    // idx 7 -> transparent
    
    printf("[PASS] Only index 3 produces alpha=255\n");
}

/**
 * Test INDEX4LSB unpacking order (low nibble first).
 */
static void test_index4lsb_nibble_order(void** state) {
    (void)state;
    
    // Byte 0x31: low nibble = 1, high nibble = 3
    // Expected: pixel 0 = index 1 (transparent), pixel 1 = index 3 (opaque)
    uint8_t test_data[1] = { 0x31 };
    uint8_t rgba[2 * 4];
    
    convert_index4lsb_to_rgba(2, 1, test_data, rgba);
    
    assert_int_equal(rgba[0*4 + 3], 0);    // First pixel: idx 1 -> transparent
    assert_int_equal(rgba[1*4 + 3], 255);  // Second pixel: idx 3 -> opaque
    
    printf("[PASS] INDEX4LSB nibble order: low nibble first\n");
}

/**
 * Test generating a visual test pattern and writing to TGA.
 */
static void test_generate_visual_pattern(void** state) {
    (void)state;
    
    const int width = 20;
    const int height = 20;
    const int pitch = width / 2;
    
    // Create a simple "X" pattern using index 3
    uint8_t src_data[10 * 20];  // 20x20 at 4bpp = 200 bytes
    memset(src_data, 0x00, sizeof(src_data));  // All index 0 (transparent)
    
    for (int y = 0; y < 20; y++) {
        // Diagonal down-right
        int x1 = y;
        int byte_idx1 = y * pitch + x1 / 2;
        if (x1 % 2 == 0) {
            src_data[byte_idx1] |= 3;  // Low nibble = index 3
        } else {
            src_data[byte_idx1] |= (3 << 4);  // High nibble = index 3
        }
        
        // Diagonal /
        int x2 = 19 - y;
        int byte_idx2 = y * pitch + x2 / 2;
        if (x2 % 2 == 0) {
            src_data[byte_idx2] |= 3;
        } else {
            src_data[byte_idx2] |= (3 << 4);
        }
    }
    
    // Convert to RGBA
    uint8_t rgba_data[20 * 20 * 4];
    convert_index4lsb_to_rgba(width, height, src_data, rgba_data);
    
    // Count opaque pixels
    int opaque_count = 0;
    for (int i = 0; i < width * height; i++) {
        if (rgba_data[i * 4 + 3] == 255) opaque_count++;
    }
    
    // X pattern should have roughly 2 * 20 = 40 pixels (minus center overlap = 39)
    assert_in_range(opaque_count, 35, 45);
    
    // Write to file for visual inspection
    if (write_tga("test_x_pattern.tga", width, height, rgba_data)) {
        printf("[INFO] Wrote test_x_pattern.tga for visual inspection\n");
    }
    
    printf("[PASS] Visual pattern generation: %d opaque pixels\n", opaque_count);
}

/**
 * Test with realistic glyph-like data (mostly index 0, some index 3).
 */
static void test_realistic_glyph_pattern(void** state) {
    (void)state;
    
    const int width = 20;
    const int height = 20;
    
    // Simulate a glyph with data starting after first 16 bytes (like real data)
    uint8_t src_data[200];
    memset(src_data, 0x00, sizeof(src_data));
    
    // Add some index 3 pixels starting at byte 16
    src_data[16] = 0x33;  // Two opaque pixels
    src_data[17] = 0x33;
    src_data[18] = 0x03;  // One opaque, one transparent
    src_data[19] = 0x30;  // One transparent, one opaque
    
    // Convert
    uint8_t rgba_data[20 * 20 * 4];
    convert_index4lsb_to_rgba(width, height, src_data, rgba_data);
    
    // Count opaque pixels
    int opaque_count = 0;
    int first_opaque = -1;
    for (int i = 0; i < width * height; i++) {
        if (rgba_data[i * 4 + 3] == 255) {
            if (first_opaque == -1) first_opaque = i;
            opaque_count++;
        }
    }
    
    assert_int_equal(opaque_count, 6);  // 2+2+1+1 = 6 opaque pixels (0x33, 0x33, 0x03, 0x30)
    assert_int_equal(first_opaque, 32); // Byte 16 = pixel 32 (2 pixels per byte)
    
    printf("[PASS] Realistic glyph: %d opaque pixels, first at pixel %d\n", 
           opaque_count, first_opaque);
}

// ============================================================================
// Test Runner
// ============================================================================

int main(void) {
    printf("=== Font Rendering Analysis Tests ===\n\n");
    
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_palette_index3_only_opaque),
        cmocka_unit_test(test_index4lsb_nibble_order),
        cmocka_unit_test(test_generate_visual_pattern),
        cmocka_unit_test(test_realistic_glyph_pattern),
    };
    
    int result = cmocka_run_group_tests(tests, NULL, NULL);
    
    printf("\n=== Tests Complete ===\n");
    printf("Generated files:\n");
    printf("  - test_x_pattern.tga (visual X pattern)\n");
    
    return result;
}
