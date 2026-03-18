/**
 * @file ap_all.c
 * @brief Self-registering delegation wrappers for all 42 appear animation types.
 *
 * Each wrapper delegates on_tick to the original Appear_NNNNN function.
 * Shared entries (aliases) register the same callback under multiple IDs.
 *
 * Uses __attribute__((constructor)) for automatic registration at startup.
 */

#include "port/appear_registry.h"
#include "sf33rd/Source/Game/animation/appear.h"

/* ── Unique callbacks (one per distinct Appear_*) ──────────────── */

static const AppearTypeCallbacks cb_00 = { Appear_00000 };
static const AppearTypeCallbacks cb_01 = { Appear_01000 };
static const AppearTypeCallbacks cb_03 = { Appear_03000 };
static const AppearTypeCallbacks cb_04 = { Appear_04000 };
static const AppearTypeCallbacks cb_05 = { Appear_05000 };
static const AppearTypeCallbacks cb_06 = { Appear_06000 };
static const AppearTypeCallbacks cb_07 = { Appear_07000 };
static const AppearTypeCallbacks cb_08 = { Appear_08000 };
static const AppearTypeCallbacks cb_09 = { Appear_09000 };
static const AppearTypeCallbacks cb_10 = { Appear_10000 };
static const AppearTypeCallbacks cb_11 = { Appear_11000 };
static const AppearTypeCallbacks cb_12 = { Appear_12000 };
static const AppearTypeCallbacks cb_13 = { Appear_13000 };
static const AppearTypeCallbacks cb_14 = { Appear_14000 };
static const AppearTypeCallbacks cb_15 = { Appear_15000 };
static const AppearTypeCallbacks cb_16 = { Appear_16000 };
static const AppearTypeCallbacks cb_17 = { Appear_17000 };
static const AppearTypeCallbacks cb_18 = { Appear_18000 };
static const AppearTypeCallbacks cb_19 = { Appear_19000 };
static const AppearTypeCallbacks cb_20 = { Appear_20000 };
static const AppearTypeCallbacks cb_21 = { Appear_21000 };
static const AppearTypeCallbacks cb_22 = { Appear_22000 };
static const AppearTypeCallbacks cb_23 = { Appear_23000 };
static const AppearTypeCallbacks cb_24 = { Appear_24000 };
static const AppearTypeCallbacks cb_25 = { Appear_25000 };
static const AppearTypeCallbacks cb_26 = { Appear_26000 };
static const AppearTypeCallbacks cb_28 = { Appear_28000 };
static const AppearTypeCallbacks cb_29 = { Appear_29000 };
static const AppearTypeCallbacks cb_30 = { Appear_30000 };
static const AppearTypeCallbacks cb_31 = { Appear_31000 };
static const AppearTypeCallbacks cb_32 = { Appear_32000 };
static const AppearTypeCallbacks cb_33 = { Appear_33000 };
static const AppearTypeCallbacks cb_34 = { Appear_34000 };
static const AppearTypeCallbacks cb_36 = { Appear_36000 };
static const AppearTypeCallbacks cb_37 = { Appear_37000 };
static const AppearTypeCallbacks cb_38 = { Appear_38000 };
static const AppearTypeCallbacks cb_39 = { Appear_39000 };
static const AppearTypeCallbacks cb_41 = { Appear_41000 };

/* ── Registration ──────────────────────────────────────────────── */

__attribute__((constructor))
static void ap_register_all(void) {
    /* Unique entries */
    AppearType_Register(APPEAR_STANDARD_WALKON,    &cb_00);
    AppearType_Register(APPEAR_WALK_POSE,          &cb_01);
    AppearType_Register(APPEAR_JUMP_IN,            &cb_03);
    AppearType_Register(APPEAR_WALK_FLOURISH,      &cb_04);
    AppearType_Register(APPEAR_DASH_IN,            &cb_05);
    AppearType_Register(APPEAR_FLY_IN,             &cb_06);
    AppearType_Register(APPEAR_VEHICLE,            &cb_07);
    AppearType_Register(APPEAR_CHARGE_IN,          &cb_08);
    AppearType_Register(APPEAR_BASKETBALL,         &cb_09);
    AppearType_Register(APPEAR_DRAMATIC_POSE,      &cb_10);
    AppearType_Register(APPEAR_CASUAL_WALKON,      &cb_11);
    AppearType_Register(APPEAR_MULTI_PHASE,        &cb_12);
    AppearType_Register(APPEAR_TAUNT,              &cb_13);
    AppearType_Register(APPEAR_14,                 &cb_14);
    AppearType_Register(APPEAR_15,                 &cb_15);
    AppearType_Register(APPEAR_16,                 &cb_16);
    AppearType_Register(APPEAR_17,                 &cb_17);
    AppearType_Register(APPEAR_18,                 &cb_18);
    AppearType_Register(APPEAR_19,                 &cb_19);
    AppearType_Register(APPEAR_20,                 &cb_20);
    AppearType_Register(APPEAR_21,                 &cb_21);
    AppearType_Register(APPEAR_22,                 &cb_22);
    AppearType_Register(APPEAR_23,                 &cb_23);
    AppearType_Register(APPEAR_24,                 &cb_24);
    AppearType_Register(APPEAR_25,                 &cb_25);
    AppearType_Register(APPEAR_26,                 &cb_26);
    AppearType_Register(APPEAR_28,                 &cb_28);
    AppearType_Register(APPEAR_29,                 &cb_29);
    AppearType_Register(APPEAR_30,                 &cb_30);
    AppearType_Register(APPEAR_31,                 &cb_31);
    AppearType_Register(APPEAR_32,                 &cb_32);
    AppearType_Register(APPEAR_33,                 &cb_33);
    AppearType_Register(APPEAR_34,                 &cb_34);
    AppearType_Register(APPEAR_36,                 &cb_36);
    AppearType_Register(APPEAR_37,                 &cb_37);
    AppearType_Register(APPEAR_38,                 &cb_38);
    AppearType_Register(APPEAR_39,                 &cb_39);
    AppearType_Register(APPEAR_41,                 &cb_41);

    /* Aliases — same handler under multiple IDs */
    AppearType_Register(APPEAR_WALK_POSE_ALT,      &cb_01); /* [2]  → Appear_01000 */
    AppearType_Register(APPEAR_WALK_POSE_ALT2,     &cb_01); /* [35] → Appear_01000 */
    AppearType_Register(APPEAR_FLY_IN_ALT1,        &cb_06); /* [27] → Appear_06000 */
    AppearType_Register(APPEAR_FLY_IN_ALT2,        &cb_06); /* [40] → Appear_06000 */
}
