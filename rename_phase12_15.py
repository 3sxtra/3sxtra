"""
Phase 12-15: Identifier Renames (struct types + field names)
Uses binary byte replacement — the proven approach from phases 1-11.

Run: python rename_phase12_15.py
Then: recompile.bat
"""
import os
import glob
import re
import sys

SRC_DIR = r"d:\3sxtra\src"

# ============================================================
# Phase 12: Core struct type names
# Order matters: longer/more-specific first to avoid partial matches
# ============================================================
PHASE_12 = {
    b"WORK_Other_CONN": b"EffectMultiSprite",
    b"SA_WORK": b"SuperArtGauge",
    b"WORK_CP": b"CommandInputState",
    # PLW must not match PLW inside other words — but as a typedef it's always
    # at word boundaries. Binary replace is safe here because PLW is 3 uppercase
    # letters that don't appear as a substring of any other identifier.
    b"PLW": b"PlayerEntity",
    b"MVXY": b"MovementVector",
    # NOTE: CONN is handled separately with regex word-boundary matching
    # because it's a substring of CONNECT, CONNECTED, CONNECTION, etc.
}

# ============================================================
# Phase 13: SA_WORK (now SuperArtGauge) field names
# These are accessed as ->field or .field, so we use those prefixes
# Longer names first to avoid partial matches
# ============================================================
PHASE_13_FIELDS = {
    # Longer compound names first
    b"bacckup_g_h": b"backup_gauge_high",
    b"dtm_mul": b"damage_time_multiplier",
    b"saeff_ok": b"super_effect_can_activate",
    b"saeff_mp": b"super_effect_meter",
    b"nmsa_g_ix": b"normal_sa_graphic_ix",
    b"exsa_g_ix": b"ex_sa_graphic_ix",
    b"exs2_g_ix": b"ex_sa2_graphic_ix",
    b"nmsa_a_ix": b"normal_sa_anim_ix",
    b"exsa_a_ix": b"ex_sa_anim_ix",
    b"exs2_a_ix": b"ex_sa2_anim_ix",
    b"store_max": b"stock_max",
    b"gauge_len": b"gauge_length",
    b"id_arts": b"super_art_id",
    b"mp_rno2": b"meter_routine_no_2",
    b"sa_rno2": b"super_art_routine_no_2",
    b"mp_rno": b"meter_routine_no",
    b"sa_rno": b"super_art_routine_no",
    b"ex_rno": b"ex_routine_no",
    # ex4th_full and ex4th_exec are already descriptive — keep as-is
    # NOTE: store, store_max moved to PHASE_13_SHORT for context-aware matching
    # because 'store' is a substring of SDL's 'store_op' etc.
}

# Short SA_WORK fields — need context-aware matching
# These will be replaced with . and -> prefixes only
PHASE_13_SHORT = {
    b"store_max": b"stock_max",
    b"store": b"stock",
    b"mp": b"meter_points",
    b"ok": b"can_activate",
    b"ex": b"ex_mode",
    b"ba": b"bar_count",
    b"gt2": b"gauge_type_2",
    b"dtm": b"damage_time",
}

# ============================================================
# Phase 14: WORK_CP (now CommandInputState) field names
# ============================================================
PHASE_14_FIELDS = {
    b"lgp": b"lever_grace_period",
    b"ca14": b"combo_btn_14",
    b"ca25": b"combo_btn_25",
    b"ca36": b"combo_btn_36",
    b"calf": b"combo_all_forward",
    b"calr": b"combo_all_reverse",
    # btix and exdt are array fields, used with [index]
    b"btix": b"button_index",
    b"exdt": b"extended_data",
}

# ============================================================
# Phase 15: State/PLW remaining cryptic fields
# ============================================================
PHASE_15_FIELDS = {
    # State fields — longer first
    b"dm_jump_att_flag": b"damage_jump_attack_flag",
    b"dm_nodeathattack": b"damage_no_death_attack",
    b"dm_arts_point": b"damage_arts_point",
    b"dm_attribute": b"damage_attribute",
    b"dm_count_up": b"damage_count_up",
    b"dm_exdm_ix": b"damage_extra_index",
    b"dm_work_id": b"damage_work_id",
    b"dm_ten_ix": b"damage_chain_index",
    b"dm_weight": b"damage_weight",
    b"dm_impact": b"damage_impact",
    b"dm_attlv": b"damage_attack_level",
    b"dm_plnum": b"damage_player_num",
    b"dm_free": b"damage_free",
    b"dm_dir": b"damage_direction",
    b"dm_dip": b"damage_dip",
    b"dm_rl": b"damage_facing",
    b"hm_dm_side": b"hitmark_damage_side",
    b"at_attribute": b"attack_attribute",
    b"at_ten_ix": b"attack_chain_index",
    b"uketa_att": b"received_attack",
    b"scr_mv_x": b"screen_move_x",
    b"scr_mv_y": b"screen_move_y",
    b"be_flag": b"active_flag",
    b"rl_flag": b"facing_flag",
    b"zu_flag": b"head_invuln_flag",
    b"dead_f": b"death_timer",
    b"attpow": b"attack_power",
    b"defpow": b"defense_power",
    b"my_mts": b"my_sprite_sheet",
    b"wrd_free": b"reserved_bytes",
    # PLW fields — longer first
    b"cat_break_ok_timer": b"catch_break_ok_timer",
    b"cat_break_reserve": b"catch_break_reserve",
    b"sa_stop_lvdir": b"super_art_stop_lever_dir",
    b"omop_vital_timer": b"emergency_vital_timer",
    b"uot_cd_ok_flag": b"ukemi_cooldown_ok",
    b"bullet_hcnt": b"bullet_hit_count",
    b"bhcnt_timer": b"bullet_hit_count_timer",
    b"sa_stop_sai": b"super_art_stop_index",
    b"permited_koa": b"permitted_art_type",
    b"hos_fi_flag": b"pushbox_finish_flag",
    b"hos_em_flag": b"pushbox_emergency_flag",
    b"dm_hos_flag": b"damage_pushbox_flag",
    b"dm_refrect": b"damage_reflect",
    b"gill_ccch_go": b"gill_catch_go",
    b"renew_attchar": b"renew_attack_char",
    b"tk_success": b"target_combo_success",
    b"running_f": b"running_flag",
    b"wkey_flag": b"wakeup_key_flag",
    b"dm_point": b"damage_point",
    b"dm_ix": b"damage_index",
}


