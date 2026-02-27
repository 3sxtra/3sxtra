/**
 * @file training_state.c
 * @brief Unified training state structure mapped from native engine structs
 *        used by the Dummy AI and Prediction engines.
 */

#include "training_state.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/plmain.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/training/trials.h"
#include <SDL3/SDL.h>

TrainingGameState g_training_state = { 0 };

static void update_player_state(TrainingPlayerState* state, PLW* wk, PLW* opponent_wk) {
    if (!wk)
        return;

    bool prev_is_airborne = state->is_airborne;
    bool prev_is_idle = state->is_idle;
    bool prev_is_attacking = state->is_attacking;
    bool prev_is_blocking = state->is_blocking;

    // Posture mapping
    state->is_standing = (wk->wu.char_state.body.fields.cg_type == 0x0 && wk->wu.xyz[1].disp.pos == 0) ||
                         wk->wu.char_state.body.fields.cg_type == 0x2 || wk->wu.char_state.body.fields.cg_type == 0x6;
    state->is_crouching =
        wk->wu.char_state.body.fields.cg_type == 0x20 || wk->wu.char_state.body.fields.cg_type == 0x21;

    // 20-30 are jump/airborne postures in engine
    state->is_jumping = (wk->wu.char_state.body.fields.cg_type >= 20 && wk->wu.char_state.body.fields.cg_type <= 30);
    state->is_airborne =
        state->is_jumping || (wk->wu.char_state.body.fields.cg_type == 0 && wk->wu.xyz[1].disp.pos != 0);
    state->is_grounded = state->is_standing || state->is_crouching ||
                         (wk->wu.char_state.body.fields.cg_type == 0 && wk->wu.xyz[1].disp.pos == 0);

    // Attacking States
    // 4 = generic attack routine.
    state->is_attacking = (wk->wu.routine_no[1] == 4);
    state->has_just_attacked = (!prev_is_attacking && state->is_attacking);
    // In plpdm.c, every damage frame starts with guard_chuu = 0, then only
    // Damage_04000 (standing guard recoil) and Damage_07000 (air guard recoil)
    // set guard_chuu = guard_kind[...] (a non-zero value).
    // All hitstun handlers leave guard_chuu = 0.
    // Therefore: guard_chuu != 0  ⟺  currently in blockstun.
    state->is_blocking = (wk->guard_chuu != 0);
    state->has_just_blocked = (!prev_is_blocking && state->is_blocking);
    state->has_just_ended_recovery = (prev_is_blocking && !state->is_blocking);

    // Throws
    state->is_being_thrown = wk->tsukamare_f;

    // Stun
    if (wk->py) {
        state->is_stunned = wk->py->flag != 0;
        state->stun_timer = wk->py->time;
    } else {
        state->is_stunned = false;
        state->stun_timer = 0;
    }

    // Combo Tracking relies on training_state_add_combo_hit() called from hitcheck.c
    // We only reset it here.

    state->has_just_landed = (prev_is_airborne && !state->is_airborne && state->is_grounded);

    // We also exclude hit_stop and dm_stop so characters frozen before hitstun don't appear idle.
    // routine_no[1] == 1 is the damage/guard processing routine.
    state->is_idle = (wk->wu.char_state.pat_status <= 3) && !state->is_attacking && !state->is_blocking &&
                     (wk->wu.routine_no[1] != 1) && (wk->wu.hit_stop == 0) && (wk->wu.dm_stop == 0);
    state->has_just_become_idle = (!prev_is_idle && state->is_idle);
    // h_att is always non-null. Check whether any slot has actual width to confirm active hitbox.
    state->has_hitboxes = false;
    if (wk->wu.h_att) {
        for (int _i = 0; _i < 4; _i++) {
            if (wk->wu.h_att->att_box[_i][1] != 0) {
                state->has_hitboxes = true;
                break;
            }
        }
    }

    // When opponent gets put into hit/blockstun during our attack calculation
    if (state->advantage_active && (opponent_wk->wu.routine_no[1] == 1 || opponent_wk->guard_chuu != 0)) {
        state->opponent_was_affected = true;
    }

    // When an attack begins
    if (!prev_is_attacking && state->is_attacking) {
        state->advantage_active = true;
        state->attack_start_frame = g_training_state.frame_number;
        state->hitbox_start_frame = 0;
        state->hitbox_end_frame = 0;
        state->player_idle_frame = 0;

        state->last_startup = 0;
        state->last_active = 0;
        state->last_recovery = 0;
        state->advantage_value = 0;
        state->opponent_idle_frame = 0;
        state->opponent_was_affected = false;
    }

    // ---- Hitbox phase tracking (independent of advantage_active) ----
    // MUST run whenever is_attacking so current_frame_state shows STARTUP/ACTIVE/RECOVERY
    // correctly even when advantage tracking has already resolved or was never active.
    if (state->is_attacking) {
        if (state->has_hitboxes && state->hitbox_start_frame == 0) {
            state->hitbox_start_frame = g_training_state.frame_number;
        }
        if (!state->has_hitboxes && state->hitbox_start_frame != 0 && state->hitbox_end_frame == 0) {
            state->hitbox_end_frame = g_training_state.frame_number;
        }
    }

    // ---- Advantage frame counts (for text display only) ----
    if (state->advantage_active) {
        // Startup count — set once when first frame number is available
        if (state->has_hitboxes && state->last_startup == 0 && state->hitbox_start_frame != 0) {
            state->last_startup = state->hitbox_start_frame - state->attack_start_frame;
        }
        // Active count
        if (!state->has_hitboxes && state->hitbox_start_frame != 0 && state->hitbox_end_frame != 0 &&
            state->last_active == 0) {
            state->last_active = state->hitbox_end_frame - state->hitbox_start_frame;
        }
        // Recovery ends → player can act again
        if ((state->has_just_become_idle || state->has_just_landed) && state->hitbox_start_frame != 0 &&
            state->player_idle_frame == 0) {
            state->player_idle_frame = g_training_state.frame_number;
            if (state->hitbox_end_frame == 0) {
                state->hitbox_end_frame = g_training_state.frame_number;
            }
            if (state->last_active == 0) {
                state->last_active = state->hitbox_end_frame - state->hitbox_start_frame;
            }
            if (state->last_startup == 0) {
                state->last_startup = state->hitbox_start_frame - state->attack_start_frame;
            }
            state->last_recovery = state->player_idle_frame - state->hitbox_end_frame;
        }
    }

    // ======== Component 6: Compute Current Frame State for Frame Meter ========
    // Derived directly from engine each frame — completely independent of advantage tracking.
    // Priority order: Down > Blockstun > Hitstun > Active > Startup/Recovery > Idle
    // IMPORTANT: Blockstun must be checked BEFORE hitstun because the engine also sets
    // cg_type >= 0x40 on blocking characters (they share the damage-state range).
    // The authoritative signal for "currently blocking" is guard_flag == 3 on the PLW.
    u8 cg = wk->wu.char_state.body.fields.cg_type;

    if (cg >= 0x54) {
        // Hard knockdown / down state range (0x54+)
        state->current_frame_state = FRAME_STATE_DOWN;
    } else if (state->is_blocking) {
        // guard_chuu != 0: character is actively in blockstun recoil.
        state->current_frame_state = FRAME_STATE_BLOCKSTUN;
    } else if (wk->wu.routine_no[1] == 1) {
        // routine_no[1] == 1 is the Player_damage global state.
        // It handles both hits and blocks, including the initial impact hitstop freeze frames.
        if (wk->wu.dm_guard_success != -1) {
            state->current_frame_state = FRAME_STATE_BLOCKSTUN;
        } else {
            state->current_frame_state = FRAME_STATE_HITSTUN;
        }
    } else if (wk->py && wk->py->flag != 0) {
        // Stun (dizzy) flag from py structure
        state->current_frame_state = FRAME_STATE_HITSTUN;
    } else if (state->has_hitboxes) {
        // Attack hitbox is out — this is the ACTIVE window
        state->current_frame_state = FRAME_STATE_ACTIVE;
    } else if (state->is_attacking) {
        // In attack routine but no hitbox: either STARTUP (before first hitbox) or RECOVERY (after)
        if (state->hitbox_start_frame != 0) {
            state->current_frame_state = FRAME_STATE_RECOVERY;
        } else {
            state->current_frame_state = FRAME_STATE_STARTUP;
        }
    } else {
        state->current_frame_state = FRAME_STATE_IDLE;
    }

    // Derive is_in_recovery from current_frame_state
    state->is_in_recovery = (state->current_frame_state == FRAME_STATE_RECOVERY);

    // Wakeup time: when in DOWN state, routine_no[3] counts down to 0
    if (state->current_frame_state == FRAME_STATE_DOWN) {
        state->remaining_wakeup_time = wk->wu.routine_no[3];
    } else {
        state->remaining_wakeup_time = 0;
    }

    // Combo Reset (if no longer in hitstun, blockstun, or down, and not being thrown)
    if (state->current_frame_state != FRAME_STATE_HITSTUN && state->current_frame_state != FRAME_STATE_BLOCKSTUN &&
        state->current_frame_state != FRAME_STATE_DOWN && !state->is_being_thrown && wk->wu.routine_no[1] != 1 &&
        wk->wu.hit_stop == 0 && wk->wu.dm_stop == 0) {
        state->combo_stun = 0;
        state->combo_hits = 0;
    }
}

