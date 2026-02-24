#!/usr/bin/env python3
import subprocess
import time
import argparse
import sys
from pathlib import Path

# Add project root
PROJECT_ROOT = Path(__file__).parent.parent
sys.path.append(str(PROJECT_ROOT))

from tools.util.bridge_state import connect_to_bridge, GameScreen  # noqa: E402
from tools.util.navigator import MenuNavigator  # noqa: E402

BUILD_DIR = PROJECT_ROOT / "build" / "application" / "bin"
EXE_PATH = BUILD_DIR / "3sx.exe"

# Global process references for liveness checks
g_p1_proc = None
g_p2_proc = None


class DesyncDetected(Exception):
    """Raised when a desync is detected (process exited unexpectedly)."""


def launch_game(suffix, extra_args=None):
    if extra_args is None:
        extra_args = []
    if not EXE_PATH.exists():
        raise RuntimeError(f"Executable not found at {EXE_PATH}")

    cmd = [str(EXE_PATH), "--shm-suffix", suffix] + extra_args
    print(f"Launching: {' '.join(cmd)}")
    return subprocess.Popen(cmd, cwd=BUILD_DIR)


def wait_for_connection(suffix, timeout=10):
    start = time.time()
    while time.time() - start < timeout:
        check_processes_alive()  # Check during connection wait
        state, shm = connect_to_bridge(suffix)
        if state:
            return state, shm
        time.sleep(0.5)
    return None, None


def check_processes_alive():
    """Check if both game processes are still running. Raises DesyncDetected if not."""
    global g_p1_proc, g_p2_proc

    p1_exit = g_p1_proc.poll() if g_p1_proc else None
    p2_exit = g_p2_proc.poll() if g_p2_proc else None

    if p1_exit is not None or p2_exit is not None:
        raise DesyncDetected(f"Process exited! P1: {p1_exit}, P2: {p2_exit}")


