/**
 * @file training_dummy.c
 * @brief Dummy AI Controller for Training Mode.
 *
 * Injects inputs directly into Lever_Buff[] and waza_flag[] to control the
 * training dummy. Uses the same Lever_Buff bitfield encoding as the native
 * CPU AI (4=left, 8=right, 2=down, 1=up).
 *
 * Parry system notes (from engine analysis):
 *   - cmd_main.c check_10() requires neutral→forward TRANSITION (case 0→1)
 *   - hitcheck.c defense_ground() checks waza_flag[3] (high), waza_flag[4] (low)
 *   - Red parry needs guard_chuu != 0 && guard_chuu < 5 (just_now flag)
 *   - Guard (blocking) uses saishin_lvdir, computed from cp->sw_lvbt from Lever_Buff
 */

#include "training_dummy.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/pls02.h"    /* random_32_com() */
#include "sf33rd/Source/Game/engine/workuser.h" /* Lever_Buff[] */

DummySettings g_dummy_settings = { 0 };

/* ------------------------------------------------------------------ */
/*  Lever_Buff helpers                                                 */
/* ------------------------------------------------------------------ */

static u16 guard_back_lever(PLW* wk) {
    return (wk->wu.rl_waza == 0) ? 0x04 : 0x08;
}

static u16 forward_lever(PLW* wk) {
    return (wk->wu.rl_waza == 0) ? 0x08 : 0x04;
}

static u16 down_forward_lever(PLW* wk) {
    return forward_lever(wk) | 0x02;
}

/* ------------------------------------------------------------------ */
/*  Mash logic                                                         */
/* ------------------------------------------------------------------ */

