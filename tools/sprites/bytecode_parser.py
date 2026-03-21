#!/usr/bin/env python3
"""
Universal CG-to-Palette Mapper for CPS3 Engine Sprites.

Parses the engine's C source code to build a complete, offline mapping from
CG frame numbers to ColorRAM palette banks (colcd). This eliminates the need
for runtime CSV dumps to determine correct sprite palettes.

Data sources:
  - bin2obj/char_table.c: Animation bytecodes encoding CG frame numbers
  - effect/eff05.c: Stage background sprite initialization (my_col_code per char_index)
  - effect/effc9.c: Judgement Gals palette assignments (ag_cc_table)

Output:
  A Python dict { cg_number: colcd } covering all stage/system sprite groups.

Usage:
  python bytecode_parser.py                    # print summary
  python bytecode_parser.py --json             # output JSON mapping
  python bytecode_parser.py --json -o map.json # save to file
"""

import re
import json
import sys
import os

from sprite_common import TEXGRPDAT

# ── Paths ─────────────────────────────────────────────────────────────────────

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CHAR_TABLE_C = os.path.join(REPO_ROOT, "src", "bin2obj", "char_table.c")
EFF05_C = os.path.join(REPO_ROOT, "src", "sf33rd", "Source", "Game", "effect", "eff05.c")
EFFC9_C = os.path.join(REPO_ROOT, "src", "sf33rd", "Source", "Game", "effect", "effc9.c")

# TEXGRPDAT imported from sprite_common (TexGroupEntry namedtuples)


# ── Stage bg_index to TEXGRPDAT group mapping ────────────────────────────────
# bg_index is the internal stage index (0-21). Maps to the TEXGRPDAT group
# that contains that stage's background sprites.

# From texgroup.c: each stage loads its stage sprites from a specific AFS entry.
# The mapping is derived from the apfn values in TEXGRPDAT cross-referenced
# with stage_pal_afs entries.

# bg_index -> TEXGRPDAT group_index (for stage background sprites)
BG_INDEX_TO_GROUP = {
    0:  52,   # Stage 00 (Gill/Boss)
    1:  44,   # Stage 01 (Alex/NY)
    2:  58,   # Stage 02 (Ryu/Japan)
    3:  45,   # Stage 03 (Yun/HK)
    4:  50,   # Stage 04 (Dudley/England)
    5:  42,   # Stage 05 (Necro/Russia)
    6:  47,   # Stage 06 (Hugo/Germany)
    7:  53,   # Stage 07 (Ibuki/Japan)
    8:  43,   # Stage 08 (Elena/Kenya)
    9:  48,   # Stage 09 (Oro/Brazil)
    10: 45,   # Stage 0A (Yun/HK alt) - reuses HK
    11: 44,   # Stage 0B (Alex/NY alt) - reuses NY
    12: 48,   # Stage 0C (Oro/Brazil alt) - reuses Brazil
    13: 49,   # Stage 0D (Urien/Egypt)
    14: 46,   # Stage 0E (Akuma/Japan)
    15: 56,   # Stage 0F (Chun-Li/China)
    16: 51,   # Stage 10 (Makoto/Japan)
    17: 44,   # Stage 11 (reuses NY)
    18: 42,   # Stage 12 (reuses Russia)
    19: 55,   # Stage 13 (Remy)
    20: 54,   # Stage 14 (Bonus car)
    21: 59,   # Stage 15 (Bonus parry)
}


# ══════════════════════════════════════════════════════════════════════════════
# Step 1: Parse ALL char_table arrays from char_table.c
# ══════════════════════════════════════════════════════════════════════════════

def parse_char_table_c(filepath):
    """Parse char_table.c and return { table_name: [u32_values, ...] }."""
    with open(filepath, "r") as f:
        content = f.read()

    # Match definitions like: u32 _xxx_char_table[] = { ... };
    # or: DATA_SECTION u32 _xxx_char_table[] = { ... };
    pattern = r'(?:DATA_SECTION\s+)?u32\s+(_\w+_(?:char|face_panel)_table)\[\]\s*=\s*\{([^;]+)\};'
    tables = {}
    for m in re.finditer(pattern, content, re.DOTALL):
        name = m.group(1)
        vals_str = m.group(2)
        tokens = re.findall(r'0x[0-9A-Fa-f]+', vals_str)
        tables[name] = [int(t, 16) for t in tokens]

    return tables


