/**
 * Font Structure Analyzer
 * Find where glyph data actually starts
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE* afs = fopen("C:\\Users\\Dov\\AppData\\Roaming\\3sxtra\\3SX\\resources\\SF33RD.AFS", "rb");
    if (!afs) return 1;
    
    fseek(afs, 8 + 81 * 8, SEEK_SET);
    uint32_t offset, size;
    fread(&offset, 4, 1, afs);
    fread(&size, 4, 1, afs);
    
    uint8_t* data = malloc(size);
    fseek(afs, offset, SEEK_SET);
    fread(data, 1, size, afs);
    fclose(afs);
    
    printf("File 81: size=%u bytes\n\n", size);
    
    // kanji_tbl[7] says: uni_table=0x3, font_max=0xC5 (197 glyphs)
    // The file appears to have a unicode table starting around 0x140
    
    // Look for where the table ends (find large blocks of 00s after the table)
    // The unicode values go up to at least 0xC100 at 0x1F0
    
    // Search for the end of the unicode table (scan for non-sequential data)
    printf("Scanning for data structures...\n");
    
    // The start bytes 01 02 might be a count: 0x0201 = 513
    uint16_t* u16 = (uint16_t*)data;
    printf("First u16: 0x%04X (%u)\n", u16[0], u16[0]);
    
    // Based on unicode table at 0x140, each entry is 2 bytes
    // If there's a header, let's check what 0x140 / 2 = 160 entries before table
    // Actually, the unicode table looks like it maps character index to unicode
    
    // The table at 0x140 starts with 01,00 = char 1 maps to unicode 0x0001?
    // Then at 0x180, 21,00 = char 33 maps to unicode 0x0021 = '!'
    // This suggests the table maps indices to unicode codepoints
    
    // Let's find where actual glyph bitmap data starts
    // Look for repeating patterns typical of bitmap data
    
    // Check around byte 0x200-0x400 for real glyph data
    printf("\nBytes 0x200-0x300:\n");
    for (int row = 0; row < 16; row++) {
        int off = 0x200 + row * 16;
        printf("%04X: ", off);
        for (int c = 0; c < 16; c++) printf("%02X ", data[off + c]);
        printf("\n");
    }
    
    // kanji_tbl type 7: font_max=0xC5 (197 glyphs), one_size=0x3C (60 bytes)
    // 197 glyphs * 60 bytes = 11820 bytes for glyph data
    // Unicode table: 197 * 2 = 394 bytes for mapping
    // Total file: 23856 bytes
    // 23856 - 11820 - 394 = 11642 bytes unaccounted (header + padding?)
    
    // Let's try to find the glyph data by looking for the start of dense bitmap data
    printf("\nScanning for dense data regions...\n");
    int window = 64;
    for (int pos = 0; pos < (int)size - window; pos += window) {
        int nonzero = 0;
        for (int i = 0; i < window; i++) {
            if (data[pos + i] != 0) nonzero++;
        }
        if (nonzero > window / 4) {
            printf("Dense region at 0x%04X: %d/%d non-zero bytes\n", pos, nonzero, window);
        }
    }
    
    // Check specific offsets based on kanji_tbl structure
    // The file should have: header + unicode_table + glyph_data
    // uni_table = 3 means some table encoding
    // Let's calculate: if unicode table is 197*2=394 bytes starting at ~0x140
    // Table ends at 0x140 + 394 = 0x2CA
    // Round up to alignment: 0x300 or 0x400 would be typical
    
    printf("\nChecking offset 0x400 for glyph data:\n");
    for (int row = 0; row < 8; row++) {
        int off = 0x400 + row * 16;
        printf("%04X: ", off);
        for (int c = 0; c < 16; c++) printf("%02X ", data[off + c]);
        printf("\n");
    }
    
    printf("\nChecking offset 0x2C (start after minimal header):\n");
    // Maybe 0x2C is where data starts (44 bytes header)
    for (int row = 0; row < 4; row++) {
        int off = 0x2C + row * 16;
        printf("%04X: ", off);
        for (int c = 0; c < 16; c++) printf("%02X ", data[off + c]);
        printf("\n");
    }
    
    free(data);
    return 0;
}