/**
 * @brief Resolve frame-advantage tracking for one player.
 *
 * Called once per frame for each player who has advantage_active set.
 * Determines when both players have returned to idle after an attack
 * and computes the advantage value.
 */
static void resolve_advantage(TrainingPlayerState* self,
                               TrainingPlayerState* opponent,
                               u32 frame, const char* label) {
    if (!self->advantage_active)
        return;

    if (opponent->has_just_become_idle || opponent->has_just_landed) {
        self->opponent_idle_frame = frame;
    }

    if (self->player_idle_frame == 0)
        return;

    // If opponent is already idle, capture the frame now if we haven't
    if (opponent->is_idle && self->opponent_idle_frame == 0) {
        self->opponent_idle_frame = frame;
    }

    if (self->opponent_idle_frame != 0 && self->is_idle && opponent->is_idle) {
        if (self->opponent_was_affected) {
            self->advantage_value =
                self->opponent_idle_frame - self->player_idle_frame;
            SDL_Log("%s ADVANTAGE RESOLVED: %+d (%s idle %d, opp idle %d)",
                    label, self->advantage_value, label,
                    self->player_idle_frame, self->opponent_idle_frame);
        } else {
            self->advantage_value = 0; // Pure whiff
        }
        self->advantage_active = false;
    }
}

