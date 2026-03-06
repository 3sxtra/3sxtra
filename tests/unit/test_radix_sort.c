/**
 * @file test_radix_sort.c
 * @brief Unit tests for the radix sort used in SDL2D z-depth ordering.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <float.h>
#include "cmocka.h"

/* Include the header-only radix sort implementation */
#include "port/sdl/radix_sort.h"

/* Scratch buffers shared across tests */
#define TEST_MAX 1024
static uint32_t test_keys[TEST_MAX];
static int      test_scratch[TEST_MAX];
static int      test_order[TEST_MAX];

/* --- Helpers --- */

/* Verify that order[] produces ascending z-values, with descending index for ties. */
static void verify_sorted(int* order, const float* z, int count) {
    for (int i = 1; i < count; i++) {
        float z_prev = z[order[i - 1]];
        float z_curr = z[order[i]];
        /* z must be non-decreasing */
        assert_true(z_prev <= z_curr);
        /* On equal z, indices must be descending (higher original index first) */
        if (z_prev == z_curr) {
            assert_true(order[i - 1] > order[i]);
        }
    }
}

/* --- Test Cases --- */

static void test_single_element(void** state) {
    (void)state;
    float z[] = { 42.0f };
    radix_sort_render_task_indices(test_order, z, 1, test_keys, test_scratch);
    assert_int_equal(test_order[0], 0);
}

static void test_empty(void** state) {
    (void)state;
    /* Should not crash */
    radix_sort_render_task_indices(test_order, NULL, 0, test_keys, test_scratch);
}

static void test_already_sorted(void** state) {
    (void)state;
    float z[] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
    int count = 5;

    radix_sort_render_task_indices(test_order, z, count, test_keys, test_scratch);
    verify_sorted(test_order, z, count);

    /* All z are unique, so order should be ascending indices */
    for (int i = 0; i < count; i++) {
        assert_int_equal(test_order[i], i);
    }
}

static void test_reverse_sorted(void** state) {
    (void)state;
    float z[] = { 5.0f, 4.0f, 3.0f, 2.0f, 1.0f };
    int count = 5;

    radix_sort_render_task_indices(test_order, z, count, test_keys, test_scratch);
    verify_sorted(test_order, z, count);

    /* Expected order: index 4 (z=1), 3 (z=2), 2 (z=3), 1 (z=4), 0 (z=5) */
    assert_int_equal(test_order[0], 4);
    assert_int_equal(test_order[1], 3);
    assert_int_equal(test_order[2], 2);
    assert_int_equal(test_order[3], 1);
    assert_int_equal(test_order[4], 0);
}

static void test_duplicate_z_tiebreak(void** state) {
    (void)state;
    /* All same z — tie-break should produce descending original index */
    float z[] = { 10.0f, 10.0f, 10.0f, 10.0f };
    int count = 4;

    radix_sort_render_task_indices(test_order, z, count, test_keys, test_scratch);
    verify_sorted(test_order, z, count);

    /* Expected: indices 3, 2, 1, 0 (descending) */
    assert_int_equal(test_order[0], 3);
    assert_int_equal(test_order[1], 2);
    assert_int_equal(test_order[2], 1);
    assert_int_equal(test_order[3], 0);
}

static void test_mixed_positive_negative(void** state) {
    (void)state;
    float z[] = { 3.0f, -1.0f, 0.0f, -5.0f, 2.0f };
    int count = 5;

    radix_sort_render_task_indices(test_order, z, count, test_keys, test_scratch);
    verify_sorted(test_order, z, count);

    /* Expected order by z: -5 (idx3), -1 (idx1), 0 (idx2), 2 (idx4), 3 (idx0) */
    assert_int_equal(test_order[0], 3); /* z = -5.0 */
    assert_int_equal(test_order[1], 1); /* z = -1.0 */
    assert_int_equal(test_order[2], 2); /* z =  0.0 */
    assert_int_equal(test_order[3], 4); /* z =  2.0 */
    assert_int_equal(test_order[4], 0); /* z =  3.0 */
}

static void test_partial_ties(void** state) {
    (void)state;
    /* Groups of equal z-values interspersed with unique ones */
    float z[] = { 1.0f, 2.0f, 1.0f, 3.0f, 2.0f, 1.0f };
    int count = 6;

    radix_sort_render_task_indices(test_order, z, count, test_keys, test_scratch);
    verify_sorted(test_order, z, count);

    /* z=1.0 group: indices 5, 2, 0 (descending within tie) */
    assert_int_equal(test_order[0], 5);
    assert_int_equal(test_order[1], 2);
    assert_int_equal(test_order[2], 0);

    /* z=2.0 group: indices 4, 1 */
    assert_int_equal(test_order[3], 4);
    assert_int_equal(test_order[4], 1);

    /* z=3.0: index 3 */
    assert_int_equal(test_order[5], 3);
}

static void test_larger_count(void** state) {
    (void)state;
    /* 256 elements in reverse order */
    float z[256];
    for (int i = 0; i < 256; i++) {
        z[i] = (float)(256 - i);
    }

    radix_sort_render_task_indices(test_order, z, 256, test_keys, test_scratch);
    verify_sorted(test_order, z, 256);
}

static void test_float_to_sortable_monotonic(void** state) {
    (void)state;
    /* Verify the float-to-sortable conversion preserves ordering */
    float values[] = { -100.0f, -1.0f, -0.001f, 0.0f, 0.001f, 1.0f, 100.0f };
    int count = 7;

    for (int i = 1; i < count; i++) {
        uint32_t a = radix_float_to_sortable(values[i - 1]);
        uint32_t b = radix_float_to_sortable(values[i]);
        assert_true(a < b);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_single_element),
        cmocka_unit_test(test_empty),
        cmocka_unit_test(test_already_sorted),
        cmocka_unit_test(test_reverse_sorted),
        cmocka_unit_test(test_duplicate_z_tiebreak),
        cmocka_unit_test(test_mixed_positive_negative),
        cmocka_unit_test(test_partial_ties),
        cmocka_unit_test(test_larger_count),
        cmocka_unit_test(test_float_to_sortable_monotonic),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
