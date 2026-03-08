/**
 * @file test_stage_config.c
 * @brief Unit tests for the stage configuration INI parser/saver.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "cmocka.h"

#include "port/mods/stage_config.h"

/* Test stage index — use 99 to avoid collision with real data */
#define TEST_STAGE 99
#define TEST_STAGE_DIR "./assets/stages/stage_99"
#define TEST_INI_PATH  TEST_STAGE_DIR "/stage_config.ini"

/* Helper: compare floats with tolerance */
static void assert_float_near(float actual, float expected, float eps) {
    assert_true(fabsf(actual - expected) < eps);
}

/* ---- Setup / Teardown ---- */

static int setup(void** state) {
    (void)state;
#ifdef _WIN32
    system("mkdir assets\\stages\\stage_99 > NUL 2>&1");
#else
    system("mkdir -p assets/stages/stage_99");
#endif
    return 0;
}

static int teardown(void** state) {
    (void)state;
    /* Remove the test INI if it exists */
    remove(TEST_INI_PATH);
    return 0;
}

/* ---- Test 1: Init sets expected defaults ---- */

static void test_init_defaults(void** state) {
    (void)state;
    StageConfig_Init();

    assert_false(g_stage_config.is_custom);

    for (int i = 0; i < MAX_STAGE_LAYERS; i++) {
        StageLayerConfig* l = &g_stage_config.layers[i];
        char expected_name[64];
        snprintf(expected_name, sizeof(expected_name), "layer_%d.png", i);
        assert_string_equal(l->filename, expected_name);
        assert_true(l->enabled);
        assert_int_equal(l->scale_mode, SCALE_MODE_FIT_HEIGHT);
        assert_float_near(l->scale_factor_x, 1.0f, 0.001f);
        assert_float_near(l->scale_factor_y, 1.0f, 0.001f);
        assert_float_near(l->parallax_y, 1.0f, 0.001f);
        assert_float_near(l->offset_x, 0.0f, 0.001f);
        assert_float_near(l->offset_y, 0.0f, 0.001f);
        assert_int_equal(l->original_bg_index, -1);
        assert_int_equal(l->z_index, i * 10);
    }
}

/* ---- Test 2: SetDefaultLayer valid index ---- */

static void test_set_default_layer_valid(void** state) {
    (void)state;
    /* Dirty layer 2, then reset it */
    memset(&g_stage_config.layers[2], 0xFF, sizeof(StageLayerConfig));
    StageConfig_SetDefaultLayer(2);

    StageLayerConfig* l = &g_stage_config.layers[2];
    assert_string_equal(l->filename, "layer_2.png");
    assert_true(l->enabled);
    assert_int_equal(l->scale_mode, SCALE_MODE_FIT_HEIGHT);
    assert_float_near(l->scale_factor_x, 1.0f, 0.001f);
    assert_int_equal(l->z_index, 20);
    assert_int_equal(l->original_bg_index, -1);
}

/* ---- Test 3: SetDefaultLayer boundary values ---- */

static void test_set_default_layer_boundary(void** state) {
    (void)state;
    StageConfig_Init();

    /* Save a copy of layer 0 */
    StageLayerConfig saved;
    memcpy(&saved, &g_stage_config.layers[0], sizeof(StageLayerConfig));

    /* Out-of-range: should be no-op */
    StageConfig_SetDefaultLayer(-1);
    StageConfig_SetDefaultLayer(MAX_STAGE_LAYERS);
    StageConfig_SetDefaultLayer(100);

    /* Verify layer 0 is untouched */
    assert_memory_equal(&g_stage_config.layers[0], &saved, sizeof(StageLayerConfig));
}

/* ---- Test 4: Load missing file preserves defaults ---- */

static void test_load_missing_file(void** state) {
    (void)state;
    /* Ensure no INI exists for stage 99 */
    remove(TEST_INI_PATH);

    StageConfig_Load(TEST_STAGE);

    /* Should have defaults */
    assert_false(g_stage_config.is_custom);
    assert_true(g_stage_config.layers[0].enabled);
    assert_string_equal(g_stage_config.layers[0].filename, "layer_0.png");
}

/* ---- Test 5: Load a well-formed INI file ---- */

