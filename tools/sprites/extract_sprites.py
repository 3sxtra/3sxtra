#!/usr/bin/env python3
"""
Unified CPS3 sprite extractor — stage sprites AND character animations.

Extracts sprites from AFS archive using the engine's texgrpdat metadata,
palette merging, and rendering mode dispatch. Supports stage backgrounds,
system/effect sprites, and full character animation sheets.

Usage:
  python extract_sprites.py stages all                     # all stage/effect groups
  python extract_sprites.py stages --group 52              # specific group
  python extract_sprites.py stages --stage 0               # all groups for a stage
  python extract_sprites.py stages --list                  # list known groups

  python extract_sprites.py chars all                      # all characters
  python extract_sprites.py chars ryu                      # specific character
  python extract_sprites.py chars ryu --frames 0-10        # frame range
  python extract_sprites.py chars --info                   # list characters

  python extract_sprites.py stages --per-character         # per-char effect variants
"""

import argparse
import struct
import os
import sys
import json
import time
from PIL import Image

from sprite_common import (
    read_afs, read_afs_file,
    TEXGRPDAT, STAGE_PAL_AFS, STAGE_TILE_AFS,
    TOTAL_BANKS,
)

from sprite_compositor import (
    SPRITE_PAL_BASE,
    load_engine_palette_map, afs_entry_to_stage, get_rendering_mode,
    load_colorram_dump, build_stage_colorram, build_char_colorram,
    load_character_palette, load_colcd_map,
    extract_stage_frame, extract_char_frame,
)


# ══════════════════════════════════════════════════════════════════════════════
# Data Tables
# ══════════════════════════════════════════════════════════════════════════════


# Character ID → name mapping
CHARACTER_NAMES = {
    0: "gill", 1: "alex", 2: "ryu", 3: "yun", 4: "dudley",
    5: "necro", 6: "hugo", 7: "ibuki", 8: "elena", 9: "oro",
    10: "yang", 11: "ken", 12: "sean", 13: "urien", 14: "akuma",
    15: "chunli", 16: "makoto", 17: "q", 18: "twelve", 19: "remy",
}

# Character palette AFS entry numbers (from color_file[0..19] in color3rd.c).
CHARACTER_PAL_AFS = {
    0: 0x5B7, 1: 0x5BA, 2: 0x5BE, 3: 0x5C1, 4: 0x5C5,
    5: 0x5C8, 6: 0x5CC, 7: 0x5D1, 8: 0x5D5, 9: 0x5D9,
    10: 0x5DC, 11: 0x5E0, 12: 0x5E4, 13: 0x5E8, 14: 0x5EC,
    15: 0x5F0, 16: 0x5F3, 17: 0x5F6, 18: 0x5F9, 19: 0x5FD,
}

# Character animation data (from texgrpdat in texgroup.c)
CHARACTERS = {
    "gill":    {"apfn": 1460, "pal_apfn": 1463, "to_tex": 210820,  "num_of_1st": 0},
    "alex":    {"apfn": 1465, "pal_apfn": 1466, "to_tex": 116432,  "num_of_1st": 1568},
    "ryu":     {"apfn": 1468, "pal_apfn": 1470, "to_tex": 72828,   "num_of_1st": 2592},
    "yun":     {"apfn": 1472, "pal_apfn": 1473, "to_tex": 114816,  "num_of_1st": 3552},
    "dudley":  {"apfn": 1476, "pal_apfn": 1477, "to_tex": 110728,  "num_of_1st": 4992},
    "necro":   {"apfn": 1479, "pal_apfn": 1480, "to_tex": 116636,  "num_of_1st": 6144},
    "hugo":    {"apfn": 1483, "pal_apfn": 1484, "to_tex": 158400,  "num_of_1st": 7392},
    "ibuki":   {"apfn": 1487, "pal_apfn": 1489, "to_tex": 151744,  "num_of_1st": 8384},
    "elena":   {"apfn": 1492, "pal_apfn": 1493, "to_tex": 142292,  "num_of_1st": 10208},
    "oro":     {"apfn": 1495, "pal_apfn": 1497, "to_tex": 137680,  "num_of_1st": 11776},
    "yang":    {"apfn": 1499, "pal_apfn": 1500, "to_tex": 116892,  "num_of_1st": 13280},
    "ken":     {"apfn": 1502, "pal_apfn": 1504, "to_tex": 71900,   "num_of_1st": 14656},
    "sean":    {"apfn": 1506, "pal_apfn": 1508, "to_tex": 80596,   "num_of_1st": 15712},
    "urien":   {"apfn": 1510, "pal_apfn": 1512, "to_tex": 135428,  "num_of_1st": 16800},
    "akuma":   {"apfn": 1514, "pal_apfn": 1516, "to_tex": 116116,  "num_of_1st": 18272},
    "chunli":  {"apfn": 1518, "pal_apfn": 1520, "to_tex": 144584,  "num_of_1st": 19456},
    "makoto":  {"apfn": 1522, "pal_apfn": 1523, "to_tex": 177724,  "num_of_1st": 21120},
    "q":       {"apfn": 1525, "pal_apfn": 1526, "to_tex": 222124,  "num_of_1st": 23008},
    "twelve":  {"apfn": 1528, "pal_apfn": 1529, "to_tex": 131348,  "num_of_1st": 24704},
    "remy":    {"apfn": 1531, "pal_apfn": 1533, "to_tex": 125420,  "num_of_1st": 25856},
}

