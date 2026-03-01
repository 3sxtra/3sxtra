#!/usr/bin/env python3
"""
lua_trial_parser.py — Extract combo trial data from sf3_3rd_trial_clean.lua
and output trials_data.inc for the native 3sx C engine.

Usage:
    python lua_trial_parser.py <input_lua> <output_inc>

Example:
    python tools/lua_trial_parser.py WIP/sf3_3rd_trial_lua/sf3_3rd_trial_clean.lua \
        src/sf33rd/Source/Game/training/trials_data.inc
"""

import re
import sys
import os

# Character names indexed 1-19 (Lua is 1-based)
# Verified by cross-referencing _SP_/_SA_ move tags in the generated trial data.
CHARA_NAMES = [
    None,  # 0 — unused
    "Alex",  # 1
    "Ryu",  # 2
    "Ken",  # 3
    "Gouki",  # 4
    "Sean",  # 5
    "Ibuki",  # 6
    "Chun-Li",  # 7
    "Elena",  # 8
    "Makoto",  # 9
    "Yun",  # 10
    "Yang",  # 11
    "Oro",  # 12
    "Dudley",  # 13
    "Urien",  # 14
    "Remy",  # 15
    "Hugo",  # 16
    "Necro",  # 17
    "Twelve",  # 18
    "Q",  # 19
]

# Character IDs in the native engine (My_char values)
# Maps Lua index → My_char value used in the game engine.
CHARA_IDS = [
    -1,  # 0 — unused
    1,  # 1  Alex     (My_char 1)
    2,  # 2  Ryu      (My_char 2)
    11,  # 3  Ken      (My_char 11)
    14,  # 4  Gouki    (My_char 14)
    12,  # 5  Sean     (My_char 12)
    7,  # 6  Ibuki    (My_char 7)
    15,  # 7  Chun-Li  (My_char 15)
    8,  # 8  Elena    (My_char 8)
    16,  # 9  Makoto   (My_char 16)
    3,  # 10 Yun      (My_char 3)
    10,  # 11 Yang     (My_char 10)
    9,  # 12 Oro      (My_char 9)
    4,  # 13 Dudley   (My_char 4)
    13,  # 14 Urien    (My_char 13)
    19,  # 15 Remy     (My_char 19)
    6,  # 16 Hugo     (My_char 6)
    5,  # 17 Necro    (My_char 5)
    18,  # 18 Twelve   (My_char 18)
    17,  # 19 Q        (My_char 17)
]


def parse_waza_id(waza_str):
    """Parse a Lua waza ID string like 'A0046003a', 'S001e001e', 'F0938', 'T00200020'.

    Returns: (req_type_str, waza_values_list)
      req_type_str: "ATTACK_HIT", "THROW_HIT", "FIREBALL_HIT"
      waza_values_list: list of unique int values
    """
    if not waza_str or len(waza_str) < 2:
        return None, []

    prefix = waza_str[0]
    hex_part = waza_str[1:]

    if prefix in ("A", "S", "N", "G"):
        # Attack/Special: "A0046003a" → two 4-hex-digit values
        req_type = "TRIAL_REQ_ATTACK_HIT"
        if len(hex_part) == 8:
            v1 = int(hex_part[0:4], 16)
            v2 = int(hex_part[4:8], 16)
            values = list(set([v1, v2]))  # deduplicate
        elif len(hex_part) == 4:
            values = [int(hex_part, 16)]
        else:
            return None, []

    elif prefix == "T":
        req_type = "TRIAL_REQ_THROW_HIT"
        if len(hex_part) == 8:
            v1 = int(hex_part[0:4], 16)
            v2 = int(hex_part[4:8], 16)
            values = list(set([v1, v2]))
        elif len(hex_part) == 4:
            values = [int(hex_part, 16)]
        else:
            return None, []

    elif prefix == "F":
        req_type = "TRIAL_REQ_FIREBALL_HIT"
        if len(hex_part) >= 4:
            values = [int(hex_part[0:4], 16)]
        else:
            return None, []

    else:
        return None, []

    return req_type, values


