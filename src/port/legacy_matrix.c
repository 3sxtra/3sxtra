/**
 * @file legacy_matrix.c
 * @brief Ninja SDK matrix math shims (njUnitMatrix, njScale, njTranslate, etc.).
 *
 * Provides a minimal 4×4 matrix stack used by the original PS2 rendering
 * code. Operations are performed on a global "current matrix" (`cmtx`)
 * unless an explicit matrix pointer is supplied.
 */
#include "port/legacy_matrix.h"
#include "common.h"
#include <string.h>

// ⚡ Bolt: SIMDe for portable SIMD intrinsics (SSE/FMA on x86, NEON on ARM)
#include <simde/x86/sse.h>
#include <simde/x86/fma.h>

MTX cmtx;

/** @brief Multiply two 4×4 matrices: dst = a × b (safe for dst aliasing a or b).
 *  ⚡ Bolt: SIMD — loads B's 4 rows once, then for each A row broadcasts each
 *  element across a vector and FMAs against B's rows. ~20 FLOPs+overhead → 4×FMA. */
static void matmul(MTX* dst, const MTX* a, const MTX* b) {
    MTX result;

    // Pre-load all 4 rows of B
    const simde__m128 b0 = simde_mm_loadu_ps(b->a[0]);
    const simde__m128 b1 = simde_mm_loadu_ps(b->a[1]);
    const simde__m128 b2 = simde_mm_loadu_ps(b->a[2]);
    const simde__m128 b3 = simde_mm_loadu_ps(b->a[3]);

    for (int i = 0; i < 4; i++) {
        // Broadcast each element of A's row i
        simde__m128 r = simde_mm_mul_ps(simde_mm_set1_ps(a->a[i][0]), b0);
        r = simde_mm_fmadd_ps(simde_mm_set1_ps(a->a[i][1]), b1, r);
        r = simde_mm_fmadd_ps(simde_mm_set1_ps(a->a[i][2]), b2, r);
        r = simde_mm_fmadd_ps(simde_mm_set1_ps(a->a[i][3]), b3, r);
        simde_mm_storeu_ps(result.a[i], r);
    }

    memcpy(dst, &result, sizeof(MTX));
}

/** @brief Load the identity matrix (NULL → use global cmtx). */
void njUnitMatrix(MTX* mtx) {
    if (mtx == NULL) {
        mtx = &cmtx;
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            mtx->a[i][j] = (i == j);
        }
    }
}

/** @brief Copy the global current matrix into m. */
void njGetMatrix(MTX* m) {
    *m = cmtx;
}

/** @brief Copy matrix ms into md (NULL md → use global cmtx). */
void njSetMatrix(MTX* md, MTX* ms) {
    if (md == NULL) {
        md = &cmtx;
    }

    *md = *ms;
}

/** @brief Apply a scale transform to the matrix (NULL → global cmtx).
 *  ⚡ Bolt: SIMD — 3× load-mul-store instead of 12 scalar multiplies. */
void njScale(MTX* mtx, f32 x, f32 y, f32 z) {
    if (mtx == NULL) {
        mtx = &cmtx;
    }

    simde_mm_storeu_ps(mtx->a[0], simde_mm_mul_ps(simde_mm_loadu_ps(mtx->a[0]), simde_mm_set1_ps(x)));
    simde_mm_storeu_ps(mtx->a[1], simde_mm_mul_ps(simde_mm_loadu_ps(mtx->a[1]), simde_mm_set1_ps(y)));
    simde_mm_storeu_ps(mtx->a[2], simde_mm_mul_ps(simde_mm_loadu_ps(mtx->a[2]), simde_mm_set1_ps(z)));
}

/** @brief Apply a translation to the matrix via pre-multiplication (NULL → global cmtx). */
void njTranslate(MTX* mtx, f32 x, f32 y, f32 z) {
    if (mtx == NULL) {
        mtx = &cmtx;
    }

    MTX translation_matrix;

    njUnitMatrix(&translation_matrix);
    translation_matrix.a[3][0] = x;
    translation_matrix.a[3][1] = y;
    translation_matrix.a[3][2] = z;

    matmul(mtx, &translation_matrix, mtx);
}

/**
 * @brief Fast Z-only translation on the global current matrix.
 *
 * Equivalent to njTranslate(NULL, 0, 0, z) but avoids the full 4×4 matrix
 * multiply. Pre-multiplying an identity-with-z translation only affects row 3:
 *   row3[j] += z * row2[j]
 *
 * ⚡ Bolt: ~130 FLOPs → 8 FLOPs per call. Called 100–300× per frame.
 */
void njTranslateZ(f32 z) {
    cmtx.a[3][0] += z * cmtx.a[2][0];
    cmtx.a[3][1] += z * cmtx.a[2][1];
    cmtx.a[3][2] += z * cmtx.a[2][2];
    cmtx.a[3][3] += z * cmtx.a[2][3];
}

/** @brief Transform a single 3D point by the matrix (NULL → global cmtx). */
void njCalcPoint(MTX* mtx, Vec3* ps, Vec3* pd) {
    if (mtx == NULL) {
        mtx = &cmtx;
    }

    const f32 x = ps->x;
    const f32 y = ps->y;
    const f32 z = ps->z;
    const f32 w = 1.0f;

    pd->x = x * mtx->a[0][0] + y * mtx->a[1][0] + z * mtx->a[2][0] + w * mtx->a[3][0];
    pd->y = x * mtx->a[0][1] + y * mtx->a[1][1] + z * mtx->a[2][1] + w * mtx->a[3][1];
    pd->z = x * mtx->a[0][2] + y * mtx->a[1][2] + z * mtx->a[2][2] + w * mtx->a[3][2];
}

/** @brief Transform an array of 3D points by the matrix (NULL → global cmtx). */
void njCalcPoints(MTX* mtx, Vec3* ps, Vec3* pd, s32 num) {
    s32 i;

    if (mtx == NULL) {
        mtx = &cmtx;
    }

    for (i = 0; i < num; i++) {
        njCalcPoint(mtx, ps++, pd++);
    }
}
