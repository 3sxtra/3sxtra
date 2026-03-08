/**
 * @file test_afs_validation.c
 * @brief Unit tests for AFS archive attribute validation logic.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include "cmocka.h"

#define AFS_ATTRIBUTE_ENTRY_SIZE 48

/**
 * @brief Copy of the static validation function from src/port/io/afs.c
 * 
 * Since the function is static in afs.c and we want to test it in isolation
 * without bringing in SDL or file I/O dependencies, we copy the logic here.
 */
static bool is_valid_attribute_data(uint32_t attributes_offset, uint32_t attributes_size, int64_t file_size,
                                    uint32_t entries_end_offset, uint32_t entry_count) {
    if ((attributes_offset == 0) || (attributes_size == 0)) {
        return false;
    }

    if (attributes_size > (file_size - (int64_t)entries_end_offset)) {
        return false;
    }

    if (attributes_size < (entry_count * AFS_ATTRIBUTE_ENTRY_SIZE)) {
        return false;
    }

    if (attributes_offset < entries_end_offset) {
        return false;
    }

    if (attributes_offset > (uint32_t)(file_size - attributes_size)) {
        return false;
    }

    return true;
}

static void test_zero_params(void** state) {
    (void)state;
    // Zero offset or size -> false
    assert_false(is_valid_attribute_data(0, 500, 2000, 100, 10));
    assert_false(is_valid_attribute_data(500, 0, 2000, 100, 10));
    assert_false(is_valid_attribute_data(0, 0, 2000, 100, 10));
}

static void test_size_exceeds_bounds(void** state) {
    (void)state;
    // attributes_size > (file_size - entries_end_offset) -> false
    // size = 1000, file_size = 1000, end_offset = 100. max_allowed_size = 900.
    assert_false(is_valid_attribute_data(200, 1000, 1000, 100, 10));
}

static void test_size_too_small_for_entries(void** state) {
    (void)state;
    // attributes_size < (entry_count * AFS_ATTRIBUTE_ENTRY_SIZE) -> false
    // entry_count = 10, required_size = 480.
    assert_false(is_valid_attribute_data(200, 479, 2000, 100, 10));
}

static void test_offset_before_entries_end(void** state) {
    (void)state;
    // attributes_offset < entries_end_offset -> false
    assert_false(is_valid_attribute_data(99, 500, 2000, 100, 10));
}

static void test_offset_after_file_bounds(void** state) {
    (void)state;
    // attributes_offset > (file_size - attributes_size) -> false
    // file_size = 1000, size = 500. max_offset = 500.
    assert_false(is_valid_attribute_data(501, 500, 1000, 100, 10));
}

static void test_valid_params(void** state) {
    (void)state;
    // attributes_offset = 600, size = 480, file_size = 2000, end_offset = 500, count = 10.
    // 1. offset != 0 && size != 0 (600, 480) -> OK
    // 2. size (480) <= (file_size (2000) - end_offset (500) = 1500) -> OK
    // 3. size (480) >= (count (10) * 48 = 480) -> OK
    // 4. offset (600) >= (end_offset (500)) -> OK
    // 5. offset (600) <= (file_size (2000) - size (480) = 1520) -> OK
    assert_true(is_valid_attribute_data(600, 480, 2000, 500, 10));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_zero_params),
        cmocka_unit_test(test_size_exceeds_bounds),
        cmocka_unit_test(test_size_too_small_for_entries),
        cmocka_unit_test(test_offset_before_entries_end),
        cmocka_unit_test(test_offset_after_file_bounds),
        cmocka_unit_test(test_valid_params),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
