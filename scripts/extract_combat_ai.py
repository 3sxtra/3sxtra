#!/usr/bin/env python3
"""
extract_combat_ai.py — Parse all ai_active_*.c files into a unified JSON.

Each character file contains:
  - A set of Pattern functions: PatternXX_YYYY(PlayerEntity* wk)
  - Each pattern is a switch(CP_Index) dispatching to AI subroutine calls
  - A pattern jump table: PatternXX_Tbl[N]

This script extracts every pattern's step sequence and outputs combat_ai.json.
"""

import re
import json
import glob
import os
import sys

# Map character file stems to their IDs (matching the roster order in Active_Char_Tbl)
CHAR_MAP = {
    "ai_active_alex":    0,
    "ai_active_ryu":     2,
    "ai_active_yun":     3,
    "ai_active_ken":     4,
    "ai_active_dudley":  5,
    "ai_active_necro":   6,
    "ai_active_hugo":    7,
    "ai_active_ibuki":   8,
    "ai_active_elena":   9,
    "ai_active_oro":     10,
    "ai_active_yang":    11,
    "ai_active_sean":    12,
    "ai_active_urien":   13,
    "ai_active_gill":    14,
    "ai_active_chun_li": 15,
    "ai_active_makoto":  16,
    "ai_active_q":       17,
    "ai_active_twelve":  18,
    "ai_active_remy":    19,
    "ai_active_akuma":   1,
}

# Regex to match a pattern function and its body
PATTERN_FUNC_RE = re.compile(
    r'static\s+void\s+(Pattern\d+_\d+)\s*\(\s*PlayerEntity\s*\*\s*wk\s*\)\s*\{(.*?)\n\}',
    re.DOTALL
)

# Regex to match a case with a subroutine call
# Handles both simple "FuncName(wk);" and "FuncName(wk, arg1, arg2, ...);"
CASE_CALL_RE = re.compile(
    r'case\s+(\d+)\s*:\s*\n\s+(\w+)\s*\(\s*wk\s*(?:,\s*(.*?))?\s*\)\s*;',
    re.DOTALL
)

# Regex for the pattern table definition to get pattern count and order
PATTERN_TBL_RE = re.compile(
    r'static\s+void\s+\(\*const\s+(Pattern\d+_Tbl)\[(\d+)\]\)',
    re.DOTALL
)

def parse_args(arg_str):
    """Parse a comma-separated argument string into a list of integer values.
    Handles hex literals, negative hex, and simple C expressions like '0 - 0x7F68'."""
    if not arg_str or arg_str.strip() == '':
        return []
    args = []
    for a in arg_str.split(','):
        a = a.strip()
        if not a:
            continue
        try:
            # Try direct int parse first (handles 0x..., -0x..., decimal)
            args.append(int(a, 0))
        except ValueError:
            # Try evaluating as a simple arithmetic expression (e.g., "0 - 0x7F68")
            try:
                args.append(int(eval(a)))
            except Exception:
                args.append(a)  # Keep as string if not parseable
    return args


def extract_character(filepath):
    """Extract all patterns from a single ai_active_*.c file."""
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    stem = os.path.splitext(os.path.basename(filepath))[0]
    char_id = CHAR_MAP.get(stem)
    if char_id is None:
        print(f"WARNING: Unknown character file: {stem}", file=sys.stderr)
        return None

    # Find pattern table to get pattern count
    tbl_match = PATTERN_TBL_RE.search(content)
    pattern_count = int(tbl_match.group(2)) if tbl_match else 0

    # Extract all pattern functions
    patterns = {}
    for m in PATTERN_FUNC_RE.finditer(content):
        func_name = m.group(1)
        body = m.group(2)

        # Extract pattern index from function name (e.g., Pattern02_0015 -> 15)
        idx_match = re.search(r'_(\d+)$', func_name)
        if not idx_match:
            continue
        pattern_idx = int(idx_match.group(1))

        # Extract each case step
        steps = []
        for cm in CASE_CALL_RE.finditer(body):
            case_num = int(cm.group(1))
            func = cm.group(2)
            arg_str = cm.group(3)
            args = parse_args(arg_str) if arg_str else []

            if func == "End_Pattern":
                continue  # Implicit — every pattern ends with End_Pattern in default

            steps.append({
                "step": case_num,
                "op": func,
                "args": args
            })

        patterns[pattern_idx] = steps

    # Sort patterns by index
    sorted_patterns = []
    for i in range(pattern_count if pattern_count > 0 else max(patterns.keys(), default=-1) + 1):
        if i in patterns:
            sorted_patterns.append(patterns[i])
        else:
            sorted_patterns.append([])  # Empty pattern (shouldn't happen)

    return {
        "char_id": char_id,
        "name": stem.replace("ai_active_", ""),
        "pattern_count": len(sorted_patterns),
        "patterns": sorted_patterns
    }


