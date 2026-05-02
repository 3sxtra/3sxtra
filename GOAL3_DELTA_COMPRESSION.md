# Phase 3 Goal 3: Delta-Compressed State Snapshotting

> **Handover document for Antigravity sessions.**
> Written 2026-05-02 after completing Goal 1 (I_System port boundary) and Goal 2 (FSM virtualization).

---

## 1. Problem Statement

The 3SX engine performs **full 18.8 KB memory copies** of the entire `GameState` struct on every frame during netplay rollback. GekkoNet (the rollback library) calls `save_state()` every frame and `load_state()` on every rollback — potentially 8+ rollback frames at 60 fps.

**Current cost per frame**: ~18.8 KB × 2 (save + potential load) = **~37.6 KB/frame** of memcpy bandwidth.

The `GameState_Save()` and `GameState_Load()` functions are 1,854 lines of hand-written `GS_SAVE(member)` / `GS_LOAD(member)` macro expansions — one per global variable. Adding any new global requires editing 3 files in lockstep (`game_state.h`, `game_state.c` save function, `game_state.c` load function). This is fragile, error-prone, and the #1 source of desync bugs.

---

## 2. Current Architecture

### 2.1 Key Files

| File | Lines | Role |
|---|---|---|
| `src/include/game_state.h` | 804 | `GameState` struct definition (all fields, ~18.8 KB on 64-bit) |
| `src/netplay/game_state.c` | 1,854 | `GameState_Save()` and `GameState_Load()` — 1,206 GS_SAVE/GS_LOAD lines |
| `src/netplay/netplay.c` | 1,127 | Rollback lifecycle: `save_state()`, `load_state()`, `advance_game()` |

### 2.2 Data Flow

```
GekkoNet event loop (netplay.c)
  │
  ├─ GekkoSaveEvent  → save_state(event)
  │                      └─ GameState_Save(&state.gs)      // 603 GS_SAVE lines
  │                      └─ EffectState save (inline)
  │                      └─ XXH3_64bits() checksum
  │
  ├─ GekkoLoadEvent  → load_state_from_event(event)
  │                      └─ GameState_Load(&state.gs)      // 603 GS_LOAD lines
  │                      └─ EffectState load (inline)
  │
  └─ GekkoAdvanceEvent → advance_game()
                           └─ step_game() → njUserMain()
```

### 2.3 The `State` Composite

```c
typedef struct State {
    GameState gs;   // ~18.8 KB — all deterministic globals
    EffectState es; // ~variable — effect work queue
} State;
```

GekkoNet allocates a ring buffer of `State` structs internally (configured via `config.state_size = sizeof(State)`).

### 2.4 The GS_SAVE/GS_LOAD Macro Pattern

```c
#define GS_SAVE(member) SDL_memcpy(&dst->member, &member, sizeof(member))
#define GS_LOAD(member) SDL_memcpy(&member, &src->member, sizeof(member))
```

Each global variable is individually memcpy'd between the global scope and the struct. The struct field name **must exactly match** the global variable name. There are 603 such lines in the save function and 603 matching lines in the load function.

### 2.5 Compile-Time Guards

```c
#define EXPECTED_GAME_STATE_SIZE 18808  // 64-bit
_Static_assert(sizeof(GameState) == EXPECTED_GAME_STATE_SIZE, ...);
_Static_assert(sizeof(struct _TASK) == EXPECTED_TASK_SIZE, ...);
```

These fire at compile time if the struct layout changes, preventing silent corruption.

### 2.6 Checksum for Desync Detection

```c
dst->state_checksum = 0;
dst->state_checksum = (u32)XXH3_64bits(dst, sizeof(GameState));
```

The full struct is hashed with XXH3 after save. GekkoNet compares checksums between peers to detect desyncs.

---

## 3. Design Goals

1. **Reduce per-frame bandwidth** from 18.8 KB to ~1-4 KB average (delta only)
2. **Eliminate the GS_SAVE/GS_LOAD maintenance burden** — new globals should be auto-captured
3. **Preserve rollback correctness** — load must restore exact state
4. **Preserve desync detection** — checksum must still work
5. **Don't break GekkoNet's API** — it expects `save_state(event)` / `load_state_from_event(event)` with a flat buffer

