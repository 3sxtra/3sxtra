#!/usr/bin/env python3
"""
State comparison utility for debugging netplay desyncs.

Uses DWARF debug info to parse struct layouts and provide symbolic paths
to differing bytes when comparing state dumps.

Based on upstream compare_states.py, adapted for local 3SX State struct layout
(GameState + EffectState).

Usage:
    python compare_states.py <path_to_debug_binary>

Prerequisites:
    - dwarfdump (MSYS2: pacman -S mingw-w64-x86_64-dwarfutils)
    - State dumps in ./states/ directory (created by dump_state() in DEBUG builds)
"""

import sys
from pathlib import Path
from enum import Enum
from dataclasses import dataclass
import re
import subprocess


@dataclass
class Member:
    name: str
    type: int
    typename: str
    location: int


@dataclass
class Struct:
    name: str
    offset: int
    size: int
    members: list[Member]


class DWARFParser:
    class TypedefParserState(Enum):
        TYPEDEF = 0
        TYPE = 1
        NAME = 2

    class StructParserState(Enum):
        STRUCT = 0
        STRUCT_NAME = 1
        STRUCT_SIZE = 2
        MEMBER = 3
        MEMBER_NAME = 4
        MEMBER_TYPE = 5
        MEMBER_LOCATION = 6
        IGNORED_TYPE = 7

    def __init__(self):
        self.typedefs: dict[int, str] = {}
        self.typedef_offset_to_name: dict[
            int, str
        ] = {}  # typedef's own DIE offset -> name
        self.structs: list[Struct] = []
        self.struct_name_to_struct: dict[str, Struct] = {}

    def parse_object(self, path: Path):
        lines = self.__run_dwarfdump(path).splitlines()
        self.__parse_typedefs(lines)
        self.__parse_structs(lines)

    def find_member(
        self, struct_name: str, offset: int, debug: bool = False
    ) -> tuple[list[str], dict]:
        path: list[str] = []
        metadata: dict = {}
        struct = self.struct_name_to_struct.get(struct_name)

        while struct:
            if offset >= struct.size:
                if debug:
                    print(
                        f"⚠️ 0x{offset:X} does not fit in {struct.name} (size 0x{struct.size:X})"
                    )
                break

            candidates = [x for x in struct.members if offset >= x.location]
            if not candidates:
                break
            member = candidates[-1]
            path_component = member.name
            offset -= member.location
            name, array_dimensions = self.__split_typename(member.typename)
            elem_size = 0

            # Handle EffectState.frw array (local struct: WORK_Other[128])
            if member.name == "frw":
                struct = self.struct_name_to_struct.get("WORK_Other")
                if struct:
                    elem_size = struct.size
                else:
                    # Fallback: try to get from WORK_Other size or use known value
                    elem_size = 448 * 8  # 3584 bytes per effect slot
                array_dimensions = [128]
                metadata["in_effect_state"] = True
            else:
                struct = self.struct_name_to_struct.get(name)

                # Fallback: resolve typedef indirection.
                # member.type may point to a DW_TAG_typedef DIE, not the struct directly.
                # Follow: member.type -> typedef name -> struct_name_to_struct
                if not struct:
                    typedef_name = self.typedef_offset_to_name.get(member.type)
                    if typedef_name:
                        struct = self.struct_name_to_struct.get(typedef_name)

                if not struct and hasattr(self, "struct_offset_to_struct"):
                    struct = self.struct_offset_to_struct.get(member.type)

                # Fallback: check if name is a hex offset string
                if not struct and name.startswith("0x"):
                    try:
                        offset_val = int(name, 16)
                        if hasattr(self, "struct_offset_to_struct"):
                            struct = self.struct_offset_to_struct.get(offset_val)
                    except ValueError:
                        pass
                if struct:
                    elem_size = struct.size

            if array_dimensions and elem_size:
                indices = self.__offset_to_indices(offset, elem_size, array_dimensions)

                for index in indices:
                    path_component += f"[{index}]"

                    if member.name == "frw":
                        metadata["effect_index"] = index

                flat_index = offset // elem_size
                offset -= flat_index * elem_size

            path.append(path_component)

        return (path, metadata)

    def __split_typename(self, typename: str) -> tuple[str, list[int]]:
        name_end: int | None = None
        dimensions: list[int] = []

        for match in re.finditer(r"\[(\d+)\]", typename):
            dimensions.append(int(match.group(1)))

            if name_end is None:
                name_end = match.start()

        name = typename

        if name_end is not None:
            name = typename[:name_end]

        return (name, dimensions)

    def __offset_to_indices(
        self, offset: int, elem_size: int, dims: list[int]
    ) -> list[int]:
        flat_index = offset // elem_size
        indices = []

        for dim in reversed(dims):
            indices.append(flat_index % dim)
            flat_index //= dim

        return list(reversed(indices))

    def __run_dwarfdump(self, path: Path) -> str:
        result = subprocess.run(
            ["dwarfdump", path], capture_output=True, text=True, check=False
        )

        if result.returncode != 0:
            raise RuntimeError(f"dwarfdump failed: {result.stderr.strip()}")

        print("DEBUG: dwarfdump output length:", len(result.stdout))
        if len(result.stdout) > 0:
            print("DEBUG: First 500 chars of output:")
            print(result.stdout[:500])
        else:
            print("DEBUG: Empty output from dwarfdump")

        return result.stdout

    def __parse_typedefs(self, lines: list[str]):
        current_type: int | None = None
        current_typedef_offset: int | None = None
        state = self.TypedefParserState.TYPEDEF

        type_re = re.compile(r"DW_AT_type\s+<(0x[\da-fA-F]+)>")
        name_re = re.compile(r"DW_AT_name\s+(.+)")
        typedef_re = re.compile(r"<\s*\d+><(0x[\da-fA-F]+)>\s+DW_TAG_typedef")

        for line in lines:
            match state:
                case self.TypedefParserState.TYPEDEF:
                    if m := re.search(typedef_re, line):
                        current_typedef_offset = int(m.group(1), base=16)
                        state = self.TypedefParserState.TYPE

                case self.TypedefParserState.TYPE:
                    if match := re.search(type_re, line):
                        current_type = int(match.group(1), base=16)
                        state = self.TypedefParserState.NAME

                case self.TypedefParserState.NAME:
                    if match := re.search(name_re, line):
                        name = match.group(1)
                        self.typedefs[current_type] = name
                        if current_typedef_offset is not None:
                            self.typedef_offset_to_name[current_typedef_offset] = name
                        state = self.TypedefParserState.TYPEDEF

    def __parse_structs(self, lines: list[str]):
        state = self.StructParserState.STRUCT
        current_struct: Struct | None = None
        current_member: Member | None = None
        return_to_state = self.StructParserState.STRUCT

        struct_re = re.compile(r"<\s*\d+><(0x[\da-fA-F]+)>\s+DW_TAG_structure_type")
        struct_size_re = re.compile(r"DW_AT_byte_size\s+(?:0x)?([\da-fA-F]+)")
        name_re = re.compile(r"DW_AT_name\s+(?:\"?)([a-zA-Z0-9_\[\] :]+)(?:\"?)")
        member_location_re = re.compile(
            r"DW_AT_data_member_location\s+(?:0x)?([\da-fA-F]+)"
        )
        member_type_re = re.compile(r"DW_AT_type\s+<(0x[\da-fA-F]+)>")

        current_level = -1
        parsing_struct_level = -1
        ignored_type_level = -1

        for line in lines:
            # debug_count += 1
            # if debug_count < 500:
            #    print(f"DEBUG: State={state}, Line='{line.strip()}'")

            level_match = re.search(r"<\s*(\d+)><0x", line)
            if level_match:
                new_level = int(level_match.group(1))

                # Check for implicit end of scope
                if state == self.StructParserState.IGNORED_TYPE:
                    if new_level <= ignored_type_level:
                        state = return_to_state
                        ignored_type_level = -1

                if state in [
                    self.StructParserState.MEMBER,
                    self.StructParserState.MEMBER_NAME,
                    self.StructParserState.MEMBER_TYPE,
                    self.StructParserState.MEMBER_LOCATION,
                ]:
                    if new_level <= parsing_struct_level:
                        # End of struct scope detected by level drop
                        if (
                            current_struct.name != ""
                            or current_struct.size > 0
                            or len(current_struct.members) > 0
                        ):
                            self.structs.append(current_struct)
                        state = self.StructParserState.STRUCT
                        parsing_struct_level = -1

                if state in [
                    self.StructParserState.STRUCT_NAME,
                    self.StructParserState.STRUCT_SIZE,
                ]:
                    if new_level <= parsing_struct_level:
                        # Struct ended before we found members? (Empty struct)
                        if current_struct.name != "" or current_struct.size > 0:
                            self.structs.append(current_struct)
                        state = self.StructParserState.STRUCT
                        parsing_struct_level = -1

                current_level = new_level

            match state:
                case self.StructParserState.STRUCT:
                    if line.strip().endswith("DW_TAG_union_type"):
                        if level_match:
                            ignored_type_level = current_level
                        else:
                            # Fallback if no level found (shouldn't happen with correct regex)
                            ignored_type_level = 999

                        return_to_state = self.StructParserState.STRUCT
                        state = self.StructParserState.IGNORED_TYPE
                        continue

                    if match := re.search(struct_re, line):
                        struct_offset = int(match.group(1), base=16)
                        if level_match:
                            parsing_struct_level = current_level
                        else:
                            parsing_struct_level = 999

                        current_struct = Struct(
                            name=self.typedefs.get(struct_offset, ""),
                            offset=struct_offset,
                            size=0,
                            members=[],
                        )

                        if current_struct.name != "":
                            state = self.StructParserState.STRUCT_SIZE
                        else:
                            state = self.StructParserState.STRUCT_NAME

                case self.StructParserState.STRUCT_NAME:
                    if match := re.search(name_re, line):
                        current_struct.name = match.group(1)
                        # After name, look for size
                        state = self.StructParserState.STRUCT_SIZE
                    elif match := re.search(struct_size_re, line):
                        # Found size before name (or no name), proceed
                        current_struct.size = int(match.group(1), base=16)
                        state = self.StructParserState.MEMBER
                    elif "DW_TAG_member" in line:
                        # Found member before name/size. Assume anonymous/unknown size.
                        current_member = Member("", 0, "", 0)
                        state = self.StructParserState.MEMBER_NAME

                case self.StructParserState.STRUCT_SIZE:
                    if match := re.search(struct_size_re, line):
                        current_struct.size = int(match.group(1), base=16)
                        state = self.StructParserState.MEMBER
                    elif "DW_TAG_member" in line:
                        # Member started, size processing done (or headers done)
                        current_member = Member("", 0, "", 0)
                        state = self.StructParserState.MEMBER_NAME

                case self.StructParserState.MEMBER:
                    # Explicit NULL check fallback
                    if "NULL" in line:
                        # End of struct
                        if (
                            current_struct.name != ""
                            or current_struct.size > 0
                            or len(current_struct.members) > 0
                        ):
                            self.structs.append(current_struct)
                        state = self.StructParserState.STRUCT
                        parsing_struct_level = -1
                        continue

                    if line.strip().endswith(
                        "DW_TAG_structure_type"
                    ) or line.strip().endswith("DW_TAG_union_type"):
                        if level_match:
                            ignored_type_level = current_level
                        else:
                            ignored_type_level = 999

                        return_to_state = self.StructParserState.MEMBER
                        state = self.StructParserState.IGNORED_TYPE
                        continue

                    if "DW_TAG_member" in line:
                        current_member = Member("", 0, "", 0)
                        state = self.StructParserState.MEMBER_NAME

                case self.StructParserState.MEMBER_NAME:
                    if match := re.search(name_re, line):
                        current_member.name = match.group(1)
                        state = self.StructParserState.MEMBER_TYPE
                    elif match := re.search(member_type_re, line):
                        # Name missing, go straight to type
                        current_member.type = int(match.group(1), base=16)
                        current_member.typename = self.typedefs.get(
                            current_member.type, f"0x{current_member.type:X}"
                        )
                        state = self.StructParserState.MEMBER_LOCATION

                case self.StructParserState.MEMBER_TYPE:
                    if match := re.search(member_type_re, line):
                        current_member.type = int(match.group(1), base=16)
                        # Try to resolve name from typedefs
                        current_member.typename = self.typedefs.get(
                            current_member.type, f"0x{current_member.type:X}"
                        )
                        state = self.StructParserState.MEMBER_LOCATION
                    elif match := re.search(member_location_re, line):
                        # Type missing?? Weird but handle it
                        current_member.location = int(match.group(1), base=16)
                        current_struct.members.append(current_member)
                        state = self.StructParserState.MEMBER

                case self.StructParserState.MEMBER_LOCATION:
                    if match := re.search(member_location_re, line):
                        current_member.location = int(match.group(1), base=16)
                        current_struct.members.append(current_member)
                        state = self.StructParserState.MEMBER

                case self.StructParserState.IGNORED_TYPE:
                    pass  # Handled by level check above

        self.struct_offset_to_struct: dict[int, Struct] = {}
        for struct in self.structs:
            self.struct_name_to_struct[struct.name] = struct
            self.struct_offset_to_struct[struct.offset] = struct


