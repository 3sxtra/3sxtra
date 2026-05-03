"""
Phase 16-17: File renames (IO, Sound, Screen, Engine, AI modules)
Uses git mv + #include fixup + header guard updates.
"""
import os
import subprocess
import glob
import re

SRC_DIR = r"d:\3sxtra\src"
GAME_DIR = os.path.join(SRC_DIR, "sf33rd", "Source", "Game")

# ============================================================
# Phase 16: IO, Sound, Screen, Engine file renames
# ============================================================
PHASE_16 = [
    ("io/pulpul", "io/rumble"),
    ("io/gd3rd", "io/afs_loader"),
    ("io/gd_data", "io/afs_data"),
    ("io/ioconv", "io/input_converter"),
    ("io/fs_sys", "io/filesystem"),
    ("io/vm_sub", "io/save_file_ops"),    # must come before vm
    ("io/vm_data", "io/save_file_data"),   # must come before vm
    ("io/vm", "io/save_filename"),
    ("sound/se_data", "sound/sound_effect_data"),  # must come before se
    ("sound/se", "sound/sound_effects"),
    ("screen/sel_pl", "screen/character_select_player"),
    ("screen/sel_data", "screen/character_select_data"),
    ("screen/n_input", "screen/name_input"),
    ("screen/vs_shell", "screen/versus_screen"),
    ("engine/bbbscom2", "engine/bonus_basketball_ai_2"),
    ("engine/pow_pow", "engine/damage_calculator"),
    ("engine/pow_data", "engine/damage_data"),
    ("engine/slowf", "engine/slow_motion"),
]

# ============================================================
# Phase 17: AI module file renames
# Character index mapping (verified from source comments):
# 00=Gill, 01=Alex, 02=Ryu, 03=Yun, 04=Dudley, 05=Necro,
# 06=Hugo, 07=Ibuki, 08=Elena, 09=Oro, 10=Yang, 11=Ken,
# 12=Sean, 13=Urien, 14=Akuma, 15=Chun-Li, 16=Makoto,
# 17=Q, 18=Twelve, 19=Remy
# ============================================================
CHAR_NAMES = [
    "gill", "alex", "ryu", "yun", "dudley", "necro",
    "hugo", "ibuki", "elena", "oro", "yang", "ken",
    "sean", "urien", "akuma", "chun_li", "makoto",
    "q", "twelve", "remy"
]

PHASE_17_ACTIVE = [(f"com/active/active{i:02d}", f"com/active/ai_active_{CHAR_NAMES[i]}") for i in range(20)]
PHASE_17_PASSIVE = [(f"com/passive/pass{i:02d}", f"com/passive/ai_passive_{CHAR_NAMES[i]}") for i in range(20)]

PHASE_17_SHELL = [
    ("com/shell/shell00", "com/shell/ai_shell_gill"),
    ("com/shell/shell01", "com/shell/ai_shell_alex"),
    ("com/shell/shell03", "com/shell/ai_shell_group_a"),  # Yun, Hugo, Elena, Oro, Yang
    ("com/shell/shell04", "com/shell/ai_shell_dudley"),
    ("com/shell/shell05", "com/shell/ai_shell_necro"),
    ("com/shell/shell07", "com/shell/ai_shell_ibuki"),
    ("com/shell/shell11", "com/shell/ai_shell_group_b"),  # Ryu, Ken, Chun-Li, Makoto, Q, Twelve, Remy
    ("com/shell/shell12", "com/shell/ai_shell_sean"),
    ("com/shell/shell13", "com/shell/ai_shell_urien"),
    ("com/shell/shell14", "com/shell/ai_shell_akuma"),
]

PHASE_17_CORE = [
    ("com/ck_pass", "com/ai_passive_check"),
    ("com/com_data", "com/ai_data_tables"),
    ("com/com_datu", "com/ai_data_utility"),
    ("com/com_pl", "com/ai_player_control"),
    ("com/com_sub", "com/ai_subroutines"),
]

PHASE_17_FOLLOW = [
    ("com/follow/fl_com00", "com/follow/ai_follow_data_00"),
    ("com/follow/fl_com02", "com/follow/ai_follow_data_02"),
    ("com/follow/follow02", "com/follow/ai_follow_action_02"),
]

PHASE_17_PASSIVE_DATA = [
    ("com/passive/pass0000", "com/passive/ai_passive_unit_data_0"),
    ("com/passive/pass0001", "com/passive/ai_passive_unit_data_1"),
    ("com/passive/pass0002", "com/passive/ai_passive_unit_data_2"),
    ("com/passive/pass0003", "com/passive/ai_passive_unit_data_3"),
]

