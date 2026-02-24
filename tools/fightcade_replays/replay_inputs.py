#!/usr/bin/env python3
"""
Replay CPS3-captured inputs into 3SX for visual parity comparison.

Loads a *_full.csv (from replay_input_dumper_v3.lua), navigates 3SX to
gameplay with the correct characters, then injects P1/P2 inputs with
banner-synchronized timing matching the original CPS3 frame alignment.

Usage:
    python tools/replay_inputs.py replaystuff/Akuma_vs_Urien_001_full.csv
    python tools/replay_inputs.py replay.csv --skip-menu
"""

import argparse
import csv
import sys
import time
from pathlib import Path

# Add project root to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from tools.util.bridge_state import (
    GameScreen,
    connect_to_bridge,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LK,
    INPUT_START,
)
from tools.util.navigator import MenuNavigator

# --- Constants ---

# Banner timing (from original parity_replay.py — empirically validated)
BANNER_DURATION_R1 = 142  # FIGHT banner duration for Round 1
BANNER_DURATION_R2_PLUS = 146  # FIGHT banner duration for Round 2+
BANNER_INJECT_WINDOW = 48  # Inject this many frames before is_in_match=1

# CPS3 character ID -> 3SX internal ID
CPS3_TO_3SX_CHAR = {
    0: 0,
    1: 1,
    2: 2,
    3: 3,
    4: 4,
    5: 5,
    6: 6,
    7: 7,
    8: 8,
    9: 9,
    10: 10,
    11: 11,
    12: 12,
    13: 13,
    14: 14,
    15: 16,  # Makoto: CPS3 15 -> 3SX 16
    16: 15,  # Chun-Li: CPS3 16 -> 3SX 15
    17: 17,
    18: 18,
    19: 19,
}

CPS3_CHAR_NAMES = {
    0: "Gill",
    1: "Alex",
    2: "Ryu",
    3: "Yun",
    4: "Dudley",
    5: "Necro",
    6: "Hugo",
    7: "Ibuki",
    8: "Elena",
    9: "Oro",
    10: "Yang",
    11: "Ken",
    12: "Sean",
    13: "Urien",
    14: "Akuma",
    15: "Makoto",
    16: "Chun-Li",
    17: "Q",
    18: "Twelve",
    19: "Remy",
}

CPS3_DEFAULT_SA = {
    0: 0,
    1: 2,
    2: 0,
    3: 2,
    4: 2,
    5: 2,
    6: 0,
    7: 0,
    8: 1,
    9: 1,
    10: 2,
    11: 2,
    12: 2,
    13: 2,
    14: 0,
    15: 0,
    16: 1,
    17: 0,
    18: 0,
    19: 1,
}


# --- CSV Loading ---


def load_csv(csv_path: Path):
    """Load CPS3 CSV. Returns (frames, p1_char, p2_char, p1_sa, p2_sa)."""
    frames = []
    with open(csv_path, "r", newline="") as f:
        for row in csv.DictReader(f):
            frames.append(row)

    if not frames:
        print(f"ERROR: Empty CSV: {csv_path}")
        sys.exit(1)

    # Find gameplay anchor (is_in_match=1) for character/SA IDs
    anchor_idx = 0
    if "is_in_match" in frames[0]:
        for i, row in enumerate(frames):
            if int(row["is_in_match"]) == 1:
                anchor_idx = i
                break

    anchor = frames[anchor_idx]
    p1_char = int(anchor["p1_char"])
    p2_char = int(anchor["p2_char"])
    p1_sa = (
        int(anchor["p1_sa"]) - 1
        if "p1_sa" in anchor
        else CPS3_DEFAULT_SA.get(p1_char, 0)
    )
    p2_sa = (
        int(anchor["p2_sa"]) - 1
        if "p2_sa" in anchor
        else CPS3_DEFAULT_SA.get(p2_char, 0)
    )

    print(f"Loaded {len(frames)} frames from {csv_path.name}")
    print(f"  P1: {CPS3_CHAR_NAMES.get(p1_char, f'ID={p1_char}')} SA{p1_sa + 1}")
    print(f"  P2: {CPS3_CHAR_NAMES.get(p2_char, f'ID={p2_char}')} SA{p2_sa + 1}")
    print(f"  Anchor (is_in_match=1): frame {anchor_idx}")
    return frames, p1_char, p2_char, p1_sa, p2_sa


def find_round_boundaries(frames):
    """Find all is_in_match 0->1 transition indices."""
    starts = []
    prev = 0
    for i, row in enumerate(frames):
        cur = int(row.get("is_in_match", 0))
        if cur == 1 and prev == 0:
            starts.append(i)
        prev = cur
    return starts