def find_state_pairs() -> list[tuple[Path, Path, int]]:
    """Find matching state dump pairs from different players."""
    pairs: list[tuple[Path, Path]] = []
    states_dir = Path("states")

    if not states_dir.exists():
        print(
            "No 'states' directory found. Run the game in DEBUG mode with netplay to generate state dumps."
        )
        return []

    files = sorted(states_dir.iterdir(), key=lambda x: x.name)

    for file in files:
        parts = file.name.split("_")
        if len(parts) != 2:
            continue

        plnum, frame = parts

        if plnum == "1":
            continue

        file1 = Path(f"states/0_{frame}")
        file2 = Path(f"states/1_{frame}")

        if not file2.exists():
            continue

        pairs.append((file1, file2, int(frame)))

    return pairs


def effect_offset(parser: DWARFParser, index: int) -> int:
    """Calculate byte offset for a specific effect slot."""
    offset = 0

    state_struct = parser.struct_name_to_struct.get("State")
    if not state_struct:
        return 0

    es_member = next((m for m in state_struct.members if m.name == "es"), None)
    if not es_member:
        return 0
    offset += es_member.location

    effect_state_struct = parser.struct_name_to_struct.get("EffectState")
    if not effect_state_struct:
        return offset

    frw_member = next((m for m in effect_state_struct.members if m.name == "frw"), None)
    if not frw_member:
        return offset
    offset += frw_member.location

    # WORK_Other size (local struct)
    work_other = parser.struct_name_to_struct.get("WORK_Other")
    elem_size = work_other.size if work_other else 448 * 8
    offset += elem_size * index

    return offset


