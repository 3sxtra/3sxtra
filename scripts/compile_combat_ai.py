import json
import struct
import sys
import os

OPCODES = {
    "WALK": 0,
    "ATTACK": 1,
    "WAIT": 2,
    "GUARD": 3,
    "CALLBACK": 4,
    "END_PATTERN": 5
}

def compile_ai(json_path, bin_path):
    with open(json_path, 'r') as f:
        data = json.load(f)

    # We will generate a binary format
    # Header:
    # u32 magic = "AIVM"
    # u32 num_chars
    # Character entries:
    # u32 char_id
    # u32 num_patterns
    # Pattern entries:
    # u32 num_ops
    # Opcode entries:
    # u16 opcode
    # u16 arg0
    # u16 arg1
    # u16 arg2

    compiled_data = bytearray()
    compiled_data.extend(b"AIVM")

    num_chars = len(data.get("characters", {}))
    compiled_data.extend(struct.pack("<I", num_chars))

    for char_id_str, char_data in data.get("characters", {}).items():
        char_id = int(char_id_str, 10)
        patterns = char_data.get("patterns", [])

        compiled_data.extend(struct.pack("<II", char_id, len(patterns)))

        for pattern in patterns:
            compiled_data.extend(struct.pack("<I", len(pattern)))

            for cmd in pattern:
                op = OPCODES[cmd["op"]]
                arg0 = 0
                arg1 = 0
                arg2 = 0

                if op == OPCODES["WALK"]:
                    arg0 = cmd.get("lever", 0)
                    arg1 = cmd.get("time", 0)
                elif op == OPCODES["ATTACK"]:
                    arg0 = cmd.get("reaction", 0)
                    arg1 = cmd.get("lever", 0)
                elif op == OPCODES["WAIT"]:
                    arg0 = cmd.get("time", 0)
                elif op == OPCODES["GUARD"]:
                    arg0 = cmd.get("type", 0)
                elif op == OPCODES["CALLBACK"]:
                    arg0 = cmd.get("index", 0)

                compiled_data.extend(struct.pack("<HHHH", op, arg0, arg1, arg2))

    with open(bin_path, 'wb') as f:
        f.write(compiled_data)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: compile_combat_ai.py <input.json> <output.dat>")
        sys.exit(1)
    compile_ai(sys.argv[1], sys.argv[2])