def find_injection_start(frames, combat_start_idx):
    """Back up from combat_start by BANNER_INJECT_WINDOW frames.
    These are the last N frames of the FIGHT banner where inputs are buffered."""
    return max(0, combat_start_idx - BANNER_INJECT_WINDOW)


# --- Frame Sync ---


def wait_for_frame_advance(state, timeout=2.0):
    """Block until frame_count changes. Returns new frame_count or -1 on timeout."""
    old = state.frame_count
    deadline = time.time() + timeout
    while time.time() < deadline:
        if state.frame_count != old:
            return state.frame_count
        time.sleep(0.0005)
    return -1


def get_banner_duration(round_num):
    """Return expected banner duration for given round (R1=142, R2+=146)."""
    return BANNER_DURATION_R1 if round_num == 1 else BANNER_DURATION_R2_PLUS


def wait_for_banner_sync(state, round_num, timeout=30.0):
    """Wait for 3SX FIGHT banner and count to the sync point.

    Strategy (same as original parity_replay.py):
      1. Clear stale inputs from SHM
      2. Wait for nav_C_No[0]==1, nav_C_No[1]==4 (FIGHT banner screen)
      3. Record the frame_count at detection (this is banner frame 0)
      4. Wait until frame_count reaches detection + (banner_dur - BANNER_INJECT_WINDOW)
      5. Return — caller starts injecting from CSV[is_in_match - BANNER_INJECT_WINDOW]

    Game loop order per frame:
      PreTick(reads SHM) → game logic(sets C_No) → PostTick(writes frame_count, C_No)
      → step_1(Interrupt_Timer++)

    Returns True if sync succeeded.
    """
    banner_dur = get_banner_duration(round_num)
    frames_to_skip = banner_dur - BANNER_INJECT_WINDOW

    print(f"\n=== BANNER SYNC (Round {round_num}) ===")
    print(f"  Banner: {banner_dur} frames, inject last {BANNER_INJECT_WINDOW}")
    print(f"  Will skip first {frames_to_skip} banner frames")

    # Clear stale inputs from navigation/previous round
    state.p1_input = 0
    state.p2_input = 0

    # Phase 1: Wait for FIGHT banner to start (C_No[0]==1, C_No[1]==4)
    print("  Waiting for FIGHT banner (C_No[0]==1, C_No[1]==4)...")
    deadline = time.time() + timeout
    while time.time() < deadline:
        if state.nav_C_No[0] == 1 and state.nav_C_No[1] == 4:
            break
        time.sleep(0.001)
    else:
        print(
            f"  ERROR: Banner timeout (C_No[0]={state.nav_C_No[0]}, C_No[1]={state.nav_C_No[1]})"
        )
        return False

    # Record detection point. PostTick for this frame already wrote frame_count.
    # This IS banner frame 0. We want to inject starting at banner frame frames_to_skip.
    detect_fc = state.frame_count
    target_fc = detect_fc + frames_to_skip
    print(f"  FIGHT banner detected at fc={detect_fc}, target fc={target_fc}")

    # Phase 2: Wait for frame_count to reach target (absolute comparison avoids off-by-one)
    deadline = time.time() + 10.0
    while time.time() < deadline:
        current_fc = state.frame_count
        if current_fc >= target_fc:
            break
        time.sleep(0.0005)
    else:
        print(
            f"  WARN: Banner frame timeout (fc={state.frame_count}, target={target_fc})"
        )

    actual_skipped = state.frame_count - detect_fc
    print(
        f"  Banner sync complete! Skipped {actual_skipped} frames (target {frames_to_skip})"
    )
    return True


# --- SA Selection ---


def select_super_art(nav, state, player_idx, target_sa):
    """Navigate SA selection using feedback from nav_Cursor_SA."""
    print(f"  P{player_idx + 1}: Selecting SA{target_sa + 1}...")
    send = nav.send_p1_input if player_idx == 0 else nav.send_p2_input

    # Wait for SA cursor to become valid
    for _ in range(30):
        cur_sa = state.nav_Cursor_SA[player_idx]
        if 0 <= cur_sa <= 2:
            break
        time.sleep(0.05)

    # Feedback-driven navigation
    for _ in range(6):
        cur_sa = state.nav_Cursor_SA[player_idx]
        if cur_sa == target_sa:
            print(f"    Reached SA{target_sa + 1}!")
            break
        forward_dist = (target_sa - cur_sa) % 3
        if forward_dist <= 1:
            send(INPUT_DOWN)
        else:
            send(INPUT_UP)
        time.sleep(0.15)
    else:
        print(f"    WARNING: Could not reach SA{target_sa + 1}")

    time.sleep(0.1)
    send(INPUT_LK)
    time.sleep(0.3)


