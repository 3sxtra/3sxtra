#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include "cmocka.h"

#include "netplay/netplay.h"
#include "game_state.h"
#include "sf33rd/Source/Game/effect/effect.h"

// The old calculate_checksum was replaced by sectioned checksums.
// This test verifies the combined hash from the sectioned approach.
typedef struct {
    uint32_t plw0;
    uint32_t plw1;
    uint32_t bg;
    uint32_t tasks;
    uint32_t effects;
    uint32_t globals;
    uint32_t combined;
} SectionedChecksum;

extern SectionedChecksum calculate_sectioned_checksums(const State* state);

void test_checksum_consistency(void **state) {
    (void) state;

    State s1;
    memset(&s1, 0, sizeof(State));
    s1.gs.gs_Round_num = 1;

    State s2;
    memset(&s2, 0, sizeof(State));
    s2.gs.gs_Round_num = 1;

    SectionedChecksum sc1 = calculate_sectioned_checksums(&s1);
    SectionedChecksum sc2 = calculate_sectioned_checksums(&s2);

    assert_int_equal(sc1.combined, sc2.combined);
    assert_int_equal(sc1.plw0, sc2.plw0);

    s2.gs.gs_Round_num = 2;
    SectionedChecksum sc3 = calculate_sectioned_checksums(&s2);
    assert_int_not_equal(sc1.combined, sc3.combined);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_checksum_consistency),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
