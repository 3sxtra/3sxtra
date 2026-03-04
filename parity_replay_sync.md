# `parity_replay.py` — Round-Start Synchronisation

## The Problem

CPS3 (original hardware, recorded to CSV) and 3SX (the port) must be at the
**exact same point in the FIGHT banner animation** when input injection begins,
or the fighting logic will diverge from frame 1.

---

## The FIGHT Banner as a Sync Anchor

The FIGHT banner has a **known, fixed duration per round:**

| Round | Banner Duration |
|-------|----------------|
| Round 1 | **142 frames** |
| Round 2+ | **146 frames** |

The extra 4 frames in later rounds come from a longer scroll/wipe animation.

---

## Round 1 Sync

### Step 1 — Find the CSV sync point

- `detect_gameplay_start()` finds the frame in the CSV where `is_in_match=1`
  (hardware-precise signal that combat is active, i.e. the banner is fully gone).
- `find_fight_banner_start()` subtracts `FIGHT_BANNER_DURATION` (120) to
  estimate banner appearance — used as a reference only.
- `BANNER_INJECT_WINDOW = 48`: only the **last 48 frames** before
  `is_in_match=1` are injected. Injecting more would overflow the game's
  ~32–48 frame motion-input buffer.
- So `gameplay_start = anchor_idx - 48`.

### Step 2 — Wait for 3SX to reach the sync point (`wait_for_banner_sync`)

1. Poll `state.nav_C_No[0] == 1 AND state.nav_C_No[1] == 4` — the FIGHT
   banner effect being active.
2. Poll `state.banner_frame_count >= (banner_duration - 48)`.
   For R1: `142 - 48 = 94` frames in.

At this point, 3SX has **94 frames left** in its banner and the CSV cursor is
**48 frames before** `is_in_match=1`. Both systems will hit combat simultaneously.

### Step 3 — Inject

Frame-by-frame inputs are fed in via `inject_frame_and_wait()` starting from
`injection_frames[gameplay_start:]`.

---

## Round 2/3 Sync (inter-round resync)

When the CSV signals `is_in_match=0` after gameplay has occurred:

1. **Find next CSV combat start** — `find_next_round_start()` scans forward
   for the next `is_in_match=1`, then backs up 48 frames → `next_inject_idx`.
2. **Wait for 3SX's next banner** — calls `wait_for_banner_sync(current_round + 1)`,
   now using duration **146**, so it waits until `banner_frame_count >= 98`.
3. **Skip ahead in CSV** — `skip_to_index` jumps the injection cursor forward
   to `next_inject_idx`, discarding KO/win-screen frames that must not be injected.
4. Control flags (`selfplay_onnx_active`, `python_connected`) are re-asserted
   and injection resumes.

---

## Summary Diagram

```
CSV (CPS3 recording):
  ... [intro frames] | [48 banner frames] | [combat: is_in_match=1] ...
                       ^                   ^
                  gameplay_start        anchor_idx

3SX (live):
  ... [banner frame 0 → 93] | [banner frame 94 → 141] | [combat] ...
                               ^
                  wait_for_banner_sync() exits here → injection begins
```

Both systems count down the last **48 frames** of their respective banners
in lockstep, so the first injected input lands on the same logical game frame.