def extract_action_tables(base_dir):
    """Extract the 5 action table files into a structured dict."""
    tables = {}
    for i in range(5):
        filepath = os.path.join(base_dir, f"ai_action_table_{i}.c")
        if not os.path.exists(filepath):
            continue
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()

        # Find all array definitions
        for arr_match in re.finditer(r'const\s+u8\s+(\w+)\[(\d+)\]\[(\d+)\]\[(\d+)\]\s*=', content):
            name = arr_match.group(1)
            dim0 = int(arr_match.group(2))
            dim1 = int(arr_match.group(3))
            dim2 = int(arr_match.group(4))

            # The array data is between this match and the next ';'
            end_idx = content.find(';', arr_match.end())
            if end_idx == -1: end_idx = len(content)
            array_content = content[arr_match.end():end_idx]

            # Extract all hex/decimal values
            values = re.findall(r'0x[0-9a-fA-F]+|\d+', array_content)
            int_values = [int(v, 0) for v in values]

            # Reshape into [dim0][dim1][dim2]
            data = []
            idx = 0
            for c in range(dim0):
                char_data = []
                for l in range(dim1):
                    level_data = int_values[idx:idx + dim2]
                    char_data.append(level_data)
                    idx += dim2
                data.append(char_data)

            tables[name] = {
                "dimensions": [dim0, dim1, dim2],
                "data": data
            }

    return tables


def main():
    base_dir = os.path.join("src", "sf33rd", "Source", "Game", "com", "active")
    out_dir = os.path.join("assets", "ai")
    os.makedirs(out_dir, exist_ok=True)

    # Extract character patterns
    files = sorted(glob.glob(os.path.join(base_dir, "ai_active_*.c")))
    characters = {}
    for f in files:
        result = extract_character(f)
        if result:
            characters[str(result["char_id"])] = result
            print(f"  [{result['char_id']:2d}] {result['name']:10s}: {result['pattern_count']} patterns")

    # Extract action tables
    action_tables = extract_action_tables(base_dir)
    for name, tbl in action_tables.items():
        dims = tbl["dimensions"]
        print(f"  Action table {name}: {dims[0]}x{dims[1]}x{dims[2]}")

    # Write combat_ai.json
    output = {
        "version": 1,
        "character_count": len(characters),
        "characters": characters
    }

    json_path = os.path.join(out_dir, "combat_ai.json")
    with open(json_path, 'w') as f:
        json.dump(output, f, indent=2)
    print(f"\nWrote {json_path} ({os.path.getsize(json_path)} bytes)")

    # Write action_tables.json
    tables_path = os.path.join(out_dir, "action_tables.json")
    with open(tables_path, 'w') as f:
        json.dump(action_tables, f, indent=2)
    print(f"Wrote {tables_path} ({os.path.getsize(tables_path)} bytes)")

    # Summary
    total_patterns = sum(c["pattern_count"] for c in characters.values())
    total_steps = sum(
        sum(len(p) for p in c["patterns"])
        for c in characters.values()
    )
    print(f"\nTotal: {len(characters)} characters, {total_patterns} patterns, {total_steps} steps")


if __name__ == "__main__":
    main()
