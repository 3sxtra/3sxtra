/**
 * @file test_legacy_matrix.c
 * @brief Unit tests for the Ninja SDK matrix math shims (legacy_matrix.c).
 *
 * Covers: njUnitMatrix, njScale, njTranslate, njTranslateZ, njCalcPoint,
 * njCalcPoints, njGetMatrix, njSetMatrix — including NULL-pointer paths
 * that operate on the global current matrix (cmtx).
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "cmocka.h"

#include "port/rendering/legacy_matrix.h"

/* The global current matrix defined in legacy_matrix.c */
extern MTX cmtx;

/* Floating-point comparison tolerance */
#define EPSILON 1e-5f

static void assert_float_eq(f32 a, f32 b) {
    assert_true(fabsf(a - b) < EPSILON);
}

/* --- Helper: check identity matrix --- */
static void assert_identity(const MTX* m) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            f32 expected = (i == j) ? 1.0f : 0.0f;
            assert_float_eq(m->a[i][j], expected);
        }
    }
}

/* --- Helper: zero a matrix --- */
static void fill_matrix(MTX* m, f32 val) {
    for (int i = 0; i < 16; i++) {
        m->f[i] = val;
    }
}

/* ==========================================================================
 * njUnitMatrix tests
 * ========================================================================== */

/** @brief njUnitMatrix with explicit matrix sets identity. */
static void test_unit_matrix_explicit(void** state) {
    (void)state;
    MTX m;
    fill_matrix(&m, 99.0f);

    njUnitMatrix(&m);
    assert_identity(&m);
}

/** @brief njUnitMatrix with NULL operates on global cmtx. */
static void test_unit_matrix_null_uses_global(void** state) {
    (void)state;
    fill_matrix(&cmtx, 42.0f);

    njUnitMatrix(NULL);
    assert_identity(&cmtx);
}

/* ==========================================================================
 * njScale tests
 * ========================================================================== */

/** @brief njScale applies scaling factors to rows 0-2. */
static void test_scale_basic(void** state) {
    (void)state;
    MTX m;
    njUnitMatrix(&m);

    njScale(&m, 2.0f, 3.0f, 4.0f);

    /* Row 0 scaled by x=2 */
    assert_float_eq(m.a[0][0], 2.0f);
    assert_float_eq(m.a[0][1], 0.0f);
    /* Row 1 scaled by y=3 */
    assert_float_eq(m.a[1][1], 3.0f);
    assert_float_eq(m.a[1][0], 0.0f);
    /* Row 2 scaled by z=4 */
    assert_float_eq(m.a[2][2], 4.0f);
    assert_float_eq(m.a[2][0], 0.0f);
    /* Row 3 unchanged */
    assert_float_eq(m.a[3][0], 0.0f);
    assert_float_eq(m.a[3][3], 1.0f);
}

/** @brief njScale with zero scale zeroes out the row. */
static void test_scale_zero(void** state) {
    (void)state;
    MTX m;
    njUnitMatrix(&m);

    njScale(&m, 0.0f, 1.0f, 1.0f);

    /* Row 0 should be all zeros */
    for (int j = 0; j < 4; j++) {
        assert_float_eq(m.a[0][j], 0.0f);
    }
    /* Rows 1-2 unchanged (identity diagonal) */
    assert_float_eq(m.a[1][1], 1.0f);
    assert_float_eq(m.a[2][2], 1.0f);
}

/** @brief njScale with NULL uses global cmtx. */
static void test_scale_null_uses_global(void** state) {
    (void)state;
    njUnitMatrix(NULL);

    njScale(NULL, 5.0f, 6.0f, 7.0f);

    assert_float_eq(cmtx.a[0][0], 5.0f);
    assert_float_eq(cmtx.a[1][1], 6.0f);
    assert_float_eq(cmtx.a[2][2], 7.0f);
}

/* ==========================================================================
 * njTranslate tests
 * ========================================================================== */

/** @brief njTranslate on identity shifts row 3 by (x,y,z). */
static void test_translate_identity(void** state) {
    (void)state;
    MTX m;
    njUnitMatrix(&m);

    njTranslate(&m, 10.0f, 20.0f, 30.0f);

    /* Translation row */
    assert_float_eq(m.a[3][0], 10.0f);
    assert_float_eq(m.a[3][1], 20.0f);
    assert_float_eq(m.a[3][2], 30.0f);
    assert_float_eq(m.a[3][3], 1.0f);
    /* Upper 3x3 still identity */
    assert_float_eq(m.a[0][0], 1.0f);
    assert_float_eq(m.a[1][1], 1.0f);
    assert_float_eq(m.a[2][2], 1.0f);
}

