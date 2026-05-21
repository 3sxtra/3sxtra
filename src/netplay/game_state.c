/**
 * @file game_state.c
 * @brief Save/load all deterministic game globals for netplay rollback.
 *
 * @netplay_sync — Adding a new global? You MUST add it to BOTH functions.
 *
 * GameState_Save() snapshots every global variable listed in GameState (see
 * game_state.h) into a flat struct. GameState_Load() restores them. Together
 * they are the core rollback primitives — GekkoNet calls save on every frame
 * and load on every rollback.
 *
 * The GS_SAVE / GS_LOAD macros rely on the struct field name matching the
 * global variable name exactly (e.g. `GS_SAVE(Random_ix16)` copies the
 * global `Random_ix16` into `dst->Random_ix16`).
 *
 * Fields are organized by source module (// comments) to match game_state.h.
 * If you add a new global that affects the simulation, add the corresponding
 * GS_SAVE and GS_LOAD line in the same section, and add the field to the
 * GameState struct in game_state.h.
 *
 * @see game_state.h for the struct definition and field documentation
 * @see gather_state(), save_state(), load_state() in netplay.c for how these
 *      are called from the rollback lifecycle
 */
#include "game_state.h"
#include "sf33rd/Source/Game/animation/appear.h"
#include "sf33rd/Source/Game/animation/animation_win_player.h"
#include "sf33rd/Source/Game/effect/effect_56_color_bonus.h"
#include "sf33rd/Source/Game/effect/effect_b2_sound_collision.h"
#include "sf33rd/Source/Game/effect/effect_b8_quake.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/engine/super_gauge.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/select_timer.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/round_timer.h"
#include "sf33rd/Source/Game/ui/hud_subroutines.h"

#include <SDL3/SDL.h>
static int battle_start_frame = -1;
#if DEBUG
#define STATE_BUFFER_MAX 20
static RollbackState state_buffer[STATE_BUFFER_MAX];
#endif

#include "netplay.h"
#define Game GekkoGame
#include "gekkonet.h"
#undef Game
#include "sf33rd/utils/djb2_hash.h"
#define XXH_INLINE_ALL
#include "sf33rd/utils/xxhash.h"

#include "main.h"
#include <stdio.h>

// ============================================================================
// Compile-time guard: sizeof(GameState) tripwire.
// If this assert fires, a field was added or removed from GameState.
// Steps: 1) Add the corresponding GS_SAVE/GS_LOAD line in this file.
//        2) Update the constant below to the new sizeof(GameState).
// ============================================================================
#include <stdint.h>

#if UINTPTR_MAX == 0xffffffff
#define EXPECTED_GAME_STATE_SIZE 17228
#define EXPECTED_TASK_SIZE 20
#else
#define EXPECTED_GAME_STATE_SIZE 18808
#define EXPECTED_TASK_SIZE 32
#endif

_Static_assert(sizeof(GameState) == EXPECTED_GAME_STATE_SIZE,
               "sizeof(GameState) changed! Did you add/remove a field in game_state.h? "
               "Update GS_SAVE/GS_LOAD in this file, then set EXPECTED_GAME_STATE_SIZE "
               "to the new sizeof(GameState).");

// Guard the task struct layout specifically — task[11] is saved/loaded wholesale
// via GS_SAVE(task)/GS_LOAD(task), so any size change causes silent corruption.
_Static_assert(sizeof(struct _TASK) == EXPECTED_TASK_SIZE,
               "sizeof(struct _TASK) changed! This struct is saved/loaded wholesale "
               "during netplay rollback. DO NOT change its layout without updating "
               "GameState and verifying rollback compatibility.");

#define GS_SAVE(member) SDL_memcpy(&dst->member, &member, sizeof(member))

void GameState_Save(GameState* dst) {
    if (!dst)
        return;
    SDL_memcpy(dst, &g_state, sizeof(GameState));
}

void GameState_Load(const GameState* src) {
    if (!src)
        return;
    SDL_memcpy(&g_state, src, sizeof(GameState));
}

#if DEBUG
// Per-subsystem checksums for faster desync triage — when a desync fires,
// we can immediately tell which section (player, bg, effects...) diverged.
typedef struct {
    uint32_t plw0;
    uint32_t plw1;
    uint32_t bg;
    uint32_t tasks;
    uint32_t effects;
    uint32_t globals;
    uint32_t combined;
} SectionedChecksum;

static SectionedChecksum saved_section_checksums[STATE_BUFFER_MAX];
static PlayerEntity saved_plw_scratch[STATE_BUFFER_MAX][2];

