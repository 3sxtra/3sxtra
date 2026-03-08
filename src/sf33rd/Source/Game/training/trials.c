#include "sf33rd/Source/Game/training/trials.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/training/training_hud.h"
#include "sf33rd/Source/Game/training/training_state.h"
#include "structs.h"
#include <SDL3/SDL.h>
#include <stdio.h> // for snprintf

/* RmlUi Phase 3 bypass */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
extern bool use_rmlui;

// Include auto-generated data (static fallback)
#include "sf33rd/Source/Game/training/trials_data.inc"

// Runtime Lua loader (preferred when available)
#include "port/sdl/rmlui/lua_trials_loader.h"

TrialsState g_trials_state = { 0 };

static const TrialCharacterDef* get_char_def(s16 chara_id) {
    // Prefer runtime-loaded Lua trial data
    int lua_count = 0;
    const TrialCharacterDef* lua_chars = lua_trials_get_characters(&lua_count);
    if (lua_chars) {
        for (int i = 0; i < lua_count; i++) {
            if (lua_chars[i].chara_id == chara_id) {
                return &lua_chars[i];
            }
        }
    }

    // Fallback to static trials_data.inc
    for (int i = 0; i < NUM_TRIAL_CHARACTERS; i++) {
        if (g_all_trial_characters[i].chara_id == chara_id) {
            return &g_all_trial_characters[i];
        }
    }
    return NULL;
}

const TrialDef* trials_get_current_def(void) {
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
    g_trials_state.pending_hit = false;
    g_trials_state.last_hit_type = TRIAL_REQ_NONE;
    g_trials_state.last_hit_waza = -1;
    g_trials_state.combo_drop_grace = 0;

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

    const TrialDef* cur_trial = trials_get_current_def();
    if (!cur_trial)
        return;

    TrainingPlayerState* p2 = &g_training_state.p2;
    PLW* pl1 = &plw[0];
    PLW* pl2 = &plw[1];

    s32 current_hits = p2->combo_hits;

    if (g_trials_state.completed) {
        g_trials_state.success_timer++;
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

    // Detect combo drop with grace period
    // Some trials (SA activations, fireballs) legitimately reset the combo counter
    // We give a 3-frame grace period before declaring a drop
    if (current_hits == 0 && g_trials_state.last_combo_hits > 0) {
        if (g_trials_state.current_step > 0 && g_trials_state.current_step < cur_trial->num_steps) {
            g_trials_state.combo_drop_grace = 3; // Start grace countdown
        }
    }
    if (g_trials_state.combo_drop_grace > 0) {
        g_trials_state.combo_drop_grace--;
        if (current_hits > 0) {
            // Combo resumed within grace period — no drop
            g_trials_state.combo_drop_grace = 0;
        } else if (g_trials_state.combo_drop_grace == 0) {
            // Grace expired with no new hits — real drop
            // But don't fail if the next step is a non-hit type (ACTIVE_MOVE, etc.)
            const TrialStep* next = &cur_trial->steps[g_trials_state.current_step];
            bool next_is_hit = (next->type == TRIAL_REQ_ATTACK_HIT || next->type == TRIAL_REQ_THROW_HIT ||
                                next->type == TRIAL_REQ_FIREBALL_HIT);
            if (next_is_hit) {
                g_trials_state.failed = true;
                g_trials_state.current_step = 0;
            }
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

    g_trials_state.step_completed_this_frame = false;

    // ─── Hit-based step matching (uses pending_hit from engine hooks) ───
    // The waza ID is globally unique across attacks/throws/fireballs so
    // we match any pending hit against any hit-requiring step type.
    if (g_trials_state.pending_hit && !g_trials_state.failed) {
        g_trials_state.pending_hit = false;

        if (g_trials_state.current_step < cur_trial->num_steps) {
            const TrialStep* req = &cur_trial->steps[g_trials_state.current_step];
            bool step_is_hit = (req->type == TRIAL_REQ_ATTACK_HIT || req->type == TRIAL_REQ_THROW_HIT ||
                                req->type == TRIAL_REQ_FIREBALL_HIT);

            if (step_is_hit) {
                if (match_waza(req, g_trials_state.last_hit_waza)) {
                    g_trials_state.current_step++;
                    g_trials_state.step_completed_this_frame = true;
                } else if (g_trials_state.current_step > 0) {
                    // Wrong waza mid-combo — fail and reset
                    g_trials_state.failed = true;
                    g_trials_state.current_step = 0;
                }
            }
            // If the step is not a hit type (ACTIVE_MOVE, ANIMATION…) ignore the
            // pending hit — it will be processed by the non-hit block below.
        }
    }

    // ─── Non-hit conditions: ACTIVE_MOVE, ANIMATION, SPECIAL_COND ───
    if (g_trials_state.current_step < cur_trial->num_steps && !g_trials_state.failed &&
        !g_trials_state.step_completed_this_frame) {
        const TrialStep* req = &cur_trial->steps[g_trials_state.current_step];

        if (req->type == TRIAL_REQ_ACTIVE_MOVE) {
            // Check P1's currently executing move
            if (match_waza(req, pl1->wu.kind_of_waza)) {
                g_trials_state.current_step++;
                g_trials_state.step_completed_this_frame = true;
            }
        } else if (req->type == TRIAL_REQ_ANIMATION) {
            // ANIM2P: check if P2 is in a specific animation state
            // The waza_ids contain both the P1 waza (index 0) and P2 animation IDs
            // We check P1's waza OR P2's animation against any of the stored IDs
            if (match_waza(req, pl1->wu.kind_of_waza) || match_waza(req, pl2->wu.kind_of_waza)) {
                g_trials_state.current_step++;
                g_trials_state.step_completed_this_frame = true;
            }
        } else if (req->type == TRIAL_REQ_SPECIAL_COND) {
            // Special conditions — placeholder for future implementation
            // For now, treat as auto-pass to not block trial progression
            g_trials_state.current_step++;
            g_trials_state.step_completed_this_frame = true;
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

    const TrialDef* cur_trial = trials_get_current_def();
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
// These are called by the engine when hits/throws/fireballs connect.
// They record the hit type and waza ID for trials_update() to consume.
// ----------------------------------------------------------------------------
void trials_on_hit_registered(s16 attacker_id, s16 kind_of_waza) {
    if (Mode_Type != MODE_TRIALS || !g_trials_state.is_active)
        return;
    // Only track P1's hits (attacker player_number == 0)
    if (attacker_id != 0)
        return;

    g_trials_state.last_hit_waza = kind_of_waza;
    g_trials_state.pending_hit = true;
}

void trials_on_parry(s16 defender_id) {
    if (Mode_Type != MODE_TRIALS || !g_trials_state.is_active)
        return;

    // Parry is used by specific trial steps (Ken Trial 8 — parry Chun-Li's SA)
    const TrialDef* cur_trial = trials_get_current_def();
    if (!cur_trial)
        return;

    if (g_trials_state.current_step < cur_trial->num_steps && !g_trials_state.failed &&
        !g_trials_state.step_completed_this_frame) {
        const TrialStep* step = &cur_trial->steps[g_trials_state.current_step];
        // In the Lua, parry was type "J" with waza N001B001B. We handle it as ACTIVE_MOVE.
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