def count_sub_animations(data):
    """Count the number of sub-animation offsets in a char_table header.
    
    The header contains byte offsets to sub-animations. The first value that
    looks like a bytecode command (rather than an offset) marks the end.
    We detect this heuristically: offsets are small and increasing,
    while bytecodes start with patterns like 0x0, 0x2, 0x0.
    """
    if len(data) < 3:
        return 0

    # The smallest possible offset is to the first sub-animation right after
    # the header. Header entries are u32 (4 bytes), so the first offset value
    # tells us how many header entries there are: offset / 4.
    if data[0] == 0:
        return 0

    n_subs = data[0] // 4
    # Validate: all entries [0..n_subs-1] should be plausible byte offsets
    # (multiples of 4, generally increasing)
    if n_subs > len(data):
        n_subs = 0
    for i in range(n_subs):
        if i >= len(data):
            break
        if data[i] == 0:
            n_subs = i
            break
    return n_subs


def extract_cgs_from_bytecode(data, start_idx, cg_min=0, cg_max=0xFFFF):
    """Extract CG frame numbers from bytecode starting at data[start_idx].
    
    CG references are u32 values where the high halfword (bits 16-31) is in
    the range [cg_min, cg_max]. The low halfword is typically 0x0000.
    
    Returns: set of CG numbers found.
    """
    cgs = set()
    i = start_idx
    safety = 0
    # Walk through bytecode. A sub-animation ends with the pattern:
    #   0x1, 0x0  (return to caller + end)
    # But 0x0 also appears as operands (e.g. coords), so we only stop
    # when we see the FINAL 0x0 that is the table terminator, not mid-sequence.
    # Instead of trying to detect end, just scan to the end of the data or
    # until we reach the start of the NEXT sub-animation.
    while i < len(data) and safety < 5000:
        val = data[i]
        hi = (val >> 16) & 0xFFFF

        # Check if high halfword is a plausible CG number
        if hi >= 0x0100 and (val & 0xFFFF) == 0:
            if cg_min <= hi <= cg_max:
                cgs.add(hi)

        i += 1
        safety += 1

    return cgs


def extract_cgs_for_sub_animation(data, sub_idx, cg_min=0, cg_max=0xFFFF):
    """Extract CG numbers for a specific sub-animation index within a char_table.
    
    sub_idx: 0-based sub-animation index
    Returns: set of CG numbers
    """
    n_subs = count_sub_animations(data)
    if sub_idx >= n_subs:
        return set()

    byte_offset = data[sub_idx]
    array_idx = byte_offset // 4

    if array_idx >= len(data):
        return set()

    # Find the end boundary: start of next sub-animation, or end of data
    if sub_idx + 1 < n_subs:
        end_idx = data[sub_idx + 1] // 4
    else:
        end_idx = len(data)

    return extract_cgs_from_bytecode(data[array_idx:end_idx], 0, cg_min, cg_max)


def extract_all_cgs(data, cg_min=0, cg_max=0xFFFF):
    """Extract ALL CG numbers from all sub-animations in a char_table."""
    n_subs = count_sub_animations(data)
    all_cgs = {}  # sub_idx -> set of CGs

    for sub_idx in range(n_subs):
        cgs = extract_cgs_for_sub_animation(data, sub_idx, cg_min, cg_max)
        if cgs:
            all_cgs[sub_idx] = cgs

    return all_cgs


# ══════════════════════════════════════════════════════════════════════════════
# Step 2: Parse eff05.c stage data tables
# ══════════════════════════════════════════════════════════════════════════════

