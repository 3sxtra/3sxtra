#!/usr/bin/env python3
"""
Convert downloaded GGPO replay data (from fcade_replay_tool.py) into the
CSV format used by replay_inputs.py.

Usage:
    python tools/fightcade_replays/ggpo_to_csv.py \
        tools/fightcade_replays/output/sfiii3nr1-TOKEN \
        --p1-char 11 --p2-char 2 --p1-sa 3 --p2-sa 1

The output CSV is written to <replay_dir>/<game>_replay_full.csv and can be
fed directly into replay_inputs.py:

    python tools/fightcade_replays/replay_inputs.py <output>.csv
"""

from __future__ import annotations

import argparse
import csv
import json
import struct
import sys
from pathlib import Path

# -- Record parsing ----------------------------------------------------------

RECORD_SIZE = 10  # 5 bytes P1 + 5 bytes P2

# FBNeo input bitmask (matches rl_bridge.h / Lua dumper)
INP_UP = 0x0001
INP_DOWN = 0x0002
INP_LEFT = 0x0004
INP_RIGHT = 0x0008
INP_LP = 0x0010
INP_MP = 0x0020
INP_HP = 0x0040
INP_LK = 0x0100
INP_MK = 0x0200
INP_HK = 0x0400
INP_START = 0x0800
INP_ATTACKS = INP_LP | INP_MP | INP_HP | INP_LK | INP_MK | INP_HK


def parse_record(data: bytes, offset: int = 0) -> tuple[int, int]:
    """Parse one 10-byte GGPO input record.

    Layout (5 bytes per player):
        u16le  input   (FBNeo button bitmask — identical to rl_bridge.h)
        u8     flag    (0x01 = valid)
        u16le  seq     (sequence/padding, ignored)

    Returns (p1_input, p2_input).
    """
    p1 = struct.unpack_from("<H", data, offset)[0]
    p2 = struct.unpack_from("<H", data, offset + 5)[0]
    return p1, p2


def is_garbage_input(val: int) -> bool:
    """Detect impossible FBNeo inputs (protocol noise / non-input data).

    Real gameplay can never have Up+Down or Left+Right simultaneously.
    """
    if (val & 0x0003) == 0x0003:  # Up + Down
        return True
    if (val & 0x000C) == 0x000C:  # Left + Right
        return True
    if val > 0x0FFF:  # Only bits 0-11 are valid
        return True
    return False


# -- Character / SA auto-detection -------------------------------------------

# Face_Cursor_Data[3][8] from sel_data.c  (-1 = empty cell)
FACE_GRID = [
    [-1, 1, 12, 7, 5, 13, 14, -1],  # row 0
    [10, 18, 16, 15, 17, 19, 3, 0],  # row 1
    [11, 6, 8, 4, 9, 2, -1, -1],  # row 2
]

CHAR_NAMES = {
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
    15: "Q",
    16: "Chun-Li",
    17: "Makoto",
    18: "Twelve",
    19: "Remy",
}