/** @brief njTranslate with negative values. */
static void test_translate_negative(void** state) {
    (void)state;
    MTX m;
    njUnitMatrix(&m);

    njTranslate(&m, -5.0f, -10.0f, -15.0f);

    assert_float_eq(m.a[3][0], -5.0f);
    assert_float_eq(m.a[3][1], -10.0f);
    assert_float_eq(m.a[3][2], -15.0f);
}

/** @brief njTranslate with NULL uses global cmtx. */
static void test_translate_null_uses_global(void** state) {
    (void)state;
    njUnitMatrix(NULL);

    njTranslate(NULL, 1.0f, 2.0f, 3.0f);

    assert_float_eq(cmtx.a[3][0], 1.0f);
    assert_float_eq(cmtx.a[3][1], 2.0f);
    assert_float_eq(cmtx.a[3][2], 3.0f);
}

/* ==========================================================================
 * njTranslateZ tests
 * ========================================================================== */

/** @brief njTranslateZ is equivalent to njTranslate(NULL, 0, 0, z). */
static void test_translate_z_equivalence(void** state) {
    (void)state;

    /* Prepare cmtx via translate then scale so it's non-trivial */
    njUnitMatrix(NULL);
    njTranslate(NULL, 1.0f, 2.0f, 3.0f);
    njScale(NULL, 2.0f, 2.0f, 2.0f);

    /* Save a copy */
    MTX reference;
    njGetMatrix(&reference);

    /* Apply njTranslateZ to original */
    njTranslateZ(5.0f);
    MTX result_fast;
    njGetMatrix(&result_fast);

    /* Apply njTranslate(NULL, 0, 0, 5) to reference */
    njSetMatrix(NULL, &reference);
    njTranslate(NULL, 0.0f, 0.0f, 5.0f);
    MTX result_full;
    njGetMatrix(&result_full);

    /* They should match */
    for (int i = 0; i < 16; i++) {
        assert_float_eq(result_fast.f[i], result_full.f[i]);
    }
}

/** @brief njTranslateZ with z=0 is a no-op. */
static void test_translate_z_zero(void** state) {
    (void)state;
    njUnitMatrix(NULL);
    njTranslate(NULL, 1.0f, 2.0f, 3.0f);

    MTX before;
    njGetMatrix(&before);

    njTranslateZ(0.0f);

    MTX after;
    njGetMatrix(&after);

    for (int i = 0; i < 16; i++) {
        assert_float_eq(before.f[i], after.f[i]);
    }
}

/* ==========================================================================
 * njCalcPoint tests
 * ========================================================================== */

/** @brief njCalcPoint with identity produces the same point. */
static void test_calc_point_identity(void** state) {
    (void)state;
    MTX m;
    njUnitMatrix(&m);

    Vec3 src = { 1.0f, 2.0f, 3.0f };
    Vec3 dst = { 0 };

    njCalcPoint(&m, &src, &dst);

    assert_float_eq(dst.x, 1.0f);
    assert_float_eq(dst.y, 2.0f);
    assert_float_eq(dst.z, 3.0f);
}

/** @brief njCalcPoint with translation matrix shifts the point. */
static void test_calc_point_translate(void** state) {
    (void)state;
    MTX m;
    njUnitMatrix(&m);
    njTranslate(&m, 10.0f, 20.0f, 30.0f);

    Vec3 src = { 1.0f, 2.0f, 3.0f };
    Vec3 dst = { 0 };

    njCalcPoint(&m, &src, &dst);

    /* Point + translation = (1+10, 2+20, 3+30) */
    assert_float_eq(dst.x, 11.0f);
    assert_float_eq(dst.y, 22.0f);
    assert_float_eq(dst.z, 33.0f);
}

/** @brief njCalcPoint with scale matrix scales the point. */
static void test_calc_point_scale(void** state) {
    (void)state;
    MTX m;
    njUnitMatrix(&m);
    njScale(&m, 2.0f, 3.0f, 4.0f);

    Vec3 src = { 1.0f, 2.0f, 3.0f };
    Vec3 dst = { 0 };

    njCalcPoint(&m, &src, &dst);

    assert_float_eq(dst.x, 2.0f);
    assert_float_eq(dst.y, 6.0f);
    assert_float_eq(dst.z, 12.0f);
}

/** @brief njCalcPoint with NULL uses global cmtx. */
static void test_calc_point_null_uses_global(void** state) {
    (void)state;
    njUnitMatrix(NULL);
    njScale(NULL, 3.0f, 3.0f, 3.0f);

    Vec3 src = { 1.0f, 1.0f, 1.0f };
    Vec3 dst = { 0 };

    njCalcPoint(NULL, &src, &dst);

    assert_float_eq(dst.x, 3.0f);
    assert_float_eq(dst.y, 3.0f);
    assert_float_eq(dst.z, 3.0f);
}

