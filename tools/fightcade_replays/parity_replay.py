#!/usr/bin/env python3
"""
CPS3 Parity Testing: Replay captured CPS3 inputs in 3SX and compare game states.

Usage:
    python parity_replay.py <cps3_full.csv>

This script:
1. Connects to 3SX via shared memory
2. Automatically navigates to VS mode with correct characters (detected from CSV)
3. Waits for round start (timer=99)
4. Injects P1/P2 inputs frame-by-frame from the captured CSV
5. Logs 3SX game state to a parallel CSV (*_3sx.csv)
6. Reports divergences in real-time

The CSV format expected:
frame,p1_input,p2_input,timer,p1_hp,p2_hp,p1_x,p1_y,p2_x,p2_y,...,p1_char,p2_char
"""

import argparse
import csv
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

# Add project root to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from tools.util.bridge_state import (
    MenuBridgeState,
    GameScreen,
    connect_to_bridge,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LK,
    INPUT_START,
)
from tools.util.navigator import MenuNavigator

# CPS3 character ID mapping (from replay_input_dumper_v3.lua - empirically validated)
# VERIFIED: 11=Ken, 16=ChunLi from Fightcade replay testing
CPS3_CHAR_NAMES = {
    0: "GILD",  # Gill
    1: "ALEX",  # Alex
    2: "RUID",  # Ryu
    3: "YUNG",  # Yun
    4: "DDON",  # Dudley
    5: "NEKO",  # Necro
    6: "HUGO",  # Hugo
    7: "IBAN",  # Ibuki
    8: "ELNN",  # Elena
    9: "OROO",  # Oro
    10: "KONG",  # Yang
    11: "KENN",  # Ken (VERIFIED)
    12: "SEAN",  # Sean
    13: "UTAN",  # Urien
    14: "GOKI",  # Akuma
    15: "MAKE",  # Makoto
    16: "CHUN",  # Chun-Li (VERIFIED)
    17: "QQQQ",  # Q
    18: "TWEL",  # Twelve
    19: "REMI",  # Remy
}

# Map CPS3 character IDs to 3SX internal IDs
# 3SX uses: 0=Gill, 1=Alex, 2=Ryu, 3=Yun, 4=Dudley, 5=Necro, 6=Hugo,
#           7=Ibuki, 8=Elena, 9=Oro, 10=Yang, 11=Ken, 12=Sean, 13=Urien,
#           14=Akuma, 15=ChunLi, 16=Makoto, 17=Q, 18=Twelve, 19=Remy
CPS3_TO_3SX_CHAR = {
    0: 0,  # Gill -> 0
    1: 1,  # Alex -> 1
    2: 2,  # Ryu -> 2
    3: 3,  # Yun -> 3
    4: 4,  # Dudley -> 4
    5: 5,  # Necro -> 5
    6: 6,  # Hugo -> 6
    7: 7,  # Ibuki -> 7
    8: 8,  # Elena -> 8
    9: 9,  # Oro -> 9
    10: 10,  # Yang -> 10
    11: 11,  # Ken -> 11
    12: 12,  # Sean -> 12
    13: 13,  # Urien -> 13
    14: 14,  # Akuma -> 14
    15: 16,  # Makoto -> 16 (CPS3 15 -> 3SX 16)
    16: 15,  # Chun-Li -> 15 (CPS3 16 -> 3SX 15)
    17: 17,  # Q -> 17
    18: 18,  # Twelve -> 18
    19: 19,  # Remy -> 19
}

# Default Super Art per character (0=SA1, 1=SA2, 2=SA3)
# Based on competitive meta / most commonly used
# Indexed by CPS3 character ID
CPS3_DEFAULT_SA = {
    0: 0,  # Gill - SA1 (doesn't matter)
    1: 2,  # Alex - SA3 (Stun Gun Headbutt)
    2: 0,  # Ryu - SA1 (Shinkuu Hadouken) or SA2
    3: 2,  # Yun - SA3 (Genei Jin)
    4: 2,  # Dudley - SA3 (Corkscrew Blow)
    5: 2,  # Necro - SA3 (Electric Snake)
    6: 0,  # Hugo - SA1 (Gigas Breaker)
    7: 0,  # Ibuki - SA1 (Kasumi Suzaku)
    8: 1,  # Elena - SA2 (Brave Dance)
    9: 1,  # Oro - SA2 (Yagyou Dama)
    10: 2,  # Yang - SA3 (Seiei Enbu)
    11: 2,  # Ken - SA3 (Shippu Jinrai Kyaku)
    12: 2,  # Sean - SA3 (Hyper Tornado)
    13: 2,  # Urien - SA3 (Aegis Reflector)
    14: 0,  # Akuma - SA1 (Messatsu Gou Hadou)
    15: 0,  # Makoto - SA1 (Seichusen Godanzuki) or SA2
    16: 1,  # Chun-Li - SA2 (Houyoku Sen)
    17: 0,  # Q - SA1 (Critical Combo Attack)
    18: 0,  # Twelve - SA1 (X.N.D.L.)
    19: 1,  # Remy - SA2 (Supreme Rising Rage Flash)
}

# FIGHT banner duration in frames (from 3SX effb2.c analysis):
# - case 0-1: 3 frames (init + hit_stop countdown)
# - case 2: 7 frames (size 0→63 by +10)
# - case 3: 65 frames (hit_stop = 64 countdown)
# - case 4: 11 frames (hit_stop = 10 countdown)
# - case 5: 11 frames (size 0→63 by +6)
# - case 6: ~1 frame (wait for rf_b2_flag from effL5)
# - case 7: ~24 frames (fight_col_chg_sub color cycling)
# - case 8: ~1 frame (wait for rf_b2_flag)
# - case 9: 1 frame (sets gs_Next_Step = 1 → allow_battle)
# TOTAL: ~120 frames from banner appearance to combat active
FIGHT_BANNER_DURATION = 120