# Effect groups that use per-character palettes at runtime.
PER_CHARACTER_EFFECT_GROUPS = set(range(61, 77))  # G61-G76

# Stage sprite groups (Mode 33: mlt_obj_disp).
# All other groups use Mode 18 CP3 (mlt_obj_trans_cp3).
STAGE_SPRITE_GROUPS = {
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 58, 59,
    89, 90, 91, 92, 93, 94
}

# Per-group default colcd (palette base).
GROUP_DEFAULT_COLCD = {
    # ── System / UI groups ──
    20: 144,  22: 144,  98: 144,
    25: 300,  24: 65,   34: 10,
    # ── Effect/system groups ── colcd 0x1AC (428)
    26: 428, 29: 428, 32: 428, 33: 428, 37: 428, 82: 428,
    61: 428, 62: 428, 63: 428, 64: 428, 65: 428, 66: 428, 67: 428,
    68: 428, 69: 428, 70: 428, 71: 428, 72: 428, 73: 428, 74: 428,
    75: 428, 76: 428, 77: 428, 78: 428, 79: 428, 80: 428, 81: 428,
    # ── Stage groups ── colcd 300
    42: 300, 43: 300, 44: 300, 45: 300, 46: 300, 47: 300,
    48: 300, 49: 300, 50: 300, 51: 300, 52: 300, 53: 300,
    54: 300, 55: 300, 56: 300, 58: 300, 59: 300,
    89: 300, 90: 300, 91: 300, 92: 300, 93: 300, 94: 300,
}


# ══════════════════════════════════════════════════════════════════════════════
# Group / Character Extraction
# ══════════════════════════════════════════════════════════════════════════════


def extract_group(afs_path, entries, grp_entry, pal_banks, output_dir,
                  colcd_override=None, subdir_suffix=None,
                  colcd_map=None, engine_pal_map=None, dry_run=False,
                  verbose=False):
    """Extract all CG frames for a texgrpdat group. Returns count extracted."""
    group_idx = grp_entry.group_idx
    num_of_1st = grp_entry.num_of_1st
    apfn = grp_entry.apfn
    to_tex = grp_entry.to_tex
    desc = grp_entry.desc

    if apfn < 0 or apfn >= len(entries):
        return 0

    data = read_afs_file(afs_path, entries[apfn])
    if len(data) < 4:
        return 0

    num_frames = struct.unpack_from("<I", data, 0)[0] // 4
    if num_frames == 0 or num_frames > 10000:
        return 0
    if to_tex >= len(data):
        print(f"  WARNING: to_tex ({to_tex}) >= data size ({len(data)}), skipping")
        return 0

    grp_dir = os.path.join(output_dir, f"group_{group_idx:02d}")
    if subdir_suffix:
        grp_dir = os.path.join(grp_dir, subdir_suffix)

    if dry_run:
        print(f"    [dry-run] Would extract {num_frames} frames → {grp_dir}/")
        return num_frames

    os.makedirs(grp_dir, exist_ok=True)

    extracted = 0
    if colcd_map is None:
        colcd_map = {}
    if engine_pal_map is None:
        engine_pal_map = {}
    stage = afs_entry_to_stage(apfn)
    group_default = GROUP_DEFAULT_COLCD.get(group_idx, SPRITE_PAL_BASE)
    mode = get_rendering_mode(group_idx, STAGE_SPRITE_GROUPS)

    for fi in range(num_frames):
        cg_number = num_of_1st + fi

        # 3-tier colcd resolution (CSV preferred — has mode info)
        colcd = None
        cg_mode = mode  # default: group-level mode

        # 1. Runtime CSV — authoritative, per-CG colcd AND mode
        if stage is not None:
            entry = colcd_map.get((stage, cg_number))
            if entry is not None:
                colcd, cg_mode = entry
        if colcd is None:
            # Search any stage for this CG in CSV
            for key, val in colcd_map.items():
                if key[1] == cg_number:
                    colcd, cg_mode = val
                    break

        # 2. Static engine map (no mode info, keep group default)
        if colcd is None:
            colcd = engine_pal_map.get(cg_number)

        # 3. Group default
        if colcd is None:
            colcd = colcd_override if colcd_override is not None else group_default

        if verbose:
            print(f"      CG {cg_number}: colcd={colcd} mode={cg_mode}")

        img = extract_stage_frame(data, to_tex, fi, pal_banks,
                                  colcd_base=colcd, rendering_mode=cg_mode)
        if img is not None:
            img.save(os.path.join(grp_dir, f"sprite_{group_idx}_{cg_number}.png"))
            extracted += 1

    meta = {
        "group_index": group_idx, "description": desc, "afs_entry": apfn,
        "num_of_1st": num_of_1st, "to_tex": to_tex,
        "num_frames": num_frames, "extracted": extracted,
    }
    with open(os.path.join(grp_dir, "metadata.json"), "w") as f:
        json.dump(meta, f, indent=2)
    return extracted


