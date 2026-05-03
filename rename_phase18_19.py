"""
Phase 18-19: Function/global renames and minor struct type renames.
Uses regex word-boundary replacement.
"""
import os
import glob
import re

SRC_DIR = r"d:\3sxtra\src"

# ============================================================
# Phase 18: Function & global variable renames
# ============================================================
PHASE_18 = {
    "TATE00": "stage_animate",
    "OPBG_Init": "opening_bg_init",
    "OPBG_Trans": "opening_bg_transform",
    "stngauge_cont_main": "stun_gauge_control_main",
    "spgauge_cont_main": "super_gauge_control_main",
    "scr_trans": "screen_transform",
    "Bg_On_R": "bg_enable_render",
    "cal_damage_vitality": "calculate_damage_vitality",
    "Additinal_Score_DM": "additional_score_damage",
    "set_conclusion_slow": "set_round_end_slowmo",
    "set_EXE_flag": "set_execute_flag",
    "init_slow_flag": "init_slowmo_flag",
    "bbbs_com_execute2": "bonus_basketball_ai_execute2",
    "chkVibUnit": "check_vibration_unit",
    "vibParamTrans": "vibration_param_transfer",
    "combo_cont_main": "combo_control_main",
    # Global variables
    "SLOW_timer": "slowmo_timer",
    "SLOW_flag": "slowmo_flag",
    "EXE_flag": "execute_flag",
    "vib_req": "rumble_request",
    "pulpul_scene": "rumble_scene",
    "ppwork": "rumble_work",
    "plt_req": "palette_request",
}

# ============================================================
# Phase 19: Minor struct type renames
# ============================================================
PHASE_19 = {
    "FMS_FRAME": "FrameHeapSlot",
    "PULREQ": "RumbleRequest",
    "PPWORK": "RumbleWorkState",
    "PULPARA": "RumbleParams",
    "ComboType": "ComboTracker",
    "LoHi16": "Int16Pair",
}


def get_source_files():
    files = glob.glob(os.path.join(SRC_DIR, "**", "*.[ch]"), recursive=True) + \
            glob.glob(os.path.join(SRC_DIR, "**", "*.cpp"), recursive=True)
    return [f for f in files if "third_party" not in f]


def regex_replace_all(files, renames, phase_name):
    print(f"\n{'='*60}")
    print(f"  {phase_name}")
    print(f"{'='*60}")
    total = 0
    files_changed = 0
    for f in files:
        try:
            with open(f, 'r', encoding='utf-8', errors='replace') as fh:
                content = fh.read()
        except:
            continue

        new_content = content
        for old, new in renames.items():
            pattern = re.compile(r'\b' + re.escape(old) + r'\b')
            new_content, count = pattern.subn(new, new_content)
            total += count

        if new_content != content:
            with open(f, 'w', encoding='utf-8', newline='') as fh:
                fh.write(new_content)
            files_changed += 1

    print(f"  Replacements: {total} across {files_changed} files")
    return total


def main():
    files = get_source_files()
    print(f"Found {len(files)} source files")

    grand_total = 0
    grand_total += regex_replace_all(files, PHASE_18, "Phase 18: Function & Global Renames")
    grand_total += regex_replace_all(files, PHASE_19, "Phase 19: Minor Struct Types")

    print(f"\n{'='*60}")
    print(f"  TOTAL: {grand_total} replacements")
    print(f"{'='*60}")
    print(f"\nNow run: recompile.bat")


if __name__ == "__main__":
    main()
