#!/usr/bin/env python3
"""
Offline Asset Compiler (Phase 1)
Builds optimized Texture Atlases (Spritesheets) and JSON metadata from CPS3 AFS archives.
Replaces the legacy `.bin` formats with direct PNG/JSON formats for engine modernization.

Usage:
  python build_atlases.py chars all --output output/atlases/chars
  python build_atlases.py stages all --output output/atlases/stages
"""

import argparse
import json
import os
import sys
import struct
from collections import namedtuple

from PIL import Image

# Import existing extraction logic
from sprite_common import read_afs_file, TEXGRPDAT, STAGE_PAL_AFS
from sprite_compositor import (
    load_colcd_map,
    load_engine_palette_map,
    build_stage_colorram,
    build_char_colorram,
    extract_stage_frame,
    _parse_tiles,
    _compute_positions,
    _composite,
    afs_entry_to_stage,
    get_rendering_mode,
    SPRITE_PAL_BASE,
    load_colorram_dump,
)

# Reuse definitions from extract_sprites.py
from extract_sprites import (
    CHARACTERS,
    STAGE_SPRITE_GROUPS,
    GROUP_DEFAULT_COLCD,
    _default_afs_path,
    _open_afs,
)

AtlasFrame = namedtuple("AtlasFrame", "cg_num img pivot_x pivot_y width height")


class TextureAtlasBuilder:
    def __init__(self, max_width=2048, max_height=2048):
        self.max_width = max_width
        self.max_height = max_height
        self.padding = 2

        # Current state
        self.frames = []  # List of AtlasFrame

    def add_frame(self, cg_num, img, pivot_x, pivot_y):
        self.frames.append(
            AtlasFrame(cg_num, img, pivot_x, pivot_y, img.width, img.height)
        )

    def build(self):
        """Simple shelf bin-packing algorithm to build atlases."""
        # Sort by height descending
        sorted_frames = sorted(self.frames, key=lambda f: f.height, reverse=True)

        atlases = []  # List of (Image, dict_of_metadata)

        current_img = Image.new("RGBA", (self.max_width, self.max_height), (0, 0, 0, 0))
        current_meta = {}

        x, y = 0, 0
        shelf_height = 0

        for f in sorted_frames:
            if x + f.width + self.padding > self.max_width:
                # Move to next shelf
                y += shelf_height + self.padding
                x = 0
                shelf_height = 0

            if y + f.height + self.padding > self.max_height:
                # Move to next atlas
                atlases.append((current_img, current_meta))
                current_img = Image.new(
                    "RGBA", (self.max_width, self.max_height), (0, 0, 0, 0)
                )
                current_meta = {}
                x, y = 0, 0
                shelf_height = 0

            # Paste image
            current_img.paste(f.img, (x, y))

            # Record metadata
            current_meta[f.cg_num] = {
                "x": x,
                "y": y,
                "w": f.width,
                "h": f.height,
                "pivot_x": f.pivot_x,
                "pivot_y": f.pivot_y,
            }

            x += f.width + self.padding
            shelf_height = max(shelf_height, f.height)

        if current_meta:
            atlases.append((current_img, current_meta))

        return atlases


def _extract_frame_with_pivot(
    data,
    to_tex,
    frame_idx,
    pal_banks,
    is_char=False,
    colcd_base=None,
    rendering_mode=18,
    engine_pal_map=None,
    colcd_map=None,
    base_cg=0,
):
    """Custom extraction that captures the pivot points (mnx, mny) of the composite."""
    tiles = _parse_tiles(data, to_tex, frame_idx)
    if not tiles:
        return None, 0, 0

    if not is_char and all(all(b == 0 for b in t.px) for t in tiles):
        return None, 0, 0

    pts, all_xflip = _compute_positions(tiles)

    # Calculate Pivot (the top-left offset of the bounding box relative to origin (0,0))
    mnx = min(p[0] for p in pts)
    mny = min(p[1] for p in pts)

    if is_char:
        cg = base_cg + frame_idx

        resolved_colcd_base = None
        colcd_mode = 17
        for key, val in (colcd_map or {}).items():
            if key[1] == cg:
                csv_colcd, colcd_mode = val
                resolved_colcd_base = csv_colcd if colcd_mode == 18 else 0
                break

        if resolved_colcd_base is None:
            resolved_colcd_base = (engine_pal_map or {}).get(cg, 0)

        def palette_fn(attr, tile_idx):
            if colcd_mode == 18:
                bank_idx = (attr & 0x1FF) + resolved_colcd_base
            else:
                bank_idx = resolved_colcd_base
            bank_idx %= len(pal_banks)
            if bank_idx >= len(pal_banks):
                return [(0, 0, 0, 0)] * 256
            return pal_banks[bank_idx]

        img = _composite(tiles, pts, all_xflip, palette_fn)
    else:
        base = colcd_base if colcd_base is not None else SPRITE_PAL_BASE

        def palette_fn(attr, tile_idx):
            if rendering_mode == 33:
                bank_idx = (attr & 0xF) + base
            else:
                bank_idx = (attr & 0x1FF) + base
            bank_idx %= len(pal_banks)
            if bank_idx >= len(pal_banks):
                return [(0, 0, 0, 0)] * 256
            return pal_banks[bank_idx]

        img = _composite(tiles, pts, all_xflip, palette_fn)

    return img, mnx, mny


