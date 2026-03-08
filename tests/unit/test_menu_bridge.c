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
#include "sf33rd/Source/Game/engine/spgauge.h"
#include "sf33rd/Source/Game/engine/stun.h"
#include "sf33rd/Source/Game/engine/plcnt.h"

// ==========================================================================
// STUBS FOR LINKING — all globals referenced by menu_bridge.c
// ==========================================================================

// PreTick: input injection
u16 p1sw_0;
u16 p1sw_buff;
u16 p2sw_0;
u16 p2sw_buff;

// PreTick: player control mode (from pulpul.h / slowf.h / workuser.h)
u8 Replay_Status[2];
s8 Operator_Status[2];
PLW plw[2];

// PreTick: RNG (from workuser.h)
s16 Random_ix16;
s16 Random_ix32;
s16 Random_ix16_ex;
s16 Random_ix32_ex;
s16 Random_ix16_com;
s16 Random_ix32_com;
s16 Random_ix16_ex_com;
s16 Random_ix32_ex_com;
s16 Random_ix16_bg;

// PostTick: navigation state (from workuser.h / game_state.h)
u8 G_No[4];
u8 S_No[4];
u8 Play_Type;
u8 Play_Game;
u8 My_char[2];
s8 Super_Arts[2];
s8 Cursor_X[2];
s8 Cursor_Y[2];
s8 ID_of_Face[3][8];
u8 Allow_a_battle_f;
s16 Arts_Y[2];
u8 C_No[4];

// PostTick: timing (from work_sys.h)
u32 Interrupt_Timer;
u16 Game_timer;

// PostTick: battle meters/positions (from spgauge.h, stun.h)
SPG_DAT spg_dat[2];
SDAT sdat[2];

// ==========================================================================

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

/* Task 5: Task 5: StepGate with no active gate.
   Before MenuBridge_Init is called, g_bridge_state is NULL.
   The function must return immediately without spinning/crashing. */
static void test_step_gate_no_active_gate(void **state) {
    (void) state;
    /* Do NOT call MenuBridge_Init() — g_bridge_state stays NULL.
       StepGate checks (!g_bridge_state) first and returns immediately. */
    MenuBridge_StepGate();
    /* If we reach this point the function returned without hanging: pass. */
    assert_true(1);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_struct_packing),
        cmocka_unit_test(test_bridge_init_creates_shm),
        /* Task 5 addition */
        cmocka_unit_test(test_step_gate_no_active_gate),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