def extract_character(afs_path, char_name, char, frame_range, output_dir,
                      pal_idx, engine_pal_map=None, colcd_map=None):
    """Extract frames for one character. Returns (extracted_count, total_frames)."""
    entries = read_afs(afs_path)
    data = read_afs_file(afs_path, entries[char["apfn"]])
    num_frames = struct.unpack_from("<I", data, 0)[0] // 4

    if frame_range == "all":
        frames = list(range(num_frames))
    elif "-" in frame_range:
        parts = frame_range.split("-")
        if len(parts) != 2:
            raise ValueError(
                f"Invalid frame range '{frame_range}': expected N-M format"
            )
        try:
            start, end = int(parts[0]), int(parts[1])
        except ValueError:
            raise ValueError(
                f"Invalid frame range '{frame_range}': "
                f"start and end must be integers"
            )
        frames = list(range(start, end + 1))
    else:
        try:
            frames = [int(frame_range)]
        except ValueError:
            raise ValueError(
                f"Invalid frame number '{frame_range}': must be an integer"
            )

    frames = [fi for fi in frames if fi < num_frames]
    os.makedirs(output_dir, exist_ok=True)

    print(f"  {char_name}: {num_frames} frames "
          f"(cg {char['num_of_1st']}\u2013{char['num_of_1st'] + num_frames - 1}), "
          f"palette {pal_idx}")

    t0 = time.perf_counter()
    pal_data = read_afs_file(afs_path, entries[char["pal_apfn"]])
    colorram = build_char_colorram(afs_path, entries, pal_data, pal_idx)

    if engine_pal_map is None:
        engine_pal_map = load_engine_palette_map()
    if colcd_map is None:
        colcd_map = load_colcd_map()

    count = 0
    for fi in frames:
        img = extract_char_frame(data, char["to_tex"], fi, colorram,
                                 base_cg=char["num_of_1st"],
                                 engine_pal_map=engine_pal_map,
                                 colcd_map=colcd_map)
        if img is None:
            continue
        cg = char["num_of_1st"] + fi
        img.save(os.path.join(output_dir, f"sprite_{cg}.png"))
        count += 1

    elapsed = time.perf_counter() - t0
    print(f"    -> {count} frames in {elapsed:.1f}s to {output_dir}/")
    return count, num_frames


# ══════════════════════════════════════════════════════════════════════════════
# CLI
# ══════════════════════════════════════════════════════════════════════════════


def _default_afs_path():
    return os.environ.get(
        "SF33RD_AFS",
        r"C:\Users\dov\AppData\Roaming\CrowdedStreet\3SX\resources\SF33RD.AFS"
    )


def _open_afs(afs_path):
    """Open and validate AFS file, with user-friendly error on failure."""
    try:
        return read_afs(afs_path)
    except FileNotFoundError:
        print(f"ERROR: AFS file not found: {afs_path}")
        print(f"Set SF33RD_AFS env var or use --afs to specify the path.")
        sys.exit(1)
    except ValueError as e:
        print(f"ERROR: {e}")
        sys.exit(1)