def process_character(afs_path, char_name, char, output_dir, pal_idx=0):
    """Processes a character into a set of atlases."""
    entries = _open_afs(afs_path)
    data = read_afs_file(afs_path, entries[char["apfn"]])
    num_frames = struct.unpack_from("<I", data, 0)[0] // 4

    os.makedirs(output_dir, exist_ok=True)
    print(f"  Building Atlases for {char_name} ({num_frames} frames)...")

    pal_data = read_afs_file(afs_path, entries[char["pal_apfn"]])
    colorram = build_char_colorram(afs_path, entries, pal_data, pal_idx)

    engine_pal_map = load_engine_palette_map()
    colcd_map = load_colcd_map()

    builder = TextureAtlasBuilder()

    count = 0
    for fi in range(num_frames):
        img, px, py = _extract_frame_with_pivot(
            data,
            char["to_tex"],
            fi,
            colorram,
            is_char=True,
            engine_pal_map=engine_pal_map,
            colcd_map=colcd_map,
            base_cg=char["num_of_1st"],
        )
        if img:
            cg = char["num_of_1st"] + fi
            builder.add_frame(cg, img, px, py)
            count += 1

    atlases = builder.build()

    master_metadata = {
        "character": char_name,
        "base_cg": char["num_of_1st"],
        "num_frames": num_frames,
        "atlases": [],
    }

    for i, (img, meta) in enumerate(atlases):
        img_filename = f"{char_name}_atlas_{i:02d}.png"
        img_path = os.path.join(output_dir, img_filename)
        img.save(img_path)

        master_metadata["atlases"].append({"image": img_filename, "frames": meta})

    with open(os.path.join(output_dir, f"{char_name}_metadata.json"), "w") as f:
        json.dump(master_metadata, f, indent=2)

    print(f"    -> Generated {len(atlases)} atlases for {count} valid frames.")
    return count


