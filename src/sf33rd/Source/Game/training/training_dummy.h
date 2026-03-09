/**
 * @file training_dummy.h
 * @brief Dummy AI Controller for Training Mode.
 */

#ifndef TRAINING_DUMMY_H
#define TRAINING_DUMMY_H

#include "structs.h"
#include "training_prediction.h"
#include "training_state.h"

// Behavior settings
typedef enum {
    DUMMY_BLOCK_NONE = 0,
    DUMMY_BLOCK_ALWAYS,
    DUMMY_BLOCK_FIRST_HIT,
    DUMMY_BLOCK_AFTER_FIRST_HIT,
    DUMMY_BLOCK_RANDOM
} DummyBlockType;

typedef enum {
    DUMMY_PARRY_NONE = 0,
    DUMMY_PARRY_HIGH,
    DUMMY_PARRY_LOW,
    DUMMY_PARRY_ALL,
    DUMMY_PARRY_RED
} DummyParryType;

typedef enum {
    DUMMY_MASH_NONE = 0,
    DUMMY_MASH_FAST, // Optimal mash out
    DUMMY_MASH_NORMAL,
    DUMMY_MASH_RANDOM,
    DUMMY_MASH_LP, // Mash specific button: LP
    DUMMY_MASH_MP, // Mash specific button: MP
    DUMMY_MASH_HP  // Mash specific button: HP
} DummyMashType;

typedef enum { DUMMY_TECH_THROW_NONE = 0, DUMMY_TECH_THROW_ALWAYS, DUMMY_TECH_THROW_RANDOM } DummyTechThrowType;

typedef enum { DUMMY_FAST_WAKEUP_NONE = 0, DUMMY_FAST_WAKEUP_ALWAYS, DUMMY_FAST_WAKEUP_RANDOM } DummyFastWakeupType;

typedef struct {
    DummyBlockType block_type;
    DummyParryType parry_type;
    DummyMashType stun_mash;
    DummyMashType wakeup_mash;
    bool wakeup_reversal;               // Independent SRK/DP on wakeup (not tied to mash)
    bool guard_low_default;             // If true, default guard direction is down-back (blocks lows)
    DummyTechThrowType tech_throw_type; // Throw tech behavior
    DummyFastWakeupType fast_wakeup;    // Fast wakeup (quick rise) behavior

    // Internal state tracking
    bool is_currently_blocking;  // Latched random-block decision per attack string
    bool first_hit_taken;        // For BLOCK_FIRST_HIT: set when dummy first gets hit
    s16 parry_cooldown;          // Frames until next parry attempt allowed
    s16 red_parry_frame_counter; // Counts the 1-frame forward tap for red parry
    s16 reversal_step;           // DP motion frame counter for wakeup reversal
} DummySettings;

extern DummySettings g_dummy_settings;

// Called every frame during input polling to override the dummy's Lever_Buff
void training_dummy_update_input(PLW* wk, s16 dummy_id);
void sync_dummy_settings_from_menu(void);

#endif // TRAINING_DUMMY_H
