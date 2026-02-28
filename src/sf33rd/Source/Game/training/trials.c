#include "sf33rd/Source/Game/training/trials.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/training/training_hud.h"
#include "sf33rd/Source/Game/training/training_state.h"
#include "structs.h"
#include <SDL3/SDL.h>
#include <stdio.h> // for snprintf

/* RmlUi Phase 3 bypass */
#include "port/sdl/rmlui_phase3_toggles.h"
extern bool use_rmlui;

// Include auto-generated data
#include "sf33rd/Source/Game/training/trials_data.inc"

TrialsState g_trials_state = { 0 };

static const TrialCharacterDef* get_char_def(s16 chara_id) {
    for (int i = 0; i < NUM_TRIAL_CHARACTERS; i++) {
        if (g_all_trial_characters[i].chara_id == chara_id) {
            return &g_all_trial_characters[i];
        }
    }
    return NULL;
}

static const TrialDef* get_current_trial_def(void) {
    const TrialCharacterDef* cdef = get_char_def(g_trials_state.current_chara_id);
    if (cdef && g_trials_state.current_trial_index < cdef->num_trials) {
        return cdef->trials[g_trials_state.current_trial_index];
    }
    return NULL;
}

void trials_reset(void) {
    g_trials_state.current_step = 0;
    g_trials_state.failed = false;
    g_trials_state.completed = false;
    g_trials_state.success_timer = 0;

    // Also reset combo tracking so if they are currently hitting it resets
    if (Mode_Type == MODE_TRIALS) {
        g_trials_state.last_combo_hits = g_training_state.p2.combo_hits;
    } else {
        g_trials_state.last_combo_hits = 0;
    }
}

void trials_select_character(s16 chara_id) {
    const TrialCharacterDef* cdef = get_char_def(chara_id);
    if (!cdef)
        return; // Character has no trials

    g_trials_state.current_chara_id = chara_id;
    g_trials_state.current_trial_index = 0;
    trials_reset();
}

void trials_next(void) {
    const TrialCharacterDef* cdef = get_char_def(g_trials_state.current_chara_id);
    if (!cdef)
        return;

    g_trials_state.current_trial_index++;
    if (g_trials_state.current_trial_index >= cdef->num_trials) {
        g_trials_state.current_trial_index = 0;
    }
    trials_reset();
}

void trials_prev(void) {
    const TrialCharacterDef* cdef = get_char_def(g_trials_state.current_chara_id);
    if (!cdef)
        return;

    if (g_trials_state.current_trial_index == 0) {
        g_trials_state.current_trial_index = cdef->num_trials - 1;
    } else {
        g_trials_state.current_trial_index--;
    }
    trials_reset();
}

void trials_init(void) {
    if (Mode_Type != MODE_TRIALS)
        return;

    g_trials_state.is_active = true;

    // Set character based on P1's selection if not already set or invalid
    s16 current_player_char = My_char[0];
    const TrialCharacterDef* cdef = get_char_def(current_player_char);

    if (cdef) {
        if (g_trials_state.current_chara_id != current_player_char) {
            trials_select_character(current_player_char);
        } else {
            trials_reset();
        }
    } else {
        // Fallback to Ryu (always exists)
        trials_select_character(1);
    }
}

static bool match_waza(const TrialStep* step, s16 waza_id) {
    for (int i = 0; i < MAX_WAZA_ALTERNATIVES; i++) {
        if (step->waza_ids[i] == (s16)0xFFFF)
            break; // End of list
        if (step->waza_ids[i] == waza_id)
            return true;
    }
    return false;
}