#endif

#define SDL_copya(dst, src) SDL_memcpy(dst, src, sizeof(src))

/**
 * @brief Snapshot the complete game state into a State struct.
 *
 * @netplay_sync
 * Called by save_state() on every GekkoSaveEvent. Copies both the GameState
 * (via GameState_Save) and the EffectState (effect pool + free list) into dst.
 * This is the "save" half of the rollback save/load cycle.
 */
static void gather_state(RollbackState* dst) {
    // GameState
    GameState* gs = &dst->gs;
    GameState_Save(gs);

    // EffectState
    EffectState* es = &dst->es;
    SDL_copya(es->frw, frw);
    SDL_copya(es->exec_tm, exec_tm);
    SDL_copya(es->frwque, frwque);
    SDL_copya(es->head_ix, head_ix);
    SDL_copya(es->tail_ix, tail_ix);
    es->frwctr = frwctr;
    es->frwctr_min = frwctr_min;
}

/// Zero pointer fields so they don't pollute checksums (ASLR makes them differ).
/// Only ever called on a scratch copy — never on a state Gekko will restore.
static void sanitize_work_pointers(State* w) {
    w->target_adrs = NULL;
    w->hit_adrs = NULL;
    w->dmg_adrs = NULL;
    w->bg_strip_offset = NULL;
    SDL_zeroa(w->char_table);
    w->se_random_table = NULL;
    w->step_xy_table = NULL;
    w->move_xy_table = NULL;
    w->overlap_char_tbl = NULL;
    w->olc_ix_table = NULL;
    w->rival_catch_tbl = NULL;
    w->curr_rca = NULL;
    w->set_char_ad = NULL;
    w->hit_ix_table = NULL;
    w->body_adrs = NULL;
    w->body_hurtbox = NULL;
    w->hand_adrs = NULL;
    w->hand_hurtbox = NULL;
    w->dumm_adrs = NULL;
    w->h_dumm = NULL;
    w->catch_adrs = NULL;
    w->catch_box = NULL;
    w->caught_adrs = NULL;
    w->caught_box = NULL;
    w->attack_adrs = NULL;
    w->attack_hitbox = NULL;
    w->h_eat = NULL;
    w->adjust_adrs = NULL;
    w->pushbox = NULL;
    w->att_ix_table = NULL;
    w->my_effadrs = NULL;
}

/// Mask rendering-only bits/fields from State color fields.
/// - current_colcd, my_col_code: strip 0x2000 player-side palette flag
/// - colcd: fully zeroed (derived from current_colcd by rendering, can differ entirely)
/// - extra_col, extra_col_2: strip 0x2000 palette flag
static void sanitize_work_rendering(State* w) {
    w->current_colcd &= ~0x2000;
    w->my_col_code &= ~0x2000;
    w->colcd = 0; // Rendering-derived, not gameplay state
    w->extra_col &= ~0x2000;
    w->extra_col_2 &= ~0x2000;
}

/// Zero all pointer fields and mask rendering bits in a PlayerEntity struct.
static void sanitize_plw_pointers(PlayerEntity* p) {
    sanitize_work_pointers(&p->wu);
    sanitize_work_rendering(&p->wu);
    p->cp = NULL;
    p->dm_step_tbl = NULL;
    p->as = NULL;
    p->sa = NULL;
    p->py = NULL;
}

#if DEBUG
/// Save state in state buffer.
/// @return Mutable pointer to state as it has been saved.
static RollbackState* note_state(const RollbackState* state, int frame) {
    if (frame < 0) {
        frame += STATE_BUFFER_MAX;
    }

    RollbackState* dst = &state_buffer[frame % STATE_BUFFER_MAX];
    SDL_memcpy(dst, state, sizeof(RollbackState));
    return dst;
}
#endif

/**
 * @brief Save game state for rollback — GekkoNet callback.
 *
 * @netplay_sync
 * Called by GekkoNet on every frame to save the current state. Computes a
 * focused gameplay checksum for desync detection in both Debug and Release.
 * In DEBUG builds, additionally saves per-subsystem checksums and PlayerEntity copies
 * for binary comparison when a desync is detected.
 *
 * The checksum only covers a whitelist of gameplay-critical fields (PlayerEntity after
 * pointer/rendering sanitization, RNG indices, round state, combat flags,
 * slow-motion flags, super gauge, stun). UI-only fields are saved but not
 * checksummed to reduce false positives from rendering-only divergence.
 */