def parse_eff05_c(filepath):
    """Parse eff05.c for stage background sprite palette assignments.
    
    Returns:
        char_add: list of 22 char_table names (one per bg_index)
        scr_obj_num: list of 22 counts (number of background objects per stage)
        scr_obj_data: list of 22 lists, each containing (my_col_code, char_index) tuples
    """
    with open(filepath, "r") as f:
        content = f.read()

    # Parse scr_obj_num
    m = re.search(r'scr_obj_num\[22\]\s*=\s*\{([^}]+)\}', content)
    obj_nums = [int(x.strip()) for x in m.group(1).split(',') if x.strip()]

    # Parse char_add array — maps bg_index to char_table name
    m = re.search(r'char_add\[22\]\s*=\s*\{([^}]+)\}', content)
    char_names_raw = re.findall(r'_\w+_char_table', m.group(1))

    # Parse all stg data tables
    stg_tables = {}
    for m in re.finditer(r'(stg\w+_data_tbl)\[(\d+)\]\s*=\s*\{([^}]+)\}', content):
        name = m.group(1)
        vals = [int(x.strip()) for x in m.group(3).split(',') if x.strip()]
        stg_tables[name] = vals

    # Parse scr_obj_data to map bg_index -> table name
    m = re.search(r'scr_obj_data\[22\]\s*=\s*\{([^}]+)\}', content)
    data_names = re.findall(r'stg\w+_data_tbl', m.group(1))

    # Build per-bg_index result: list of (my_col_code, char_index) tuples
    bg_data = []
    for bg_idx in range(22):
        n_objs = obj_nums[bg_idx]
        table_name = data_names[bg_idx]
        vals = stg_tables.get(table_name, [])

        entries = []
        for obj_i in range(n_objs):
            base = obj_i * 8
            if base + 7 >= len(vals):
                break
            # Fields: dead_f, my_family, my_col_code, x, y, priority, char_index, sync_suzi
            my_col_code = vals[base + 2]
            char_index = vals[base + 6]
            entries.append((my_col_code, char_index))

        bg_data.append(entries)

    return char_names_raw, obj_nums, bg_data


# ══════════════════════════════════════════════════════════════════════════════
# Step 3: Parse effc9.c for Judgement Gals
# ══════════════════════════════════════════════════════════════════════════════

def parse_effc9_c(filepath):
    """Parse effc9.c for Judgement Gals ag_cc_table.
    
    Returns: dict { charset_id: colcd }
    """
    with open(filepath, "r") as f:
        content = f.read()

    # Match ag_cc_table[8] = { 8257, 8258, ... }
    m = re.search(r'ag_cc_table\[\d*\]\s*=\s*\{([^}]+)\}', content)
    if not m:
        return {}

    vals = [int(x.strip()) for x in m.group(1).split(',') if x.strip()]
    # ag_cc_table values are my_col_code values like 0x2041
    # Masked with 0x1FF -> ColorRAM bank
    result = {}
    for i, v in enumerate(vals):
        result[i] = v & 0x1FF
    return result


# ══════════════════════════════════════════════════════════════════════════════
# Step 4: Build complete CG-to-colcd map
# ══════════════════════════════════════════════════════════════════════════════

def cg_to_group(cg_number):
    """Find which TEXGRPDAT group a CG number belongs to."""
    best_group = None
    best_start = -1
    for grp in TEXGRPDAT:
        if grp.num_of_1st <= cg_number and grp.num_of_1st > best_start:
            best_start = grp.num_of_1st
            best_group = grp.group_idx
    return best_group


