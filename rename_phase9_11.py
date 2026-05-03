import os
import glob
import re
import subprocess
import codecs

SRC_DIR = r"d:\3sxtra\src"
GAME_DIR = os.path.join(SRC_DIR, "sf33rd", "Source", "Game")

HARDCODED_MAP = {
    # Rendering
    "rendering/aboutspr": "rendering/sprite_utilities",
    "rendering/chren3rd": "rendering/character_rendering",
    "rendering/color3rd": "rendering/color_palette",
    "rendering/meta_col": "rendering/metamorphosis_color",
    "rendering/mmtmcnt": "rendering/memory_texture_control",
    "rendering/mtrans": "rendering/rendering_transform",
    "rendering/texcash": "rendering/texture_cache",
    "rendering/texgroup": "rendering/texture_group",
    # UI
    "ui/count": "ui/round_timer",
    "ui/flash_lp": "ui/flash_lamp",
    "ui/sc_cockpit": "ui/hud",
    "ui/sc_data": "ui/hud_data",
    "ui/sc_names": "ui/nameplates",
    "ui/sc_sub": "ui/hud_subroutines",
    "ui/sc_timer": "ui/hud_timer",
    # System
    "system/ramcnt": "system/ram_control",
    "system/saver": "system/save_manager",
    "system/sys_options": "system/system_options",
    "system/sys_ranking": "system/system_ranking",
    "system/sys_replay": "system/system_replay",
    "system/sys_score": "system/system_score",
    "system/sys_sub": "system/system_subroutines",
    "system/sys_sub2": "system/system_subroutines_2",
    "system/sysdir": "system/system_director",
    # Stage
    "stage/bg_constants": "stage/stage_constants",
    "stage/bg_data": "stage/stage_data",
    "stage/bg_load": "stage/stage_load",
    "stage/bg_rewrite": "stage/stage_rewrite",
    "stage/bg_sub": "stage/stage_subroutines",
    "stage/ta_sub": "stage/target_subroutines",
    "stage/bns_bg2": "stage/stage_bonus_2",
    "stage/bonus_bg": "stage/stage_bonus_1",
    "stage/tate00": "stage/stage_animation",
}

def clean_semantic_name(text):
    # Remove "Effect:" or "Stage:" prefix
    text = re.sub(r'^(Effect|Stage):\s*', '', text, flags=re.IGNORECASE)
    # Remove generic filler words
    text = re.sub(r'\b(Effect|Stage)\b', '', text, flags=re.IGNORECASE)
    # Replace non-alphanumeric with space
    text = re.sub(r'[^a-zA-Z0-9]', ' ', text)
    # Condense spaces to single underscore
    text = re.sub(r'\s+', '_', text).strip('_').lower()
    return text

def extract_semantic_name(filepath):
    try:
        with codecs.open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            lines = [f.readline() for _ in range(10)]
            
            for i, line in enumerate(lines):
                if '@file' in line:
                    if i + 1 < len(lines):
                        next_line = lines[i+1].strip()
                        # Clean up comment syntax
                        next_line = re.sub(r'^[\s/*]+', '', next_line).strip()
                        return clean_semantic_name(next_line)
    except Exception as e:
        pass
    return ""

def generate_dynamic_maps():
    dynamic_map = {}
    
    # Process Effects
    eff_files = glob.glob(os.path.join(GAME_DIR, "effect", "eff*.c"))
    for f in eff_files:
        basename = os.path.basename(f)
        if basename == "effect.c": continue
        
        name_no_ext = os.path.splitext(basename)[0]
        eff_id = name_no_ext[3:] # e.g. "00" from "eff00"
        
        semantic = extract_semantic_name(f)
        if semantic:
            new_name = f"effect_{eff_id}_{semantic}"
        else:
            new_name = f"effect_{eff_id}"
        
        dynamic_map[f"effect/{name_no_ext}"] = f"effect/{new_name}"

    # Process Stages (bg[0-9]{3})
    bg_files = glob.glob(os.path.join(GAME_DIR, "stage", "bg[0-9][0-9][0-9].c"))
    for f in bg_files:
        basename = os.path.basename(f)
        name_no_ext = os.path.splitext(basename)[0]
        bg_id = name_no_ext[2:] # e.g. "000" from "bg000"
        
        semantic = extract_semantic_name(f)
        if semantic:
            new_name = f"stage_{bg_id}_{semantic}"
        else:
            new_name = f"stage_{bg_id}"
            
        dynamic_map[f"stage/{name_no_ext}"] = f"stage/{new_name}"

    return dynamic_map