# --- Menu Navigation ---


def navigate_to_gameplay(state, nav, p1_char_cps3, p2_char_cps3, p1_sa, p2_sa):
    """Navigate from wherever we are to gameplay with the right characters + SAs."""
    p1_3sx = CPS3_TO_3SX_CHAR.get(p1_char_cps3, 11)
    p2_3sx = CPS3_TO_3SX_CHAR.get(p2_char_cps3, 2)

    print("\n=== NAVIGATION ===")
    print(
        f"  P1: {CPS3_CHAR_NAMES.get(p1_char_cps3, '?')} SA{p1_sa + 1} (CPS3 {p1_char_cps3} -> 3SX {p1_3sx})"
    )
    print(
        f"  P2: {CPS3_CHAR_NAMES.get(p2_char_cps3, '?')} SA{p2_sa + 1} (CPS3 {p2_char_cps3} -> 3SX {p2_3sx})"
    )

    state.menu_input_active = 1

    # Phase 1: Get to Main Menu
    print("\nPhase 1: Navigate to Main Menu...")
    if not nav.navigate_title_to_menu():
        screen = nav.get_current_screen()
        if screen not in (
            GameScreen.MAIN_MENU,
            GameScreen.CHARACTER_SELECT,
            GameScreen.GAMEPLAY,
        ):
            for _ in range(5):
                nav.send_p1_input(INPUT_START)
                time.sleep(0.5)

    # Phase 2: Select Versus mode
    screen = nav.get_current_screen()
    if screen == GameScreen.MAIN_MENU:
        print("\nPhase 2: Selecting Versus mode...")
        nav.send_p1_input(INPUT_DOWN)
        time.sleep(0.2)
        nav.send_p1_input(INPUT_LK)
        time.sleep(1.0)

    # Phase 3: Wait for Character Select
    print("\nPhase 3: Waiting for Character Select...")
    for _ in range(30):
        screen = nav.get_current_screen()
        if screen == GameScreen.CHARACTER_SELECT:
            break
        if screen == GameScreen.GAMEPLAY:
            state.menu_input_active = 0
            return True
        time.sleep(0.3)
    else:
        print("  ERROR: Never reached Character Select!")
        return False

    time.sleep(0.5)

    # Phase 4-7: Select characters and SAs
    print(f"\nPhase 4: Selecting P1 ({CPS3_CHAR_NAMES.get(p1_char_cps3, '?')})...")
    if not nav.select_character(0, p1_3sx):
        return False
    print("\nPhase 5: P1 Super Art...")
    time.sleep(0.3)
    select_super_art(nav, state, 0, p1_sa)

    print(f"\nPhase 6: Selecting P2 ({CPS3_CHAR_NAMES.get(p2_char_cps3, '?')})...")
    if not nav.select_character(1, p2_3sx):
        return False
    print("\nPhase 7: P2 Super Art...")
    time.sleep(0.3)
    select_super_art(nav, state, 1, p2_sa)

    # Phase 8: Skip pre-match screens
    print("\nPhase 8: Skipping pre-match screens...")
    for _ in range(15):
        screen = nav.get_current_screen()
        if screen == GameScreen.GAMEPLAY:
            break
        nav.send_p1_input(INPUT_LK)
        nav.send_p2_input(INPUT_LK)
        time.sleep(0.3)

    state.menu_input_active = 0
    print("\nNavigation complete!")
    return True


# --- Main Injection Loop ---