static void inject_mash(s16 dummy_id, DummyMashType type) {
    if (type == DUMMY_MASH_NONE)
        return;

    u32 rnd = (u32)random_32_com();

    switch (type) {
    case DUMMY_MASH_FAST:
        Lever_Buff[dummy_id] = (rnd & 0x0F) | 0x10 | 0x20;
        break;
    case DUMMY_MASH_NORMAL:
        if (g_training_state.frame_number & 1) {
            Lever_Buff[dummy_id] = (rnd & 0x0F) | 0x10;
        } else {
            Lever_Buff[dummy_id] = (rnd & 0x0F) | 0x20;
        }
        break;
    case DUMMY_MASH_RANDOM:
        Lever_Buff[dummy_id] = (rnd & 0x0F) | ((rnd >> 4) & 0x3F0);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  Wakeup reversal (DP input injection)                               */
/* ------------------------------------------------------------------ */

static bool try_wakeup_reversal(PLW* wk, s16 dummy_id) {
    TrainingPlayerState* dummy = get_training_player(dummy_id);
    if (!dummy)
        return false;
    if (g_dummy_settings.wakeup_mash == DUMMY_MASH_NONE)
        return false;

    if (dummy->current_frame_state != FRAME_STATE_DOWN) {
        g_dummy_settings.reversal_step = 0;
        return false;
    }

    /* Inject DP motion during last 5 frames of getup */
    if (dummy->remaining_wakeup_time > 0 && dummy->remaining_wakeup_time <= 5) {
        u16 fwd = forward_lever(wk);
        u16 dfwd = down_forward_lever(wk);

        switch (g_dummy_settings.reversal_step) {
        case 0:
            Lever_Buff[dummy_id] = fwd;
            g_dummy_settings.reversal_step = 1;
            break;
        case 1:
            Lever_Buff[dummy_id] = 0x02;
            g_dummy_settings.reversal_step = 2;
            break;
        case 2:
            Lever_Buff[dummy_id] = dfwd | 0x10;
            g_dummy_settings.reversal_step = 3;
            break;
        default:
            Lever_Buff[dummy_id] = dfwd | 0x10;
            break;
        }
        return true;
    }

    return false;
}

static void try_wakeup_mash(PLW* wk, s16 dummy_id) {
    TrainingPlayerState* dummy = get_training_player(dummy_id);
    if (!dummy)
        return;

    if (dummy->current_frame_state == FRAME_STATE_DOWN) {
        if (try_wakeup_reversal(wk, dummy_id))
            return;
    }

    if (dummy->current_frame_state == FRAME_STATE_DOWN || dummy->current_frame_state == FRAME_STATE_HITSTUN ||
        dummy->is_in_recovery || dummy->remaining_wakeup_time > 0) {
        inject_mash(dummy_id, g_dummy_settings.wakeup_mash);
    }
}

static void try_stun_mash(s16 dummy_id) {
    TrainingPlayerState* dummy = get_training_player(dummy_id);
    if (!dummy)
        return;

    if (dummy->is_stunned) {
        inject_mash(dummy_id, g_dummy_settings.stun_mash);
    }
}

/* ------------------------------------------------------------------ */
/*  Blocking & Parrying                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Inject a parry into the dummy.
 *
 * The engine's parry system (cmd_main.c check_10) requires:
 *   1. Lever at neutral (sw_lever == 0) for at least 1 frame
 *   2. Then lever at forward direction (sw_lever == w_lvr)
 *
 * We alternate: even frames = neutral, odd frames = forward.
 * We also directly set waza_flag[3] (high) or waza_flag[4] (low) to a
 * high value (0x10) so the hitcheck threshold (grdb) is met.
 */
static void inject_parry(PLW* wk, s16 dummy_id, bool low) {
    /* Alternate neutral/forward to create the transition check_10 needs */
    if (g_training_state.frame_number & 1) {
        /* Forward frame */
        if (low) {
            Lever_Buff[dummy_id] = 0x02; /* Down */
        } else {
            Lever_Buff[dummy_id] = forward_lever(wk);
        }
    } else {
        /* Neutral frame — required before forward for check_10 case 0→1 */
        Lever_Buff[dummy_id] = 0;
    }

    /* Directly set waza_flag to guarantee parry detection in hitcheck.
       Value 0x10 exceeds any grdb threshold. */
    if (wk->cp) {
        if (low) {
            wk->cp->waza_flag[4] = 0x10;
        } else {
            wk->cp->waza_flag[3] = 0x10;
        }
    }
}

/**
 * @brief Inject a red parry into the dummy.
 *
 * Red parry in 3S: while in blockstun (guard_chuu != 0), tap forward
 * to parry the next hit. hitcheck checks just_now = (guard_chuu < 5)
 * and waza_flag[3/4] >= grdb threshold.
 *
 * So we set waza_flag directly to 0x10 while guard_chuu is active.
 */
static void inject_red_parry(PLW* wk, s16 dummy_id, bool low) {
    /* Set the direction lever — forward (or down for low) */
    if (low) {
        Lever_Buff[dummy_id] = 0x02;
    } else {
        Lever_Buff[dummy_id] = forward_lever(wk);
    }

    /* Directly set parry flag to high value to exceed grdb threshold */
    if (wk->cp) {
        if (low) {
            wk->cp->waza_flag[4] = 0x10;
        } else {
            wk->cp->waza_flag[3] = 0x10;
        }
    }
}

static void execute_block_or_parry(PLW* wk, s16 dummy_id) {
    TrainingPlayerState* dummy = get_training_player(dummy_id);
    TrainingPlayerState* opponent = get_training_player(dummy_id == 1 ? 0 : 1);
    if (!dummy || !opponent)
        return;

    /* ----- Reset first_hit_taken when dummy returns to neutral ----- */
    if (g_dummy_settings.block_type == DUMMY_BLOCK_FIRST_HIT) {
        if (dummy->is_idle && !opponent->is_attacking && !opponent->has_just_attacked) {
            g_dummy_settings.first_hit_taken = false;
        }
        if (!g_dummy_settings.first_hit_taken && (dummy->current_frame_state == FRAME_STATE_HITSTUN ||
                                                  dummy->current_frame_state == FRAME_STATE_BLOCKSTUN)) {
            g_dummy_settings.first_hit_taken = true;
        }
    }

    bool should_block = false;
    bool try_parry = false;
    bool try_red_parry = false;
    bool parry_low = false;

    bool is_threat = opponent->is_attacking || opponent->has_just_attacked || dummy->is_blocking ||
                     dummy->is_in_recovery || dummy->has_just_blocked;

    /* 1. Determine Blocking */
    switch (g_dummy_settings.block_type) {
    case DUMMY_BLOCK_ALWAYS:
        if (is_threat)
            should_block = true;
        break;

    case DUMMY_BLOCK_FIRST_HIT:
        if (g_dummy_settings.first_hit_taken && is_threat) {
            should_block = true;
        }
        break;

    case DUMMY_BLOCK_RANDOM:
        if (opponent->has_just_attacked && !dummy->is_blocking && !dummy->is_in_recovery) {
            g_dummy_settings.is_currently_blocking = (random_32_com() & 1) != 0;
        }
        if (is_threat && g_dummy_settings.is_currently_blocking) {
            should_block = true;
        }
        break;

    default:
        break;
    }

    /* 2. Determine Parrying */
    switch (g_dummy_settings.parry_type) {
    case DUMMY_PARRY_HIGH:
        if (is_threat) {
            try_parry = true;
            parry_low = false;
        }
        break;
    case DUMMY_PARRY_LOW:
        if (is_threat) {
            try_parry = true;
            parry_low = true;
        }
        break;
    case DUMMY_PARRY_ALL:
        if (is_threat) {
            try_parry = true;
            parry_low = opponent->is_crouching;
        }
        break;
    case DUMMY_PARRY_RED:
        /*
         * Red parry: block the first hit, then parry subsequent hits.
         *
         * guard_chuu != 0 means we're in blockstun (engine field).
         * When guard_chuu is active and < 5, that's "just_now" — the
         * window where red parry is checked by hitcheck.c.
         *
         * Strategy:
         *   - If NOT yet blocking: hold back to block the first hit
         *   - If IN blockstun (guard_chuu active): inject red parry
         */
        if (wk->guard_chuu != 0) {
            /* Currently in blockstun — inject red parry for next hit */
            try_red_parry = true;
            parry_low = opponent->is_crouching;
        } else if (is_threat) {
            /* Not in blockstun yet — block the first hit */
            should_block = true;
        }
        break;
    default:
        break;
    }

    /* 3. Inject Inputs — parry takes priority over block */
    if (try_red_parry) {
        inject_red_parry(wk, dummy_id, parry_low);
    } else if (try_parry) {
        inject_parry(wk, dummy_id, parry_low);
    } else if (should_block) {
        u16 back = guard_back_lever(wk);
        if (opponent->is_crouching) {
            Lever_Buff[dummy_id] = back | 0x02; /* Down-Back */
        } else {
            Lever_Buff[dummy_id] = back; /* Back */
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Main entry point                                                   */
/* ------------------------------------------------------------------ */

void training_dummy_update_input(PLW* wk, s16 dummy_id) {
    if (!wk)
        return;

    /* Mash takes full control of Lever_Buff when active */
    try_stun_mash(dummy_id);
    try_wakeup_mash(wk, dummy_id);

    /* Block/parry only modifies if mash didn't already take over */
    execute_block_or_parry(wk, dummy_id);
}