def build_universal_map():
    """Build the complete CG-to-colcd mapping from source code."""
    tables = parse_char_table_c(CHAR_TABLE_C)
    char_names, obj_nums, bg_data = parse_eff05_c(EFF05_C)
    ag_cc = parse_effc9_c(EFFC9_C)

    cg_map = {}  # { cg_number: colcd }
    stats = {
        'stages_parsed': 0,
        'cgs_mapped': 0,
        'groups_hit': set(),
    }

    # ── Stage background sprites (eff05.c) ────────────────────────────────
    for bg_idx in range(22):
        if not bg_data[bg_idx]:
            continue

        char_table_name = char_names[bg_idx]
        if char_table_name not in tables:
            print(f"  WARNING: {char_table_name} not found in char_table.c")
            continue

        char_data = tables[char_table_name]
        stats['stages_parsed'] += 1

        for my_col_code, char_index in bg_data[bg_idx]:
            cgs = extract_cgs_for_sub_animation(char_data, char_index)
            # Resolve colcd: my_col_code can be 300 (stage palette), 8492 (0x212C -> 0x12C=300)
            # Mask with 0x1FF to get the ColorRAM bank
            colcd = my_col_code & 0x1FF
            for cg in cgs:
                cg_map[cg] = colcd
                stats['cgs_mapped'] += 1
                grp = cg_to_group(cg)
                if grp is not None:
                    stats['groups_hit'].add(grp)

    # ── Judgement Gals (effc9.c / Group 24) ───────────────────────────────
    face_panel = tables.get('_ag_face_panel_table', [])
    for charset_id in range(8):
        ag_name = f'_ag_{charset_id:02d}_char_table'
        ag_data = tables.get(ag_name, [])
        if not ag_data:
            continue

        # ag tables have offsets into _ag_face_panel_table
        colcd = ag_cc.get(charset_id, 65 + charset_id)

        for offset_bytes in ag_data:
            if offset_bytes == 0:
                break
            array_idx = offset_bytes // 4
            if array_idx >= len(face_panel):
                continue
            cgs = extract_cgs_from_bytecode(face_panel, array_idx,
                                             cg_min=0x71E0, cg_max=0x72FF)
            for cg in cgs:
                cg_map[cg] = colcd
                stats['cgs_mapped'] += 1
                stats['groups_hit'].add(24)

    stats['groups_hit'] = sorted(stats['groups_hit'])
    return cg_map, stats


# ══════════════════════════════════════════════════════════════════════════════
# Main
# ══════════════════════════════════════════════════════════════════════════════

def main():
    output_json = '--json' in sys.argv
    output_file = None
    if '-o' in sys.argv:
        idx = sys.argv.index('-o')
        if idx + 1 < len(sys.argv):
            output_file = sys.argv[idx + 1]

    print("=== Universal CG-to-Palette Mapper ===\n")

    # Parse source files
    print("Parsing char_table.c...")
    tables = parse_char_table_c(CHAR_TABLE_C)
    print(f"  Found {len(tables)} char_table arrays")
    for name, data in sorted(tables.items()):
        n_subs = count_sub_animations(data)
        print(f"    {name}: {len(data)} u32s, {n_subs} sub-animations")

    print(f"\nParsing eff05.c...")
    char_names, obj_nums, bg_data = parse_eff05_c(EFF05_C)
    for bg_idx in range(22):
        if bg_data[bg_idx]:
            print(f"  bg_index {bg_idx:2d}: {char_names[bg_idx]}, "
                  f"{len(bg_data[bg_idx])} objects -> "
                  f"{', '.join(f'ci={ci} col={cc}' for cc, ci in bg_data[bg_idx])}")

    print(f"\nParsing effc9.c...")
    ag_cc = parse_effc9_c(EFFC9_C)
    print(f"  ag_cc_table: {ag_cc}")

    # Build map
    print(f"\nBuilding universal CG-to-colcd map...")
    cg_map, stats = build_universal_map()

    print(f"\n=== Results ===")
    print(f"  Stages parsed: {stats['stages_parsed']}")
    print(f"  CGs mapped: {stats['cgs_mapped']}")
    print(f"  Groups covered: {stats['groups_hit']}")

    # Print per-group summary
    group_cgs = {}
    for cg, colcd in sorted(cg_map.items()):
        grp = cg_to_group(cg)
        if grp not in group_cgs:
            group_cgs[grp] = []
        group_cgs[grp].append((cg, colcd))

    print(f"\n  Per-group breakdown:")
    for grp in sorted(group_cgs.keys()):
        entries = group_cgs[grp]
        colcds = set(c for _, c in entries)
        grp_entry = next((g for g in TEXGRPDAT if g.group_idx == grp), None)
        desc = grp_entry.desc if grp_entry else "Unknown"
        print(f"    Group {grp:2d} ({desc}): {len(entries)} CGs, "
              f"colcds: {sorted(colcds)}")

    # Output
    if output_json:
        # Convert keys to strings for JSON
        json_map = {str(k): v for k, v in sorted(cg_map.items())}
        if output_file:
            with open(output_file, 'w') as f:
                json.dump(json_map, f, indent=2)
            print(f"\n  Saved to {output_file}")
        else:
            print(json.dumps(json_map, indent=2))

    return cg_map


if __name__ == "__main__":
    main()
