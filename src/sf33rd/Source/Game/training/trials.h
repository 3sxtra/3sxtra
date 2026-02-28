#ifndef TRIALS_H
#define TRIALS_H

#include "types.h"
#include <stdbool.h>

// Types of trial requirements
typedef enum {
    TRIAL_REQ_NONE = 0,
    TRIAL_REQ_ATTACK_HIT,   // Normal/Special/Super attack connects
    TRIAL_REQ_THROW_HIT,    // Throw connects
    TRIAL_REQ_FIREBALL_HIT, // Projectile connects
    TRIAL_REQ_ACTIVE_MOVE,  // Player executes active move (Lua 'D'/'J'/'K' type)
    TRIAL_REQ_SPECIAL_COND, // Special conditions (Lua 'U' type, etc.)
    TRIAL_REQ_ANIMATION     // Player or enemy enters specific animation
} TrialRequirementType;

#define MAX_WAZA_ALTERNATIVES 4

// Standard signature of a waza (move)
typedef struct {
    TrialRequirementType type;
    s16 waza_ids[MAX_WAZA_ALTERNATIVES]; // Multiple allowed move/object IDs. 0xFFFF is the end-of-list sentinel.
    const char* display_name;            // String to show in HUD (e.g. "JHK")
    const char* kadai_input;             // Internal input hint notation string (e.g. "_COMMON_EX _SP_RYU4")
} TrialStep;

#define MAX_TRIAL_STEPS 20

typedef struct {
    s16 chara_id;
    s16 difficulty; // 1-10 rating
    s16 gauge_max;  // Boolean/flag for unlimited gauge
    s16 num_steps;
    TrialStep steps[MAX_TRIAL_STEPS];
} TrialDef;

typedef struct {
    s16 chara_id;
    s16 num_trials;
    const TrialDef** trials;
    const char* chara_name;
} TrialCharacterDef;

// Active state of tracking
typedef struct {
    bool is_active;
    s16 current_chara_id;
    s16 current_trial_index;

    s16 current_step;
    bool step_completed_this_frame;
    bool failed;
    bool completed;    // Successfully finished all steps
    s32 success_timer; // Frames since completion message shown

    s32 last_combo_hits; // To detect combo drops
} TrialsState;

extern TrialsState g_trials_state;

void trials_init(void);
void trials_update(void);
void trials_draw(void);

// Engine hooks
void trials_on_attack_hit(s16 attacker_id, s16 kind_of_waza);
void trials_on_throw_hit(s16 attacker_id, s16 kind_of_waza);
void trials_on_fireball_hit(s16 attacker_id, s16 kind_of_waza);
void trials_on_parry(s16 defender_id);

// Navigation
void trials_next(void);
void trials_prev(void);
void trials_select_character(s16 chara_id);
void trials_reset(void);

#endif // TRIALS_H