def parse_lua_trials(lua_path):
    """Parse the Lua file and extract all trial data.

    Returns:
        combo_tests: dict of {(chara, trial): [steps]}
            where each step is {'name': str, 'type': str, 'waza_ids': [str]}
        kadais: dict of {(chara, trial): [strings]}
        difficulties: dict of {(chara, trial): int}
        gauge_max_flags: dict of {(chara, trial): int}
    """
    with open(lua_path, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    combo_tests = {}
    kadais = {}
    difficulties = {}
    gauge_max_flags = {}

    # Parse comboTest[chara][trial] = { {step1}, {step2}, ... }
    # Pattern: comboTest[N][M]={  followed by multiple {fields} entries
    combo_pattern = re.compile(
        r"comboTest\[(\d+)\]\[(\d+)\]\s*=\s*\{(.*?)\n\s*\}", re.DOTALL
    )

    for match in combo_pattern.finditer(content):
        chara = int(match.group(1))
        trial = int(match.group(2))
        body = match.group(3)

        steps = []
        # Parse each step: {field1, field2, ...}
        step_pattern = re.compile(r"\{([^{}]+)\}")
        for step_match in step_pattern.finditer(body):
            fields_raw = step_match.group(1)
            # Split by comma, handling quoted strings
            fields = []
            current = ""
            in_quote = False
            for ch in fields_raw:
                if ch == '"':
                    in_quote = not in_quote
                elif ch == "," and not in_quote:
                    fields.append(current.strip().strip('"'))
                    current = ""
                    continue
                current += ch
            if current.strip():
                fields.append(current.strip().strip('"'))

            if len(fields) >= 2:
                # Handle sub-arrays for waza IDs like {"A00000000", {0x422A,0x42E9}}
                # Our simple split by comma might split the sub-array improperly.
                # Actually, the parsing logic above is a bit basic. It might have given us fields like
                # ['"ANIM2P"', '"A00000000"', '{0x422A', '0x42E9}'] instead of extracting correctly.
                # Let's clean up the list of waza_ids manually.
                waza_fields = []
                for fld in fields[2:]:
                    # If it's something like '{0x422A', remove braces and try to parse it if we cared,
                    # but parse_waza_id handles hex strings starting with 'A/S/F/N'.
                    # Nested array ints don't matter to us because we just want the 'A00000000' part.
                    # We will collect all fields and let parse_waza_id filter out invalid ones.
                    clean = fld.replace("{", "").replace("}", "").strip()
                    if clean:
                        waza_fields.append(clean)

                step = {"name": fields[0], "type": fields[1], "waza_ids": waza_fields}
                steps.append(step)

        if steps:
            combo_tests[(chara, trial)] = steps

    # Parse kadai[chara][trial] = { {field}, {field} }
    kadai_pattern = re.compile(
        r"kadai\[(\d+)\]\[(\d+)\]\s*=\s*\{(.*?)\n\s*\}", re.DOTALL
    )

    for match in kadai_pattern.finditer(content):
        chara = int(match.group(1))
        trial = int(match.group(2))
        body = match.group(3)

        steps = []
        # Parse each step: {field1, field2, ...}
        step_pattern = re.compile(r"\{([^{}]+)\}")
        for step_match in step_pattern.finditer(body):
            fields_raw = step_match.group(1)
            # Find the first quoted string directly, avoid complex comma splitting
            # Usually kadai uses {"_NM_SCLP"} or {"_COMMON_L","_SP_ALEX4"}
            # We want to join them all, but typically they form ONE input string visually.
            # Let's just extract all double-quoted strings and join them with spaces.
            parts = re.findall(r'"([^"]*)"', fields_raw)
            if parts:
                steps.append(" ".join(parts))
            else:
                steps.append("")

        if steps:
            kadais[(chara, trial)] = steps

    # Parse difficulty[chara][trial] = N
    diff_pattern = re.compile(r"difficulty\[(\d+)\]\[(\d+)\]\s*=\s*(\d+)")
    for match in diff_pattern.finditer(content):
        chara = int(match.group(1))
        trial = int(match.group(2))
        diff = int(match.group(3))
        difficulties[(chara, trial)] = diff

    # Parse gaugeMaxflg[chara][trial] = N
    gauge_pattern = re.compile(r"gaugeMaxflg\[(\d+)\]\[(\d+)\]\s*=\s*(\d+)")
    for match in gauge_pattern.finditer(content):
        chara = int(match.group(1))
        trial = int(match.group(2))
        flag = int(match.group(3))
        gauge_max_flags[(chara, trial)] = flag

    return combo_tests, kadais, difficulties, gauge_max_flags


def generate_c_data(combo_tests, kadais, difficulties, gauge_max_flags, output_path):
    """Generate the trials_data.inc C file."""

    lines = []
    lines.append("/* Auto-generated by lua_trial_parser.py — DO NOT EDIT */")
    lines.append("")
    lines.append('#include "sf33rd/Source/Game/training/trials.h"')
    lines.append("")

    # Track per-character trial arrays
    char_trial_names = {}  # chara_idx -> list of trial var names

    # Sort by character then trial
    sorted_keys = sorted(combo_tests.keys())

    for chara, trial in sorted_keys:
        if chara < 1 or chara > 19:
            continue

        steps = combo_tests[(chara, trial)]
        kadai_list = kadais.get((chara, trial), [])
        diff = difficulties.get((chara, trial), 1)
        gauge_max = gauge_max_flags.get((chara, trial), 0)
        chara_name = CHARA_NAMES[chara] if chara < len(CHARA_NAMES) else f"Chara{chara}"
        chara_name_c = chara_name.replace("-", "_").replace(" ", "_").lower()
        var_name = f"g_trial_{chara_name_c}_{trial}"

        if chara not in char_trial_names:
            char_trial_names[chara] = []
        char_trial_names[chara].append(var_name)

        # Generate TrialDef
        lines.append(f"/* {chara_name} Trial {trial} (difficulty {diff}) */")
        lines.append(f"static const TrialDef {var_name} = {{")
        lines.append(f"    .chara_id = {CHARA_IDS[chara]},")
        lines.append(f"    .difficulty = {diff},")
        lines.append(f"    .gauge_max = {gauge_max},")
        lines.append(f"    .num_steps = {len(steps)},")
        lines.append("    .steps = {")

        for i, step in enumerate(steps):
            # Determine the requirement type and collect all waza values
            all_waza_values = []
            req_type = "TRIAL_REQ_ATTACK_HIT"

            step_type = step["type"]

            # Map Lua type to C type
            if step_type in ("H", "HR"):
                req_type = "TRIAL_REQ_ATTACK_HIT"
            elif step_type == "T":
                req_type = "TRIAL_REQ_THROW_HIT"
            elif step_type == "F":
                req_type = "TRIAL_REQ_FIREBALL_HIT"
            elif step_type == "D":
                req_type = "TRIAL_REQ_ACTIVE_MOVE"
            elif step_type == "J":
                req_type = "TRIAL_REQ_ACTIVE_MOVE"
            elif step_type == "K":
                req_type = "TRIAL_REQ_ACTIVE_MOVE"
            elif step_type == "K12K":
                req_type = "TRIAL_REQ_ACTIVE_MOVE"
            elif step_type == "U":
                req_type = "TRIAL_REQ_SPECIAL_COND"
            elif step_type == "ANIM2P":
                req_type = "TRIAL_REQ_ANIMATION"
            else:
                req_type = "TRIAL_REQ_ATTACK_HIT"

            # Parse all waza ID strings for this step
            for waza_str in step["waza_ids"]:
                # Skip non-waza fields (timer values stored as plain numbers for J/K types)
                if waza_str.isdigit():
                    continue
                # Skip sub-arrays (some K12K steps have nested stuff)
                if waza_str.startswith("{"):
                    continue
                _, values = parse_waza_id(waza_str)
                all_waza_values.extend(values)

            # Deduplicate and limit to MAX_WAZA_ALTERNATIVES (4)
            all_waza_values = list(dict.fromkeys(all_waza_values))[:4]

            # Pad to 4 using 0xFFFF (so 0x0000 is a valid waza)
            while len(all_waza_values) < 4:
                all_waza_values.append(0xFFFF)

            # Escape display name for C string
            display_name = step["name"].replace('"', '\\"')

            # Fetch kadai input
            kadai_str = ""
            if i < len(kadai_list):
                kadai_str = kadai_list[i].replace('"', '\\"')

            waza_str_c = ", ".join(f"0x{v:04X}" for v in all_waza_values)
            comma = "," if i < len(steps) - 1 else ""
            lines.append(
                f'        {{ {req_type}, {{ {waza_str_c} }}, "{display_name}", "{kadai_str}" }}{comma}'
            )

        lines.append("    }")
        lines.append("};")
        lines.append("")

    # Generate per-character trial lists
    lines.append("/* Per-character trial lists */")

    for chara in range(1, 20):
        if chara not in char_trial_names:
            continue

        chara_name = CHARA_NAMES[chara]
        chara_name_c = chara_name.replace("-", "_").replace(" ", "_").lower()
        trial_names = char_trial_names[chara]

        lines.append(f"static const TrialDef* g_trials_{chara_name_c}[] = {{")
        for tn in trial_names:
            lines.append(f"    &{tn},")
        lines.append("};")
        lines.append("")

    # Generate master lookup table
    lines.append("/* Master character-to-trials lookup */")
    lines.append("static const TrialCharacterDef g_all_trial_characters[] = {")

    for chara in range(1, 20):
        if chara not in char_trial_names:
            continue

        chara_name = CHARA_NAMES[chara]
        chara_name_c = chara_name.replace("-", "_").replace(" ", "_").lower()
        num_trials = len(char_trial_names[chara])

        lines.append(
            f'    {{ {CHARA_IDS[chara]}, {num_trials}, g_trials_{chara_name_c}, "{chara_name}" }},'
        )

    lines.append("};")
    lines.append("")
    lines.append(f"#define NUM_TRIAL_CHARACTERS {len(char_trial_names)}")
    lines.append("")

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
        f.write("\n")

    return len(combo_tests), len(char_trial_names)


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input_lua> <output_inc>")
        sys.exit(1)

    lua_path = sys.argv[1]
    output_path = sys.argv[2]

    if not os.path.exists(lua_path):
        print(f"Error: input file not found: {lua_path}")
        sys.exit(1)

    print(f"Parsing {lua_path}...")
    combo_tests, kadais, difficulties, gauge_max_flags = parse_lua_trials(lua_path)

    print(f"Found {len(combo_tests)} trial definitions")
    print(f"Found {len(kadais)} kadai definitions")
    print(f"Found {len(difficulties)} difficulty ratings")
    print(f"Found {len(gauge_max_flags)} gauge max flags")

    print(f"Generating {output_path}...")
    num_trials, num_chars = generate_c_data(
        combo_tests, kadais, difficulties, gauge_max_flags, output_path
    )

    print(f"Done! Generated {num_trials} trials for {num_chars} characters")


if __name__ == "__main__":
    main()