class CharSelectSimulator:
    """Simulate the SF3:3S character select cursor from sel_pl.c.

    Replays decoded GGPO inputs through the exact cursor movement and
    wrapping logic to determine which characters and Super Arts were
    selected without needing an emulator.
    """

    # Default starting positions (Fightcade sfiii3nr1.state.net savestate)
    P1_START = (1, 0)  # Alex  — Face_Cursor_Data[0][1] = 1
    P2_START = (5, 2)  # Ryu   — Face_Cursor_Data[2][5] = 2

    def __init__(self):
        self.grid = FACE_GRID
        # Per-player state: (cursor_x, cursor_y)
        self.cx = [self.P1_START[0], self.P2_START[0]]
        self.cy = [self.P1_START[1], self.P2_START[1]]
        # Phase: 0=char select, 1=SA select, 2=done
        self.phase = [0, 0]
        # Previous raw input (for edge-trigger detection)
        self.prev_inp = [0, 0]
        # SA cursor (0=SA1, 1=SA2, 2=SA3)
        self.arts_y = [0, 0]
        # Results
        self.char_id = [None, None]
        self.sa = [None, None]

    def _is_valid(self, x: int, y: int) -> bool:
        """Check if grid cell [y][x] is a valid character (not -1)."""
        if y < 0 or y > 2 or x < 0 or x > 7:
            return False
        return self.grid[y][x] >= 0

    def _move_right(self, pid: int) -> None:
        """Cursor right = Y++ with row-dependent wrapping (Sel_PL_Sub_CR)."""
        if self.cx[pid] == 7:
            return
        while True:
            self.cy[pid] += 1
            if self.cx[pid] == 6:
                if self.cy[pid] > 1:
                    self.cy[pid] = 1
                    self.cx[pid] = 0
            else:
                if self.cy[pid] > 2:
                    self.cy[pid] = 0
                    self.cx[pid] += 1
            if self._is_valid(self.cx[pid], self.cy[pid]):
                break

    def _move_left(self, pid: int) -> None:
        """Cursor left = Y-- with row-dependent wrapping (Sel_PL_Sub_CL)."""
        if self.cx[pid] == 7:
            return
        while True:
            self.cy[pid] -= 1
            if self.cx[pid] == 0:
                if self.cy[pid] <= 0:
                    self.cy[pid] = 1
                    self.cx[pid] = 6
            elif self.cx[pid] == 1:
                if self.cy[pid] < 0:
                    self.cy[pid] = 2
                    self.cx[pid] = 0
            else:
                if self.cy[pid] < 0:
                    self.cy[pid] = 2
                    self.cx[pid] -= 1
            if self._is_valid(self.cx[pid], self.cy[pid]):
                break

    def _move_up(self, pid: int) -> None:
        """Cursor up = X++ with column-dependent wrapping (Sel_PL_Sub_CU)."""
        while True:
            self.cx[pid] += 1
            if self.cy[pid] == 0:
                if self.cx[pid] > 6:
                    self.cx[pid] = 1
            elif self.cy[pid] == 1:
                if self.cx[pid] > 7:
                    self.cx[pid] = 0
            else:
                if self.cx[pid] > 5:
                    self.cx[pid] = 0
            if self._is_valid(self.cx[pid], self.cy[pid]):
                break

    def _move_down(self, pid: int) -> None:
        """Cursor down = X-- with column-dependent wrapping (Sel_PL_Sub_CD)."""
        while True:
            self.cx[pid] -= 1
            if self.cy[pid] == 0:
                if self.cx[pid] <= 0:
                    self.cx[pid] = 6
            elif self.cy[pid] == 1:
                if self.cx[pid] < 0:
                    self.cx[pid] = 7
            else:
                if self.cx[pid] < 0:
                    self.cx[pid] = 5
            if self._is_valid(self.cx[pid], self.cy[pid]):
                break

    def _process_char_select(self, pid: int, inp: int, prev: int) -> None:
        """Process one frame of character selection input."""
        # Edge-triggered: only act on newly pressed buttons
        new_press = inp & ~prev

        # Directional movement (edge-triggered like the game's cursor timer)
        if new_press & INP_RIGHT:
            self._move_right(pid)
        elif new_press & INP_LEFT:
            self._move_left(pid)
        elif new_press & INP_UP:
            self._move_up(pid)
        elif new_press & INP_DOWN:
            self._move_down(pid)

        # Attack button confirms character
        if new_press & INP_ATTACKS:
            self.char_id[pid] = self.grid[self.cy[pid]][self.cx[pid]]
            self.phase[pid] = 1  # Move to SA selection
            self.arts_y[pid] = 0  # SA starts at 0 (SA1)

    def _process_sa_select(self, pid: int, inp: int, prev: int) -> None:
        """Process one frame of Super Art selection input."""
        new_press = inp & ~prev

        # UP/DOWN cycle through SA options
        if new_press & INP_DOWN:
            self.arts_y[pid] = (self.arts_y[pid] + 1) % 3
        elif new_press & INP_UP:
            self.arts_y[pid] = (self.arts_y[pid] - 1) % 3

        # Attack button confirms SA (game stores 0-indexed, we return 1-indexed)
        if new_press & INP_ATTACKS:
            self.sa[pid] = self.arts_y[pid] + 1  # 1=SAI, 2=SAII, 3=SAIII
            self.phase[pid] = 2  # Done

    def feed(self, p1_inp: int, p2_inp: int) -> bool:
        """Feed one frame of inputs. Returns True when both players are done."""
        for pid, inp in enumerate((p1_inp, p2_inp)):
            prev = self.prev_inp[pid]
            if self.phase[pid] == 0:
                self._process_char_select(pid, inp, prev)
            elif self.phase[pid] == 1:
                self._process_sa_select(pid, inp, prev)
            self.prev_inp[pid] = inp
        return self.phase[0] == 2 and self.phase[1] == 2

    @property
    def done(self) -> bool:
        return self.phase[0] == 2 and self.phase[1] == 2

    def results(self) -> tuple:
        """Returns (p1_char, p1_sa, p2_char, p2_sa) or raises if incomplete."""
        if not self.done:
            # Return best guess from current cursor position
            for pid in range(2):
                if self.char_id[pid] is None:
                    self.char_id[pid] = self.grid[self.cy[pid]][self.cx[pid]]
                if self.sa[pid] is None:
                    self.sa[pid] = 1  # Default SA1
        return self.char_id[0], self.sa[0], self.char_id[1], self.sa[1]