/* ==========================================================================
 * njCalcPoints tests
 * ========================================================================== */

/** @brief njCalcPoints transforms an array of points. */
static void test_calc_points_array(void** state) {
    (void)state;
    MTX m;
    njUnitMatrix(&m);
    njTranslate(&m, 1.0f, 0.0f, 0.0f);

    Vec3 src[3] = {
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 1.0f },
        { -1.0f, -1.0f, -1.0f },
    };
    Vec3 dst[3] = { 0 };

    njCalcPoints(&m, src, dst, 3);

    /* Each point's x should be shifted by +1 */
    assert_float_eq(dst[0].x, 1.0f);
    assert_float_eq(dst[0].y, 0.0f);
    assert_float_eq(dst[0].z, 0.0f);

    assert_float_eq(dst[1].x, 2.0f);
    assert_float_eq(dst[1].y, 1.0f);
    assert_float_eq(dst[1].z, 1.0f);

    assert_float_eq(dst[2].x, 0.0f);
    assert_float_eq(dst[2].y, -1.0f);
    assert_float_eq(dst[2].z, -1.0f);
}

/** @brief njCalcPoints with count=0 is a no-op. */
static void test_calc_points_zero_count(void** state) {
    (void)state;
    MTX m;
    njUnitMatrix(&m);

    Vec3 src = { 1.0f, 2.0f, 3.0f };
    Vec3 dst = { 99.0f, 99.0f, 99.0f };

    njCalcPoints(&m, &src, &dst, 0);

    /* dst should be unchanged */
    assert_float_eq(dst.x, 99.0f);
    assert_float_eq(dst.y, 99.0f);
    assert_float_eq(dst.z, 99.0f);
}

/* ==========================================================================
 * njGetMatrix / njSetMatrix tests
 * ========================================================================== */

/** @brief njGetMatrix / njSetMatrix round-trip preserves matrix. */
static void test_get_set_roundtrip(void** state) {
    (void)state;

    /* Set up cmtx with known values */
    njUnitMatrix(NULL);
    njTranslate(NULL, 7.0f, 8.0f, 9.0f);
    njScale(NULL, 2.0f, 3.0f, 4.0f);

    /* Save it */
    MTX saved;
    njGetMatrix(&saved);

    /* Clobber cmtx */
    njUnitMatrix(NULL);

    /* Restore */
    njSetMatrix(NULL, &saved);

    /* Verify round-trip */
    MTX restored;
    njGetMatrix(&restored);

    for (int i = 0; i < 16; i++) {
        assert_float_eq(saved.f[i], restored.f[i]);
    }
}

/** @brief njSetMatrix with explicit dst copies correctly. */
static void test_set_matrix_explicit_dst(void** state) {
    (void)state;
    MTX src, dst;
    njUnitMatrix(&src);
    njScale(&src, 5.0f, 6.0f, 7.0f);
    fill_matrix(&dst, 0.0f);

    njSetMatrix(&dst, &src);

    for (int i = 0; i < 16; i++) {
        assert_float_eq(src.f[i], dst.f[i]);
    }
}

/* ==========================================================================
 * Main
 * ========================================================================== */

int main(void) {
    const struct CMUnitTest tests[] = {
        /* njUnitMatrix */
        cmocka_unit_test(test_unit_matrix_explicit),
        cmocka_unit_test(test_unit_matrix_null_uses_global),
        /* njScale */
        cmocka_unit_test(test_scale_basic),
        cmocka_unit_test(test_scale_zero),
        cmocka_unit_test(test_scale_null_uses_global),
        /* njTranslate */
        cmocka_unit_test(test_translate_identity),
        cmocka_unit_test(test_translate_negative),
        cmocka_unit_test(test_translate_null_uses_global),
        /* njTranslateZ */
        cmocka_unit_test(test_translate_z_equivalence),
        cmocka_unit_test(test_translate_z_zero),
        /* njCalcPoint */
        cmocka_unit_test(test_calc_point_identity),
        cmocka_unit_test(test_calc_point_translate),
        cmocka_unit_test(test_calc_point_scale),
        cmocka_unit_test(test_calc_point_null_uses_global),
        /* njCalcPoints */
        cmocka_unit_test(test_calc_points_array),
        cmocka_unit_test(test_calc_points_zero_count),
        /* njGetMatrix / njSetMatrix */
        cmocka_unit_test(test_get_set_roundtrip),
        cmocka_unit_test(test_set_matrix_explicit_dst),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