PHASE_17_ACTION = [
    ("com/active/ac0000", "com/active/ai_action_table_0"),
    ("com/active/ac0001", "com/active/ai_action_table_1"),
    ("com/active/ac0002", "com/active/ai_action_table_2"),
    ("com/active/ac0003", "com/active/ai_action_table_3"),
    ("com/active/ac0004", "com/active/ai_action_table_4"),
]


def get_source_files():
    files = glob.glob(os.path.join(SRC_DIR, "**", "*.[ch]"), recursive=True) + \
            glob.glob(os.path.join(SRC_DIR, "**", "*.cpp"), recursive=True)
    return [f for f in files if "third_party" not in f]


def update_includes_and_guards(files, old_base, new_base):
    """Update #include paths and header guards in all source files."""
    total = 0
    for ext in [".c", ".h", ".cpp"]:
        old_inc = old_base + ext
        new_inc = new_base + ext
        for f in files:
            try:
                with open(f, 'r', encoding='utf-8', errors='replace') as fh:
                    content = fh.read()
            except:
                continue

            new_content = content.replace(old_inc, new_inc)
            if new_content != content:
                with open(f, 'w', encoding='utf-8', newline='') as fh:
                    fh.write(new_content)
                total += 1

    # Update header guard
    old_guard = os.path.basename(old_base).upper() + "_H"
    new_guard = os.path.basename(new_base).upper() + "_H"
    header_path_h = os.path.join(GAME_DIR, new_base + ".h")
    if os.path.exists(header_path_h):
        try:
            with open(header_path_h, 'r', encoding='utf-8', errors='replace') as fh:
                content = fh.read()
            new_content = content.replace(old_guard, new_guard)
            if new_content != content:
                with open(header_path_h, 'w', encoding='utf-8', newline='') as fh:
                    fh.write(new_content)
                total += 1
        except:
            pass
    return total


def git_mv(old_path, new_path):
    """Run git mv if old_path exists."""
    if os.path.exists(old_path):
        result = subprocess.run(["git", "mv", old_path, new_path],
                              capture_output=True, text=True, cwd=r"d:\3sxtra")
        if result.returncode != 0:
            print(f"    ERROR: git mv {old_path} -> {new_path}: {result.stderr}")
            return False
        return True
    return False


def rename_file_pair(old_base, new_base, files):
    """Rename .c and .h files and update all references."""
    moved = 0
    for ext in [".c", ".h"]:
        old_path = os.path.join(GAME_DIR, old_base + ext)
        new_path = os.path.join(GAME_DIR, new_base + ext)
        if git_mv(old_path, new_path):
            moved += 1

    if moved > 0:
        refs = update_includes_and_guards(files, old_base, new_base)
        print(f"  {old_base} -> {new_base} ({moved} files moved, {refs} refs updated)")
    return moved


def run_rename_phase(name, renames, files):
    print(f"\n{'='*60}")
    print(f"  {name}")
    print(f"{'='*60}")
    total = 0
    for old, new in renames:
        total += rename_file_pair(old, new, files)
    print(f"  Total: {total} files moved")
    return total


def main():
    files = get_source_files()
    print(f"Found {len(files)} source files")
    grand_total = 0

    grand_total += run_rename_phase("Phase 16: IO/Sound/Screen/Engine File Renames", PHASE_16, files)

    # Refresh file list after renames
    files = get_source_files()

    # Phase 17 — longer names first to avoid partial matches in includes
    grand_total += run_rename_phase("Phase 17a: AI Passive Data Banks", PHASE_17_PASSIVE_DATA, files)
    files = get_source_files()
    grand_total += run_rename_phase("Phase 17b: AI Action Tables", PHASE_17_ACTION, files)
    files = get_source_files()
    grand_total += run_rename_phase("Phase 17c: AI Active (per-character)", PHASE_17_ACTIVE, files)
    files = get_source_files()
    grand_total += run_rename_phase("Phase 17d: AI Passive (per-character)", PHASE_17_PASSIVE, files)
    files = get_source_files()
    grand_total += run_rename_phase("Phase 17e: AI Shell (grouped)", PHASE_17_SHELL, files)
    files = get_source_files()
    grand_total += run_rename_phase("Phase 17f: AI Follow Data", PHASE_17_FOLLOW, files)
    files = get_source_files()
    grand_total += run_rename_phase("Phase 17g: AI Core Files", PHASE_17_CORE, files)

    print(f"\n{'='*60}")
    print(f"  TOTAL: {grand_total} files moved")
    print(f"{'='*60}")
    print(f"\nNow run: recompile.bat")


if __name__ == "__main__":
    main()
