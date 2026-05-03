/**
 * @file efff5.c
 * Effect: Visual Effect (Generic)
 */

#include "sf33rd/Source/Game/effect/efff5.h"
#include "common.h"

static void efff5_0000(State_Other* /* unused */);
static void efff5_1000(State_Other* /* unused */);
static void efff5_2000(State_Other* /* unused */);
static void efff5_3000(State_Other* /* unused */);
static void efff5_4000(State_Other* /* unused */);
static void efff5_5000(State_Other* /* unused */);
static void efff5_6000(State_Other* /* unused */);
static void efff5_7000(State_Other* /* unused */);
static void efff5_8000(State_Other* /* unused */);
static void efff5_9000(State_Other* /* unused */);
static void efff5_A000(State_Other* /* unused */);
static void efff5_B000(State_Other* /* unused */);

void effect_F5_move(State_Other* ewk) {
    void (*efff5_jp[12])(State_Other*) = { efff5_0000, efff5_1000, efff5_2000, efff5_3000, efff5_4000, efff5_5000,
                                          efff5_6000, efff5_7000, efff5_8000, efff5_9000, efff5_A000, efff5_B000 };

    efff5_jp[ewk->wu.routine_no[0]](ewk);
}

void efff5_0000(State_Other* /* unused */) {}

void efff5_1000(State_Other* /* unused */) {}

void efff5_2000(State_Other* /* unused */) {}

void efff5_3000(State_Other* /* unused */) {}

void efff5_4000(State_Other* /* unused */) {}

void efff5_5000(State_Other* /* unused */) {}

void efff5_6000(State_Other* /* unused */) {}

void efff5_7000(State_Other* /* unused */) {}

void efff5_8000(State_Other* /* unused */) {}

void efff5_9000(State_Other* /* unused */) {}

void efff5_A000(State_Other* /* unused */) {}

void efff5_B000(State_Other* /* unused */) {}

s32 effect_F5_init(s16 /* unused */) {
    return 0;
}
