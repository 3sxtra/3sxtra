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
import sys
import time
import subprocess
from pathlib import Path
from typing import Optional

# Add parent to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from src.bridge.game_bridge import GameBridge
from src.bridge.state import RLBridgeState
from src.config import EnvConfig
from src.environments.menu_navigator import MenuNavigator, NavConfig, GameMode

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


def ensure_game_running():
    """Ensure 3SX is running and connected - KILLING any old instances first."""
    cwd = Path.cwd()
    exe_path = cwd / "3sx" / "build" / "application" / "bin" / "3sx.exe"

    # 1. Force Kill existing instances
    print("Killing existing 3SX instances to ensure clean state...")
    try:
        subprocess.run(
            ["taskkill", "/F", "/IM", "3sx.exe"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        time.sleep(1.0)  # Wait for release
    except Exception as e:
        print(f"Warning: Failed to kill 3sx: {e}")

    # 2. Connect or Launch
    # Use GameBridge to handle connection
    config = EnvConfig(render_game=True)
    bridge = GameBridge(config)

    if not exe_path.exists():
        raise FileNotFoundError(f"3sx.exe not found at {exe_path}")

    print(f"Launching clean instance: {exe_path}")
    process = subprocess.Popen(
        [
            str(exe_path),
            "--rl-training",
            "--instance-id",
            "0",
            "--window-pos",
            "1920,32",
            "--window-size",
            "640x480",
        ],
        cwd=exe_path.parent,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    # Wait loop
    print("Waiting for shared memory connection...")
    for _ in range(20):
        if bridge.connect():
            print("Connected to 3SX!")
            return bridge.state
        time.sleep(1)

    raise TimeoutError("Failed to connect to 3SX after launch")


def load_csv(csv_path: Path) -> tuple[list[dict], int, int, int | None, int | None]:
    """Load CPS3 CSV and extract character IDs and optional SA values.

    Returns:
        (frames, p1_char, p2_char, p1_sa, p2_sa)
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

    return frames, p1_char, p2_char, p1_sa, p2_sa


def wait_for_shm_field(
    state: RLBridgeState, field: str, value: int, timeout: float = 30.0
) -> bool:
    """Wait for a shared memory field to reach a specific value."""
    start = time.time()
    while time.time() - start < timeout:
        if getattr(state, field) == value:
            return True
        time.sleep(0.016)
    return False


def inject_frame_and_wait(state: RLBridgeState, p1_input: int, p2_input: int) -> int:
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


def wait_for_round_start(state: RLBridgeState, timeout: float = 60.0) -> bool:
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


def navigate_arcade_p2_join(
    state: RLBridgeState,
    p1_char_cps3: int,
    p2_char_cps3: int,
    p1_sa: int = 2,
    p2_sa: int = 1,
) -> bool:
    """
    Navigate using Arcade mode + P2 join flow:
    1. P1 enters Arcade mode
    2. At character select, P2 presses START to join
    3. Both players select their characters

    This is more reliable than VS mode menu navigation.
    """
    from src.environments.constants import RL_INPUT_START, RL_INPUT_LK
    from src.environments.menu_navigator import GameScreen

    # Convert CPS3 IDs to 3SX IDs
    p1_char_3sx = CPS3_TO_3SX_CHAR.get(p1_char_cps3, 11)  # Default Ken
    p2_char_3sx = CPS3_TO_3SX_CHAR.get(p2_char_cps3, 2)  # Default Ryu

    print("\n=== ARCADE + P2 JOIN NAVIGATION ===")
    print(f"P1: CPS3 ID {p1_char_cps3} -> 3SX ID {p1_char_3sx}")
    print(f"P2: CPS3 ID {p2_char_cps3} -> 3SX ID {p2_char_3sx}")

    # Configure for Arcade mode (P2 will join at char select)
    config = NavConfig(
        game_mode=GameMode.ARCADE,  # Start as Arcade
        target_character_p1=p1_char_3sx,
        target_super_art_p1=p1_sa,  # SA from CLI args or defaults
        target_character_p2=p2_char_3sx,
        target_super_art_p2=p2_sa,  # SA from CLI args or defaults
    )

    navigator = MenuNavigator(state, config)

    # Helper functions
    def wait_frames(n: int):
        target = state.frame_count + n
        while state.frame_count < target:
            time.sleep(0.001)

    def send_p1_input(mask: int, frames: int = 6):
        state.p1_input = mask
        wait_frames(frames)
        state.p1_input = 0
        wait_frames(2)

    def send_p2_input(mask: int, frames: int = 6):
        state.p2_input = mask
        wait_frames(frames)
        state.p2_input = 0
        wait_frames(2)

    # Enable RL mode and turbo sync for proper game state updates
    state.rl_mode_active = 1
    state.menu_input_active = 1
    state.turbo_force = 1  # Enable C-side frame sync (updates SHM fields)
    state.turbo_mode = 0  # Visual mode (not headless)

    print("Phase 1: Navigate to character select (Arcade mode)...")

    # Navigate through boot/title to main menu
    max_iterations = 30
    for i in range(max_iterations):
        screen = navigator.get_current_screen()

        if screen == GameScreen.CHARACTER_SELECT:
            print("Reached character select!")
            break
        elif screen == GameScreen.GAMEPLAY:
            print("Already in gameplay!")
            return True
        elif screen in [
            GameScreen.BOOT_INTRO,
            GameScreen.TITLE_SCREEN,
            GameScreen.ATTRACT_SCREEN,
            GameScreen.UNKNOWN,
        ]:
            print(f"  Screen: {screen.name} - pressing Start")
            send_p1_input(RL_INPUT_START)
            wait_frames(30)
        elif screen == GameScreen.MAIN_MENU:
            # Select Arcade (first option, Y=0)
            print("  Main Menu - selecting Arcade")
            send_p1_input(RL_INPUT_LK)
            wait_frames(30)
        elif screen == GameScreen.ENTRY_SCREEN:
            print("  Entry screen - confirming")
            send_p1_input(RL_INPUT_LK)
            wait_frames(30)
        else:
            print(f"  Screen: {screen.name} - pressing Start")
            send_p1_input(RL_INPUT_START)
            wait_frames(30)
    else:
        print("Failed to reach character select!")
        return False

    wait_frames(30)

    print("Phase 2: P2 presses START to join...")
    send_p2_input(RL_INPUT_START)
    wait_frames(60)  # Wait for P2 cursor to appear

    # Verify P2 joined (cursor should be valid)
    print(f"  P2 cursor: {state.nav_Cursor_Char[1]}")

    print("Phase 3: Select characters...")
    # Now both players select characters using the existing selector
    config.game_mode = GameMode.VERSUS  # Switch config for character selection
    navigator.config = config

    success = navigator.character_selector.navigate_versus(verbose=True)
    if not success:
        print("Character selection failed!")
        return False

    print("Phase 4: Handling pre-match screens...")
    # Handle handicap/stage select screens by pressing LK
    for _ in range(10):
        send_p1_input(RL_INPUT_LK)
        send_p2_input(RL_INPUT_LK)
        wait_frames(30)

        if state.nav_Play_Game >= 1:
            print("Entered gameplay!")
            break

        screen = navigator.get_current_screen()
        if screen == GameScreen.GAMEPLAY:
            print("Reached gameplay!")
            break

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
    frames, p1_char, p2_char, csv_p1_sa, csv_p2_sa = load_csv(csv_path)

    # BANNER-START SYNC: Only inject the LAST N frames before combat starts
    # Injecting too many frames during banner overflows the motion buffer!
    # The game only buffers ~32-48 frames of motion inputs, so we inject just enough
    BANNER_INJECT_WINDOW = 48  # Only inject last N frames of banner period

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

    state = ensure_game_running()

    # Reset 3SX state and navigate to gameplay
    print("Resetting 3SX state...")
    time.sleep(1.0)
    state.reset_requested = 1

    # Wait for reset handling
    time.sleep(2.0)

    # Enable Python control
    state.rl_mode_active = 1
    state.menu_input_active = 1  # Allow menu navigation

    if not skip_menu:
        # Use Arcade + P2 join navigation
        if not navigate_arcade_p2_join(state, p1_char, p2_char, p1_sa, p2_sa):
            raise RuntimeError("Failed to navigate to gameplay")

        if not wait_for_round_start(state, timeout=30.0):
            raise RuntimeError("Timeout waiting for round start")

    # CRITICAL: Enable selfplay mode for P2 input injection
    # Without this, C-side won't apply p2_input during gameplay
    state.selfplay_onnx_active = 1
    state.python_connected = 1  # Mark Python as connected for input injection

    print("Round started! Beginning input injection...")

    # Open output CSV
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
    ]

    divergences = []
    with open(output_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        # === BANNER-START SYNCHRONIZED INJECTION ===
        #
        # SYNC STRATEGY (same for all rounds):
        #   1. Wait for 3SX nav_C_No[0]==1, nav_C_No[1]==4 (FIGHT banner)
        #   2. Wait for 3SX to be (142 - 48 + MARGIN) frames into banner
        #   3. Inject from CSV at (is_in_match=1 - 48) frame
        #   4. Both systems reach combat together!
        #
        # SAFETY MARGIN: Banner duration varies (142-146+ frames depending on scroll).
        # Injecting LATE = chars stand still (harmless). Injecting EARLY = desync!
        # DISCOVERED: R1 always 142 frames, R2+ always 146 frames!
        #
        def get_banner_duration(round_num: int) -> int:
            """Return expected banner duration for given round."""
            if round_num == 1:
                return 142
            else:
                return 146

        def get_frames_to_skip(round_num: int) -> int:
            """Calculate frames to skip based on round-specific banner duration."""
            return get_banner_duration(round_num) - BANNER_INJECT_WINDOW

        def wait_for_banner_sync(round_num: int) -> bool:
            """Wait for 3SX to reach the sync point in its banner animation.

            Returns True if sync succeeded, False on timeout.
            """
            print(f"\n=== SYNC POINT (ROUND {round_num}) ===")
            print("Waiting for 3SX FIGHT banner (nav_C_No[0]==1 AND nav_C_No[1]==4)...")

            # Wait for banner to start
            timeout = time.time() + 30.0
            while True:
                if state.nav_C_No[0] == 1 and state.nav_C_No[1] == 4:
                    break
                if time.time() > timeout:
                    print(
                        f"ERROR: Timeout waiting for banner (C_No[0]={state.nav_C_No[0]}, C_No[1]={state.nav_C_No[1]})"
                    )
                    return False
                state.step_requested = 1
                time.sleep(0.001)

            print("3SX FIGHT banner started!")

            # Get round-specific skip value
            frames_to_skip_3sx = get_frames_to_skip(round_num)

            # Wait for 3SX to be at the right point in banner
            print(
                f"Waiting for 3SX to reach {frames_to_skip_3sx} frames into banner (R{round_num}: {get_banner_duration(round_num)}-frame banner)..."
            )
            timeout = time.time() + 10.0
            while True:
                banner_frames = state.banner_frame_count
                if banner_frames >= frames_to_skip_3sx:
                    break
                if time.time() > timeout:
                    print(f"WARN: Timeout (got {banner_frames}/{frames_to_skip_3sx})")
                    break
                state.step_requested = 1
                time.sleep(0.001)

            print(f"3SX at {state.banner_frame_count} banner frames - synced!")
            return True

        # Round 1 sync
        if not wait_for_banner_sync(1):
            return False
        print(
            f"Injecting from CSV frame {gameplay_start} (last {BANNER_INJECT_WINDOW} frames of banner)"
        )

        MAX_HP = 160  # Maximum valid HP value
        current_round = 1
        in_round_transition = False
        round_ever_started = False  # Will become true when is_in_match=1
        round_stats = {}  # Per-round divergence stats
        round_frame_count = 0  # Frames in current round
        total_frames_injected = 0  # Track actual injections across all rounds

        # Create injection frames slice starting from match_state=1 (Game02 entry)
        # This includes the intro period where inputs are buffered!
        injection_frames = frames[gameplay_start:]
        print(f"Starting injection from CSV frame {gameplay_start} (Game02 entry)")
        print(
            f"Total frames to inject: {len(injection_frames)} (includes intro buffer period)"
        )

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
                    wait_for_banner_sync(current_round + 1)

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
                    state.selfplay_onnx_active = 1
                    state.python_connected = 1

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

            # === DEBUG: Sync verification for first 100 frames ===
            DEBUG_FRAME_LIMIT = 20
            if i < DEBUG_FRAME_LIMIT:
                # Show CSV data vs 3SX state
                csv_timer = int(cps3_row.get("timer", 0))
                csv_is_in_match = int(cps3_row.get("is_in_match", 0))
                csv_match_state = int(cps3_row.get("match_state", 0))

                print(
                    f"[SYNC {i:04d}] CSV: frame={frame_num}, match_state={csv_match_state}, is_in_match={csv_is_in_match}, "
                    f"timer={csv_timer}, HP={cps3_p1_hp}/{cps3_p2_hp} | "
                    f"3SX: allow_battle={state.allow_battle}, timer={state.time_remaining}, "
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

            # Read 3SX state
            sx_row = {
                "frame": i,
                "p1_input": p1_input,
                "p2_input": p2_input,
                "timer": state.time_remaining,
                "p1_hp": state.p1_health,
                "p2_hp": state.p2_health,
                "p1_x": state.p1_pos_x,
                "p1_y": state.p1_pos_y,
                "p2_x": state.p2_pos_x,
                "p2_y": state.p2_pos_y,
                "p1_facing": state.p1_side,
                "p2_facing": state.p2_side,
                "p1_meter": state.p1_meter,
                "p2_meter": state.p2_meter,
                "p1_stun": state.p1_stun,
                "p2_stun": state.p2_stun,
                "p1_char": state.p1_character,
                "p2_char": state.p2_character,
            }
            writer.writerow(sx_row)

            # Compare HP
            sx_hp_valid = sx_row["p1_hp"] <= MAX_HP and sx_row["p2_hp"] <= MAX_HP

            if cps3_hp_valid and sx_hp_valid:
                hp_diff = abs(sx_row["p1_hp"] - cps3_p1_hp) + abs(
                    sx_row["p2_hp"] - cps3_p2_hp
                )

                if hp_diff > 0:
                    div = {
                        "frame": i,
                        "round": current_round,
                        "cps3_hp": (cps3_p1_hp, cps3_p2_hp),
                        "3sx_hp": (sx_row["p1_hp"], sx_row["p2_hp"]),
                    }
                    divergences.append(div)
                    print(
                        f"[R{current_round} F{round_frame_count}] HP DIVERGENCE: CPS3={div['cps3_hp']} vs 3SX={div['3sx_hp']} (inj_idx={i}, frame={frame_num})"
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
                "✓ PERFECT"
                if stats["divergences"] == 0
                else f"✗ {stats['divergences']} divergences"
            )
            print(f"  Round {rnd}: {stats['frames']} frames - {status}")
        print(
            f"  Total: {total_frames_injected} frames across {len(round_stats)} round(s)"
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