def cmd_stages(args):
    """Handle the 'stages' subcommand."""
    afs_path = args.afs
    output_dir = args.output
    verbose = args.verbose
    dry_run = args.dry_run

    if args.list_only:
        print(f"{'Grp':>4} {'CG Range':>14} {'AFS':>5} {'to_tex':>7}  {'Stage':>6}  Description")
        print("─" * 75)
        afs_entries = None
        if os.path.exists(afs_path):
            try:
                afs_entries = read_afs(afs_path)
            except Exception:
                pass
        for grp in TEXGRPDAT:
            stage = afs_entry_to_stage(grp.apfn)
            stage_str = f"  {stage:>2}" if stage is not None else "   -"
            end_str = "???"
            if afs_entries and 0 <= grp.apfn < len(afs_entries):
                try:
                    data = read_afs_file(afs_path, afs_entries[grp.apfn])
                    if len(data) >= 4:
                        nf = struct.unpack_from("<I", data, 0)[0] // 4
                        end_str = str(grp.num_of_1st + nf - 1)
                except Exception:
                    pass
            print(f" {grp.group_idx:>3}  {grp.num_of_1st:>5}-{end_str:<5}    {grp.apfn:>5}  {grp.to_tex:>6}  {stage_str}    {grp.desc}")
        return

    entries = _open_afs(afs_path)
    os.makedirs(output_dir, exist_ok=True)

    colcd_map = load_colcd_map()
    engine_pal_map = load_engine_palette_map()

    groups_to_extract = []
    for grp in TEXGRPDAT:
        if args.group is not None and grp.group_idx != args.group:
            continue
        if args.stage is not None:
            stage = afs_entry_to_stage(grp.apfn)
            if stage != args.stage:
                continue
        groups_to_extract.append(grp)

    if not groups_to_extract:
        print("No matching groups found.")
        return

    print(f"\n🎬 Stage Sprite Extraction")
    print(f"📁 Output: {output_dir}")
    print(f"📊 Groups: {len(groups_to_extract)}")
    if dry_run:
        print(f"⏩ Mode: DRY RUN (no files will be written)")
    print(f"{'─' * 60}")

    total_frames = 0
    for grp in groups_to_extract:
        stage = afs_entry_to_stage(grp.apfn)

        stage_pal_afs = STAGE_PAL_AFS.get(stage) if stage is not None else None
        pal_banks = build_stage_colorram(afs_path, entries, stage_pal_afs)

        # Overlay runtime ColorRAM dump (authoritative source)
        rt_dump = None
        if not dry_run:
            if stage is not None:
                rt_dump = load_colorram_dump(stage)
                if rt_dump and verbose:
                    print(f"    Merging runtime ColorRAM dump for stage {stage:02d}")
            if rt_dump is None:
                rt_dump = load_colorram_dump(0)
                if rt_dump and verbose:
                    print(f"    Merging runtime ColorRAM dump (fallback stage 00)")

            if rt_dump:
                for bank_idx in range(512):
                    has_data = any(
                        rt_dump[bank_idx][c][0] or rt_dump[bank_idx][c][1] or rt_dump[bank_idx][c][2]
                        for c in range(1, min(16, len(rt_dump[bank_idx])))
                    )
                    if has_data:
                        pal_banks[bank_idx] = rt_dump[bank_idx]

        stage_str = f"stage {stage:02d}" if stage is not None else "effect"
        print(f"\n  Group {grp.group_idx:02d} ({stage_str}): {grp.desc}")
        if verbose:
            print(f"    AFS={grp.apfn} CG_start={grp.num_of_1st} to_tex={grp.to_tex}")

        per_char = args.per_character and grp.group_idx in PER_CHARACTER_EFFECT_GROUPS

        if per_char:
            for char_id, char_name in sorted(CHARACTER_NAMES.items()):
                char_pal = [bank[:] for bank in pal_banks]
                load_character_palette(afs_path, entries, char_id, char_pal,
                                       CHARACTER_PAL_AFS)
                extracted = extract_group(afs_path, entries, grp, char_pal,
                                         output_dir, colcd_override=4,
                                         subdir_suffix=char_name,
                                         colcd_map=colcd_map,
                                         engine_pal_map=engine_pal_map,
                                         dry_run=dry_run, verbose=verbose)
                total_frames += extracted
            print(f"    Extracted: {len(CHARACTER_NAMES)} character variants")
        else:
            extracted = extract_group(afs_path, entries, grp, pal_banks,
                                     output_dir, colcd_map=colcd_map,
                                     engine_pal_map=engine_pal_map,
                                     dry_run=dry_run, verbose=verbose)
            total_frames += extracted
            print(f"    Extracted: {extracted} frames")

    print(f"\n{'─' * 60}")
    print(f"🎉 Total: {total_frames} frames from {len(groups_to_extract)} groups")
    print(f"   Output: {output_dir}/")