def compare_states(parser: DWARFParser):
    """Compare all state dump pairs and report differences."""
    pairs = find_state_pairs()

    if not pairs:
        return

    print(f"Found {len(pairs)} state dump pairs to compare")
    print("=" * 60)

    for pl1_state_path, pl2_state_path, frame in pairs:
        pl1_state = pl1_state_path.read_bytes()
        pl2_state = pl2_state_path.read_bytes()

        length = min(len(pl1_state), len(pl2_state))

        # First pass: find all diffs and group them
        all_diffs: list[tuple[int, int, int]] = []  # (offset, byte1, byte2)
        for i in range(length):
            if pl1_state[i] != pl2_state[i]:
                all_diffs.append((i, pl1_state[i], pl2_state[i]))

        print(f"Total differing bytes: {len(all_diffs)}")

        # Group consecutive diffs into ranges
        ranges: list[tuple[int, int]] = []  # (start_offset, end_offset)
        if all_diffs:
            start = all_diffs[0][0]
            prev = all_diffs[0][0]
            for offset, _, _ in all_diffs[1:]:
                if offset != prev + 1:
                    ranges.append((start, prev))
                    start = offset
                prev = offset
            ranges.append((start, prev))

        # One-time debug: resolve the first diff to see why lookup fails
        debug_done = False

        # Group ranges by resolved path
        path_groups: dict[str, list[tuple[int, int, bytes, bytes]]] = {}
        state_struct = parser.struct_name_to_struct.get("State")
        struct_name = state_struct.name if state_struct else "State"

        for start, end in ranges:
            do_debug = not debug_done
            debug_done = True
            path_components, metadata = parser.find_member(
                struct_name, start, debug=do_debug
            )
            path = ".".join(path_components)

            size = end - start + 1
            v1 = pl1_state[start : end + 1]
            v2 = pl2_state[start : end + 1]

            if path not in path_groups:
                path_groups[path] = []
            path_groups[path].append((start, size, v1, v2))

        # Print grouped results
        for path, diffs in path_groups.items():
            total_bytes = sum(d[1] for d in diffs)
            if len(diffs) == 1:
                start, size, v1, v2 = diffs[0]
                if size <= 8:
                    print(
                        f"{frame}: {path} @ 0x{start:06X} ({size}B): {v1.hex()} vs {v2.hex()}"
                    )
                else:
                    print(
                        f"{frame}: {path} @ 0x{start:06X} ({size}B): {v1[:4].hex()}... vs {v2[:4].hex()}..."
                    )
            else:
                print(
                    f"{frame}: {path} — {len(diffs)} ranges, {total_bytes} bytes total"
                )
                for start, size, v1, v2 in diffs[:5]:
                    if size <= 8:
                        print(f"    0x{start:06X} ({size}B): {v1.hex()} vs {v2.hex()}")
                    else:
                        print(
                            f"    0x{start:06X} ({size}B): {v1[:4].hex()}... vs {v2[:4].hex()}..."
                        )
                if len(diffs) > 5:
                    print(f"    ... and {len(diffs) - 5} more ranges")


