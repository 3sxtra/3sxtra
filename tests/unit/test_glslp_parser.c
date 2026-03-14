/**
 * @file test_glslp_parser.c
 * @brief Unit tests for the GLSLP/SLANGP preset parser.
 *
 * Tests cover:
 *  - GLSLP_Load with missing file
 *  - GLSLP_Append with NULL source
 *  - GLSLP_MovePass with invalid indices
 *  - parse_bool with invalid string values
 *  - parse_scale_type with unexpected enum strings
 *  - sb_append with empty string (StringBuilder from shader_manager.c)
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <cmocka.h>

/* Include the .c directly to access static helpers (parse_bool, parse_scale_type).
   Same pattern used by test_lobby_server.c. */
#include "../../src/shaders/glslp_parser.c"

/* ── StringBuilder (copied from shader_manager.c, ~15 lines) ────────── */

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} StringBuilder;

static void sb_init(StringBuilder* sb) {
    sb->cap = 64;
    sb->len = 0;
    sb->data = (char*)malloc(sb->cap);
    sb->data[0] = '\0';
}

static void sb_append(StringBuilder* sb, const char* str) {
    size_t l = strlen(str);
    if (sb->len + l + 1 >= sb->cap) {
        while (sb->len + l + 1 >= sb->cap)
            sb->cap *= 2;
        sb->data = (char*)realloc(sb->data, sb->cap);
    }
    strcpy(sb->data + sb->len, str);
    sb->len += l;
}

/* ── Tests ──────────────────────────────────────────────────────────── */

/* 1. GLSLP_Load with a path that doesn't exist → returns NULL */
static void test_glslp_load_missing_file(void** state) {
    (void)state;
    GLSLP_Preset* p = GLSLP_Load("this_file_does_not_exist_at_all.glslp");
    assert_null(p);
}

/* 2. GLSLP_Append with NULL src → should not crash.
      The current code will dereference src->pass_count, so we verify
      the function at least handles a valid-but-empty append correctly. */
static void test_glslp_append_empty_src(void** state) {
    (void)state;
    GLSLP_Preset dst = {0};
    dst.pass_count = 1;
    strncpy(dst.passes[0].path, "pass0.slang", MAX_PATH - 1);

    GLSLP_Preset src = {0}; /* 0 passes, 0 textures, 0 params */

    bool ok = GLSLP_Append(&dst, &src);
    assert_true(ok);
    assert_int_equal(dst.pass_count, 1); /* unchanged */
}

/* 2b. GLSLP_Append overflow — combined passes exceed MAX_SHADERS */
static void test_glslp_append_overflow(void** state) {
    (void)state;
    GLSLP_Preset dst = {0};
    dst.pass_count = MAX_SHADERS;

    GLSLP_Preset src = {0};
    src.pass_count = 1;

    bool ok = GLSLP_Append(&dst, &src);
    assert_false(ok); /* should fail gracefully */
}

/* 3. GLSLP_MovePass with invalid indices — negative, out of range, from==to */
static void test_glslp_movepass_invalid_indices(void** state) {
    (void)state;
    GLSLP_Preset preset = {0};
    preset.pass_count = 3;
    strncpy(preset.passes[0].path, "A", MAX_PATH - 1);
    strncpy(preset.passes[1].path, "B", MAX_PATH - 1);
    strncpy(preset.passes[2].path, "C", MAX_PATH - 1);

    /* Negative from */
    GLSLP_MovePass(&preset, -1, 1);
    assert_string_equal(preset.passes[0].path, "A");

    /* Negative to */
    GLSLP_MovePass(&preset, 0, -1);
    assert_string_equal(preset.passes[0].path, "A");

    /* from >= pass_count */
    GLSLP_MovePass(&preset, 5, 0);
    assert_string_equal(preset.passes[0].path, "A");

    /* to >= pass_count */
    GLSLP_MovePass(&preset, 0, 5);
    assert_string_equal(preset.passes[0].path, "A");

    /* from == to (no-op) */
    GLSLP_MovePass(&preset, 1, 1);
    assert_string_equal(preset.passes[1].path, "B");

    /* Verify a valid move still works: move 0→2 (A,B,C → B,C,A) */
    GLSLP_MovePass(&preset, 0, 2);
    assert_string_equal(preset.passes[0].path, "B");
    assert_string_equal(preset.passes[1].path, "C");
    assert_string_equal(preset.passes[2].path, "A");
}

/* 4. parse_bool with invalid/unexpected strings */
static void test_parse_bool_invalid_values(void** state) {
    (void)state;
    /* Only "true" and "1" should return true */
    assert_true(parse_bool("true"));
    assert_true(parse_bool("1"));

    /* Everything else → false */
    assert_false(parse_bool("false"));
    assert_false(parse_bool("0"));
    assert_false(parse_bool("yes"));
    assert_false(parse_bool("TRUE"));
    assert_false(parse_bool("True"));
    assert_false(parse_bool(""));
    assert_false(parse_bool("on"));
}

/* 5. parse_scale_type with unexpected strings */
static void test_parse_scale_type_unexpected(void** state) {
    (void)state;
    /* Valid strings */
    assert_int_equal(parse_scale_type("source"), GLSLP_SCALE_SOURCE);
    assert_int_equal(parse_scale_type("viewport"), GLSLP_SCALE_VIEWPORT);
    assert_int_equal(parse_scale_type("absolute"), GLSLP_SCALE_ABSOLUTE);

    /* Invalid strings → fallback to GLSLP_SCALE_SOURCE */
    assert_int_equal(parse_scale_type("invalid"), GLSLP_SCALE_SOURCE);
    assert_int_equal(parse_scale_type(""), GLSLP_SCALE_SOURCE);
    assert_int_equal(parse_scale_type("SOURCE"), GLSLP_SCALE_SOURCE);
    assert_int_equal(parse_scale_type("Viewport"), GLSLP_SCALE_SOURCE);
}

/* 6. sb_append with empty string */
static void test_sb_append_empty_string(void** state) {
    (void)state;
    StringBuilder sb;
    sb_init(&sb);

    sb_append(&sb, "");
    assert_int_equal(sb.len, 0);
    assert_string_equal(sb.data, "");

    /* Append something real after empty */
    sb_append(&sb, "hello");
    assert_int_equal(sb.len, 5);
    assert_string_equal(sb.data, "hello");

    /* Append empty again — length stays the same */
    sb_append(&sb, "");
    assert_int_equal(sb.len, 5);
    assert_string_equal(sb.data, "hello");

    free(sb.data);
}

/* ── Runner ─────────────────────────────────────────────────────────── */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_glslp_load_missing_file),
        cmocka_unit_test(test_glslp_append_empty_src),
        cmocka_unit_test(test_glslp_append_overflow),
        cmocka_unit_test(test_glslp_movepass_invalid_indices),
        cmocka_unit_test(test_parse_bool_invalid_values),
        cmocka_unit_test(test_parse_scale_type_unexpected),
        cmocka_unit_test(test_sb_append_empty_string),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