def cmd_chars(args):
    """Handle the 'chars' subcommand."""
    afs_path = args.afs

    if args.info:
        entries = _open_afs(afs_path)
        print(f"{'Character':<10} {'Frames':>6} {'CG Start':>9} {'CG End':>9} {'AFS#':>5}")
        print("-" * 45)
        total = 0
        for name in sorted(CHARACTERS.keys(), key=lambda k: CHARACTERS[k]["num_of_1st"]):
            char = CHARACTERS[name]
            data = read_afs_file(afs_path, entries[char["apfn"]])
            num_frames = struct.unpack_from("<I", data, 0)[0] // 4
            cg_start = char["num_of_1st"]
            cg_end = cg_start + num_frames - 1
            print(f"{name:<10} {num_frames:>6} {cg_start:>9} {cg_end:>9} {char['apfn']:>5}")
            total += num_frames
        print("-" * 45)
        print(f"{'TOTAL':<10} {total:>6}")
        return

    char_name = args.character.lower()
    frame_range = args.frames
    output_dir = args.output
    pal_idx = args.palette

    if char_name == "all":
        chars = sorted(CHARACTERS.keys(), key=lambda k: CHARACTERS[k]["num_of_1st"])
    elif char_name in CHARACTERS:
        chars = [char_name]
    else:
        print(f"Unknown character: {char_name}")
        print(f"Available: {', '.join(sorted(CHARACTERS.keys()))}")
        sys.exit(1)

    engine_pal_map = load_engine_palette_map()
    grand_total = 0
    for name in chars:
        char = CHARACTERS[name]
        char_dir = os.path.join(output_dir, name) if len(chars) > 1 else output_dir
        try:
            count, _ = extract_character(afs_path, name, char, frame_range,
                                         char_dir, pal_idx,
                                         engine_pal_map=engine_pal_map)
        except ValueError as e:
            print(f"ERROR: {e}")
            sys.exit(1)
        grand_total += count

    if len(chars) > 1:
        print(f"\nTotal: {grand_total} frames across {len(chars)} characters")


def main():
    parser = argparse.ArgumentParser(
        description="CPS3 sprite extractor — stages, effects, and characters."
    )
    parser.add_argument("--afs", default=_default_afs_path(),
                        help="Path to SF33RD.AFS")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Show detailed palette resolution and debug info")
    sub = parser.add_subparsers(dest="command")

    # ── stages subcommand ──
    p_stages = sub.add_parser("stages", help="Extract stage/effect sprite groups")
    p_stages.add_argument("action", nargs="?", default=None,
                          help="'all' to extract all groups")
    p_stages.add_argument("--group", type=int, default=None,
                          help="Extract a specific group index")
    p_stages.add_argument("--stage", type=int, default=None,
                          help="Extract all groups for a stage index")
    p_stages.add_argument("--list", dest="list_only", action="store_true",
                          help="List all known groups and exit")
    p_stages.add_argument("--output", default="output/stage_sprites",
                          help="Output directory")
    p_stages.add_argument("--per-character", dest="per_character",
                          action="store_true",
                          help="Extract per-character variants for effect groups")
    p_stages.add_argument("--dry-run", dest="dry_run", action="store_true",
                          help="Show what would be extracted without writing files")

    # ── chars subcommand ──
    p_chars = sub.add_parser("chars", help="Extract character animation sprites")
    p_chars.add_argument("character", nargs="?", default=None,
                         help="Character name or 'all'")
    p_chars.add_argument("--info", action="store_true",
                         help="List all characters with frame counts")
    p_chars.add_argument("--frames", default="all",
                         help="Frame range: N, N-M, or 'all' (default: all)")
    p_chars.add_argument("--output", default="output/sprites",
                         help="Output directory")
    p_chars.add_argument("--palette", type=int, default=0,
                         help="Costume palette index 0-13 (default: 0)")

    args = parser.parse_args()

    # Propagate verbose to subcommands
    if not hasattr(args, 'verbose'):
        args.verbose = False
    if not hasattr(args, 'dry_run'):
        args.dry_run = False

    if args.command == "stages":
        cmd_stages(args)
    elif args.command == "chars":
        if not args.info and not args.character:
            p_chars.print_help()
            sys.exit(1)
        cmd_chars(args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