def print_struct_info(parser: DWARFParser):
    """Print parsed struct information for debugging."""
    print("Parsed structs:")
    for name in ["State", "GameState", "EffectState", "PLW", "WORK", "WORK_Other"]:
        struct = parser.struct_name_to_struct.get(name)
        if struct:
            print(
                f"  {name}: size=0x{struct.size:X} ({struct.size} bytes), {len(struct.members)} members"
            )
        else:
            print(f"  {name}: NOT FOUND")


def main():
    if len(sys.argv) < 2:
        print("Usage: python compare_states.py <path_to_debug_binary> [--info]")
        print("")
        print("Arguments:")
        print("  path_to_debug_binary  Path to the compiled binary with debug symbols")
        print("  --info                Print parsed struct information and exit")
        print("")
        print("Prerequisites:")
        print("  - dwarfdump (MSYS2: pacman -S mingw-w64-x86_64-dwarfutils)")
        print("  - State dumps in ./states/ directory (run game with DEBUG=1)")
        return 1

    obj_path = Path(sys.argv[1])
    show_info = "--info" in sys.argv

    if not obj_path.exists():
        print(f"Error: {obj_path} not found")
        return 1

    print(f"Parsing DWARF info from: {obj_path}")
    parser = DWARFParser()

    try:
        parser.parse_object(obj_path)
    except RuntimeError as e:
        print(f"Error: {e}")
        print(
            "Make sure dwarfdump is installed (MSYS2: pacman -S mingw-w64-x86_64-dwarfutils)"
        )
        return 1

    print(f"Parsed {len(parser.structs)} structs, {len(parser.typedefs)} typedefs")

    if show_info:
        print_struct_info(parser)
        return 0

    compare_states(parser)
    return 0


if __name__ == "__main__":
    sys.exit(main())
