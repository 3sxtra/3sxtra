/**
 * Font Data Dump Utility
 * 
 * Loads the kanji font from AFS file 81 (English font) and dumps the 
 * raw data for analysis. Exits immediately after dump.
 * 
 * Build: Add to CMake as a test executable
 * Run: ./font_dump_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "port/io/afs.h"

// Font file number for English (from savesub.c line 33: font_fnum[1] = 81)
#define FONT_FILE_NUM 81

// Expected font size based on kanji_tbl type 7 (see knjsub.c line 211)
// grada=2, font_max=0x53, one_size=0x64, file_size=0x352C
#define EXPECTED_SIZE 0x352C

int main(int argc, char* argv[]) {
    printf("=== Font Data Dump Utility ===\n\n");
    
    // Initialize AFS
    if (!AFS_Initialize()) {
        printf("ERROR: Failed to initialize AFS\n");
        return 1;
    }
    
    // Open font file
    printf("Opening AFS file %d (English font)...\n", FONT_FILE_NUM);
    int handle = AFS_Open(FONT_FILE_NUM);
    if (handle == AFS_NONE) {
        printf("ERROR: Failed to open AFS file %d\n", FONT_FILE_NUM);
        return 1;
    }
    
    // Get file size
    int sector_count = AFS_GetSectorCount(handle);
    int file_size = sector_count << 11;  // sectors * 2048
    printf("File size: %d sectors (%d bytes, expected ~%d)\n", 
           sector_count, file_size, EXPECTED_SIZE);
    
    // Allocate buffer
    uint8_t* buffer = (uint8_t*)malloc(file_size);
    if (!buffer) {
        printf("ERROR: Failed to allocate %d bytes\n", file_size);
        AFS_Close(handle);
        return 1;
    }
    memset(buffer, 0xCD, file_size);  // Fill with sentinel
    
    // Read file (synchronous for test)
    printf("Reading file...\n");
    AFS_Read(handle, sector_count, buffer);
    
    // Wait for read to complete
    while (AFS_GetState(handle) == AFS_READ_STATE_READING) {
        // Busy wait (not ideal but simple for test)
    }
    AFS_Close(handle);
    
    // Check if data was read (look for sentinel override)
    int first_non_sentinel = -1;
    for (int i = 0; i < file_size; i++) {
        if (buffer[i] != 0xCD) {
            first_non_sentinel = i;
            break;
        }
    }
    
    if (first_non_sentinel == -1) {
        printf("WARNING: No data was read (buffer still full of sentinel 0xCD)\n");
    } else {
        printf("Data read successfully (first non-sentinel byte at offset %d)\n", first_non_sentinel);
    }
    
    // Dump header (first 256 bytes)
    printf("\n=== Font File Header (first 256 bytes) ===\n");
    for (int row = 0; row < 16; row++) {
        printf("%04X: ", row * 16);
        for (int col = 0; col < 16; col++) {
            printf("%02X ", buffer[row * 16 + col]);
        }
        printf(" | ");
        for (int col = 0; col < 16; col++) {
            uint8_t c = buffer[row * 16 + col];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
    }
    
    // Analyze glyph structure based on kanji_tbl type 7:
    // fontw=0x14 (20), fonth=0x14 (20), grada=2, one_size=0x64 (100 bytes)
    // 4-bit indexed = 10 bytes per row, 20 rows = 200 pixels = 100 bytes
    int glyph_size = 100;  // 20x20 @ 4bpp = 100 bytes
    int glyph_count = file_size / glyph_size;
    
    printf("\n=== Glyph Analysis ===\n");
    printf("Glyph dimensions: 20x20 @ 4bpp\n");
    printf("Bytes per glyph: %d\n", glyph_size);
    printf("Estimated glyph count: %d\n", glyph_count);
    
    // Dump first 5 glyphs
    for (int g = 0; g < 5 && g * glyph_size < file_size; g++) {
        uint8_t* glyph = buffer + (g * glyph_size);
        
        // Count index usage
        int idx_count[16] = {0};
        int nonzero = 0;
        for (int i = 0; i < glyph_size; i++) {
            uint8_t byte = glyph[i];
            idx_count[byte & 0x0F]++;
            idx_count[(byte >> 4) & 0x0F]++;
            if (byte != 0) nonzero++;
        }
        
        printf("\n--- Glyph %d (offset 0x%X) ---\n", g, g * glyph_size);
        printf("Non-zero bytes: %d/%d\n", nonzero, glyph_size);
        printf("Index usage: ");
        for (int i = 0; i < 16; i++) {
            if (idx_count[i] > 0) {
                printf("%X:%d ", i, idx_count[i]);
            }
        }
        printf("\n");
        
        // Dump first 32 bytes of glyph
        printf("Data: ");
        for (int i = 0; i < 32 && i < glyph_size; i++) {
            printf("%02X ", glyph[i]);
        }
        printf("...\n");
    }
    
    // Find a glyph with actual data (skip empty ones)
    printf("\n=== Finding Non-Empty Glyphs ===\n");
    int found = 0;
    for (int g = 0; g < glyph_count && found < 3; g++) {
        uint8_t* glyph = buffer + (g * glyph_size);
        int nonzero = 0;
        for (int i = 0; i < glyph_size; i++) {
            if (glyph[i] != 0) nonzero++;
        }
        
        if (nonzero > 10) {  // Likely a real character
            printf("\nNon-empty glyph #%d at offset 0x%X (%d non-zero bytes)\n", 
                   g, g * glyph_size, nonzero);
            
            // Count index usage
            int idx_count[16] = {0};
            for (int i = 0; i < glyph_size; i++) {
                uint8_t byte = glyph[i];
                idx_count[byte & 0x0F]++;
                idx_count[(byte >> 4) & 0x0F]++;
            }
            printf("Index histogram: ");
            for (int i = 0; i < 16; i++) {
                if (idx_count[i] > 0) {
                    printf("%X=%d ", i, idx_count[i]);
                }
            }
            printf("\n");
            
            // Full hex dump
            for (int row = 0; row < (glyph_size + 15) / 16; row++) {
                printf("  %02X: ", row * 16);
                for (int col = 0; col < 16 && row * 16 + col < glyph_size; col++) {
                    printf("%02X ", glyph[row * 16 + col]);
                }
                printf("\n");
            }
            
            found++;
        }
    }
    
    free(buffer);
    
    printf("\n=== Done ===\n");
    return 0;
}