uint32_t save_current_state(void* buffer, int frame) {
    RollbackState* dst = (RollbackState*)buffer;
    gather_state(dst);

    // Activate checksumming from the very first synced frame (not just battle).
    // This catches desyncs during character select, not only during gameplay.
    if (battle_start_frame < 0) {
        battle_start_frame = frame;
        SDL_Log("[P%d] checksumming active from frame %d (g_state.fsm[1]=%d)",
                configuration.netplay.port,
                frame,
                g_state.fsm[1]);
    }

    const bool checksumming_active = battle_start_frame >= 0;

#if DEBUG
    note_state(dst, frame);
#endif

    // Sanitize non-functional data in dst (safe for rollback restore):
    // inactive effect slots, padding arrays, EffectMultiSprite unused tails.
    {
        EffectState* es = &dst->es;
        for (int i = 0; i < EFFECT_MAX; i++) {
            State* w = (State*)es->frw[i];
            if (w->active_flag == 0) {
                s16 before = w->before;
                s16 behind = w->behind;
                s16 myself = w->myself;
                SDL_memset(es->frw[i], 0, sizeof(es->frw[i]));
                w->before = before;
                w->behind = behind;
                w->myself = myself;
            } else {
                SDL_zeroa(w->reserved_bytes);
                State_Other* wo = (State_Other*)w;
                SDL_zeroa(wo->et_free);
            }
        }
#if DEBUG
        note_state(dst, frame);
#endif
    }

    if (checksumming_active) {
        // === Focused gameplay checksum ===
        // Instead of checksumming the full 478KB State and sanitizing ~50 fields,
        // we checksum ONLY gameplay-critical data:
        //   PlayerEntity[2]: copied and sanitized (pointers, rendering, linked-list zeroed)
        //   Globals: explicit whitelist of deterministic fields
        //   Effects, BG, tasks, zanzou: excluded entirely

        // --- Sanitized PlayerEntity copies ---
        static PlayerEntity plw_scratch[2];
        for (int p = 0; p < 2; p++) {
            SDL_memcpy(&plw_scratch[p], &dst->gs.plw[p], sizeof(PlayerEntity));
            sanitize_plw_pointers(&plw_scratch[p]);
            sanitize_work_rendering(&plw_scratch[p].wu);

            // Linked-list indices and timing differ per allocation order
            plw_scratch[p].wu.before = 0;
            plw_scratch[p].wu.behind = 0;
            plw_scratch[p].wu.myself = 0;
            plw_scratch[p].wu.listix = 0;
            plw_scratch[p].wu.timing = 0;

            // Sweep remaining pointer-like values in PlayerEntity.
            // Use fixed uint64_t stride so both 32-bit and 64-bit platforms
            // scan the same bytes and produce identical checksums.
            //
            // Heuristic: zero any 8-byte word that looks like a pointer.
            // On 64-bit: valid userspace addresses are > 0x1000 and typically
            //   < 0x800000000000 (48-bit canonical on x86-64 and AArch64).
            //   We also check that it's above 4GB to avoid false-matching
            //   ordinary integer values packed in the struct.
            // On 32-bit: pointers are 4 bytes, so a uint64_t spanning two
            //   adjacent fields can't reliably detect them — skip the sweep
            //   since sanitize_plw_pointers() already zeroed known pointers.
#if UINTPTR_MAX > 0xFFFFFFFFULL
            uint64_t* words = (uint64_t*)&plw_scratch[p];
            const size_t count = sizeof(PlayerEntity) / sizeof(uint64_t);
            for (size_t i = 0; i < count; i++) {
                uint64_t v = words[i];
                // turbo
                // What: Replace branching pointer check with branchless bitwise mask subtraction.
                // Why: Avoids pipeline branch misprediction stalls in a hot frame-rate loop.
                // Expected Impact: Marginally lowers per-frame simulation timing overhead.
                uint64_t mask = ((v - 0x100000001ULL) < 0x7FFEFFFFFFFFULL) ? 0ULL : ~0ULL;
                words[i] &= mask;
            }
#endif
        }

        // --- Build combined hash from PlayerEntity + whitelisted globals ---
        const GameState* gs = &dst->gs;
        uint32_t h = djb2_init();

        // PlayerEntity (sanitized)
        h = djb2_update_mem(h, (const uint8_t*)&plw_scratch[0], sizeof(PlayerEntity));
        h = djb2_update_mem(h, (const uint8_t*)&plw_scratch[1], sizeof(PlayerEntity));

        // RNG indices
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix16, sizeof(gs->Random_ix16));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix32, sizeof(gs->Random_ix32));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix16_ex, sizeof(gs->Random_ix16_ex));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix32_ex, sizeof(gs->Random_ix32_ex));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix16_com, sizeof(gs->Random_ix16_com));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix32_com, sizeof(gs->Random_ix32_com));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix16_ex_com, sizeof(gs->Random_ix16_ex_com));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix32_ex_com, sizeof(gs->Random_ix32_ex_com));

        // Round/match
        h = djb2_update_mem(h, (const uint8_t*)&gs->Round_num, sizeof(gs->Round_num));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Round_Level, sizeof(gs->Round_Level));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Round_Result, sizeof(gs->Round_Result));
        h = djb2_update_mem(h, (const uint8_t*)&gs->PL_Wins, sizeof(gs->PL_Wins));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Conclusion_Type, sizeof(gs->Conclusion_Type));
        h = djb2_update_mem(h, (const uint8_t*)&gs->win_type, sizeof(gs->win_type));

        // Player identity
        h = djb2_update_mem(h, (const uint8_t*)&gs->My_char, sizeof(gs->My_char));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Super_Arts, sizeof(gs->Super_Arts));

        // Combat flags
        h = djb2_update_mem(h, (const uint8_t*)&gs->Attack_Flag, sizeof(gs->Attack_Flag));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Counter_Attack, sizeof(gs->Counter_Attack));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Guard_Flag, sizeof(gs->Guard_Flag));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Flip_Flag, sizeof(gs->Flip_Flag));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Lie_Flag, sizeof(gs->Lie_Flag));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Attack_Counter, sizeof(gs->Attack_Counter));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Bullet_No, sizeof(gs->Bullet_No));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Bullet_Counter, sizeof(gs->Bullet_Counter));
        h = djb2_update_mem(h, (const uint8_t*)&gs->parry_counter, sizeof(gs->parry_counter));

        // Game flow
        h = djb2_update_mem(h, (const uint8_t*)&gs->Present_Mode, sizeof(gs->Present_Mode));
        h = djb2_update_mem(h, (const uint8_t*)&gs->VS_Stage, sizeof(gs->VS_Stage));

        // Slow motion
        h = djb2_update_mem(h, (const uint8_t*)&gs->slowmo_timer, sizeof(gs->slowmo_timer));
        h = djb2_update_mem(h, (const uint8_t*)&gs->slowmo_flag, sizeof(gs->slowmo_flag));
        h = djb2_update_mem(h, (const uint8_t*)&gs->execute_flag, sizeof(gs->execute_flag));

        // Super gauge / stun
        h = djb2_update_mem(h, (const uint8_t*)&gs->super_arts, sizeof(gs->super_arts));
        h = djb2_update_mem(h, (const uint8_t*)&gs->stun_state, sizeof(gs->stun_state));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Max_vitality, sizeof(gs->Max_vitality));