def run_git_mv(old_path, new_path):
    print(f"git mv {old_path} {new_path}")
    subprocess.run(["git", "mv", old_path, new_path], cwd=SRC_DIR)

def safe_replace(file_path, replacements_dict):
    try:
        with open(file_path, 'rb') as f:
            content = f.read()
            
        new_content = content
        for old_str, new_str in replacements_dict.items():
            new_content = new_content.replace(old_str.encode('utf-8'), new_str.encode('utf-8'))
            
        if new_content != content:
            with open(file_path, 'wb') as f:
                f.write(new_content)
    except Exception as e:
        print(f"Error processing {file_path}: {e}")

def update_header_guards(file_path, old_name, new_name):
    old_guard = old_name.replace('.', '_').upper().encode('utf-8')
    new_guard = new_name.replace('.', '_').upper().encode('utf-8')
    
    try:
        with open(file_path, 'rb') as f:
            content = f.read()

        # Simple byte replacement for the header guards
        # #ifndef OLD_GUARD -> #ifndef NEW_GUARD
        # #define OLD_GUARD -> #define NEW_GUARD
        
        # We need to be a bit careful about regex in binary, but simple replace works if we construct it
        ifndef_old = b"#ifndef " + old_guard
        ifndef_new = b"#ifndef " + new_guard
        define_old = b"#define " + old_guard
        define_new = b"#define " + new_guard
        
        new_content = content.replace(ifndef_old, ifndef_new).replace(define_old, define_new)
        
        if new_content != content:
            with open(file_path, 'wb') as f:
                f.write(new_content)
    except Exception as e:
        print(f"Error processing header guard for {file_path}: {e}")

def main():
    dynamic_map = generate_dynamic_maps()
    full_map = {**HARDCODED_MAP, **dynamic_map}
    
    print(f"Total files to rename: {len(full_map) * 2}") # .c and .h
    
    # 1. Rename files via git mv
    for old_base, new_base in full_map.items():
        old_c = os.path.join(GAME_DIR, f"{old_base}.c")
        new_c = os.path.join(GAME_DIR, f"{new_base}.c")
        old_h = os.path.join(GAME_DIR, f"{old_base}.h")
        new_h = os.path.join(GAME_DIR, f"{new_base}.h")
        
        if os.path.exists(old_c):
            run_git_mv(old_c, new_c)
        if os.path.exists(old_h):
            run_git_mv(old_h, new_h)

    # 2. Build replacement dictionaries for #includes
    # We want to replace "sf33rd/Source/Game/effect/eff00.h" with "sf33rd/Source/Game/effect/effect_00_judge.h"
    include_replacements = {}
    for old_base, new_base in full_map.items():
        old_inc = f"sf33rd/Source/Game/{old_base}.h"
        new_inc = f"sf33rd/Source/Game/{new_base}.h"
        include_replacements[old_inc] = new_inc

    # 3. Global search and replace
    all_files = glob.glob(os.path.join(SRC_DIR, "**", "*.[ch]"), recursive=True) + \
                glob.glob(os.path.join(SRC_DIR, "**", "*.cpp"), recursive=True)
                
    print(f"Scanning {len(all_files)} files for #include replacements...")
    for f in all_files:
        safe_replace(f, include_replacements)
        
    # 4. Update header guards for the renamed headers
    print("Updating header guards...")
    for old_base, new_base in full_map.items():
        new_h = os.path.join(GAME_DIR, f"{new_base}.h")
        if os.path.exists(new_h):
            old_filename = os.path.basename(old_base) + ".h"
            new_filename = os.path.basename(new_base) + ".h"
            update_header_guards(new_h, old_filename, new_filename)

    # 5. Generate legacy dictionary
    dict_path = os.path.join(SRC_DIR, "..", "legacy_to_modern_mapping.md")
    with open(dict_path, "w", encoding="utf-8") as f:
        f.write("# Legacy to Modern File Mapping\n\n")
        f.write("| Legacy Identifier | Modern Semantic Identifier |\n")
        f.write("| --- | --- |\n")
        for old_base, new_base in sorted(full_map.items()):
            f.write(f"| `{old_base}.[c|h]` | `{new_base}.[c|h]` |\n")
            
    print(f"Legacy dictionary written to {dict_path}")
    print("Done!")

if __name__ == "__main__":
    main()
