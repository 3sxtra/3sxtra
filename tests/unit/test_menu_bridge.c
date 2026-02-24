#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// Undefine conflicting macros from Windows headers
#ifdef cmb2
#undef cmb2
#endif
#ifdef cmb3
#undef cmb3
#endif
#ifdef s_addr
#undef s_addr
#endif
#endif

#include "menu_bridge.h"

// Includes required to define the mocked globals types
#include "game_state.h"
#include "sf33rd/Source/Game/system/work_sys.h"

// === MOCKS FOR LINKING ===
// Used in PreTick
u16 p1sw_0;
u16 p1sw_buff;
u16 p2sw_0;
u16 p2sw_buff;
u16 PLsw[2][2]; 

// Used in PostTick
u8 G_No[4];
u8 S_No[4];
u8 Play_Type;
u8 Play_Game;
u8 My_char[2];
s8 Super_Arts[2]; // note: s8 in game_state.h struct, let's verify type if build fails
s8 Cursor_X[2];
s8 Cursor_Y[2];
s8 ID_of_Face[3][8];

// Note: game_state.h says:
// u8 My_char[2];
// s8 Super_Arts[2];
// s8 Cursor_X[2];
// s8 ID_of_Face[3][8];
// u8 G_No[4];
// u8 Play_Type;
// u8 Play_Game;

// =========================

static void test_struct_packing(void **state) {
    (void) state;
    MenuBridgeState state_obj;
    assert_true(sizeof(state_obj) > 0);
    assert_true(sizeof(state_obj) < 200); 
}

static void test_bridge_init_creates_shm(void **state) {
    (void) state;
    
    #ifdef _WIN32
    MenuBridge_Init(NULL);
    
    // Try to open the mapping created by Init
    HANDLE hMapFile = OpenFileMappingA(
        FILE_MAP_READ,
        FALSE,
        MENU_BRIDGE_SHM_NAME
    );
    
    // This should PASS now that Init is implemented
    assert_non_null(hMapFile);
    
    if (hMapFile) {
        CloseHandle(hMapFile);
    }
    #else
    skip(); 
    #endif
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_struct_packing),
        cmocka_unit_test(test_bridge_init_creates_shm),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