def process_stage_group(afs_path, entries, grp, output_dir, engine_pal_map, colcd_map):
    """Processes a stage group into a set of atlases."""
    group_idx = grp.group_idx

    if grp.apfn < 0 or grp.apfn >= len(entries):
        return 0

    data = read_afs_file(afs_path, entries[grp.apfn])
    if len(data) < 4:
        return 0

    num_frames = struct.unpack_from("<I", data, 0)[0] // 4
    if num_frames == 0 or num_frames > 10000:
        return 0

    if grp.to_tex >= len(data):
        return 0

    stage = afs_entry_to_stage(grp.apfn)

    # Groups skipped by previous scripts because they don't cleanly map their apfn,
    # but they strictly belong to the Bonus modes.
    if grp.group_idx == 32:
        stage = 20  # Bonus Car
    elif grp.group_idx == 33:
        stage = 21  # Bonus Parry

    stage_pal_afs = STAGE_PAL_AFS.get(stage) if stage is not None else None
    pal_banks = build_stage_colorram(afs_path, entries, stage_pal_afs, apply_clut=False)

    # Load rt_dump when we have a specific stage, OR for groups without stage
    # mapping that need runtime palette data (player banks 0-31, or stage banks).
    # groups 32/33 are excluded — they have hardcoded overrides below.
    RT_DUMP_FALLBACK_GROUPS = (
        set(range(61, 77))  # Effect groups (CSV: colcd {0,4,8,12}, mode 33)
    )
    CONSENSUS_FALLBACK_GROUPS = {24, 25, 82}  # UI/System groups

    if stage is not None:
        rt_dump = load_colorram_dump(stage)
    elif group_idx in CONSENSUS_FALLBACK_GROUPS:
        from sprite_compositor import load_consensus_colorram_dump

        rt_dump = load_consensus_colorram_dump()
    elif group_idx in RT_DUMP_FALLBACK_GROUPS:
        rt_dump = load_colorram_dump(0)  # Stage 0 dump has player palette data
    else:
        rt_dump = None

    if rt_dump:
        for bank_idx in range(512):
            has_data = any(
                rt_dump[bank_idx][c][0]
                or rt_dump[bank_idx][c][1]
                or rt_dump[bank_idx][c][2]
                for c in range(1, min(16, len(rt_dump[bank_idx])))
            )
            if has_data:
                pal_banks[bank_idx] = rt_dump[bank_idx]

    os.makedirs(output_dir, exist_ok=True)
    grp_name = f"group_{group_idx:02d}"
    print(f"  Building Atlases for {grp_name} (Stage {stage}) ({num_frames} frames)...")

    builder = TextureAtlasBuilder()

    group_default = GROUP_DEFAULT_COLCD.get(group_idx, SPRITE_PAL_BASE)
    mode = get_rendering_mode(group_idx, STAGE_SPRITE_GROUPS)

    count = 0
    last_colcd = group_default
    last_mode = mode

    for fi in range(num_frames):
        cg_number = grp.num_of_1st + fi
        colcd = None
        cg_mode = None

        if stage is not None:
            entry = colcd_map.get((stage, cg_number))
            if entry is not None:
                colcd, cg_mode = entry

        if colcd is None:
            for key, val in colcd_map.items():
                if key[1] == cg_number:
                    colcd, cg_mode = val
                    break

        if colcd is None:
            colcd = engine_pal_map.get(cg_number)
            # engine_pal_map doesn't specify rendering mode, fall back to last
            if colcd is not None:
                cg_mode = last_mode

        if colcd is None:
            colcd = last_colcd
            cg_mode = last_mode
        else:
            if cg_mode is None:
                cg_mode = last_mode
            last_colcd = colcd
            last_mode = cg_mode

        # Hardcode override based on debug script findings for Bonus groups
        # Hardcode override based on debug script findings for Bonus groups
        # to ensure they absolutely don't inherit faulty state/mode fallbacks.
        if grp.group_idx in {32, 33}:
            colcd = 300
            cg_mode = 18

        # [PATCH] Fix 3rd Strike ROM bug for Skater Girl in Group 24
        # The game's embedded attribute data incorrectly sets the palette bank to 69
        # instead of 68 during her turn-around sequence, exposing a blonde/red palette
        # offset when statically extracted. Overriding to 68 forces her natural colors.
        if group_idx == 24 and cg_number in {
            29207,
            29211,
            29212,
            29213,
            29214,
            29215,
            29216,
            29217,
        }:
            colcd = 68

        # Parse tiles just to grab exact bounding box pivots (what extract_stage_frame drops)
        tiles = _parse_tiles(data, grp.to_tex, fi)
        if not tiles:
            continue

        pts, _ = _compute_positions(tiles)
        px = min(p[0] for p in pts)
        py = min(p[1] for p in pts)

        img = extract_stage_frame(
            data, grp.to_tex, fi, pal_banks, colcd_base=colcd, rendering_mode=cg_mode
        )
        if img:
            builder.add_frame(cg_number, img, px, py)
            count += 1

    atlases = builder.build()

    if not atlases:
        return 0

    master_metadata = {
        "group": grp_name,
        "stage": stage,
        "description": grp.desc,
        "base_cg": grp.num_of_1st,
        "num_frames": num_frames,
        "atlases": [],
    }

    for i, (img, meta) in enumerate(atlases):
        img_filename = f"{grp_name}_atlas_{i:02d}.png"
        img_path = os.path.join(output_dir, img_filename)
        img.save(img_path)

        master_metadata["atlases"].append({"image": img_filename, "frames": meta})

    with open(os.path.join(output_dir, f"{grp_name}_metadata.json"), "w") as f:
        json.dump(master_metadata, f, indent=2)

    print(f"    -> Generated {len(atlases)} atlases for {count} valid frames.")
    return count