void update_training_state(void) {
    g_training_state.is_in_match = true; // Assuming we're in match when this updates
    g_training_state.frame_number++;

    // Map P1
    update_player_state(&g_training_state.p1, &plw[0], &plw[1]);
    // Map P2
    update_player_state(&g_training_state.p2, &plw[1], &plw[0]);

    // Calculate Advantage cross-states
    resolve_advantage(&g_training_state.p1, &g_training_state.p2,
                      g_training_state.frame_number, "P1");
    resolve_advantage(&g_training_state.p2, &g_training_state.p1,
                      g_training_state.frame_number, "P2");

    trials_update();

    static bool printed_offsets = false;
    if (!printed_offsets) {
        printed_offsets = true;
        SDL_Log("WORK cmoa: %zu", offsetof(WORK, cmoa));
        SDL_Log("WORK now_koc: %zu", offsetof(WORK, now_koc));
        SDL_Log("WORK char_state: %zu", offsetof(WORK, char_state));
        SDL_Log("WORK cg_type: %zu", offsetof(WORK, char_state.body.fields.cg_type));
        SDL_Log("WORK cg_number: %zu", offsetof(WORK, char_state.body.fields.cg_number));
        SDL_Log("WORK char_state cgd_type: %zu", offsetof(WORK, char_state.cgd_type));
        SDL_Log("WORK char_state kind_of_waza: %zu", offsetof(WORK, char_state.kind_of_waza));
        SDL_Log("WORK hit_work_id: %zu", offsetof(WORK, hit_work_id));
        SDL_Log("PLW current_attack: %zu", offsetof(PLW, current_attack));
        SDL_Log("PLW waza_flag: %zu", offsetof(WORK_CP, waza_flag));
    }
}

TrainingPlayerState* get_training_player(s16 id) {
    if (id == 0)
        return &g_training_state.p1;
    if (id == 1)
        return &g_training_state.p2;
    return 0;
}

void training_state_add_combo_hit(s16 target_id, s32 added_stun) {
    if ((Mode_Type != MODE_NORMAL_TRAINING && Mode_Type != MODE_TRIALS))
        return;

    TrainingPlayerState* p = get_training_player(target_id);
    if (!p)
        return;

    // Only accumulate if not currently dizzy
    if (!p->is_stunned && added_stun > 0) {
        p->combo_stun += added_stun;
    }
    p->combo_hits += 1;
}
