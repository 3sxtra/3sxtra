#!/usr/bin/env python3
"""
compile_combat_ai.py — Compile combat_ai.json + action_tables.json to binary .dat files.

Binary format for combat_sequences.dat:
  Header: 4B magic "AIVM", 2B version, 2B num_characters
  Character directory: num_characters × (2B char_id, 2B pattern_count, 4B offset)
  Pattern data at each offset:
    Per pattern: 1B step_count
      Per step: 1B opcode, 1B arg_count, arg_count × 2B (s16) args

Binary format for action_tables.dat:
  Header: 4B magic "AIAT", 2B version, 2B num_tables
  Table directory: num_tables × (2B dim0, 2B dim1, 2B dim2, 4B offset)
  Raw u8 data at each offset
"""

import json
import struct
import sys
import os

# Opcode mapping — must match ai_combat_core.h AIOp enum exactly
OPCODES = {
    "End_Pattern":                       0,
    "Lever_Off":                         1,
    "Look":                              2,
    "Wait":                              3,
    "Walk":                              4,
    "Jump":                              5,
    "Hi_Jump":                           6,
    "Normal_Attack":                     7,
    "Normal_Attack_SP":                  8,
    "Adjust_Attack":                     9,
    "Lever_Attack":                     10,
    "Lever_Attack_SP":                  11,
    "Command_Attack":                   12,
    "Jump_Attack":                      13,
    "Jump_Command_Attack":              14,
    "Rapid_Command_Attack":             15,
    "Jump_Command_Attack_Term":         16,
    "Hi_Jump_Attack":                   17,
    "Hi_Jump_Attack_Term":              18,
    "Hi_Jump_Command_Attack_Term":      19,
    "Check_Jump_Attack_Conditions":     20,
    "Check_Enemy_Distance":             21,
    "Approach_Walk":                    22,
    "Check_Super_Art_Conditions":       23,
    "Check_SA":                         24,
    "Check_EX":                         25,
    "Check_SA_Full":                    26,
    "AI_Random_Action_Select":          27,
    "Branch_By_Distance":               28,
    "Enable_Overhead_Attack_Flag":      29,
    "Lever_On":                         30,
    "Only_Shot":                        31,
    "Turn_Over_On":                     32,
    "Setup_DENJIN_LEVEL":               33,
    "Hold_Attack_Button":               34,
    "Keep_Away":                        35,
    "Check_Safe_Retreat_Space":         36,
    "Provoke":                          37,
    "Next_Another_Menu":                38,
    "Check_Miscellaneous_Conditions":   39,
    "Oro_Check_Jump_Attack":            40,
    "Oro_Check_High_Jump_Attack":       41,
    "Oro_Check_Jump_Command_Attack":    42,
    "Oro_Check_High_Jump_Command_Attack": 43,
}

def compile_combat_ai(json_path, bin_path):
    with open(json_path, 'r') as f:
        data = json.load(f)

    characters = data["characters"]
    num_chars = len(characters)

    # Build pattern data for each character
    char_entries = []  # (char_id, pattern_count, pattern_bytes)
    for char_id_str in sorted(characters.keys(), key=int):
        char = characters[char_id_str]
        char_id = char["char_id"]
        patterns = char["patterns"]

        pat_bytes = bytearray()
        for pattern in patterns:
            pat_bytes.append(len(pattern))  # step_count (u8)
            for step in pattern:
                op_name = step["op"]
                opcode = OPCODES.get(op_name)
                if opcode is None:
                    print(f"ERROR: Unknown opcode '{op_name}' in char {char_id}", file=sys.stderr)
                    sys.exit(1)
                args = step["args"]
                pat_bytes.append(opcode)             # opcode (u8)
                pat_bytes.append(len(args))           # arg_count (u8)
                for arg in args:
                    # Clamp to s16 range
                    val = int(arg) & 0xFFFF
                    pat_bytes.extend(struct.pack("<H", val))  # arg (u16, treated as s16)

        char_entries.append((char_id, len(patterns), pat_bytes))

    # Calculate offsets
    header_size = 8  # magic(4) + version(2) + num_chars(2)
    dir_size = num_chars * 8  # per char: char_id(2) + pattern_count(2) + offset(4)
    data_offset = header_size + dir_size

    out = bytearray()
    # Header
    out.extend(b"AIVM")
    out.extend(struct.pack("<HH", 1, num_chars))

    # Directory
    current_offset = data_offset
    for char_id, pat_count, pat_bytes in char_entries:
        out.extend(struct.pack("<HHI", char_id, pat_count, current_offset))
        current_offset += len(pat_bytes)

    # Pattern data
    for _, _, pat_bytes in char_entries:
        out.extend(pat_bytes)

    with open(bin_path, 'wb') as f:
        f.write(out)

    print(f"Wrote {bin_path} ({len(out)} bytes, {num_chars} characters)")


def compile_action_tables(json_path, bin_path):
    with open(json_path, 'r') as f:
        tables = json.load(f)

    num_tables = len(tables)
    header_size = 8  # magic(4) + version(2) + num_tables(2)
    dir_size = num_tables * 10  # per table: dim0(2) + dim1(2) + dim2(2) + offset(4)

    table_entries = []
    for name in sorted(tables.keys()):
        tbl = tables[name]
        dims = tbl["dimensions"]
        flat_data = bytearray()
        for char_data in tbl["data"]:
            for level_data in char_data:
                flat_data.extend(bytes(level_data))
        table_entries.append((name, dims, flat_data))

    data_offset = header_size + dir_size

    out = bytearray()
    out.extend(b"AIAT")
    out.extend(struct.pack("<HH", 1, num_tables))

    current_offset = data_offset
    for name, dims, flat_data in table_entries:
        out.extend(struct.pack("<HHHI", dims[0], dims[1], dims[2], current_offset))
        current_offset += len(flat_data)

    for _, _, flat_data in table_entries:
        out.extend(flat_data)

    with open(bin_path, 'wb') as f:
        f.write(out)

    print(f"Wrote {bin_path} ({len(out)} bytes, {num_tables} tables)")


def main():
    ai_json = os.path.join("assets", "ai", "combat_ai.json")
    ai_bin = os.path.join("assets", "ai", "combat_sequences.dat")
    tbl_json = os.path.join("assets", "ai", "action_tables.json")
    tbl_bin = os.path.join("assets", "ai", "action_tables.dat")

    if not os.path.exists(ai_json):
        print(f"ERROR: {ai_json} not found. Run extract_combat_ai.py first.", file=sys.stderr)
        sys.exit(1)

    compile_combat_ai(ai_json, ai_bin)

    if os.path.exists(tbl_json):
        compile_action_tables(tbl_json, tbl_bin)


if __name__ == "__main__":
    main()
