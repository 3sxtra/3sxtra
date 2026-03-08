/**
 * @file test_adx_decoder.c
 * @brief Unit tests for the ADX ADPCM decoder.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include "cmocka.h"
#include "port/sound/adx_decoder.h"

static void test_adx_init_valid_mono(void** state) {
    (void)state;
    ADXContext ctx;
    u8 header[16] = {0};
    header[0] = 0x80; // Magic
    header[1] = 0x03; // Encoding
    header[2] = 0x00; // Copyright offset high
    header[3] = 0x0C; // Copyright offset low (12) -> data_offset = 12 + 4 = 16
    header[5] = 18;   // Block size
    header[7] = 1;    // Channels
    header[8] = 0x00; // Sample rate high
    header[9] = 0x00;
    header[10] = 0xAC;
    header[11] = 0x44; // 44100 Hz
    header[12] = 0x00; // Total samples high
    header[13] = 0x01;
    header[14] = 0x00;
    header[15] = 0x00; // 65536 samples

    int result = ADX_InitContext(&ctx, header, sizeof(header));
    assert_int_equal(result, 0);
    assert_int_equal(ctx.channels, 1);
    assert_int_equal(ctx.sample_rate, 44100);
    assert_int_equal(ctx.block_size, 18);
    assert_int_equal(ctx.data_offset, 16);
    assert_int_equal(ctx.samples_per_block, 32);
    assert_int_equal(ctx.frame_size, 18);
}

static void test_adx_init_valid_stereo(void** state) {
    (void)state;
    ADXContext ctx;
    u8 header[16] = {0};
    header[0] = 0x80;
    header[1] = 0x03;
    header[2] = 0x00;
    header[3] = 0x20; // data_offset = 32 + 4 = 36
    header[5] = 18;
    header[7] = 2;    // Channels
    header[8] = 0x00;
    header[9] = 0x00;
    header[10] = 0x56;
    header[11] = 0x22; // 22050 Hz

    int result = ADX_InitContext(&ctx, header, sizeof(header));
    assert_int_equal(result, 0);
    assert_int_equal(ctx.channels, 2);
    assert_int_equal(ctx.sample_rate, 22050);
    assert_int_equal(ctx.frame_size, 36);
}

static void test_adx_init_invalid_magic(void** state) {
    (void)state;
    ADXContext ctx;
    u8 header[16] = {0};
    header[0] = 0x00; // Wrong magic
    int result = ADX_InitContext(&ctx, header, sizeof(header));
    assert_int_equal(result, -1);
}

static void test_adx_init_too_small(void** state) {
    (void)state;
    ADXContext ctx;
    u8 header[15] = {0}; // Less than 16
    int result = ADX_InitContext(&ctx, header, sizeof(header));
    assert_int_equal(result, -1);
}

static void test_adx_init_bad_channels(void** state) {
    (void)state;
    ADXContext ctx;
    u8 header[16] = {0x80, 3, 0, 12, 0, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    header[7] = 0; // 0 channels
    assert_int_equal(ADX_InitContext(&ctx, header, 16), -1);
    header[7] = 3; // > MAX_CHANNELS
    assert_int_equal(ADX_InitContext(&ctx, header, 16), -1);
}

static void test_adx_decode_invalid_args(void** state) {
    (void)state;
    ADXContext ctx;
    u8 in[18] = {0};
    s16 out[32];
    s32 out_samples = 32;
    s32 bytes_consumed = 0;

    assert_int_equal(ADX_Decode(NULL, in, 18, out, &out_samples, &bytes_consumed), -1);
    assert_int_equal(ADX_Decode(&ctx, NULL, 18, out, &out_samples, &bytes_consumed), -1);
    assert_int_equal(ADX_Decode(&ctx, in, 18, NULL, &out_samples, &bytes_consumed), -1);
    assert_int_equal(ADX_Decode(&ctx, in, 18, out, NULL, &bytes_consumed), -1);
    assert_int_equal(ADX_Decode(&ctx, in, 18, out, &out_samples, NULL), -1);
}

static void test_adx_decode_zero_frame_size(void** state) {
    (void)state;
    ADXContext ctx = {0}; // frame_size = 0
    u8 in[18] = {0};
    s16 out[32];
    s32 out_samples = 32;
    s32 bytes_consumed = 0;
    assert_int_equal(ADX_Decode(&ctx, in, 18, out, &out_samples, &bytes_consumed), -1);
}

static void test_adx_decode_synthetic_frame(void** state) {
    (void)state;
    ADXContext ctx;
    // Setup a 44100Hz mono header
    u8 header[16] = {0};
    header[0] = 0x80;
    header[1] = 0x03;
    header[3] = 0x0C;
    header[5] = 18;
    header[7] = 1;
    header[10] = 0xAC;
    header[11] = 0x44;
    
    ADX_InitContext(&ctx, header, 16);

    // Create a synthetic block
    // Scale = 1
    // Samples: all 0x11 (two 4-bit nibbles of 1)
    u8 block[18] = {0};
    block[0] = 0x00; 
    block[1] = 0x01; // Scale = 1
    memset(block + 2, 0x11, 16); 

    s16 out[32];
    s32 out_samples = 32;
    s32 bytes_consumed = 0;

    int result = ADX_Decode(&ctx, block, 18, out, &out_samples, &bytes_consumed);
    assert_int_equal(result, 0);
    assert_int_equal(out_samples, 32);
    assert_int_equal(bytes_consumed, 18);

    // First sample: scale=1, nibble=1 -> s = 1*1 + (c1*0 + c2*0)>>12 = 1
    assert_int_equal(out[0], 1);
    // Second sample: scale=1, nibble=1 -> s = 1*1 + (c1*1 + c2*0)>>12 = 1 + (c1>>12)
    // coeff1 for 44100Hz is ~7332. 7332 >> 12 = 1. So 1 + 1 = 2.
    assert_int_equal(out[1], 2);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_adx_init_valid_mono),
        cmocka_unit_test(test_adx_init_valid_stereo),
        cmocka_unit_test(test_adx_init_invalid_magic),
        cmocka_unit_test(test_adx_init_too_small),
        cmocka_unit_test(test_adx_init_bad_channels),
        cmocka_unit_test(test_adx_decode_invalid_args),
        cmocka_unit_test(test_adx_decode_zero_frame_size),
        cmocka_unit_test(test_adx_decode_synthetic_frame),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
