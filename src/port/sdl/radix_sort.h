/**
 * @file radix_sort.h
 * @brief O(n) radix sort for z-depth render task ordering.
 *
 * Sorts an index array by float z-keys using a 2-pass 8-bit radix sort
 * on the low 16 bits of the IEEE 754 sortable representation.
 * Stable sort with tie-breaking by descending original index (FIFO).
 *
 * Usage:
 *   Provide arrays of z-values (float) and scratch space. The function
 *   writes the sorted permutation into the output order[] array.
 */
#ifndef RADIX_SORT_H
#define RADIX_SORT_H

#include <stdint.h>
#include <string.h>

/**
 * Convert IEEE 754 float to a sortable unsigned integer.
 *
 * Positive floats already sort correctly as unsigned ints.
 * Negative floats need all bits flipped. This transform makes
 * the unsigned comparison equivalent to float comparison.
 */
static inline uint32_t radix_float_to_sortable(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    /* If sign bit is set (negative), flip all bits.
     * If sign bit is clear (positive), flip only the sign bit.
     * This maps the full float range to monotonically increasing uint32. */
    uint32_t mask = -(bits >> 31) | 0x80000000u;
    return bits ^ mask;
}

/**
 * Radix sort render task indices by float z-values.
 *
 * Sorts `order[0..count-1]` such that `z_values[order[i]]` is in ascending
 * order. For equal z-values, higher original indices come first (descending
 * index order), matching the existing comparator's FIFO-preserving behavior.
 *
 * @param order       Output: sorted index permutation (must hold `count` ints)
 * @param z_values    Input: z-depth per render task (indexed by task index)
 * @param count       Number of tasks to sort
 * @param keys        Scratch: uint32_t[count] for sortable keys
 * @param scratch     Scratch: int[count] for intermediate permutation
 */
static inline void radix_sort_render_task_indices(
    int*       order,
    const float* z_values,
    int        count,
    uint32_t*  keys,
    int*       scratch)
{
    if (count <= 1) {
        if (count == 1) order[0] = 0;
        return;
    }

    /* Initialize order in reverse so that a stable sort on equal keys
     * naturally produces descending-index order (matching the existing
     * comparator: equal z => higher original_index first). */
    for (int i = 0; i < count; i++) {
        order[i] = count - 1 - i;
    }

    /* Build sortable keys */
    for (int i = 0; i < count; i++) {
        keys[i] = radix_float_to_sortable(z_values[i]);
    }

    /* 2-pass radix sort: 16 bits total, 8 bits per pass.
     * Pass 0: sort by bits [0..7]   (low byte)
     * Pass 1: sort by bits [8..15]  (high byte of low 16)
     *
     * We use only the top 16 bits of the 32-bit sortable key for the radix,
     * which provides enough precision for z-depth ordering (65536 distinct
     * buckets). The full 32-bit key is only 4 passes but 2 passes suffices
     * for the typical z-range in this renderer.
     *
     * Actually — to guarantee correctness across the full float range,
     * we do a full 4-pass (32-bit) radix sort. The cost is still O(n)
     * and for n < 8192 the constant factor is negligible. */

    int* src = order;
    int* dst = scratch;

    for (int pass = 0; pass < 4; pass++) {
        const int shift = pass * 8;
        int counts[256] = {0};

        /* Count occurrences of each radix digit */
        for (int i = 0; i < count; i++) {
            uint32_t digit = (keys[src[i]] >> shift) & 0xFF;
            counts[digit]++;
        }

        /* Convert counts to prefix sums (exclusive scan) */
        int offsets[256];
        offsets[0] = 0;
        for (int i = 1; i < 256; i++) {
            offsets[i] = offsets[i - 1] + counts[i - 1];
        }

        /* Scatter elements into destination in sorted order */
        for (int i = 0; i < count; i++) {
            uint32_t digit = (keys[src[i]] >> shift) & 0xFF;
            dst[offsets[digit]++] = src[i];
        }

        /* Swap src/dst for next pass */
        int* tmp = src;
        src = dst;
        dst = tmp;
    }

    /* After 4 passes (even number), src == order. No final copy needed.
     * src alternates: pass0→scratch, pass1→order, pass2→scratch, pass3→order.
     * So after pass 3, src == order. ✓ */
}

#endif /* RADIX_SORT_H */
