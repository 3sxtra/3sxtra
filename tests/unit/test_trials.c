#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include "cmocka.h"

#include "sf33rd/Source/Game/training/trials.h"
#include "sf33rd/Source/Game/training/training_state.h"

// --- Mocks ---
u8 My_char[2] = { 0, 0 }; // 0 = Alex
s16 Mode_Type = MODE_TRIALS;
u16 p1sw_0 = 0;
u16 p1sw_1 = 0;
PLW plw[2] = { 0 };

s32 SSPutStrPro_Scale(u16 flag, f32 x, f32 y, u8 atr, u32 vtxcol, s8* str, f32 sc) {
    (void)flag; (void)x; (void)y; (void)atr; (void)vtxcol; (void)str; (void)sc;
    return (s32)x;
}

TrainingGameState g_training_state = { 0 };
// -----------

static void test_trial_navigation(void **state) {
    (void) state;

    My_char[0] = 1; // Alex
    Mode_Type = MODE_TRIALS;
    
    trials_init();
    assert_true(g_trials_state.is_active);
    assert_int_equal(g_trials_state.current_chara_id, 1);
    assert_int_equal(g_trials_state.current_trial_index, 0);

    // Press right bumper (next trial) -> SWK_RIGHT_TRIGGER (1 << 10)
    p1sw_1 = 0;
    p1sw_0 = (1 << 10);
    trials_update();
    
    assert_int_equal(g_trials_state.current_trial_index, 1);
    
    // Press left bumper (prev trial) -> SWK_LEFT_TRIGGER (1 << 11)
    p1sw_1 = 0;
    p1sw_0 = (1 << 11);
    trials_update();
    
    assert_int_equal(g_trials_state.current_trial_index, 0);
}

static void test_trial_validation_flow(void **state) {
    (void) state;

    My_char[0] = 1; // Alex
    trials_init();
    
    // Set up Alex's first trial: SCLP -> CLP -> EX Air Knee Smash
    // Step 0: SCLP (0x0000)
    p1sw_0 = 0;
    p1sw_1 = 0;
    assert_int_equal(g_trials_state.current_step, 0);
    
    g_training_state.p2.combo_hits = 1;
    plw[1].wu.dm_kind_of_waza = 0x0000;
    trials_update();
    
    // Should advance to step 1
    assert_int_equal(g_trials_state.current_step, 1);
    assert_false(g_trials_state.failed);
    
    // Step 1: CLP (0x0012)
    g_training_state.p2.combo_hits = 2;
    plw[1].wu.dm_kind_of_waza = 0x0012; 
    trials_update();
    
    // Should advance to step 2
    assert_int_equal(g_trials_state.current_step, 2);
    assert_false(g_trials_state.failed);

    // Drop combo manually
    g_training_state.p2.combo_hits = 0;
    trials_update();
    
    // Should fail and reset
    assert_true(g_trials_state.failed);
    assert_int_equal(g_trials_state.current_step, 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_trial_navigation),
        cmocka_unit_test(test_trial_validation_flow),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