def main():
    global g_p1_proc, g_p2_proc

    parser = argparse.ArgumentParser(description="3SX Automated Netplay Sync Test")
    parser.add_argument(
        "--p1-char", type=int, default=-1, help="P1 Character ID (-1 = auto)"
    )
    parser.add_argument(
        "--p2-char", type=int, default=-1, help="P2 Character ID (-1 = auto)"
    )
    args = parser.parse_args()

    print("=== 3SX Automated Sync Test ===")

    # Kill stale processes from previous runs to avoid port conflicts
    try:
        subprocess.run(
            ["taskkill", "/F", "/IM", "3sx.exe", "/T"],
            capture_output=True,
            timeout=5,
            check=False,
        )
        time.sleep(1.0)  # Give OS time to release ports
    except Exception:
        pass  # No stale processes — that's fine

    exit_code = 1  # Default to failure

    try:
        # Launch P1 (Host)
        g_p1_proc = launch_game(
            "p1", ["--sync-test", "--window-pos", "50,100", "--window-size", "640x480"]
        )

        # Launch P2 (Client) - give P1 a moment to bind port
        time.sleep(2.0)
        g_p2_proc = launch_game(
            "p2",
            [
                "--sync-test-client",
                "--window-pos",
                "700,100",
                "--window-size",
                "640x480",
            ],
        )

        print("Waiting for Shared Memory...")
        p1_state, _ = wait_for_connection("p1")
        p2_state, _ = wait_for_connection("p2")

        if not p1_state or not p2_state:
            print("ERROR: Failed to connect to game instances.")
            return 1

        print("Connected! Initializing Navigators...")
        p1_nav = MenuNavigator(p1_state)
        p2_nav = MenuNavigator(p2_state)

        # Navigate P1 to Network
        print("Navigating P1 to Network...")
        check_processes_alive()
        p1_nav.navigate_title_to_menu()
        check_processes_alive()
        p1_nav.navigate_main_menu_to_network()
        check_processes_alive()

        # Navigate P2 to Network
        print("Navigating P2 to Network...")
        check_processes_alive()
        p2_nav.navigate_title_to_menu()
        check_processes_alive()
        p2_nav.navigate_main_menu_to_network()
        check_processes_alive()

        # Wait for both to reach Player Entry or Character Select
        print("Waiting for players to connect...")
        if not _wait_for_screen_with_check(
            p1_state, [GameScreen.PLAYER_ENTRY, GameScreen.CHARACTER_SELECT], timeout=15
        ):
            print("ERROR: P1 did not reach Player Entry/Character Select")
            return 1
        if not _wait_for_screen_with_check(
            p2_state, [GameScreen.PLAYER_ENTRY, GameScreen.CHARACTER_SELECT], timeout=15
        ):
            print("ERROR: P2 did not reach Player Entry/Character Select")
            return 1

        # Handle Player Entry if needed
        _handle_player_entry(p1_nav, p1_state)
        check_processes_alive()
        _handle_player_entry(p2_nav, p2_state)
        check_processes_alive()

        # Wait for Character Select
        print("Waiting for Character Select...")
        if not _wait_for_screen_with_check(
            p1_state, [GameScreen.CHARACTER_SELECT], timeout=10
        ):
            print("ERROR: P1 did not reach Character Select")
            return 1
        if not _wait_for_screen_with_check(
            p2_state, [GameScreen.CHARACTER_SELECT], timeout=10
        ):
            print("ERROR: P2 did not reach Character Select")
            return 1

        # Select characters (with liveness checks)
        print(f"P1 selecting character {args.p1_char}...")
        check_processes_alive()
        if not _select_character_with_check(p1_nav, 0, args.p1_char):
            print("WARNING: P1 character selection may have failed")

        print(f"P2 selecting character {args.p2_char}...")
        check_processes_alive()
        if not _select_character_with_check(p2_nav, 1, args.p2_char):
            print("WARNING: P2 character selection may have failed")

        # Confirm Super Arts
        check_processes_alive()
        time.sleep(0.5)
        p1_nav.select_super_art(0, 1)
        check_processes_alive()
        p2_nav.select_super_art(1, 1)
        check_processes_alive()

        # Wait for gameplay
        print("Waiting for gameplay to begin...")
        if _wait_for_screen_with_check(p1_state, [GameScreen.GAMEPLAY], timeout=30):
            print("SUCCESS: Gameplay started!")
            p1_nav.release_control()
            p2_nav.release_control()
        else:
            print("WARNING: Did not detect gameplay screen")

        # Monitor for desync during gameplay
        print("Test running. Monitoring for desync. Press Ctrl+C to stop.")
        while True:
            check_processes_alive()
            time.sleep(0.5)

    except DesyncDetected as e:
        print(f"\n{'=' * 50}")
        print(f"DESYNC DETECTED: {e}")
        print(f"{'=' * 50}")

        # Wait a moment for both to exit
        time.sleep(0.5)
        p1_exit = g_p1_proc.poll() if g_p1_proc else None
        p2_exit = g_p2_proc.poll() if g_p2_proc else None
        print(f"Final exit codes - P1: {p1_exit}, P2: {p2_exit}")

        # Run state comparison for helpful diagnostics
        _run_state_comparison()

        exit_code = 1

    except KeyboardInterrupt:
        print("\nStopping (user interrupt)...")
        exit_code = 0

    except Exception as e:
        print(f"ERROR: {e}")
        exit_code = 1

    finally:
        if g_p1_proc:
            g_p1_proc.terminate()
        if g_p2_proc:
            g_p2_proc.terminate()

    return exit_code


def _wait_for_screen_with_check(state, target_screens, timeout=10):
    """Wait for state to reach one of the target screens, checking process liveness."""
    start = time.time()
    while time.time() - start < timeout:
        check_processes_alive()
        current = GameScreen.from_gno(state.nav_G_No[0], state.nav_G_No[1])
        if current in target_screens:
            return True
        time.sleep(0.1)
    return False


def _handle_player_entry(nav, state):
    """If on Player Entry screen, press START to join."""
    current = GameScreen.from_gno(state.nav_G_No[0], state.nav_G_No[1])
    if current == GameScreen.PLAYER_ENTRY:
        nav.process_step()  # Will press START
        time.sleep(1.0)


