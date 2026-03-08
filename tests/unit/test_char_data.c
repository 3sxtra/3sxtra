/**
 * @file test_char_data.c
 * @brief Unit tests for CharData_ApplyFixups() in src/port/char_data.c
 *
 * CharData_ApplyFixups patches hitbox data for specific characters.
 * Currently only character 14 (Akuma/Gouki) is handled: the throw box
 * is removed from the overhead chop by zeroing hiit[0x5A..0x5D].cuix.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <cmocka.h>

#include "port/char_data.h"

/* Number of hiit entries needed for the Akuma fixup (0x5D + 1 = 0x5E) */
#define HIIT_LEN 0x5E

/* Helper: build a CharInitData with hiit filled with a sentinel value */
static UNK_0 g_hiit[HIIT_LEN];

static CharInitData make_data_filled(u16 sentinel) {
    CharInitData data;
    memset(&data, 0, sizeof(data));
    for (int i = 0; i < HIIT_LEN; i++) {
        g_hiit[i].cuix = sentinel;
    }
    data.hiit = g_hiit;
    return data;
}

/* ───────────────────────────────────────────── Tests ─── */

static void test_akuma_fixup_zeroes_cuix_range(void **state) {
    (void) state;
    CharInitData data = make_data_filled(0xBEEF);

    CharData_ApplyFixups(&data, 14); /* character 14 = Akuma */

    /* Entries 0x5A..0x5D must be zeroed */
    for (int i = 0x5A; i <= 0x5D; i++) {
        assert_int_equal(data.hiit[i].cuix, 0);
    }

    /* Entries outside that range must be unchanged */
    for (int i = 0; i < 0x5A; i++) {
        assert_int_equal(data.hiit[i].cuix, 0xBEEF);
    }
}

static void test_non_akuma_chars_unmodified(void **state) {
    (void) state;

    /* Test a few representative non-Akuma character IDs */
    int test_chars[] = {0, 1, 2, 13, 15, 19};
    for (size_t c = 0; c < sizeof(test_chars) / sizeof(test_chars[0]); c++) {
        CharInitData data = make_data_filled(0x1234);
        CharData_ApplyFixups(&data, test_chars[c]);

        for (int i = 0; i < HIIT_LEN; i++) {
            assert_int_equal(data.hiit[i].cuix, 0x1234);
        }
    }
}

static void test_char_13_not_akuma(void **state) {
    (void) state;
    CharInitData data = make_data_filled(0xFFFF);
    CharData_ApplyFixups(&data, 13); /* Urien — must NOT apply fixup */

    for (int i = 0; i < HIIT_LEN; i++) {
        assert_int_equal(data.hiit[i].cuix, 0xFFFF);
    }
}

static void test_char_15_not_akuma(void **state) {
    (void) state;
    CharInitData data = make_data_filled(0xAAAA);
    CharData_ApplyFixups(&data, 15); /* Chun-Li — must NOT apply fixup */

    for (int i = 0; i < HIIT_LEN; i++) {
        assert_int_equal(data.hiit[i].cuix, 0xAAAA);
    }
}

static void test_akuma_fixup_idempotent(void **state) {
    (void) state;
    CharInitData data = make_data_filled(0xBEEF);

    /* Applying fixup twice should be safe and leave same result */
    CharData_ApplyFixups(&data, 14);
    CharData_ApplyFixups(&data, 14);

    for (int i = 0x5A; i <= 0x5D; i++) {
        assert_int_equal(data.hiit[i].cuix, 0);
    }
}

static void test_null_data_no_crash(void **state) {
    (void) state;
    /* NULL pointer must not crash — function should early-out */
    CharData_ApplyFixups(NULL, 14);
}

static void test_negative_char_id_no_crash(void **state) {
    (void) state;
    CharInitData data = make_data_filled(0x5555);
    CharData_ApplyFixups(&data, -1); /* Invalid ID — should be ignored */

    for (int i = 0; i < HIIT_LEN; i++) {
        assert_int_equal(data.hiit[i].cuix, 0x5555);
    }
}

static void test_large_char_id_no_crash(void **state) {
    (void) state;
    CharInitData data = make_data_filled(0x3333);
    CharData_ApplyFixups(&data, 9999); /* Out-of-range — should be ignored */

    for (int i = 0; i < HIIT_LEN; i++) {
        assert_int_equal(data.hiit[i].cuix, 0x3333);
    }
}

/* ─────────────────────────────────────── Test runner ─── */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_akuma_fixup_zeroes_cuix_range),
        cmocka_unit_test(test_non_akuma_chars_unmodified),
        cmocka_unit_test(test_char_13_not_akuma),
        cmocka_unit_test(test_char_15_not_akuma),
        cmocka_unit_test(test_akuma_fixup_idempotent),
        cmocka_unit_test(test_null_data_no_crash),
        cmocka_unit_test(test_negative_char_id_no_crash),
        cmocka_unit_test(test_large_char_id_no_crash),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