# Banner timing (empirically validated)
BANNER_DURATION_R1 = 142  # FIGHT banner duration for Round 1
BANNER_DURATION_R2_PLUS = 146  # FIGHT banner duration for Round 2+
BANNER_INJECT_WINDOW = 48  # Only inject last N frames of banner period


def find_fight_banner_start(frames: list[dict]) -> int:
    """
    Find the CSV frame where FIGHT banner appears.

    Strategy: Find is_in_match=1 frame and subtract FIGHT_BANNER_DURATION.
    This gives us the sync point for when 3SX shows the FIGHT banner.

    Returns:
        Frame index where FIGHT banner appears in CPS3 recording
    """
    # Find is_in_match=1 frame (combat start, banner disappears)
    combat_start = -1
    for i, row in enumerate(frames):
        if "is_in_match" in row and int(row["is_in_match"]) == 1:
            combat_start = i
            break

    if combat_start == -1:
        # Fallback: use match_state=2
        for i, row in enumerate(frames):
            if int(row.get("match_state", 0)) == 2:
                combat_start = i
                break

    if combat_start == -1:
        print("Warning: Could not find combat start. Using frame 0.")
        return 0

    # Calculate banner appearance: combat_start - FIGHT_BANNER_DURATION
    banner_start = max(0, combat_start - FIGHT_BANNER_DURATION)
    print(
        f"FIGHT banner start: frame {banner_start} ({FIGHT_BANNER_DURATION} frames before combat at {combat_start})"
    )
    return banner_start


def find_next_round_start(
    frames: list[dict], current_idx: int, banner_inject_window: int
) -> tuple[int, int]:
    """
    Find the next round's injection start point in CSV frames.

    After a round ends (is_in_match goes 0), find when the NEXT round's
    is_in_match=1 starts, then back up by banner_inject_window frames.

    Args:
        frames: All CSV frame rows
        current_idx: Current position in frames list
        banner_inject_window: How many frames before combat to start injection

    Returns:
        Tuple of (next_injection_start_idx, next_combat_start_idx)
        Returns (-1, -1) if no more rounds found
    """
    # First find the next is_in_match=1 after current position
    next_combat_start = -1
    for i in range(current_idx, len(frames)):
        if int(frames[i].get("is_in_match", 0)) == 1:
            next_combat_start = i
            break

    if next_combat_start == -1:
        return (-1, -1)  # No more rounds

    # Back up by banner_inject_window to get injection start
    next_injection_start = max(current_idx, next_combat_start - banner_inject_window)
    return (next_injection_start, next_combat_start)


def find_game02_start(frames: list[dict]) -> int:
    """
    Find the frame where the game enters Game02 (match_state transitions to 1).

    This is the sync anchor point for 3SX - when nav_Play_Game goes 0→1,
    we should be injecting from this CSV frame.

    Returns:
        Frame index where match_state first becomes 1 (or 2 if 1 not found)
    """
    # Look for first match_state=1 (entering Game02 - gameplay)
    for i, row in enumerate(frames):
        match_state = int(row.get("match_state", 0))
        if match_state == 1:
            print(f"Game02 start found at CSV frame {i} (match_state=1)")
            return i

    # Fallback: maybe CSV starts directly with match_state=2 (is_in_match)
    for i, row in enumerate(frames):
        match_state = int(row.get("match_state", 0))
        if match_state == 2:
            print(
                f"Game02 start fallback: CSV frame {i} (match_state=2, no match_state=1 found)"
            )
            return i

    # Last resort: use is_in_match or timer heuristic
    print("Warning: No match_state signal found, falling back to is_in_match")
    for i, row in enumerate(frames):
        if int(row.get("is_in_match", 0)) == 1:
            return i

    # Absolute fallback
    print("Warning: Could not find game start. Using frame 0.")
    return 0


def detect_gameplay_start(frames: list[dict]) -> tuple[int, int]:
    """
    Find the anchor frame and activity start for injection alignment.

    Returns:
        (anchor_idx, activity_start):
            - anchor_idx: Frame where is_in_match=1 - use this for 3SX sync
            - activity_start: First frame with input activity (for logging only)

    Strategy:
    1. Find "Anchor Point" using is_in_match=1 (hardware-precise, from match_state_byte=0x02)
    2. Fallback to 'live' column for older CSVs
    3. Fallback to timer heuristic if neither available

    CRITICAL: When 3SX reports allow_battle=1, we should inject from anchor_idx,
    NOT from activity_start. The intro inputs (before anchor) are buffered by CPS3
    but ignored - injecting them into 3SX causes desync.
    """
    anchor_idx = -1
    signal_used = "none"

    # 1. PRIMARY: Use is_in_match (hardware-precise from v3 dumper)
    if "is_in_match" in frames[0]:
        for i, row in enumerate(frames):
            if int(row["is_in_match"]) == 1:
                anchor_idx = i
                signal_used = "is_in_match"
                break

    # 2. FALLBACK: Use 'live' column for older CSVs
    if anchor_idx == -1 and "live" in frames[0]:
        for i, row in enumerate(frames):
            if int(row["live"]) == 1:
                anchor_idx = i
                signal_used = "live"
                break

    # 3. LAST RESORT: Timer-based heuristic
    if anchor_idx == -1:
        # Find round start (T=99)
        search_start = 0
        for i, row in enumerate(frames):
            if int(row["timer"]) == 99 and int(row["p1_hp"]) == 160:
                search_start = i
                break
        # Find timer drop
        for i in range(search_start, len(frames)):
            if int(frames[i]["timer"]) < 99 or int(frames[i]["p1_hp"]) < 160:
                anchor_idx = i
                signal_used = "timer_heuristic"
                break

    if anchor_idx == -1:
        print("Warning: Could not find gameplay anchor. Using frame 0.")
        return 0, 0

    print(f"Using {signal_used} signal for synchronization")

    print(f"Anchor found at frame {anchor_idx} (Timer started counting/Live)")

    # 2. Find first activity (for informational purposes only)
    scan_start = max(0, anchor_idx - 300)
    activity_start = anchor_idx  # Default to anchor if no earlier activity

    for i in range(scan_start, anchor_idx + 1):
        row = frames[i]
        prev = frames[i - 1] if i > 0 else row

        p1_val = int(row["p1_input"])
        p2_val = int(row["p2_input"])
        p1_changed = p1_val != int(prev["p1_input"])
        p2_changed = p2_val != int(prev["p2_input"])

        if p1_val != 0 or p2_val != 0 or p1_changed or p2_changed:
            activity_start = i
            break

    if activity_start < anchor_idx:
        time_offset = (anchor_idx - activity_start) / 60.0
        print(
            f"Input activity detected at frame {activity_start} ({time_offset:.2f}s before anchor)"
        )
        print(
            f"NOTE: Intro inputs will be SKIPPED - injection starts at anchor frame {anchor_idx}"
        )

    return anchor_idx, activity_start