def run_replay(csv_path: Path, skip_menu: bool = False):
    """Main replay loop with banner-synchronized injection."""
    frames, p1_char, p2_char, p1_sa, p2_sa = load_csv(csv_path)

    # Find round boundaries (is_in_match 0->1 transitions)
    round_starts = find_round_boundaries(frames)
    if not round_starts:
        print("ERROR: No is_in_match=1 transitions found!")
        sys.exit(1)
    print(
        f"Found {len(round_starts)} round(s), combat starts at CSV frames: {round_starts}"
    )

    # Compute injection start points (BANNER_INJECT_WINDOW frames before each combat start)
    inject_starts = [find_injection_start(frames, rs) for rs in round_starts]
    for r, (inj, cbt) in enumerate(zip(inject_starts, round_starts)):
        print(
            f"  R{r + 1}: inject from CSV[{inj}], combat at CSV[{cbt}] ({cbt - inj} pre-combat frames)"
        )

    # Connect to 3SX
    print("\nConnecting to 3SX...")
    state, shm = connect_to_bridge()
    if state is None:
        print("ERROR: Could not connect to 3SX shared memory.")
        sys.exit(1)
    print(f"Connected! frame_count={state.frame_count}")

    nav = MenuNavigator(state)

    # Navigate to gameplay
    if not skip_menu:
        if not navigate_to_gameplay(state, nav, p1_char, p2_char, p1_sa, p2_sa):
            print("ERROR: Navigation failed!")
            sys.exit(1)
    else:
        print("--skip-menu: Assuming game is approaching round start")

    # --- Find round end boundaries (is_in_match 1->0 transitions) ---
    round_ends = []
    for r_idx, rs in enumerate(round_starts):
        # Search forward from combat start for the first is_in_match=0
        end_idx = len(frames)
        for i in range(rs + 1, len(frames)):
            if int(frames[i].get("is_in_match", 1)) == 0:
                end_idx = i
                break
        round_ends.append(end_idx)
    for r, (inj, cbt, end) in enumerate(zip(inject_starts, round_starts, round_ends)):
        print(f"  R{r + 1}: inject CSV[{inj}..{end}), combat CSV[{cbt}..{end})")

    # --- Inject each round ---
    total_injected = 0
    for r_idx in range(len(round_starts)):
        round_num = r_idx + 1
        inj_start = inject_starts[r_idx]
        combat_start = round_starts[r_idx]
        inj_end = round_ends[r_idx]  # Precise: stops at is_in_match=0
        count = inj_end - inj_start

        # Banner sync: wait for FIGHT banner and count to injection point
        if not wait_for_banner_sync(state, round_num):
            print(f"ERROR: Banner sync failed for Round {round_num}!")
            break

        # Inject frames: write input, then wait for game to consume it
        # Game reads SHM in PreTick (before game logic), so the input we write
        # NOW will be consumed by the NEXT frame's PreTick.
        print(
            f"\n=== ROUND {round_num}: {count} frames (CSV [{inj_start}..{inj_end})) ==="
        )
        state.menu_input_active = 1
        injected = 0
        timeouts = 0

        for i in range(inj_start, inj_end):
            row = frames[i]
            p1_input = int(row["p1_input"])
            p2_input = int(row["p2_input"])

            # Write inputs into SHM — game's next PreTick will read these
            state.p1_input = p1_input
            state.p2_input = p2_input

            # Wait for game to advance one frame (PreTick consumed our input)
            new_fc = wait_for_frame_advance(state, timeout=2.0)
            if new_fc < 0:
                timeouts += 1
                if timeouts >= 5:
                    print(f"  [R{round_num}] Too many timeouts! Aborting.")
                    break
                continue

            injected += 1

            # Debug first 20 frames
            if injected <= 20:
                phase = "BANNER" if i < combat_start else "COMBAT"
                print(
                    f"  [R{round_num}] {phase} frame {injected}: p1=0x{p1_input:04x} p2=0x{p2_input:04x} fc={new_fc}"
                )
            elif injected == 21:
                print(f"  [R{round_num}] (suppressing per-frame output)")

            if injected % 500 == 0:
                print(f"  [R{round_num}] {injected}/{count} frames")

        # Clear inputs
        state.p1_input = 0
        state.p2_input = 0
        state.menu_input_active = 0

        print(
            f"  [R{round_num}] Done: {injected}/{count} frames (timeouts: {timeouts})"
        )
        total_injected += injected

        # Inter-round: wait for game to finish its round transition
        if r_idx + 1 < len(round_starts):
            print(f"\nWaiting for Round {round_num + 1}...")

            # Wait for allow_battle to go to 0 (game's round ended)
            deadline = time.time() + 30.0
            while time.time() < deadline and state.allow_battle == 1:
                time.sleep(0.05)

            print("  Game round ended (allow_battle=0)")
            # Next iteration's wait_for_banner_sync handles the rest

    # Summary
    print("\n=== REPLAY COMPLETE ===")
    print(f"  Rounds: {len(round_starts)}")
    print(f"  Total frames injected: {total_injected}")

    state.menu_input_active = 0
    state.p1_input = 0
    state.p2_input = 0


def main():
    parser = argparse.ArgumentParser(description="Replay CPS3 inputs into 3SX")
    parser.add_argument("csv_path", type=Path, help="Path to CPS3 *_full.csv")
    parser.add_argument(
        "--skip-menu",
        action="store_true",
        help="Skip menu navigation (game must already be at round start)",
    )
    args = parser.parse_args()

    if not args.csv_path.exists():
        print(f"ERROR: CSV not found: {args.csv_path}")
        sys.exit(1)

    try:
        run_replay(args.csv_path, args.skip_menu)
    except KeyboardInterrupt:
        print("\nAborted by user")
        sys.exit(1)


if __name__ == "__main__":
    main()
