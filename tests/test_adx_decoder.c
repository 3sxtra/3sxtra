#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "../../ADX_NOFFMPEG/adx_decoder.h"

void test_init() {
    uint8_t header[16] = {0};
    header[0] = 0x80;
    header[2] = 0x00; header[3] = 0x0C; // data_offset = 16
    header[5] = 18; // block_size
    header[7] = 2;  // channels
    // 48000 Hz (0xBB80)
    header[8] = 0x00; header[9] = 0x00; header[10] = 0xBB; header[11] = 0x80;
    // 1000 samples
    header[12] = 0x00; header[13] = 0x00; header[14] = 0x03; header[15] = 0xE8;

    ADXContext ctx;
    int ret = ADX_InitContext(&ctx, header, sizeof(header));
    assert(ret == 0);
    assert(ctx.channels == 2);
    assert(ctx.sample_rate == 48000);
    assert(ctx.block_size == 18);
    assert(ctx.samples_per_block == 32); // (18-2)*2
    assert(ctx.data_offset == 16);
    printf("test_init passed\n");
}

void test_decode_basic() {
    // Mock 1 frame (stereo)
    // Frame size = block_size * channels = 18 * 2 = 36 bytes
    uint8_t input[36] = {0};
    // Block 1 (L): Scale = 0x0100
    input[0] = 0x01; input[1] = 0x00;
    // Nibbles: 0x12, 0x34... (random data)
    for(int i=2; i<18; i++) input[i] = 0x11; 
    
    // Block 2 (R): Scale = 0x0200
    input[18] = 0x02; input[19] = 0x00;
    for(int i=20; i<36; i++) input[i] = 0x22;

    uint8_t header[16] = {0x80, 0, 0, 12, 3, 18, 4, 2, 0, 0, 0xBB, 0x80, 0, 0, 0, 100};
    ADXContext ctx;
    ADX_InitContext(&ctx, header, 16);

    int16_t out[100];
    int out_samples = 100;
    int bytes_consumed = 0;
    int ret = ADX_Decode(&ctx, input, 36, out, &out_samples, &bytes_consumed);
    
    assert(ret == 0);
    assert(bytes_consumed == 36);
    assert(out_samples == 64); // 32 samples * 2 channels
    printf("test_decode_basic passed\n");
}

int main() {
    test_init();
    test_decode_basic();
    printf("All decoder tests passed!\n");
    return 0;
}