#if DEBUG
        // Per-section checksums for desync triage (debug-only diagnostic)
        SectionedChecksum sc;
        uint32_t sh;
        sh = djb2_init();
        sh = djb2_update_mem(sh, (const uint8_t*)&plw_scratch[0], sizeof(PlayerEntity));
        sc.plw0 = sh;
        sh = djb2_init();
        sh = djb2_update_mem(sh, (const uint8_t*)&plw_scratch[1], sizeof(PlayerEntity));
        sc.plw1 = sh;
        sc.bg = 0;
        sc.tasks = 0;
        sc.effects = 0;
        sc.combined = h;
        sc.globals = h ^ sc.plw0 ^ sc.plw1;
        saved_section_checksums[frame % STATE_BUFFER_MAX] = sc;
        SDL_memcpy(&saved_plw_scratch[frame % STATE_BUFFER_MAX][0], &plw_scratch[0], sizeof(PlayerEntity));
        SDL_memcpy(&saved_plw_scratch[frame % STATE_BUFFER_MAX][1], &plw_scratch[1], sizeof(PlayerEntity));
#endif
        return h;
    }
    return 0;
}

void save_state(const GekkoGameEvent* event) {
    *event->data.save.state_len = sizeof(RollbackState);
    uint32_t h = save_current_state(event->data.save.state, event->data.save.frame);
    *event->data.save.checksum = h;
}