void trials_update(void) {
    if (Mode_Type != MODE_TRIALS || !g_trials_state.is_active)
        return;

    const TrialDef* cur_trial = get_current_trial_def();
    if (!cur_trial)
        return;

    TrainingPlayerState* p2 = &g_training_state.p2;
    PLW* pl1 = &plw[0];

    s32 current_hits = p2->combo_hits;
    // The engine stores the move that *caused* the damage on the defender's WORK struct
    s16 kow = plw[1].wu.dm_kind_of_waza;

    if (g_trials_state.completed) {
        g_trials_state.success_timer++;
        // Keep tracking hits just to detect combo end for visual reset if desired
        if (current_hits == 0 && g_trials_state.last_combo_hits > 0) {
            // Auto-advance to next trial after completion? Leaving manual for now.
        }
        g_trials_state.last_combo_hits = current_hits;
        return;
    }

    // Handle Gauge Max
    if (cur_trial->gauge_max) {
        if (pl1->sa) {
            pl1->sa->store = pl1->sa->store_max;
            pl1->sa->gauge.s.h = pl1->sa->gauge_len;
        }
    }

    // Detect combo drop (only drop if we're past step 1 and the combo resets natively)
    if (current_hits == 0 && g_trials_state.last_combo_hits > 0) {
        if (g_trials_state.current_step > 0 && g_trials_state.current_step < cur_trial->num_steps) {
            // Give a 1-frame grace period for multi-hit linkage, or fail if it truly dropped
            g_trials_state.failed = true;
            g_trials_state.current_step = 0; // Reset
        }
    }

    // Handle Trial Navigation Inputs (L/R Triggers)
    extern u16 p1sw_0;
    extern u16 p1sw_1;
    u16 edges = ~p1sw_1 & p1sw_0;
    if (edges & (1 << 11)) {
        trials_prev();
        g_trials_state.last_combo_hits = current_hits;
        return;
    }
    if (edges & (1 << 10)) {
        trials_next();
        g_trials_state.last_combo_hits = current_hits;
        return;
    }

    // Detect new hit (ATTACK_HIT, FIREBALL_HIT, THROW_HIT)
    if (current_hits > 0 && current_hits > g_trials_state.last_combo_hits) {
        g_trials_state.failed = false;

        if (g_trials_state.current_step < cur_trial->num_steps) {
            const TrialStep* req = &cur_trial->steps[g_trials_state.current_step];

            if (req->type == TRIAL_REQ_ATTACK_HIT || req->type == TRIAL_REQ_FIREBALL_HIT ||
                req->type == TRIAL_REQ_THROW_HIT) {
                if (match_waza(req, kow)) {
                    g_trials_state.current_step++;
                    g_trials_state.step_completed_this_frame = true;
                } else if (g_trials_state.current_step > 0) { // Only fail if they used a wrong move mid-combo
                    g_trials_state.failed = true;
                    g_trials_state.current_step = 0;
                }
            }
        }
    } else {
        g_trials_state.step_completed_this_frame = false;
    }

    // Check for non-hit conditions (ACTIVE_MOVE)
    if (g_trials_state.current_step < cur_trial->num_steps && !g_trials_state.failed &&
        !g_trials_state.step_completed_this_frame) {
        const TrialStep* req = &cur_trial->steps[g_trials_state.current_step];
        if (req->type == TRIAL_REQ_ACTIVE_MOVE) {
            if (match_waza(req, pl1->wu.kind_of_waza)) { // Active moves use P1's current animating waza
                g_trials_state.current_step++;
                g_trials_state.step_completed_this_frame = true;
            }
        }
    }

    // Check completion
    if (g_trials_state.current_step >= cur_trial->num_steps) {
        g_trials_state.completed = true;
        g_trials_state.success_timer = 0;
    }

    g_trials_state.last_combo_hits = current_hits;
}

