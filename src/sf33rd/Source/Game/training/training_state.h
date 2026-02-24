/**
 * @file training_state.h
 * @brief Unified training state structure mapped from native engine structs
 *        used by the Dummy AI and Prediction engines. Modeled after CPS3 Lua integration.
 */

#ifndef TRAINING_STATE_H
#define TRAINING_STATE_H

#include "structs.h"

typedef struct {
    u16 up : 1;
    u16 down : 1;
    u16 left : 1;
    u16 right : 1;
    u16 lp : 1;
    u16 mp : 1;
    u16 hp : 1;
    u16 lk : 1;
    u16 mk : 1;
    u16 hk : 1;
} InputSet;

typedef enum {
    FRAME_STATE_IDLE = 0,
    FRAME_STATE_STARTUP,
    FRAME_STATE_ACTIVE,
    FRAME_STATE_RECOVERY,
    FRAME_STATE_HITSTUN,
    FRAME_STATE_BLOCKSTUN,
    FRAME_STATE_DOWN
} TrainingFrameState;

typedef struct {
    TrainingFrameState current_frame_state;

    bool is_standing;
    bool is_crouching;
    bool is_jumping;
    bool is_airborne;
    bool is_grounded;

    bool is_attacking;
    bool has_just_attacked;
    bool is_in_recovery;
    bool has_just_ended_recovery;

    bool is_blocking;
    bool has_just_blocked;
    bool has_just_parried;
    bool has_just_red_parried;

    bool is_being_thrown;
    bool has_just_been_thrown;

    // Frame Advantage Tracking
    bool is_idle;
    bool has_hitboxes;
    bool advantage_active;
    s32 attack_start_frame;
    s32 hitbox_start_frame;
    s32 hitbox_end_frame;
    s32 player_idle_frame;
    s32 opponent_idle_frame;

    s32 advantage_value;
    s32 connection_frame;
    s16 last_startup;
    s16 last_active;
    s16 last_recovery;
    bool opponent_was_affected;

    bool has_just_become_idle;
    bool has_just_landed;

    bool is_stunned;
    s16 stun_timer;

    s16 remaining_wakeup_time;
    s16 throw_invulnerability_cooldown;

    // Combo Tracking
    s32 combo_stun;
    s32 combo_hits;

    InputSet pressed;
    InputSet released;
    InputSet down;
} TrainingPlayerState;

typedef struct {
    TrainingPlayerState p1;
    TrainingPlayerState p2;

    s32 frame_number;
    bool is_in_match;
} TrainingGameState;

extern TrainingGameState g_training_state;

void update_training_state(void);
TrainingPlayerState* get_training_player(s16 id);

// Hook for engine to report exact damage/stun on hit
void training_state_add_combo_hit(s16 target_id, s32 added_stun);

#endif // TRAINING_STATE_H