def detect_chars_from_inputs(
    inputs: list[tuple[int, int]],
    max_frames: int = 3000,
) -> tuple[int, int, int, int]:
    """Auto-detect character and SA selections from replay inputs.

    Simulates the character select cursor for up to *max_frames* frames
    (default ~50s) which is more than enough for any selection sequence.

    Returns (p1_char, p1_sa, p2_char, p2_sa).
    """
    sim = CharSelectSimulator()
    for i, (p1, p2) in enumerate(inputs[:max_frames]):
        if is_garbage_input(p1) or is_garbage_input(p2):
            continue
        if sim.feed(p1, p2):
            print(f"  Character selection complete at frame {i}")
            break
    else:
        print(f"  WARNING: Selection not fully confirmed in {max_frames} frames")

    p1_char, p1_sa, p2_char, p2_sa = sim.results()
    p1_name = CHAR_NAMES.get(p1_char, f"ID{p1_char}")
    p2_name = CHAR_NAMES.get(p2_char, f"ID{p2_char}")
    print(f"  Detected: P1={p1_name} SA{p1_sa} vs P2={p2_name} SA{p2_sa}")
    return p1_char, p1_sa, p2_char, p2_sa


# -- Main conversion ---------------------------------------------------------


def load_ggpo_inputs(replay_dir: Path) -> list[tuple[int, int]]:
    """Read all type-13 binaries in message order, return [(p1, p2), ...].

    Only reads the first record_count × record_size bytes from each message,
    ignoring any trailing GGPO sync/savestate data in oversized messages.
    """
    summary_path = replay_dir / "summary.json"
    if not summary_path.exists():
        print(f"ERROR: {summary_path} not found", file=sys.stderr)
        sys.exit(1)

    summary = json.loads(summary_path.read_text(encoding="utf-8"))

    # Build a lookup: msg_index → (record_count, record_size)
    msg_info = {}
    for m in summary["messages"]:
        if m.get("type") == -13:
            msg_info[m["index"]] = (
                m.get("record_count", 60),
                m.get("record_size", RECORD_SIZE),
            )

    minus13_indices = sorted(msg_info.keys())
    print(f"Found {len(minus13_indices)} type-13 messages in summary")

    all_inputs: list[tuple[int, int]] = []
    garbage_count = 0
    for idx in minus13_indices:
        bin_path = replay_dir / f"msg_{idx:04d}_minus13.bin"
        if not bin_path.exists():
            print(f"  WARNING: {bin_path.name} missing, skipping")
            continue
        data = bin_path.read_bytes()
        record_count, record_size = msg_info[idx]

        # Only parse the declared number of records (ignore trailing sync data)
        num_records = min(record_count, len(data) // record_size)
        for r in range(num_records):
            p1, p2 = parse_record(data, r * record_size)
            # Skip individual garbage records instead of truncating the stream
            if is_garbage_input(p1) or is_garbage_input(p2):
                garbage_count += 1
                all_inputs.append((0, 0))  # Replace with neutral
            else:
                all_inputs.append((p1, p2))

    print(f"Total frames: {len(all_inputs)}")
    if garbage_count:
        print(f"Replaced {garbage_count} garbage records with neutral inputs")
    return all_inputs


def find_activity_start(inputs: list[tuple[int, int]], end: int) -> int:
    """Find the first frame where either player presses a button."""
    for i in range(end):
        p1, p2 = inputs[i]
        if p1 != 0 or p2 != 0:
            return i
    return 0


def get_metadata(replay_dir: Path) -> dict:
    """Extract player names and game info from summary.json."""
    summary = json.loads((replay_dir / "summary.json").read_text(encoding="utf-8"))
    meta = {
        "game": summary.get("game", "sfiii3nr1"),
        "emulator": summary.get("emulator", "fbneo"),
        "token": summary.get("token", ""),
    }

    # Try to decode player names from type-3 metadata
    for m in summary["messages"]:
        if m.get("type") == 3:
            trailing = m.get("trailing_hex", "")
            if trailing:
                try:
                    raw = bytes.fromhex(trailing)
                    # Decode as null-terminated strings
                    parts = raw.split(b"\x00")
                    names = [
                        p.decode("utf-8", errors="replace")
                        for p in parts
                        if p and len(p) > 2
                    ]
                    meta["players"] = names
                except Exception:
                    pass
            if "strings" in m and m["strings"]:
                meta["players"] = m["strings"]
    return meta


def convert(
    replay_dir: Path,
    output_path: Path | None,
    p1_char: int | None,
    p2_char: int | None,
    p1_sa: int | None,
    p2_sa: int | None,
    trim_idle: bool,
) -> Path:
    """Convert GGPO type-13 data to replay_inputs.py-compatible CSV."""
    meta = get_metadata(replay_dir)
    print(f"\nGame: {meta['game']}")
    if "players" in meta:
        print(f"Players: {meta['players']}")

    # Load all inputs (garbage records already replaced with neutral)
    inputs = load_ggpo_inputs(replay_dir)
    if not inputs:
        print("ERROR: No input data found!", file=sys.stderr)
        sys.exit(1)

    # Auto-detect character/SA if not provided
    if p1_char is None or p2_char is None or p1_sa is None or p2_sa is None:
        print("\nAuto-detecting characters and Super Arts from inputs...")
        det_char1, det_sa1, det_char2, det_sa2 = detect_chars_from_inputs(inputs)
        if p1_char is None:
            p1_char = det_char1
        if p2_char is None:
            p2_char = det_char2
        if p1_sa is None:
            p1_sa = det_sa1
        if p2_sa is None:
            p2_sa = det_sa2

    # Optionally trim leading idle frames
    start = 0
    if trim_idle:
        activity = find_activity_start(inputs, len(inputs))
        if activity > 0:
            # Keep a small buffer before first input for banner sync
            start = max(0, activity - 60)
            print(f"Trimmed {start} idle frames (activity starts at {activity})")

    clean_inputs = inputs[start:]
    p1_name = CHAR_NAMES.get(p1_char, f"ID{p1_char}")
    p2_name = CHAR_NAMES.get(p2_char, f"ID{p2_char}")
    print(f"Output: {len(clean_inputs)} frames ({len(clean_inputs) / 60:.1f}s)")
    print(f"  P1={p1_name} SA{p1_sa}, P2={p2_name} SA{p2_sa}")

    # Build output path
    if output_path is None:
        output_path = replay_dir / f"{p1_name}_vs_{p2_name}_full.csv"

    # Write CSV matching the format expected by replay_inputs.py
    fieldnames = [
        "frame",
        "p1_input",
        "p2_input",
        "match_state",
        "is_in_match",
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
        "live",
        "p1_sa",
        "p2_sa",
    ]

    with open(output_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        for i, (p1, p2) in enumerate(clean_inputs):
            writer.writerow(
                {
                    "frame": i,
                    "p1_input": p1,
                    "p2_input": p2,
                    "match_state": 2,
                    "is_in_match": 1,
                    "timer": max(1, 99 - i // 60),
                    "p1_hp": 160,
                    "p2_hp": 160,
                    "p1_x": 0,
                    "p1_y": 0,
                    "p2_x": 0,
                    "p2_y": 0,
                    "p1_facing": 0,
                    "p2_facing": 1,
                    "p1_meter": 0,
                    "p2_meter": 0,
                    "p1_stun": 0,
                    "p2_stun": 0,
                    "p1_char": p1_char,
                    "p2_char": p2_char,
                    "live": 1,
                    "p1_sa": p1_sa,
                    "p2_sa": p2_sa,
                }
            )

    print(f"\nCSV written: {output_path}")
    print("\nTo replay in 3SX:")
    print(f"  python tools/fightcade_replays/replay_inputs.py {output_path}")
    return output_path


# -- CLI ---------------------------------------------------------------------


def main():
    char_help = ", ".join(f"{k}={v}" for k, v in CHAR_NAMES.items())

    parser = argparse.ArgumentParser(
        description="Convert GGPO replay download to CSV for replay_inputs.py",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"Character IDs: {char_help}",
    )
    parser.add_argument(
        "replay_dir",
        type=Path,
        help="Directory containing summary.json and msg_*_minus13.bin files",
    )
    parser.add_argument("-o", "--output", type=Path, help="Output CSV path")
    parser.add_argument(
        "--p1-char",
        type=int,
        default=None,
        help="P1 character ID (auto-detected if omitted)",
    )
    parser.add_argument(
        "--p2-char",
        type=int,
        default=None,
        help="P2 character ID (auto-detected if omitted)",
    )
    parser.add_argument(
        "--p1-sa",
        type=int,
        default=None,
        help="P1 Super Art 1-3 (auto-detected if omitted)",
    )
    parser.add_argument(
        "--p2-sa",
        type=int,
        default=None,
        help="P2 Super Art 1-3 (auto-detected if omitted)",
    )
    parser.add_argument(
        "--trim-idle",
        action="store_true",
        help="Trim leading idle frames (before first input)",
    )

    args = parser.parse_args()

    if not args.replay_dir.is_dir():
        print(f"ERROR: {args.replay_dir} is not a directory", file=sys.stderr)
        sys.exit(1)

    convert(
        replay_dir=args.replay_dir,
        output_path=args.output,
        p1_char=args.p1_char,
        p2_char=args.p2_char,
        p1_sa=args.p1_sa,
        p2_sa=args.p2_sa,
        trim_idle=args.trim_idle,
    )


if __name__ == "__main__":
    main()
