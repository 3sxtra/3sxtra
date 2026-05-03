/**
 * @file appear_registry.h
 * @brief Registry for character entrance (appear) animation types.
 *
 * Replaces the hard-coded stack-allocated appear_jmp_tbl[42] in appear.c
 * with a data-driven registry using self-registering delegation wrappers.
 *
 * Each AppearTypeId maps to a single on_tick callback that drives the
 * entrance animation state machine for that type.
 */
#ifndef APPEAR_REGISTRY_H
#define APPEAR_REGISTRY_H

#include "structs.h"

/**
 * @brief Indices into the appear dispatch table.
 *
 * These map 1:1 to the original appear_jmp_tbl[] entries in appear.c.
 * Shared entries (e.g., indices 2, 27, 35, 40 which alias other handlers)
 * are listed as named aliases to preserve the original dispatch semantics.
 */
typedef enum AppearTypeId {
    APPEAR_STANDARD_WALKON = 0, /**< Appear_00000 — instant walk-on               */
    APPEAR_WALK_POSE = 1,       /**< Appear_01000 — walk-on with initial pose      */
    APPEAR_WALK_POSE_ALT = 2,   /**< Appear_01000 alias                            */
    APPEAR_JUMP_IN = 3,         /**< Appear_03000 — jump-in entrance               */
    APPEAR_WALK_FLOURISH = 4,   /**< Appear_04000 — walk-on with flourish          */
    APPEAR_DASH_IN = 5,         /**< Appear_05000 — dash-in entrance               */
    APPEAR_FLY_IN = 6,          /**< Appear_06000 — flying/airborne entrance       */
    APPEAR_VEHICLE = 7,         /**< Appear_07000 — vehicle/ride-in entrance       */
    APPEAR_CHARGE_IN = 8,       /**< Appear_08000 — charge-in entrance             */
    APPEAR_BASKETBALL = 9,      /**< Appear_09000 — Sean's basketball entrance     */
    APPEAR_DRAMATIC_POSE = 10,  /**< Appear_10000 — dramatic pose entrance         */
    APPEAR_CASUAL_WALKON = 11,  /**< Appear_11000 — casual walk-on variant         */
    APPEAR_MULTI_PHASE = 12,    /**< Appear_12000 — multi-phase entrance anim      */
    APPEAR_TAUNT = 13,          /**< Appear_13000 — character taunt entrance       */
    APPEAR_14 = 14,             /**< Appear_14000                                  */
    APPEAR_15 = 15,             /**< Appear_15000                                  */
    APPEAR_16 = 16,             /**< Appear_16000                                  */
    APPEAR_17 = 17,             /**< Appear_17000 — Gill-specific entrance         */
    APPEAR_18 = 18,             /**< Appear_18000                                  */
    APPEAR_19 = 19,             /**< Appear_19000                                  */
    APPEAR_20 = 20,             /**< Appear_20000                                  */
    APPEAR_21 = 21,             /**< Appear_21000                                  */
    APPEAR_22 = 22,             /**< Appear_22000                                  */
    APPEAR_23 = 23,             /**< Appear_23000                                  */
    APPEAR_24 = 24,             /**< Appear_24000                                  */
    APPEAR_25 = 25,             /**< Appear_25000                                  */
    APPEAR_26 = 26,             /**< Appear_26000                                  */
    APPEAR_FLY_IN_ALT1 = 27,    /**< Appear_06000 alias (airborne variant)         */
    APPEAR_28 = 28,             /**< Appear_28000                                  */
    APPEAR_29 = 29,             /**< Appear_29000                                  */
    APPEAR_30 = 30,             /**< Appear_30000                                  */
    APPEAR_31 = 31,             /**< Appear_31000                                  */
    APPEAR_32 = 32,             /**< Appear_32000                                  */
    APPEAR_33 = 33,             /**< Appear_33000                                  */
    APPEAR_34 = 34,             /**< Appear_34000                                  */
    APPEAR_WALK_POSE_ALT2 = 35, /**< Appear_01000 alias                            */
    APPEAR_36 = 36,             /**< Appear_36000                                  */
    APPEAR_37 = 37,             /**< Appear_37000                                  */
    APPEAR_38 = 38,             /**< Appear_38000                                  */
    APPEAR_39 = 39,             /**< Appear_39000                                  */
    APPEAR_FLY_IN_ALT2 = 40,    /**< Appear_06000 alias (Makoto airborne variant)  */
    APPEAR_41 = 41,             /**< Appear_41000 — delayed entrance (Q-specific)  */
    APPEAR_TYPE_COUNT = 42      /**< Sentinel — total entries in dispatch table     */
} AppearTypeId;

/** @brief Callback table for a single appear animation type. */
typedef struct AppearTypeCallbacks {
    void (*on_tick)(PlayerEntity* wk); /**< Per-frame animation driver */
} AppearTypeCallbacks;

/**
 * @brief Register an appear type's callbacks.
 *
 * Called from __attribute__((constructor)) functions in ap_*.c wrappers.
 * Asserts if the slot was already registered (detects duplicates).
 */
void AppearType_Register(AppearTypeId id, const AppearTypeCallbacks* cb);

/**
 * @brief Look up the callbacks for an appear type.
 *
 * @return Pointer to the registered callbacks, or NULL if unregistered.
 */
const AppearTypeCallbacks* AppearType_Get(AppearTypeId id);

#endif /* APPEAR_REGISTRY_H */
