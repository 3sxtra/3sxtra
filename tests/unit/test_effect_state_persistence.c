#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include "cmocka.h"

#include "netplay/netplay.h"
#include "game_state.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "gekkonet.h"

// EffectState, State, save_state, load_state_from_event are now declared in game_state.h

static void test_effect_persistence(void **state) {
    (void) state;

    // 1. Setup initial state
    // frw is uintptr_t, using hex constant
    frw[0][0] = 0xDEADBEEF;
    exec_tm[0] = 42;
    frwctr = 100;

    // 2. Save
    GekkoGameEvent event;
    State saved_state_storage;
    // Initialize storage to 0 to avoid garbage
    memset(&saved_state_storage, 0, sizeof(State));
    
    unsigned int len = sizeof(State);
    uint32_t checksum = 0;

    event.type = GekkoSaveEvent;
    event.data.save.state = (unsigned char*)&saved_state_storage;
    event.data.save.state_len = &len;
    event.data.save.checksum = &checksum;
    event.data.save.frame = 0;

    save_state(&event);

    // 3. Modify globals
    frw[0][0] = 0;
    exec_tm[0] = 0;
    frwctr = 0;

    // 4. Load
    event.type = GekkoLoadEvent;
    event.data.load.state = (unsigned char*)&saved_state_storage;
    event.data.load.state_len = len;
    
    load_state_from_event(&event);

    // 5. Verify
    assert_true(frw[0][0] == 0xDEADBEEF);
    assert_int_equal(exec_tm[0], 42);
    assert_int_equal(frwctr, 100);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_effect_persistence),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