---

## 4. Proposed Design: Flat Struct + Delta Compression

### 4.1 Core Idea

Instead of 603 individual `GS_SAVE(member)` calls, do ONE `memcpy` of the entire flat struct, then delta-compress against the previous frame's snapshot.

```
Frame N:
  1. memcpy(snapshot_current, &game_globals, sizeof(GameState))   // flat copy
  2. delta = xor(snapshot_current, snapshot_previous)              // bitwise diff
  3. compressed = rle_encode(delta)                                // run-length encode zeros
  4. store compressed in ring buffer
  5. snapshot_previous = snapshot_current
```

On rollback:
```
  1. compressed = ring_buffer[target_frame]
  2. delta = rle_decode(compressed)
  3. snapshot = xor(snapshot_previous_at_target, delta)
  4. memcpy(&game_globals, snapshot, sizeof(GameState))            // flat restore
```

### 4.2 The Hard Part: Flat Copy

The current architecture stores game state as **603 separate global variables** scattered across dozens of `.c` files. The `GameState` struct mirrors these globals field-by-field. To do a flat memcpy, we have two options:

#### Option A: Keep globals, use single memcpy per field group (incremental)
Group the 603 fields into ~10-20 contiguous memory regions and memcpy each group. Less invasive but still O(N) copies.

#### Option B: Consolidate globals into a single `GameState` instance (ideal)
Move all 603 globals into a single `GameState g_state;` struct. All game code accesses `g_state.Round_num` instead of the bare global `Round_num`. This enables a single `memcpy(&snapshot, &g_state, sizeof(GameState))`.

**Recommendation**: Option A first (can be done incrementally), then migrate toward Option B over time.

### 4.3 Delta Ring Buffer

```c
#define SNAPSHOT_RING_SIZE 16  // Must cover max rollback depth (8) + margin

typedef struct {
    GameState full;           // Full snapshot for this frame
    u32 checksum;             // XXH3 hash for desync detection
    int frame;                // Frame number
} SnapshotEntry;

static SnapshotEntry snapshot_ring[SNAPSHOT_RING_SIZE];
static int ring_head = 0;
```

### 4.4 Delta Encoding (XOR + RLE)

For an 18.8 KB struct where typically only ~500 bytes change per frame (inputs, positions, timers):

```c
// XOR the two snapshots to produce a delta buffer
void delta_xor(const u8* a, const u8* b, u8* out, size_t len) {
    for (size_t i = 0; i < len; i++) out[i] = a[i] ^ b[i];
}

// RLE-encode runs of zeros (the unchanged bytes)
// Format: [0x00, run_length_u16] for zero runs, [byte] for non-zero
size_t rle_encode(const u8* delta, size_t len, u8* out);
size_t rle_decode(const u8* compressed, size_t comp_len, u8* out, size_t out_max);
```

Expected compression: 18.8 KB → ~1-3 KB typical (>90% of bytes unchanged between frames).

---

## 5. Implementation Phases

### Phase 1: Ring Buffer Foundation (LOW RISK)
- Create `src/netplay/state_snapshot.h` and `state_snapshot.c`
- Implement `SnapshotRing` with `Snapshot_Save(frame)` / `Snapshot_Load(frame)`
- Initially: just full memcpy (no delta yet) — functionally identical to current
- Wire into `save_state()` / `load_state_from_event()` in `netplay.c`
- **Verify**: netplay rollback still works identically

### Phase 2: Delta Compression (MEDIUM RISK)
- Implement XOR + RLE delta encoding
- Store deltas in ring buffer instead of full snapshots
- Keep one "keyframe" (full snapshot) every N frames as recovery anchor
- **Verify**: netplay rollback still works, measure bandwidth reduction