def process_per_character_group(
    afs_path, entries, grp, output_dir, engine_pal_map, colcd_map
):
    """Extract a PER_CHARACTER_EFFECT_GROUP once per character.

    Groups 61-76 use mode 17 (exchange_current_colcd) at runtime, which
    redirects palette lookups to the active character's banks (0-15).
    We replicate this offline by loading each character's palette file
    and extracting with colcd_base=0.
    """
    group_idx = grp.group_idx

    if grp.apfn < 0 or grp.apfn >= len(entries):
        return 0

    data = read_afs_file(afs_path, entries[grp.apfn])
    if len(data) < 4:
        return 0

    num_frames = struct.unpack_from("<I", data, 0)[0] // 4
    if num_frames == 0 or num_frames > 10000:
        return 0
    if grp.to_tex >= len(data):
        return 0

    os.makedirs(output_dir, exist_ok=True)
    grp_name = f"group_{group_idx:02d}"

    total_count = 0
    char_names = sorted(CHARACTERS.keys(), key=lambda k: CHARACTERS[k]["num_of_1st"])

    for char_name in char_names:
        char = CHARACTERS[char_name]
        pal_data = read_afs_file(afs_path, entries[char["pal_apfn"]])
        # Build full ColorRAM with this character's palette in banks 0-15
        pal_banks = build_char_colorram(afs_path, entries, pal_data, 0)

        print(
            f"  Building {grp_name} with {char_name}'s palette ({num_frames} frames)..."
        )

        builder = TextureAtlasBuilder()
        count = 0

        for fi in range(num_frames):
            cg_number = grp.num_of_1st + fi

            tiles = _parse_tiles(data, grp.to_tex, fi)
            if not tiles:
                continue

            pts, _ = _compute_positions(tiles)
            px = min(p[0] for p in pts)
            py = min(p[1] for p in pts)

            # Resolve colcd/mode from CSV (cross-stage scan), then group defaults
            colcd = None
            cg_mode = None
            for key, val in colcd_map.items():
                if key[1] == cg_number:
                    colcd, cg_mode = val
                    break
            if colcd is None:
                colcd = engine_pal_map.get(cg_number)
            if colcd is None:
                colcd = GROUP_DEFAULT_COLCD.get(group_idx, 0)
            if cg_mode is None:
                cg_mode = get_rendering_mode(group_idx, STAGE_SPRITE_GROUPS)

            img = extract_stage_frame(
                data,
                grp.to_tex,
                fi,
                pal_banks,
                colcd_base=colcd,
                rendering_mode=cg_mode,
            )
            if img:
                builder.add_frame(cg_number, img, px, py)
                count += 1

        atlases = builder.build()
        if not atlases:
            continue

        master_metadata = {
            "group": grp_name,
            "character": char_name,
            "description": grp.desc,
            "base_cg": grp.num_of_1st,
            "num_frames": num_frames,
            "atlases": [],
        }

        for i, (img, meta) in enumerate(atlases):
            img_filename = f"{grp_name}_atlas_{char_name}_{i:02d}.png"
            img_path = os.path.join(output_dir, img_filename)
            img.save(img_path)

            master_metadata["atlases"].append({"image": img_filename, "frames": meta})

        with open(
            os.path.join(output_dir, f"{grp_name}_{char_name}_metadata.json"), "w"
        ) as f:
            json.dump(master_metadata, f, indent=2)

        print(f"    -> {char_name}: {len(atlases)} atlases, {count} frames.")
        total_count += count

    return total_count


def cmd_chars(args):
    afs_path = args.afs
    output_dir = args.output
    char_name = args.character.lower()

    if char_name == "all":
        chars = sorted(CHARACTERS.keys(), key=lambda k: CHARACTERS[k]["num_of_1st"])
    elif char_name in CHARACTERS:
        chars = [char_name]
    else:
        print(f"Unknown character: {char_name}")
        sys.exit(1)

    grand_total = 0
    for name in chars:
        char = CHARACTERS[name]
        char_dir = os.path.join(output_dir, name)
        grand_total += process_character(afs_path, name, char, char_dir, args.palette)

    print(f"\nDone! Processed {grand_total} total frames.")


def cmd_stages(args):
    afs_path = args.afs
    output_dir = args.output

    entries = _open_afs(afs_path)
    engine_pal_map = load_engine_palette_map()
    colcd_map = load_colcd_map()

    target_group = None
    if getattr(args, "target", "all") != "all":
        try:
            target_group = int(args.target)
        except ValueError:
            print(f"Unknown group: {args.target}. Expected integer or 'all'.")
            sys.exit(1)

    grand_total = 0

    for grp in TEXGRPDAT:
        if (
            target_group is not None
            and target_group != "all"
            and grp.group_idx != target_group
        ):
            continue

        grp_dir = os.path.join(output_dir, f"group_{grp.group_idx:02d}")
        grand_total += process_stage_group(
            afs_path, entries, grp, grp_dir, engine_pal_map, colcd_map
        )

    print(f"\nDone! Processed {grand_total} total frames.")


def main():
    parser = argparse.ArgumentParser(
        description="CPS3 Offline Asset Compiler (Atlas Builder)"
    )
    parser.add_argument("--afs", default=_default_afs_path(), help="Path to SF33RD.AFS")

    sub = parser.add_subparsers(dest="command")

    p_chars = sub.add_parser("chars", help="Build character atlases")
    p_chars.add_argument(
        "character", nargs="?", default="all", help="Character name or 'all'"
    )
    p_chars.add_argument(
        "--output", default="output/atlases/chars", help="Output directory"
    )
    p_chars.add_argument("--palette", type=int, default=0, help="Palette index")

    p_stages = sub.add_parser("stages", help="Build stage atlases")
    p_stages.add_argument(
        "target", nargs="?", default="all", help="Group index or 'all'"
    )
    p_stages.add_argument(
        "--output", default="output/atlases/stages", help="Output directory"
    )

    args = parser.parse_args()

    if args.command == "chars":
        cmd_chars(args)
    elif args.command == "stages":
        cmd_stages(args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
