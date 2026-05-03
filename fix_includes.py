"""Fix all stale #include paths left after file renames."""
import glob
import os

SRC_DIR = r"d:\3sxtra\src"

# Complete mapping of old file basenames -> new file basenames
RENAMES = {
    "pulpul": "rumble",
    "gd3rd": "afs_loader",
    "gd_data": "afs_data",
    "ioconv": "input_converter",
    "fs_sys": "filesystem",
    "vm_sub": "save_file_ops",
    "vm_data": "save_file_data",
    "vm": "save_filename",
    "se_data": "sound_effect_data",
    "se": "sound_effects",
    "sel_pl": "character_select_player",
    "sel_data": "character_select_data",
    "n_input": "name_input",
    "vs_shell": "versus_screen",
    "bbbscom2": "bonus_basketball_ai_2",
    "pow_pow": "damage_calculator",
    "pow_data": "damage_data",
    "slowf": "slow_motion",
    "ck_pass": "ai_passive_check",
    "com_data": "ai_data_tables",
    "com_datu": "ai_data_utility",
    "com_pl": "ai_player_control",
    "com_sub": "ai_subroutines",
    "fl_com00": "ai_follow_data_00",
    "fl_com02": "ai_follow_data_02",
    "follow02": "ai_follow_action_02",
}

CHAR_NAMES = [
    "gill", "alex", "ryu", "yun", "dudley", "necro",
    "hugo", "ibuki", "elena", "oro", "yang", "ken",
    "sean", "urien", "akuma", "chun_li", "makoto",
    "q", "twelve", "remy"
]

for i in range(20):
    RENAMES[f"active{i:02d}"] = f"ai_active_{CHAR_NAMES[i]}"
    RENAMES[f"pass{i:02d}"] = f"ai_passive_{CHAR_NAMES[i]}"

for i, name in [(0, "gill"), (1, "alex"), (3, "group_a"), (4, "dudley"),
                (5, "necro"), (7, "ibuki"), (11, "group_b"), (12, "sean"),
                (13, "urien"), (14, "akuma")]:
    RENAMES[f"shell{i:02d}"] = f"ai_shell_{name}"

for i in range(5):
    RENAMES[f"ac{i:04d}"] = f"ai_action_table_{i}"

for i in range(4):
    RENAMES[f"pass{i:04d}"] = f"ai_passive_unit_data_{i}"


def fix_files():
    files = [f for f in
             glob.glob(os.path.join(SRC_DIR, "**", "*.[ch]"), recursive=True) +
             glob.glob(os.path.join(SRC_DIR, "**", "*.cpp"), recursive=True)
             if "third_party" not in f]

    total = 0
    # Sort renames by length (longer first) to avoid partial matches
    sorted_renames = sorted(RENAMES.items(), key=lambda x: len(x[0]), reverse=True)

    for f in files:
        try:
            with open(f, 'r', encoding='utf-8', errors='replace') as fh:
                content = fh.read()
        except:
            continue

        new_content = content
        for old, new in sorted_renames:
            # Replace in #include paths and header guards
            for ext in [".h", ".c"]:
                new_content = new_content.replace(old + ext, new + ext)
            # Also replace header guards (uppercase)
            old_guard = old.upper() + "_H"
            new_guard = new.upper() + "_H"
            new_content = new_content.replace(old_guard, new_guard)

        if new_content != content:
            with open(f, 'w', encoding='utf-8', newline='') as fh:
                fh.write(new_content)
            total += 1
            print(f"  Fixed: {os.path.basename(f)}")

    print(f"\nFixed {total} files")


if __name__ == "__main__":
    fix_files()