def _select_character_with_check(nav, player_idx, target_char_id):
    """Select character with periodic liveness checks.

    If target_char_id is -1, just confirm whoever the cursor starts on.
    Otherwise navigate the cursor to the target character.
    """
    MAX_MOVE_ATTEMPTS = 40

    # Wait for character select screen to fully initialize
    time.sleep(1.5)

    # Debug: Print initial state
    cx = nav.state.nav_Cursor_X[player_idx]
    cy = nav.state.nav_Cursor_Y[player_idx]
    cc = nav.state.nav_Cursor_Char[player_idx]
    print(
        f"  [DEBUG] P{player_idx + 1} initial cursor: X={cx}, Y={cy}, Char={cc}, Target={target_char_id}"
    )

    # If -1, just confirm whoever is under cursor
    if target_char_id == -1:
        target_char_id = cc
        print(
            f"  [DEBUG] P{player_idx + 1} auto-selecting current char {target_char_id}"
        )

    # Step 1: Navigate cursor to target character
    for attempt in range(MAX_MOVE_ATTEMPTS):
        check_processes_alive()

        current_char = nav.state.nav_Cursor_Char[player_idx]
        if attempt < 5 or attempt % 10 == 0:
            print(
                f"  [DEBUG] P{player_idx + 1} attempt {attempt}: Cursor_Char={current_char}"
            )

        if current_char == target_char_id:
            break

        # Move cursor right
        if player_idx == 0:
            nav.send_p1_input(0x8)  # INPUT_RIGHT
        else:
            nav.send_p2_input(0x8)
        time.sleep(0.15)
    else:
        print(f"  [DEBUG] P{player_idx + 1} FAILED: never found char {target_char_id}")
        return False  # Never found the target

    # Step 2: Confirm selection (press LK)
    check_processes_alive()
    print(
        f"  [DEBUG] P{player_idx + 1} on target char {target_char_id}, pressing LK to confirm"
    )
    if player_idx == 0:
        nav.send_p1_input(0x100)  # INPUT_LK
    else:
        nav.send_p2_input(0x100)

    time.sleep(0.5)
    return True


def _run_state_comparison():
    """Run state comparison to show exactly where states diverged."""
    states_dir = BUILD_DIR / "states"

    # Check if state dumps exist
    if not states_dir.exists():
        print("\n[State Comparison] No states directory found.")
        return

    state_files = list(states_dir.glob("*_*"))
    if len(state_files) < 2:
        print("\n[State Comparison] Not enough state dumps to compare.")
        return

    print(f"\n{'=' * 50}")
    print("STATE COMPARISON ANALYSIS")
    print(f"{'=' * 50}")
    print(f"Found {len(state_files)} state dumps in {states_dir}")

    # Try to run compare_states.py directly (simpler approach)
    compare_script = PROJECT_ROOT / "tools" / "compare_states.py"
    binary_path = PROJECT_ROOT / "build" / "3sx.exe"

    # Check if we have the binary with debug info
    if not binary_path.exists():
        # Try alternative location
        binary_path = BUILD_DIR / "3sx.exe"

    if compare_script.exists() and binary_path.exists():
        print("\nRunning state comparison...")
        print(f"Binary: {binary_path}")
        print("-" * 50)

        try:
            # Run compare_states.py from the build dir where states/ is located
            result = subprocess.run(
                ["python", str(compare_script), str(binary_path)],
                cwd=BUILD_DIR,
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )

            if result.stdout:
                print(result.stdout)
            if result.stderr:
                print(f"[stderr] {result.stderr}")
            if result.returncode != 0:
                print(
                    f"[State Comparison] compare_states.py exited with code {result.returncode}"
                )

        except subprocess.TimeoutExpired:
            print("[State Comparison] Timed out (dwarfdump may be slow on first run)")
        except FileNotFoundError as e:
            print(f"[State Comparison] Could not run compare_states.py: {e}")
        except Exception as e:
            print(f"[State Comparison] Error: {e}")
    else:
        # Fallback: just show a simple byte-level comparison
        print("\n[State Comparison] compare_states.py or binary not found.")
        print("Falling back to simple byte comparison...")
        _simple_state_compare(states_dir)


def _simple_state_compare(states_dir):
    """Simple fallback comparison when compare_states.py isn't available."""
    try:
        p1_state_file = states_dir / "0_0"
        p2_state_file = states_dir / "1_0"

        if not p1_state_file.exists() or not p2_state_file.exists():
            # Find whatever files exist
            files = sorted(states_dir.glob("*"))
            print(f"Available state files: {[f.name for f in files]}")
            return

        p1_data = p1_state_file.read_bytes()
        p2_data = p2_state_file.read_bytes()

        print(f"\nP1 state size: {len(p1_data)} bytes")
        print(f"P2 state size: {len(p2_data)} bytes")

        # Find first 10 differences
        diffs = []
        for i in range(min(len(p1_data), len(p2_data))):
            if p1_data[i] != p2_data[i]:
                diffs.append((i, p1_data[i], p2_data[i]))
                if len(diffs) >= 10:
                    break

        if diffs:
            print(f"\nFirst {len(diffs)} byte differences:")
            for offset, b1, b2 in diffs:
                print(f"  Offset 0x{offset:04X}: P1=0x{b1:02X}, P2=0x{b2:02X}")
        else:
            print("\nNo byte differences found (states are identical?)")

    except Exception as e:
        print(f"[Simple Compare] Error: {e}")


if __name__ == "__main__":
    sys.exit(main() or 0)