def load_csv(csv_path: Path) -> tuple[list[dict], int, int, int | None, int | None]:
    """Load CPS3 CSV and extract character IDs and optional SA values.

    Returns:
        (frames, p1_char, p2_char, p1_sa, p2_sa, rng_seeds)
        SA values are None if not present in CSV.
    """
    frames = []
    with open(csv_path, "r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            frames.append(row)

    if not frames:
        raise ValueError(f"Empty CSV: {csv_path}")

    # Find gameplay anchor to get accurate character IDs and SA
    anchor_idx, _ = detect_gameplay_start(frames)
    anchor_frame = frames[anchor_idx]
    p1_char = int(anchor_frame["p1_char"])
    p2_char = int(anchor_frame["p2_char"])

    # Extract SA from CSV if present (v3 format)
    p1_sa = (
        int(anchor_frame["p1_sa"]) - 1 if "p1_sa" in anchor_frame else None
    )  # CSV is 1-indexed
    p2_sa = int(anchor_frame["p2_sa"]) - 1 if "p2_sa" in anchor_frame else None

    print(f"Loaded {len(frames)} frames, injection starts at anchor frame {anchor_idx}")
    print(
        f"P1: {CPS3_CHAR_NAMES.get(p1_char, f'ID={p1_char}')} (CPS3 ID={p1_char})",
        end="",
    )
    if p1_sa is not None:
        print(f" SA{p1_sa + 1}")
    else:
        print()
    print(
        f"P2: {CPS3_CHAR_NAMES.get(p2_char, f'ID={p2_char}')} (CPS3 ID={p2_char})",
        end="",
    )
    if p2_sa is not None:
        print(f" SA{p2_sa + 1}")
    else:
        print()

    rng_seeds = {
        "rng_16": int(anchor_frame.get("rng_16", 0)),
        "rng_32": int(anchor_frame.get("rng_32", 0)),
        "rng_16_ex": int(anchor_frame.get("rng_16_ex", 0)),
        "rng_32_ex": int(anchor_frame.get("rng_32_ex", 0)),
        "stage": int(anchor_frame.get("stage", 1)),
    }
    print(f"RNG Seeds Extracted: {rng_seeds}")

    return frames, p1_char, p2_char, p1_sa, p2_sa, rng_seeds


def wait_for_shm_field(
    state: MenuBridgeState, field: str, value: int, timeout: float = 30.0
) -> bool:
    """Wait for a shared memory field to reach a specific value."""
    start = time.time()
    while time.time() - start < timeout:
        if getattr(state, field) == value:
            return True
        time.sleep(0.016)
    return False


def inject_frame_and_wait(state: MenuBridgeState, p1_input: int, p2_input: int) -> int:
    """Inject inputs and wait for frame advance."""
    current_frame = state.frame_count
    state.p1_input = p1_input
    state.p2_input = p2_input
    state.step_requested = 1

    # Wait for frame to advance
    timeout_start = time.time()
    while state.frame_count == current_frame:
        if time.time() - timeout_start > 1.0:
            return -1  # Frame advance timeout
        time.sleep(0.001)

    return state.frame_count


def wait_for_round_start(state: MenuBridgeState, timeout: float = 60.0) -> bool:
    """
    Wait for Game02 entry (nav_Play_Game >= 1).

    CRITICAL: For buffer-aware replay sync, we must NOT wait for allow_battle=1.
    We need to return at Game02 entry so the injection loop can inject intro frames
    that will be buffered by the game engine (charge inputs, etc.)
    """
    print("Waiting for Game02 entry (nav_Play_Game >= 1)...")
    start = time.time()

    while time.time() - start < timeout:
        state.step_requested = 1

        if state.nav_Play_Game >= 1:
            print(f"  Game02 entry detected! nav_Play_Game={state.nav_Play_Game}")
            print("  Returning immediately - injection loop will handle intro frames")
            return True

        time.sleep(0.016)
    else:
        print(f"Timeout! nav_Play_Game={state.nav_Play_Game}")
        return False

    return True


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


def navigate_to_gameplay(
    state: MenuBridgeState,
    nav: MenuNavigator,
    p1_char_cps3: int,
    p2_char_cps3: int,
    p1_sa: int = 2,
    p2_sa: int = 1,
) -> bool:
    """
    Navigate to gameplay using MenuNavigator (same pattern as replay_inputs.py).

    Converts CPS3 character IDs to 3SX IDs and uses the navigator's
    existing flow: boot → title → main menu → versus → char select → gameplay.
    """
    # Convert CPS3 IDs to 3SX IDs
    p1_3sx = CPS3_TO_3SX_CHAR.get(p1_char_cps3, 11)  # Default Ken
    p2_3sx = CPS3_TO_3SX_CHAR.get(p2_char_cps3, 2)  # Default Ryu

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

    # Phase 2: Select Arcade mode
    screen = nav.get_current_screen()
    if screen == GameScreen.MAIN_MENU:
        print("\nPhase 2: Selecting Arcade mode...")
        # Arcade is the default option at the top of the menu
        time.sleep(0.2)
        nav.send_p1_input(INPUT_LK)
        time.sleep(1.0)

    # Phase 3: Wait for Character Select and Trigger P2 Join
    print("\nPhase 3: Waiting for P1 Character Select to load...")
    for _ in range(30):
        screen = nav.get_current_screen()
        if screen == GameScreen.CHARACTER_SELECT:
            break
        if screen == GameScreen.PLAYER_ENTRY:
            nav.send_p1_input(INPUT_START)
        if screen == GameScreen.GAMEPLAY:
            state.menu_input_active = 0
            return True
        time.sleep(0.3)
    else:
        print("  ERROR: Never reached Character Select!")
        return False

    time.sleep(0.5)

    # Phase 3.5: P2 Joins! (New Challenger)
    print("\nPhase 3.5: P2 Joins! (Pressing P2 Start)")
    nav.send_p2_input(INPUT_START)

    print("Waiting for 'New Challenger' screen to pass...")
    # It takes a few seconds to go through the New Challenger animation
    # and return to the character select screen with both players active.
    time.sleep(4.0)
    for _ in range(30):
        screen = nav.get_current_screen()
        if screen == GameScreen.CHARACTER_SELECT:
            break
        time.sleep(0.3)
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
    print("Navigation complete!")
    return True


def run_parity_test(
    csv_path: Path,
    output_path: Optional[Path] = None,
    skip_menu: bool = False,
    p1_sa: Optional[int] = None,
    p2_sa: Optional[int] = None,
) -> dict:
    """
    Run the parity test.

    Args:
        csv_path: Path to CPS3 *_full.csv
        output_path: Path for 3SX output CSV (default: *_3sx.csv)
        skip_menu: If True, assume game is already at round start

    Returns:
        Dict with divergence statistics
    """
    # Load CPS3 data (includes SA from v3 format if present)
    frames, p1_char, p2_char, csv_p1_sa, csv_p2_sa, rng_seeds = load_csv(csv_path)

    # BANNER-START SYNC: Only inject the LAST N frames before combat starts
    # Injecting too many frames during banner overflows the motion buffer!
    # The game only buffers ~32-48 frames of motion inputs, so we inject just enough

    anchor_idx, _ = detect_gameplay_start(frames)  # is_in_match=1 frame (combat start)
    banner_start_csv = find_fight_banner_start(frames)  # Full banner start in CSV
    csv_banner_duration = anchor_idx - banner_start_csv

    # Only inject the last N frames before combat, not the full banner
    # This prevents motion buffer overflow while still capturing charge inputs
    limited_banner_start = max(banner_start_csv, anchor_idx - BANNER_INJECT_WINDOW)
    gameplay_start = limited_banner_start

    print("\n=== BANNER-START SYNC (LIMITED WINDOW) ===")
    print(
        f"CSV banner: frame {banner_start_csv} to {anchor_idx} ({csv_banner_duration} frames)"
    )
    print(f"INJECT WINDOW: last {BANNER_INJECT_WINDOW} frames only")
    print(f"Will inject from CSV frame {gameplay_start} <- SYNC POINT")
    print(f"Frames to inject during banner: {anchor_idx - gameplay_start}")

    # Resolve SA: CSV (from v3) > CLI arg > character default
    if p1_sa is None:
        if csv_p1_sa is not None:
            p1_sa = csv_p1_sa
            print(f"P1 SA from CSV: SA{p1_sa + 1}")
        else:
            p1_sa = CPS3_DEFAULT_SA.get(p1_char, 2)
            print(
                f"P1 SA not specified, using default for {CPS3_CHAR_NAMES.get(p1_char, '?')}: SA{p1_sa + 1}"
            )
    if p2_sa is None:
        if csv_p2_sa is not None:
            p2_sa = csv_p2_sa
            print(f"P2 SA from CSV: SA{p2_sa + 1}")
        else:
            p2_sa = CPS3_DEFAULT_SA.get(p2_char, 1)
            print(
                f"P2 SA not specified, using default for {CPS3_CHAR_NAMES.get(p2_char, '?')}: SA{p2_sa + 1}"
            )

    # Prepare output path
    if output_path is None:
        stem = csv_path.stem.replace("_full", "")
        output_path = csv_path.parent / f"{stem}_3sx.csv"

    # Connect to 3SX via shared memory (auto-launch if not running)
    print("\nConnecting to 3SX...")
    state, shm = connect_to_bridge()

    # On Windows, mmap(-1, ..., tagname=...) creates shared memory if it doesn't
    # exist, so connect_to_bridge() always "succeeds". Verify the game is actually
    # alive by checking if frame_count advances.
    game_alive = False
    if state is not None:
        fc1 = state.frame_count
        time.sleep(0.2)
        fc2 = state.frame_count
        game_alive = fc2 != fc1 or fc2 > 0

    if not game_alive:
        # Auto-launch the game
        script_dir = Path(__file__).resolve().parent
        project_root = script_dir.parent.parent
        exe_path = project_root / "build" / "application" / "bin" / "3sx.exe"
        if not exe_path.exists():
            raise RuntimeError(
                f"3sx.exe not found at {exe_path}. Build first with compile.bat."
            )
        print(f"Game not running. Launching: {exe_path}")
        subprocess.Popen(
            [str(exe_path)],
            cwd=str(exe_path.parent),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        # Wait for shared memory to become active (frame_count advancing)
        print("Waiting for game to start...")
        for attempt in range(30):
            time.sleep(1.0)
            state, shm = connect_to_bridge()
            if state is not None and state.frame_count > 0:
                break
        else:
            raise RuntimeError("Timeout: game did not start within 30 seconds")

    print(f"Connected! frame_count={state.frame_count}")

    if not skip_menu:
        # Navigate menus normally (title → versus → char select → gameplay)
        nav = MenuNavigator(state)
        if not navigate_to_gameplay(state, nav, p1_char, p2_char, p1_sa, p2_sa):
            raise RuntimeError("Failed to navigate to gameplay")

        # RNG seeding is deferred to wait_for_banner_sync
        # (seeds applied at the exact moment allow_battle=1 fires)

    # CRITICAL: Enable selfplay mode for P2 input injection
    # Without this, C-side won't apply p2_input during gameplay
    state.selfplay_active = 1
    state.step_mode_active = 1

    print("Round started! Beginning input injection...")

    # Open output CSV — matches CPS3 CSV columns for offline diff
    fieldnames = [
        "frame",
        "p1_input",
        "p2_input",
        "timer",
        "p1_hp",
        "p2_hp",
        "p1_x",
        "p1_y",
        "p2_x",
        "p2_y",
        "p1_facing",
        "p2_facing",
        "p1_meter",
        "p2_meter",
        "p1_stun",
        "p2_stun",
        "p1_char",
        "p2_char",
        "p1_busy",
        "p2_busy",
        "rng_16",
        "rng_32",
        "rng_16_ex",
        "rng_32_ex",
        "p1_action",
        "p2_action",
        "p1_animation",
        "p2_animation",
        "p1_posture",
        "p2_posture",
        "p1_freeze",
        "p2_freeze",
        "is_in_match",
    ]

    # Multi-field parity comparison config
    # (csv_field, 3sx_field, format) — format is 'd' (decimal) or 'x' (hex)
    PARITY_FIELDS = [
        ("p1_hp", "p1_hp", "d"),
        ("p2_hp", "p2_hp", "d"),
        ("p1_x", "p1_x", "d"),
        ("p2_x", "p2_x", "d"),
        ("p1_y", "p1_y", "d"),
        ("p2_y", "p2_y", "d"),
        # Meter: CPS3 Lua reads rb() (1 byte) but 3SX current_spg is s16 — not comparable
        # ("p1_meter",    "p1_meter",    "d"),
        # ("p2_meter",    "p2_meter",    "d"),
        # Stun: Same byte-width mismatch as meter
        # ("p1_stun_bar", "p1_stun",     "d"),
        # ("p2_stun_bar", "p2_stun",     "d"),
        # Facing indices on CPS3 do not map cleanly to 3SX's direction field
        # ("p1_facing",   "p1_facing",   "d"),
        # ("p2_facing",   "p2_facing",   "d"),
        # RNG
        ("rng_16", "rng_16", "d"),
        ("rng_32", "rng_32", "d"),
        ("rng_16_ex", "rng_16_ex", "d"),
        ("rng_32_ex", "rng_32_ex", "d"),
        # Action/Animation/Posture/Freeze are fundamentally different internal indices
        # ("p1_action",   "p1_action",   "x"),
        # ("p2_action",   "p2_action",   "x"),
        # ("p1_animation","p1_animation","x"),
        # ("p2_animation","p2_animation","x"),
        # ("p1_posture",  "p1_posture",  "d"),
        # ("p2_posture",  "p2_posture",  "d"),
        # ("p1_freeze",   "p1_freeze",   "d"),
        # ("p2_freeze",   "p2_freeze",   "d"),
    ]

    divergences = []
    first_divergence = {}  # field_name -> {frame, round, cps3_val, sx_val, fmt}
    with open(output_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        # === BANNER-START SYNCHRONIZED INJECTION ===
        #
        # SYNC STRATEGY (same for all rounds):
        #   1. Wait for 3SX nav_C_No[0]==1, nav_C_No[1]==4 (FIGHT banner)
        #   2. Count frames from banner start until we reach the skip point
        #   3. Inject from CSV at (is_in_match=1 - 48) frame
        #   4. Both systems reach combat together!
        #
        def wait_for_banner_sync(round_num: int, round_anchor_idx: int) -> int:
            """Wait for 3SX FIGHT banner, then step until allow_battle=1.

            Dynamically detects actual banner duration instead of hardcoding it.
            Injects CPS3 banner inputs during the last BANNER_INJECT_WINDOW frames
            before combat starts, timed against the expected banner duration.

            Phase 1: Step with neutral inputs until near the inject window
            Phase 2: Inject actual CPS3 banner inputs for the last N frames

            Args:
                round_num: Round number (1-based)
                round_anchor_idx: Index into frames[] for this round's is_in_match=1

            Returns round_anchor_idx on success, -1 on failure.
            """
            print(f"\n=== SYNC POINT (ROUND {round_num}) ===")
            print("Waiting for 3SX FIGHT banner (nav_C_No[0]==1 AND nav_C_No[1]==4)...")

            timeout = time.time() + 30.0
            while True:
                if state.nav_C_No[0] == 1 and state.nav_C_No[1] == 4:
                    break
                if time.time() > timeout:
                    print("ERROR: Timeout waiting for banner")
                    return -1
                state.step_requested = 1
                time.sleep(0.001)

            print("3SX FIGHT banner started!")

            # Determine expected banner duration and compute inject window
            expected_banner = (
                BANNER_DURATION_R1 if round_num == 1 else BANNER_DURATION_R2_PLUS
            )
            neutral_phase = max(0, expected_banner - BANNER_INJECT_WINDOW)

            # Gather CSV banner inputs (the last BANNER_INJECT_WINDOW frames before combat)
            banner_csv_start = max(0, round_anchor_idx - BANNER_INJECT_WINDOW)
            banner_csv_inputs = []
            for bi in range(banner_csv_start, round_anchor_idx):
                if bi < len(frames):
                    banner_csv_inputs.append(
                        (
                            int(frames[bi]["p1_input"]),
                            int(frames[bi]["p2_input"]),
                        )
                    )

            # Phase 1: Step through banner with neutral inputs
            banner_frames = 0
            timeout = time.time() + 30.0
            while banner_frames < neutral_phase:
                if state.allow_battle == 1:
                    break
                if time.time() > timeout:
                    print(
                        f"ERROR: Timeout waiting for allow_battle (stepped {banner_frames} frames)"
                    )
                    return -1
                inject_frame_and_wait(state, 0, 0)
                banner_frames += 1

            # Phase 2: Inject actual CPS3 banner inputs
            csv_input_idx = 0
            while True:
                if state.allow_battle == 1:
                    break
                if time.time() > timeout:
                    print(
                        f"ERROR: Timeout waiting for allow_battle (stepped {banner_frames} frames)"
                    )
                    return -1
                # Use CSV inputs if available, otherwise neutral
                if csv_input_idx < len(banner_csv_inputs):
                    p1_in, p2_in = banner_csv_inputs[csv_input_idx]
                    csv_input_idx += 1
                else:
                    p1_in, p2_in = 0, 0
                inject_frame_and_wait(state, p1_in, p2_in)
                banner_frames += 1

            if csv_input_idx > 0:
                print(
                    f"3SX combat ready (allow_battle=1) after {banner_frames} banner frames "
                    f"({csv_input_idx}/{len(banner_csv_inputs)} CSV banner inputs injected)."
                )
            else:
                print(
                    f"3SX combat ready (allow_battle=1) after {banner_frames} banner frames."
                )

            # Per-round RNG snapshot validation
            # Compare CPS3 CSV RNG seeds with 3SX RNG state at combat start
            csv_anchor = frames[round_anchor_idx]
            rng_match = True
            for rng_field in ["rng_16", "rng_32", "rng_16_ex", "rng_32_ex"]:
                csv_rng = int(csv_anchor.get(rng_field, 0))
                sx_rng = getattr(state, rng_field)
                if csv_rng != sx_rng:
                    print(
                        f"  RNG MISMATCH at R{round_num} start: {rng_field} CPS3={csv_rng} 3SX={sx_rng}"
                    )
                    rng_match = False
            if rng_match:
                print(
                    f"  RNG snapshot OK: all 4 indices match CPS3 at R{round_num} start"
                )

            # Seed RNG from this round's CSV anchor values
            # For R1 this uses the initial rng_seeds; for R2+ it uses the per-round values
            round_rng = {
                "rng_16": int(csv_anchor.get("rng_16", 0)),
                "rng_32": int(csv_anchor.get("rng_32", 0)),
                "rng_16_ex": int(csv_anchor.get("rng_16_ex", 0)),
                "rng_32_ex": int(csv_anchor.get("rng_32_ex", 0)),
            }
            print(f"Seeding RNG at combat start (R{round_num} CSV anchor)...")
            state.fm_stage = int(csv_anchor.get("stage", rng_seeds.get("stage", 1)))
            state.fm_rng_16 = round_rng["rng_16"]
            state.fm_rng_32 = round_rng["rng_32"]
            state.fm_rng_16_ex = round_rng["rng_16_ex"]
            state.fm_rng_32_ex = round_rng["rng_32_ex"]
            state.force_match_active = 1
            # Do NOT step an extra frame here — the main injection loop's
            # first inject_frame_and_wait() will be when PreTick picks up
            # the seeds and the game tick runs with actual CSV inputs.

            print(f"Starting combat injection from CSV anchor frame {round_anchor_idx}")
            return round_anchor_idx

        # Round 1 sync
        combat_start = wait_for_banner_sync(1, anchor_idx)
        if combat_start < 0:
            return False

        MAX_HP = 160  # Maximum valid HP value
        current_round = 1
        in_round_transition = False
        round_ever_started = False  # Will become true when is_in_match=1
        round_stats = {}  # Per-round divergence stats
        round_frame_count = 0  # Frames in current round
        total_frames_injected = 0  # Track actual injections across all rounds

        # Create injection frames slice starting from combat anchor (is_in_match=1)
        # Banner inputs were already handled (neutral) during wait_for_banner_sync
        injection_frames = frames[combat_start:]
        print(f"Starting injection from CSV frame {combat_start} (is_in_match=1)")
        print(f"Total frames to inject: {len(injection_frames)}")

        skip_to_index = (
            -1
        )  # Set to positive value to skip frames during inter-round sync

        for i, cps3_row in enumerate(injection_frames):
            # Skip frames if we're in inter-round sync (jumping ahead in CSV)
            if skip_to_index > 0 and i < skip_to_index:
                continue  # Skip this CSV frame, don't inject
            elif i == skip_to_index:
                skip_to_index = -1  # Reached target, stop skipping
                print(f"[Frame {i}] Reached resync point - resuming injection")

            frame_num = int(cps3_row["frame"])
            p1_input = int(cps3_row["p1_input"])
            p2_input = int(cps3_row["p2_input"])

            # Check CPS3 gameplay-active status from CSV
            # Prefer is_in_match (hardware-precise) over live (heuristic)
            if "is_in_match" in cps3_row:
                cps3_live = int(cps3_row["is_in_match"]) == 1
            else:
                cps3_live = int(cps3_row.get("live", 1)) == 1

            # Get CPS3 HP values
            cps3_p1_hp = int(cps3_row["p1_hp"])
            cps3_p2_hp = int(cps3_row["p2_hp"])
            cps3_hp_valid = cps3_p1_hp <= MAX_HP and cps3_p2_hp <= MAX_HP

            # Track round transitions for statistics only
            # BUT: Inject ALL frames regardless of is_in_match - game buffers inputs!
            if not cps3_live:
                # Only treat as "round ended" if we've actually seen gameplay this round
                if round_ever_started and not in_round_transition:
                    # Just entered transition - record stats for completed round
                    if round_frame_count > 0:  # Only record if we had gameplay frames
                        round_stats[current_round] = {
                            "divergences": len(
                                [
                                    d
                                    for d in divergences
                                    if d.get("round") == current_round
                                ]
                            ),
                            "frames": round_frame_count,
                        }
                        print(
                            f"[Frame {i}] Round {current_round} ended (live=0, HP: {cps3_p1_hp}/{cps3_p2_hp})"
                        )
                        print(
                            f"  Round {current_round} complete: {round_stats[current_round]['frames']} frames, {round_stats[current_round]['divergences']} divergences"
                        )

                    in_round_transition = True

                    # === INTER-ROUND SYNC ===
                    # Pause CSV injection and wait for 3SX to enter next round's banner
                    print(
                        f"\n=== INTER-ROUND SYNC (Round {current_round} -> {current_round + 1}) ==="
                    )

                    # Find where next round starts in CSV
                    # Use absolute frame index: gameplay_start + i is current position in original frames list
                    current_abs_idx = gameplay_start + i
                    next_inject_idx, next_combat_idx = find_next_round_start(
                        frames, current_abs_idx, BANNER_INJECT_WINDOW
                    )

                    if next_inject_idx == -1:
                        print("No more rounds in CSV - match complete!")
                        break

                    print(
                        f"Next round in CSV: combat at frame {frames[next_combat_idx]['frame']}, inject from {frames[next_inject_idx]['frame']}"
                    )

                    # Use the same sync function as Round 1!
                    # Pass per-round anchor so RNG is seeded from this round's CSV values
                    wait_for_banner_sync(current_round + 1, next_combat_idx)

                    # Skip ahead in injection_frames to the next round's start
                    # Calculate how many frames to skip: difference between current and next injection point
                    frames_to_skip_csv = next_inject_idx - current_abs_idx
                    print(f"Skipping {frames_to_skip_csv} CSV frames to resync")

                    # Set a skip target - we'll skip frames until we reach this index
                    skip_to_index = i + frames_to_skip_csv

                    # Update round state BEFORE continuing
                    current_round += 1
                    round_frame_count = 0
                    round_ever_started = False  # New round hasn't started yet
                    in_round_transition = False

                    # RE-ASSERT CONTROL FLAGS
                    state.selfplay_active = 1

                    print(
                        f"Ready for Round {current_round}! (will skip to frame index {skip_to_index})"
                    )

                # If in transition, check if we should skip this frame
                if in_round_transition:
                    # During transition, don't inject from CSV (3SX is doing its own thing)
                    # Just advance 3SX step-by-step until we exit transition
                    state.step_requested = 1
                    time.sleep(0.001)
                    total_frames_injected += 1
                    continue  # Don't inject, don't record, just keep 3SX moving

            else:
                # CPS3 is live - mark round as started
                if not round_ever_started:
                    round_ever_started = True
                    print(
                        f"[Frame {i}] Round {current_round} gameplay active (is_in_match=1)"
                    )

            # === DEBUG: Sync verification for first 20 frames ===
            DEBUG_FRAME_LIMIT = 20
            if i < DEBUG_FRAME_LIMIT:
                # Show CSV data vs 3SX state
                csv_timer = int(cps3_row.get("timer", 0))
                csv_is_in_match = int(cps3_row.get("is_in_match", 0))
                csv_match_state = int(cps3_row.get("match_state", 0))

                print(
                    f"[SYNC {i:04d}] CSV: frame={frame_num}, match_state={csv_match_state}, is_in_match={csv_is_in_match}, "
                    f"timer={csv_timer}, HP={cps3_p1_hp}/{cps3_p2_hp} | "
                    f"3SX: allow_battle={state.allow_battle}, timer={state.game_timer}, "
                    f"HP={state.p1_health}/{state.p2_health} | "
                    f"IN: p1=0x{p1_input:04x} p2=0x{p2_input:04x}"
                )
            elif i == DEBUG_FRAME_LIMIT:
                print(
                    f"[SYNC] ... (suppressing debug after {DEBUG_FRAME_LIMIT} frames)"
                )

            # Inject and wait
            new_frame = inject_frame_and_wait(state, p1_input, p2_input)
            if new_frame < 0:
                print(f"Frame advance timeout at frame {frame_num}")
                break

            round_frame_count += 1
            total_frames_injected += 1

            # Read 3SX state (full parity CSV — matches CPS3 columns)
            sx_row = {
                "frame": i,
                "p1_input": p1_input,
                "p2_input": p2_input,
                "timer": state.game_timer,
                "p1_hp": state.p1_health,
                "p2_hp": state.p2_health,
                "p1_x": state.p1_pos_x,
                "p1_y": state.p1_pos_y,
                "p2_x": state.p2_pos_x,
                "p2_y": state.p2_pos_y,
                "p1_facing": state.p1_facing,
                "p2_facing": state.p2_facing,
                "p1_meter": state.p1_meter,
                "p2_meter": state.p2_meter,
                "p1_stun": state.p1_stun,
                "p2_stun": state.p2_stun,
                "p1_char": state.nav_My_char[0],
                "p2_char": state.nav_My_char[1],
                "p1_busy": state.p1_busy,
                "p2_busy": state.p2_busy,
                "rng_16": state.rng_16,
                "rng_32": state.rng_32,
                "rng_16_ex": state.rng_16_ex,
                "rng_32_ex": state.rng_32_ex,
                "p1_action": state.p1_action,
                "p2_action": state.p2_action,
                "p1_animation": state.p1_animation,
                "p2_animation": state.p2_animation,
                "p1_posture": state.p1_posture,
                "p2_posture": state.p2_posture,
                "p1_freeze": state.p1_freeze,
                "p2_freeze": state.p2_freeze,
                "is_in_match": state.is_in_match,
            }
            writer.writerow(sx_row)

            # Multi-field parity comparison
            frame_diffs = {}  # field -> (cps3_val, sx_val, fmt)
            for csv_field, sx_field, fmt in PARITY_FIELDS:
                if csv_field not in cps3_row:
                    continue
                cps3_val = int(cps3_row[csv_field])
                sx_val = int(sx_row[sx_field])
                # Skip HP comparison when CPS3 Lua reports invalid values (>160).
                # The Lua readbyte at base+0x9F returns 0xFF during win/round-end
                # state, producing false divergences throughout subsequent rounds.
                if csv_field in ("p1_hp", "p2_hp") and cps3_val > MAX_HP:
                    continue
                if cps3_val != sx_val:
                    frame_diffs[csv_field] = (cps3_val, sx_val, fmt)
                    # Track first divergence per field
                    if csv_field not in first_divergence:
                        first_divergence[csv_field] = {
                            "frame": i,
                            "round": current_round,
                            "cps3_val": cps3_val,
                            "sx_val": sx_val,
                            "fmt": fmt,
                        }

            if frame_diffs:
                div = {
                    "frame": i,
                    "round": current_round,
                    "fields": {k: (v[0], v[1]) for k, v in frame_diffs.items()},
                }
                divergences.append(div)
                # Rich log: show all differing fields with Δ values
                diff_strs = []
                for k, (cv, sv, fmt) in frame_diffs.items():
                    delta = sv - cv
                    if fmt == "x":
                        diff_strs.append(f"{k}: CPS3=0x{cv:X} 3SX=0x{sv:X}")
                    else:
                        diff_strs.append(f"{k}: CPS3={cv} 3SX={sv} (d={delta:+d})")
                print(
                    f"[R{current_round} F{round_frame_count}] DIVERGENCE: {', '.join(diff_strs)} (inj_idx={i}, frame={frame_num})"
                )

            # Progress indicator
            if total_frames_injected % 500 == 0:
                print(
                    f"[R{current_round}] Progress: {round_frame_count} round frames, {total_frames_injected} total"
                )

        # Record final round stats if we ended mid-round
        if round_frame_count > 0 and current_round not in round_stats:
            round_stats[current_round] = {
                "divergences": len(
                    [d for d in divergences if d.get("round") == current_round]
                ),
                "frames": round_frame_count,
            }

        # Print per-round summary
        print("\n=== MULTI-ROUND SUMMARY ===")
        for rnd, stats in round_stats.items():
            status = (
                "OK PERFECT"
                if stats["divergences"] == 0
                else f"X  {stats['divergences']} divergences"
            )
            print(f"  Round {rnd}: {stats['frames']} frames - {status}")
        print(
            f"  Total: {total_frames_injected} frames across {len(round_stats)} round(s)"
        )

        # Print first-divergence summary (root cause analysis)
        if first_divergence:
            print("\n=== FIRST DIVERGENCE PER FIELD ===")
            # Sort by frame number to show earliest divergence first
            sorted_fields = sorted(
                first_divergence.items(), key=lambda x: x[1]["frame"]
            )
            for field, info in sorted_fields:
                fmt = info.get("fmt", "d")
                if fmt == "x":
                    val_str = f"CPS3=0x{info['cps3_val']:X}  3SX=0x{info['sx_val']:X}"
                else:
                    delta = info["sx_val"] - info["cps3_val"]
                    val_str = (
                        f"CPS3={info['cps3_val']}  3SX={info['sx_val']}  (d={delta:+d})"
                    )
                print(f"  {field:16s}: R{info['round']} F{info['frame']:5d}  {val_str}")
            earliest = sorted_fields[0]
            print(
                f"  >>> ROOT CAUSE CANDIDATE: '{earliest[0]}' first diverged at Round {earliest[1]['round']} Frame {earliest[1]['frame']}"
            )

    # Report
    print("\n=== PARITY TEST COMPLETE ===")
    print(
        f"Injected: {total_frames_injected} frames across {len(round_stats)} round(s)"
    )
    print(f"Divergences: {len(divergences)}")
    print(f"Output: {output_path}")

    return {
        "frames_injected": total_frames_injected,
        "rounds_tested": len(round_stats),
        "round_stats": round_stats,
        "divergences": divergences,
        "first_divergence": first_divergence,
        "output_path": str(output_path),
    }


def main():
    parser = argparse.ArgumentParser(description="CPS3 Parity Testing for 3SX")
    parser.add_argument("csv_path", type=Path, help="Path to CPS3 *_full.csv")
    parser.add_argument("-o", "--output", type=Path, help="Output path for 3SX CSV")
    parser.add_argument(
        "--skip-menu",
        action="store_true",
        help="Skip menu navigation (assume game at round start)",
    )
    parser.add_argument(
        "--p1-sa",
        type=int,
        default=None,
        choices=[0, 1, 2],
        help="P1 Super Art (0=SA1, 1=SA2, 2=SA3, default: auto from character)",
    )
    parser.add_argument(
        "--p2-sa",
        type=int,
        default=None,
        choices=[0, 1, 2],
        help="P2 Super Art (0=SA1, 1=SA2, 2=SA3, default: auto from character)",
    )

    args = parser.parse_args()

    if not args.csv_path.exists():
        print(f"Error: CSV not found: {args.csv_path}")
        sys.exit(1)

    try:
        results = run_parity_test(
            args.csv_path, args.output, args.skip_menu, args.p1_sa, args.p2_sa
        )
        print(
            f"\nResults: {results['frames_injected']} frames across {results['rounds_tested']} round(s), {len(results['divergences'])} divergences"
        )
        for rnd, stats in results.get("round_stats", {}).items():
            status = (
                "PERFECT"
                if stats["divergences"] == 0
                else f"{stats['divergences']} divergences"
            )
            print(f"  Round {rnd}: {stats['frames']} frames - {status}")
    except KeyboardInterrupt:
        print("\nAborted by user")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