static void test_load_ini_file(void** state) {
    (void)state;

    /* Write a test INI */
    FILE* f = fopen(TEST_INI_PATH, "w");
    assert_non_null(f);
    fprintf(f, "; test config\n");
    fprintf(f, "[layer_0]\n");
    fprintf(f, "filename=custom_bg.png\n");
    fprintf(f, "enabled=true\n");
    fprintf(f, "scale_mode=stretch\n");
    fprintf(f, "scale_x=2.500\n");
    fprintf(f, "scale_y=1.500\n");
    fprintf(f, "parallax_x=0.750\n");
    fprintf(f, "parallax_y=0.500\n");
    fprintf(f, "offset_x=10.0\n");
    fprintf(f, "offset_y=-5.0\n");
    fprintf(f, "original_bg_index=3\n");
    fprintf(f, "z_index=42\n");
    fprintf(f, "\n");
    fprintf(f, "[layer_2]\n");
    fprintf(f, "enabled=0\n");
    fprintf(f, "scale_mode=native\n");
    fclose(f);

    StageConfig_Load(TEST_STAGE);

    assert_true(g_stage_config.is_custom);

    StageLayerConfig* l0 = &g_stage_config.layers[0];
    assert_string_equal(l0->filename, "custom_bg.png");
    assert_true(l0->enabled);
    assert_int_equal(l0->scale_mode, SCALE_MODE_STRETCH);
    assert_float_near(l0->scale_factor_x, 2.5f, 0.01f);
    assert_float_near(l0->scale_factor_y, 1.5f, 0.01f);
    assert_float_near(l0->parallax_x, 0.75f, 0.01f);
    assert_float_near(l0->parallax_y, 0.5f, 0.01f);
    assert_float_near(l0->offset_x, 10.0f, 0.1f);
    assert_float_near(l0->offset_y, -5.0f, 0.1f);
    assert_int_equal(l0->original_bg_index, 3);
    assert_int_equal(l0->z_index, 42);

    StageLayerConfig* l2 = &g_stage_config.layers[2];
    assert_false(l2->enabled);
    assert_int_equal(l2->scale_mode, SCALE_MODE_NATIVE);

    /* Layer 1 should still be defaults */
    StageLayerConfig* l1 = &g_stage_config.layers[1];
    assert_string_equal(l1->filename, "layer_1.png");
    assert_true(l1->enabled);
}

/* ---- Test 6: Save then Load round-trip ---- */

static void test_save_load_roundtrip(void** state) {
    (void)state;

    /* Start from defaults */
    StageConfig_Init();

    /* Customize layer 0 */
    StageLayerConfig* l0 = &g_stage_config.layers[0];
    strncpy(l0->filename, "roundtrip.png", sizeof(l0->filename) - 1);
    l0->scale_mode = SCALE_MODE_MANUAL;
    l0->scale_factor_x = 3.14f;
    l0->scale_factor_y = 2.72f;
    l0->parallax_x = 0.5f;
    l0->offset_x = 100.0f;
    l0->z_index = 99;

    /* Save */
    StageConfig_Save(TEST_STAGE);

    /* Verify file was written */
    FILE* f = fopen(TEST_INI_PATH, "r");
    assert_non_null(f);
    fclose(f);

    /* Reload */
    StageConfig_Load(TEST_STAGE);

    assert_true(g_stage_config.is_custom);
    l0 = &g_stage_config.layers[0];
    assert_string_equal(l0->filename, "roundtrip.png");
    assert_int_equal(l0->scale_mode, SCALE_MODE_MANUAL);
    assert_float_near(l0->scale_factor_x, 3.14f, 0.01f);
    assert_float_near(l0->scale_factor_y, 2.72f, 0.01f);
    assert_float_near(l0->parallax_x, 0.5f, 0.01f);
    assert_float_near(l0->offset_x, 100.0f, 0.5f);
    assert_int_equal(l0->z_index, 99);
}

/* ---- Test 7: Malformed INI (no crash, graceful) ---- */

static void test_load_malformed_ini(void** state) {
    (void)state;

    FILE* f = fopen(TEST_INI_PATH, "w");
    assert_non_null(f);
    /* Various malformed lines */
    fprintf(f, "garbage no section\n");
    fprintf(f, "[layer_0]\n");
    fprintf(f, "no_equals_sign\n");
    fprintf(f, "=no_key\n");
    fprintf(f, "parallax_x=0.333\n");  /* This one should still work */
    fprintf(f, "[bad_section]\n");
    fprintf(f, "scale_x=999\n");       /* Should be ignored (not layer_N section) */
    fprintf(f, "[layer_999]\n");       /* Out of range */
    fprintf(f, "enabled=1\n");
    fclose(f);

    StageConfig_Load(TEST_STAGE);

    assert_true(g_stage_config.is_custom);
    /* The valid parallax_x=0.333 in layer_0 should have been parsed */
    assert_float_near(g_stage_config.layers[0].parallax_x, 0.333f, 0.01f);
    /* Layer 1 should be defaults (bad_section had no effect) */
    assert_true(g_stage_config.layers[1].enabled);
}

/* ---- Test 8: Intelligent bg_index defaults from use_real_scr ---- */

static void test_bg_index_intelligent_defaults(void** state) {
    (void)state;

    /* Stage 0: use_real_scr[0]=2 → foreground = stage_bgw_number[0][1] = 1 */
    remove(TEST_INI_PATH);
    StageConfig_Load(0);
    assert_int_equal(g_stage_config.layers[0].original_bg_index, 1);

    /* Stage 1: use_real_scr[1]=1 → foreground = stage_bgw_number[1][0] = 0 */
    StageConfig_Load(1);
    assert_int_equal(g_stage_config.layers[0].original_bg_index, 0);

    /* Layer 1+ should have -1 (no reference / static) */
    assert_int_equal(g_stage_config.layers[1].original_bg_index, -1);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_defaults),
        cmocka_unit_test(test_set_default_layer_valid),
        cmocka_unit_test(test_set_default_layer_boundary),
        cmocka_unit_test_setup_teardown(test_load_missing_file, setup, teardown),
        cmocka_unit_test_setup_teardown(test_load_ini_file, setup, teardown),
        cmocka_unit_test_setup_teardown(test_save_load_roundtrip, setup, teardown),
        cmocka_unit_test_setup_teardown(test_load_malformed_ini, setup, teardown),
        cmocka_unit_test(test_bg_index_intelligent_defaults),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
