#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include "cmocka.h"

#include "state_snapshot.h"
#include "game_state.h"

/**
 * Test: Init zeros the ring buffer.
 */
static void test_init_clears_ring(void **state) {
    (void)state;

    Snapshot_Init();
    assert_int_equal(Snapshot_GetValidCount(), 0);

    GameState out_gs;
    /* All frames should fail (return -1) after init */
    assert_int_not_equal(Snapshot_Get(0, &out_gs, NULL), 0);
    assert_int_not_equal(Snapshot_Get(1, &out_gs, NULL), 0);
    assert_int_not_equal(Snapshot_Get(SNAPSHOT_RING_SIZE - 1, &out_gs, NULL), 0);
}

/**
 * Test: Save then Get round-trips a GameState.
 */
static void test_save_get_roundtrip(void **state) {
    (void)state;

    Snapshot_Init();

    /* Set known globals via g_state */
    g_state.Round_num = 3;
    g_state.fsm[0] = 7;
    g_state.fsm[1] = 2;
    g_state.My_char[0] = 5;
    g_state.My_char[1] = 10;

    /* Save via GameState_Save (as production code does) */
    GameState gs;
    GameState_Save(&gs);

    /* Feed into snapshot ring — (frame, state, checksum) */
    Snapshot_SaveFromState(0, &gs, gs.state_checksum);

    assert_int_equal(Snapshot_GetValidCount(), 1);

    /* Retrieve and verify */
    GameState out_gs;
    u32 out_chk = 0;
    assert_int_equal(Snapshot_Get(0, &out_gs, &out_chk), 0);
    assert_int_equal(out_chk, gs.state_checksum);
    assert_int_equal(out_gs.Round_num, 3);
    assert_int_equal(out_gs.fsm[0], 7);
    assert_int_equal(out_gs.fsm[1], 2);
    assert_int_equal(out_gs.My_char[0], 5);
    assert_int_equal(out_gs.My_char[1], 10);
}

/**
 * Test: Multiple frames fill the ring sequentially, testing keyframes and deltas.
 */
static void test_multiple_frames(void **state) {
    (void)state;

    Snapshot_Init();

    GameState gs;

    for (int f = 0; f < 8; f++) {
        g_state.Round_num = (u8)f;
        GameState_Save(&gs);
        Snapshot_SaveFromState(f, &gs, gs.state_checksum);
    }

    assert_int_equal(Snapshot_GetValidCount(), 8);

    /* Verify each frame's snapshot is retrievable.
       Frames 1-7 will be reconstructed via delta XOR & RLE! */
    GameState out_gs;
    for (int f = 0; f < 8; f++) {
        assert_int_equal(Snapshot_Get(f, &out_gs, NULL), 0);
        assert_int_equal(out_gs.Round_num, (u8)f);
    }
}

/**
 * Test: Ring wraparound overwrites old entries.
 * When we save more than SNAPSHOT_RING_SIZE frames, the oldest slots
 * get overwritten and are no longer retrievable by their old frame number.
 */
static void test_ring_wraparound(void **state) {
    (void)state;

    Snapshot_Init();

    GameState gs;

    /* Fill the entire ring plus 4 more frames */
    const int total_frames = SNAPSHOT_RING_SIZE + 4;
    for (int f = 0; f < total_frames; f++) {
        g_state.Round_num = (u8)(f & 0xFF);
        GameState_Save(&gs);
        Snapshot_SaveFromState(f, &gs, gs.state_checksum);
    }

    /* Valid count should cap at SNAPSHOT_RING_SIZE */
    assert_int_equal(Snapshot_GetValidCount(), SNAPSHOT_RING_SIZE);

    GameState out_gs;
    /* Old frames (0-3) should be gone — their slots were overwritten.
       Frames 4-7 may also be orphaned if their keyframe (0) was overwritten. */
    for (int f = 0; f < 4; f++) {
        assert_int_not_equal(Snapshot_Get(f, &out_gs, NULL), 0);
    }

    /* Recent frames (total_frames - SNAPSHOT_RING_SIZE through total_frames-1)
       should be retrievable */
    for (int f = total_frames - SNAPSHOT_RING_SIZE + 8; f < total_frames; f++) {
        assert_int_equal(Snapshot_Get(f, &out_gs, NULL), 0);
        assert_int_equal(out_gs.Round_num, (u8)(f & 0xFF));
    }
}

/**
 * Test: Get with negative frame returns failure.
 */
static void test_negative_frame(void **state) {
    (void)state;

    Snapshot_Init();
    GameState out_gs;
    assert_int_not_equal(Snapshot_Get(-1, &out_gs, NULL), 0);
    assert_int_not_equal(Snapshot_Get(-100, &out_gs, NULL), 0);
}

/**
 * Test: Checksum field is correctly propagated from GameState.
 */
static void test_checksum_propagation(void **state) {
    (void)state;

    Snapshot_Init();

    /* Clear g_state so GameState_Save produces a deterministic checksum */
    memset(&g_state, 0, sizeof(GameState));

    GameState gs;
    GameState_Save(&gs);

    Snapshot_SaveFromState(42, &gs, gs.state_checksum);

    GameState out_gs;
    u32 out_chk = 0;
    assert_int_equal(Snapshot_Get(42, &out_gs, &out_chk), 0);
    assert_int_equal(out_chk, gs.state_checksum);
}

/**
 * Test: Full restore cycle — save to ring, mutate globals, restore from ring.
 * This tests that the ring buffer stores a complete, restorable GameState.
 */
static void test_full_restore_via_ring(void **state) {
    (void)state;

    Snapshot_Init();

    /* Set known values via g_state */
    g_state.Round_num = 7;
    g_state.fsm[0] = 11;
    g_state.fsm[1] = 22;
    g_state.fsm[2] = 33;
    g_state.fsm[3] = 44;
    g_state.plw[0].wu.position_x = 12345;
    g_state.plw[1].wu.position_x = 30000;
    g_state.Mode_Type = MODE_VERSUS;

    /* Save to GameState, then to ring */
    GameState gs;
    GameState_Save(&gs);
    Snapshot_SaveFromState(100, &gs, gs.state_checksum);

    /* Mutate all globals */
    g_state.Round_num = 0;
    g_state.fsm[0] = 0;
    g_state.fsm[1] = 0;
    g_state.plw[0].wu.position_x = 0;
    g_state.plw[1].wu.position_x = 0;
    g_state.Mode_Type = MODE_ARCADE;

    /* Retrieve from ring and restore */
    GameState out_gs;
    assert_int_equal(Snapshot_Get(100, &out_gs, NULL), 0);
    GameState_Load(&out_gs);

    /* Verify all globals restored */
    assert_int_equal(g_state.Round_num, 7);
    assert_int_equal(g_state.fsm[0], 11);
    assert_int_equal(g_state.fsm[1], 22);
    assert_int_equal(g_state.fsm[2], 33);
    assert_int_equal(g_state.fsm[3], 44);
    assert_int_equal(g_state.plw[0].wu.position_x, 12345);
    assert_int_equal(g_state.plw[1].wu.position_x, 30000);
    assert_int_equal(g_state.Mode_Type, MODE_VERSUS);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_clears_ring),
        cmocka_unit_test(test_save_get_roundtrip),
        cmocka_unit_test(test_multiple_frames),
        cmocka_unit_test(test_ring_wraparound),
        cmocka_unit_test(test_negative_frame),
        cmocka_unit_test(test_checksum_propagation),
        cmocka_unit_test(test_full_restore_via_ring),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