### Phase 3: Flat Copy Consolidation (HIGH RISK, HIGH REWARD)
- Replace the 1,206 GS_SAVE/GS_LOAD lines with a flat `memcpy` approach
- This requires careful ordering to match the struct layout exactly
- Remove the macro-per-field pattern
- **Verify**: full regression test, arcade parity check

### Phase 4 (Optional): Global Struct Migration
- Move bare globals into `g_state.member` pattern
- This is the largest change and should be done module-by-module
- Each module gets its own sub-struct within GameState

---

## 6. Risk Assessment

| Risk | Impact | Mitigation |
|---|---|---|
| Silent data corruption from layout mismatch | Desync, crash | Keep `_Static_assert` guards, add byte-level round-trip tests |
| RLE decode overflow | Buffer overrun | Fixed output size = `sizeof(GameState)`, bounds check in decode |
| Rollback depth exceeding ring size | Stale state load | Assert in `Snapshot_Load`, ring size > max rollback window |
| EffectState not covered | Incomplete rollback | EffectState is already separate — keep it as full-copy for now |
| Pointer fields in GameState | Delta XOR breaks on ASLR | Audit struct for pointers — `plw[].wu.as` and task pointers are known; these must be excluded from delta or handled specially |

---

## 7. Key Pointers into the Codebase

### Where GekkoNet calls save/load:
- `src/netplay/netplay.c:684` — `save_state(event)` (GekkoSaveEvent)
- `src/netplay/netplay.c:672` — `load_state_from_event(event)` (GekkoLoadEvent)

### Where GameState is defined:
- `src/include/game_state.h:54` — `typedef struct GameState { ... }`
- `src/include/game_state.h:782` — `typedef struct State { GameState gs; EffectState es; }`

### Where GS_SAVE/GS_LOAD are defined:
- `src/netplay/game_state.c:89` — `#define GS_SAVE(member) SDL_memcpy(&dst->member, &member, sizeof(member))`
- `src/netplay/game_state.c:770` — `#define GS_LOAD(member) SDL_memcpy(&member, &src->member, sizeof(member))`

### Size constants:
- `EXPECTED_GAME_STATE_SIZE` = 18808 (64-bit) / 17228 (32-bit)
- `config.state_size = sizeof(State)` in `configure_gekko()` at `netplay.c:429`

### Checksum:
- `game_state.c:762-763` — XXH3_64bits over entire GameState after save

### Known pointer fields in GameState (DANGER for delta XOR):
- `plw[2]` contains `wu.as` (animation state pointer) — see `game_state.h`
- `task[11]` contains function pointers — saved/loaded wholesale
- These MUST be handled carefully in any delta scheme

---

## 8. Prerequisites

Before starting Goal 3, ensure:
- [x] Goal 1 (I_System port boundary) is committed
- [x] Goal 2 (FSM virtualization) is committed
- [ ] `game_state.c` still has `#include <SDL3/SDL.h>` on line 43 — this should be migrated to `I_System.h` as part of the port boundary work, but it's in `src/netplay/` which is outside the game core
- [ ] Build the engine and confirm netplay rollback works before making changes

---

## 9. Verification Strategy

1. **Unit test**: Round-trip test — `GameState_Save()` then `GameState_Load()`, compare all globals
2. **Delta test**: Save frame N, save frame N+1, delta-encode, delta-decode, verify identical
3. **Netplay test**: Play a full match with rollback, verify no desyncs
4. **Arcade parity**: Run STATCHECK replay comparison before and after
5. **Benchmark**: Measure `save_state()` / `load_state()` time before and after with Tracy zones

---

## 10. Session Startup Checklist

When resuming this work in a new session:

1. Read this document (`GOAL3_DELTA_COMPRESSION.md`)
2. Review `src/include/game_state.h` — understand the struct layout
3. Review `src/netplay/game_state.c` lines 89-764 (save) and 770-1450 (load)
4. Review `src/netplay/netplay.c` lines 660-695 (the GekkoNet event loop)
5. Start with Phase 1 (ring buffer, no delta) — get the plumbing right before optimizing
6. Commit after each phase