def safe_replace_binary(file_path, replacements):
    """Replace byte sequences in a file. Returns count of replacements made."""
    try:
        with open(file_path, 'rb') as f:
            content = f.read()
    except Exception as e:
        return 0

    new_content = content
    total = 0
    for old, new in replacements.items():
        count = new_content.count(old)
        if count > 0:
            new_content = new_content.replace(old, new)
            total += count

    if new_content != content:
        with open(file_path, 'wb') as f:
            f.write(new_content)
    return total


def safe_replace_context_aware(file_path, short_fields):
    """Replace short field names using regex word-boundary after . or -> prefix,
    AND as word-boundary matches for struct definitions."""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
    except Exception:
        return 0

    new_content = content
    total = 0
    for old, new in short_fields.items():
        old_s = old.decode('utf-8')
        new_s = new.decode('utf-8')
        # Strategy: use full word-boundary regex \bOLD\b
        # This handles both struct definitions AND access patterns
        # Word boundary prevents matching as substring (e.g. 'store' won't match 'store_op')
        pattern = re.compile(r'\b' + re.escape(old_s) + r'\b')
        new_content, count = pattern.subn(new_s, new_content)
        total += count

    if new_content != content:
        with open(file_path, 'w', encoding='utf-8', newline='') as f:
            f.write(new_content)
    return total


def regex_replace_word_boundary(file_path, old_word, new_word):
    """Replace old_word with new_word using word-boundary regex. Returns count."""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
    except Exception:
        return 0

    pattern = re.compile(r'\b' + re.escape(old_word) + r'\b')
    new_content, count = pattern.subn(new_word, content)

    if count > 0:
        with open(file_path, 'w', encoding='utf-8', newline='') as f:
            f.write(new_content)
    return count


def get_source_files():
    """Get all C/C++ source files under src/, excluding third_party."""
    files = glob.glob(os.path.join(SRC_DIR, "**", "*.[ch]"), recursive=True) + \
            glob.glob(os.path.join(SRC_DIR, "**", "*.cpp"), recursive=True)
    return [f for f in files if "third_party" not in f]


def run_phase(name, replacements, files, context_aware_fields=None):
    print(f"\n{'='*60}")
    print(f"  {name}")
    print(f"{'='*60}")
    total = 0
    files_changed = 0
    for f in files:
        count = safe_replace_binary(f, replacements)
        if context_aware_fields:
            count += safe_replace_context_aware(f, context_aware_fields)
        if count > 0:
            files_changed += 1
            total += count
    print(f"  Replacements: {total} across {files_changed} files")
    return total


def main():
    files = get_source_files()
    print(f"Found {len(files)} source files")

    grand_total = 0

    # Phase 12 — binary replacement for safe tokens
    grand_total += run_phase("Phase 12: Core Struct Types", PHASE_12, files)

    # Phase 12b — regex word-boundary for CONN (unsafe as binary due to CONNECT etc)
    print(f"\n{'='*60}")
    print(f"  Phase 12b: CONN -> SpriteConnection (regex)")
    print(f"{'='*60}")
    conn_total = 0
    conn_files = 0
    for f in files:
        c = regex_replace_word_boundary(f, "CONN", "SpriteConnection")
        if c > 0:
            conn_total += c
            conn_files += 1
    print(f"  Replacements: {conn_total} across {conn_files} files")
    grand_total += conn_total

    # Phase 13 — compound fields first, then short context-aware fields
    grand_total += run_phase("Phase 13: SuperArtGauge Fields",
                             PHASE_13_FIELDS, files,
                             context_aware_fields=PHASE_13_SHORT)

    # Phase 14
    grand_total += run_phase("Phase 14: CommandInputState Fields",
                             PHASE_14_FIELDS, files)

    # Phase 15
    grand_total += run_phase("Phase 15: State/PlayerEntity Fields",
                             PHASE_15_FIELDS, files)

    print(f"\n{'='*60}")
    print(f"  TOTAL: {grand_total} replacements")
    print(f"{'='*60}")
    print(f"\nNow run: recompile.bat")


if __name__ == "__main__":
    main()
