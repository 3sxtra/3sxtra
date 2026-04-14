#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "port/ui/native_imgui.h"
#include "types.h"

// Mocks for engine globals
u8 Order[148];
u8 Order_Timer[148];
u8 Order_Dir[148];
s8 Menu_Cursor_Y[4];
u8 Menu_Suicide[4];
s32 Menu_Cursor_Move;

// Mock effect functions
s32 effect_61_init(s16 unused1, u8 slot, s16 unused2, s16 master_player, s16 graphic_index, s16 cursor_index, u16 letter_type) {
    check_expected(slot);
    check_expected(master_player);
    check_expected(graphic_index);
    check_expected(cursor_index);
    check_expected(letter_type);
    return 0;
}

s32 effect_64_init(u8 slot, s16 sync_bg, s16 master_player, s16 letter_type, s16 cursor_index, u16 char_offset,
                   s16 pos_index, s16 convert_id, s16 convert_id2) {
    check_expected(slot);
    check_expected(master_player);
    check_expected(cursor_index);
    return 0;
}

s32 effect_57_init(u8 slot, int header_type, s16 unused1, s16 unused2, s16 unused3) {
    check_expected(slot);
    check_expected(header_type);
    return 0;
}

// Mock other dependencies
void push_effect_work(void* work) {}
void* frw[128]; // Simplified mock for frame work pool

static void test_native_ui_basic_begin_end(void **state) {
    NativeUI_Clear();
    NativeUI_Begin(0, 0, UI_DIR_VERTICAL);
    NativeUI_End();
    assert_int_equal(Menu_Cursor_Y[0], 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_native_ui_basic_begin_end),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