#if DEBUG
/**
 * @brief Dump desync diagnostic data to the states/ directory.
 *
 * Called from the GekkoDesyncDetected handler in netplay.c. Writes:
 *  1. states/desync_F<frame>.txt — per-section checksums for the desync frame
 *     and a window of surrounding frames from the ring buffer.
 *  2. states/desync_F<frame>_plw0.bin / _plw1.bin — raw sanitized PlayerEntity snapshots
 *     for binary comparison with xxd (or a hex diff tool).
 *  3. states/desync_F<frame>_state.bin — full State snapshot for the frame.
 *
 * All data comes from the static ring buffers populated by save_state() in
 * DEBUG builds (saved_section_checksums, saved_plw_scratch, state_buffer).
 */
void dump_desync_state(int frame, uint32_t local_checksum, uint32_t remote_checksum) {
    const int slot = frame % STATE_BUFFER_MAX;

    // --- 1. Text summary with section checksums ---
    char path[256];
    SDL_snprintf(path, sizeof(path), "states/desync_F%d.txt", frame);
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "=== DESYNC DETECTED ===\n");
        fprintf(f, "Frame:           %d\n", frame);
        fprintf(f, "Local checksum:  0x%08x\n", local_checksum);
        fprintf(f, "Remote checksum: 0x%08x\n", remote_checksum);
        fprintf(f, "STATE_BUFFER_MAX: %d\n", STATE_BUFFER_MAX);
        fprintf(f,
                "sizeof(PlayerEntity): %zu  sizeof(RollbackState): %zu\n\n",
                sizeof(PlayerEntity),
                sizeof(RollbackState));

        fprintf(f, "--- Per-section checksums (ring buffer) ---\n");
        fprintf(f,
                "%8s  %10s  %10s  %10s  %10s  %10s  %10s  %10s\n",
                "frame",
                "combined",
                "plw0",
                "plw1",
                "globals",
                "bg",
                "tasks",
                "effects");

        // Print a window of frames around the desync
        const int window = STATE_BUFFER_MAX;
        for (int i = 0; i < window; i++) {
            int f_idx = (frame - window + 1 + i);
            if (f_idx < 0)
                continue;
            int s = f_idx % STATE_BUFFER_MAX;
            const SectionedChecksum* sc = &saved_section_checksums[s];
            const char* marker = (f_idx == frame) ? " <== DESYNC" : "";
            fprintf(f,
                    "%8d  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x%s\n",
                    f_idx,
                    sc->combined,
                    sc->plw0,
                    sc->plw1,
                    sc->globals,
                    sc->bg,
                    sc->tasks,
                    sc->effects,
                    marker);
        }
        fclose(f);
        SDL_Log("[desync] Wrote section checksums to %s", path);
    } else {
        SDL_Log("[desync] ERROR: Could not open %s for writing", path);
    }

    // --- 2. Binary PlayerEntity dumps for xxd diffing ---
    for (int p = 0; p < 2; p++) {
        SDL_snprintf(path, sizeof(path), "states/desync_F%d_plw%d.bin", frame, p);
        f = fopen(path, "wb");
        if (f) {
            fwrite(&saved_plw_scratch[slot][p], sizeof(PlayerEntity), 1, f);
            fclose(f);
            SDL_Log("[desync] Wrote PlayerEntity[%d] snapshot (%zu bytes) to %s", p, sizeof(PlayerEntity), path);
        }
    }

    // --- 3. Full State binary dump ---
    SDL_snprintf(path, sizeof(path), "states/desync_F%d_state.bin", frame);
    f = fopen(path, "wb");
    if (f) {
        fwrite(&state_buffer[slot], sizeof(RollbackState), 1, f);
        fclose(f);
        SDL_Log("[desync] Wrote full State (%zu bytes) to %s", sizeof(RollbackState), path);
    }
}
#endif

/**
 * @brief Restore game state from a rollback — GekkoNet callback.
 *
 * @netplay_sync
 * Called by GekkoNet when a rollback is needed. Restores all globals from the
 * saved State snapshot: GameState_Load for game globals, then manually restores
 * the effect pool state (frw, frwque, head_ix, tail_ix, frwctr, frwctr_min).
 */
void load_state(const RollbackState* src) {
    // GameState
    const GameState* gs = &src->gs;
    GameState_Load(gs);

    // EffectState
    const EffectState* es = &src->es;
    SDL_copya(frw, es->frw);
    SDL_copya(exec_tm, es->exec_tm);
    SDL_copya(frwque, es->frwque);
    SDL_copya(head_ix, es->head_ix);
    SDL_copya(tail_ix, es->tail_ix);
    frwctr = es->frwctr;
    frwctr_min = es->frwctr_min;
}

void load_state_from_event(const GekkoGameEvent* event) {
    const RollbackState* src = (RollbackState*)event->data.load.state;
    load_state(src);
}