void trials_draw(void) {
    if (Mode_Type != MODE_TRIALS || !g_trials_state.is_active)
        return;
    if (use_rmlui && rmlui_screen_trials)
        return;

    const TrialCharacterDef* cdef = get_char_def(g_trials_state.current_chara_id);
    if (!cdef)
        return;

    const TrialDef* cur_trial = get_current_trial_def();
    if (!cur_trial)
        return;

    char buf[128];

    // Draw header
    snprintf(buf,
             sizeof(buf),
             "TRIAL: %s %d/%d (L/R skip)",
             cdef->chara_name,
             g_trials_state.current_trial_index + 1,
             cdef->num_trials);
    SSPutStrPro_Scale(0, 16.0f, 40.0f, 9, 0xFFFFFFFF, (s8*)buf, 1.0f); // White header

    if (cur_trial->gauge_max) {
        SSPutStrPro_Scale(0, 240.0f, 40.0f, 9, 0xFF00FFFF, (s8*)"MAX GAUGE", 1.0f); // Yellow alert
    }

    // Draw steps
    s16 start_y = 100;
    for (int i = 0; i < cur_trial->num_steps; i++) {
        const TrialStep* step = &cur_trial->steps[i];

        u32 color = 0xFFFFFFFF; // default white
        if (i < g_trials_state.current_step) {
            color = 0xFF00FF00; // green (completed)
        } else if (i == g_trials_state.current_step) {
            if (g_trials_state.failed) {
                color = 0xFF0000FF; // red (failed)
            } else {
                color = 0xFF00AACC; // yellow (active)
            }
        } else {
            color = 0xFF888888; // gray (pending)
        }

        SSPutStrPro_Scale(0, 16.0f, (f32)(start_y + (i * 14)), 9, color, (s8*)step->display_name, 1.0f);

        // Draw kadai input on the active step
        if (i == g_trials_state.current_step && step->kadai_input && step->kadai_input[0] != '\0') {
            s16 text_y = start_y + (cur_trial->num_steps * 14) + 15;
            char kadai_buf[256];
            snprintf(kadai_buf, sizeof(kadai_buf), "INPUT: %s", step->kadai_input);
            SSPutStrPro_Scale(0, 16.0f, (f32)text_y, 9, 0xFF00AACC, (s8*)kadai_buf, 1.0f);
        }
    }

    // Draw completion banner
    if (g_trials_state.completed) {
        // Flash color based on timer (white / green)
        u32 flash = (g_trials_state.success_timer / 4) % 2 ? 0xFF00FF00 : 0xFFFFFFFF;
        SSPutStrPro_Scale(0, 150.0f, 150.0f, 9, flash, (s8*)"COMPLETE!", 1.0f);
    }
}

// ----------------------------------------------------------------------------
// Engine Event Hooks
// ----------------------------------------------------------------------------
void trials_on_attack_hit(s16 attacker_id, s16 kind_of_waza) {
    if (Mode_Type != MODE_TRIALS || !g_trials_state.is_active)
        return;
    (void)attacker_id;
    (void)kind_of_waza;
}

void trials_on_throw_hit(s16 attacker_id, s16 kind_of_waza) {
    if (Mode_Type != MODE_TRIALS || !g_trials_state.is_active)
        return;
    (void)attacker_id;
    (void)kind_of_waza;
}

void trials_on_fireball_hit(s16 attacker_id, s16 kind_of_waza) {
    if (Mode_Type != MODE_TRIALS || !g_trials_state.is_active)
        return;
    (void)attacker_id;
    (void)kind_of_waza;
}

void trials_on_parry(s16 defender_id) {
    if (Mode_Type != MODE_TRIALS || !g_trials_state.is_active)
        return;

    // Example: If a trial step requires a parry (not actively used in standard trials
    // outside of specific edge cases, but we have a hook ready).
    const TrialDef* cur_trial = get_current_trial_def();
    if (!cur_trial)
        return;

    if (g_trials_state.current_step < cur_trial->num_steps && !g_trials_state.failed &&
        !g_trials_state.step_completed_this_frame) {
        const TrialStep* step = &cur_trial->steps[g_trials_state.current_step];
        // Lua N001B001B was parry. We check if active move matches 0x001B
        if (step->type == TRIAL_REQ_ACTIVE_MOVE && match_waza(step, 0x001B)) {
            g_trials_state.current_step++;
            g_trials_state.step_completed_this_frame = true;
        }
    }
}

// ─── RmlUi helper accessors ──────────────────────────────────
const char* trials_get_current_char_name(void) {
    const TrialCharacterDef* cdef = get_char_def(g_trials_state.current_chara_id);
    return cdef ? cdef->chara_name : NULL;
}

int trials_get_current_total(void) {
    const TrialCharacterDef* cdef = get_char_def(g_trials_state.current_chara_id);
    return cdef ? cdef->num_trials : 0;
}

bool trials_current_has_gauge_max(void) {
    const TrialCharacterDef* cdef = get_char_def(g_trials_state.current_chara_id);
    if (!cdef || g_trials_state.current_trial_index >= cdef->num_trials)
        return false;
    return cdef->trials[g_trials_state.current_trial_index]->gauge_max != 0;
}
