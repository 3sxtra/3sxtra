import os
import re
import subprocess
import glob

# The directory to run replacements in
ROOT_DIR = "d:/3sxtra/src"

file_mapping = {
    'plcnt.c': 'player_control.c',
    'plcnt.h': 'player_control.h',
    'plcnt2.c': 'player_control_2.c',
    'plcnt2.h': 'player_control_2.h',
    'plcnt3.c': 'player_control_3.c',
    'plcnt3.h': 'player_control_3.h',
    'plmain.c': 'player_main.c',
    'plmain.h': 'player_main.h',
    'plmain2.c': 'player_main_2.c',
    'plmain2.h': 'player_main_2.h',
    'plpca.c': 'player_grab_controller.c',
    'plpca.h': 'player_grab_controller.h',
    'plpcu.c': 'player_grabbed_controller.c',
    'plpcu.h': 'player_grabbed_controller.h',
    'plpdm.c': 'player_damage_controller.c',
    'plpdm.h': 'player_damage_controller.h',
    'plpnm.c': 'player_normal_state.c',
    'plpnm.h': 'player_normal_state.h',
    'pls00.c': 'player_state_dispatcher.c',
    'pls00.h': 'player_state_dispatcher.h',
    'pls01.c': 'player_common_mechanics.c',
    'pls01.h': 'player_common_mechanics.h',
    'pls02.c': 'player_system_utilities.c',
    'pls02.h': 'player_system_utilities.h',
    'pls03.c': 'player_special_attacks.c',
    'pls03.h': 'player_special_attacks.h',
    'bbbscom.c': 'bonus_basketball_ai.c',
    'bbbscom.h': 'bonus_basketball_ai.h',
    'hitefef.c': 'hit_effect_effect.c',
    'hitefef.h': 'hit_effect_effect.h',
    'hitefpl.c': 'hit_effect_player.c',
    'hitefpl.h': 'hit_effect_player.h',
    'hitplef.c': 'hit_player_effect.c',
    'hitplef.h': 'hit_player_effect.h',
    'hitplpl.c': 'hit_player_player.c',
    'hitplpl.h': 'hit_player_player.h',
    'caldir.c': 'calculate_direction.c',
    'caldir.h': 'calculate_direction.h',
    'caldir_data.c': 'calculate_direction_data.c',
    'caldir_data.h': 'calculate_direction_data.h',
    'spgauge.c': 'super_gauge.c',
    'spgauge.h': 'super_gauge.h',
    'cmb_win.c': 'combo_window.c',
    'cmb_win.h': 'combo_window.h',
    'workuser.c': 'state_user.c',
    'workuser.h': 'state_user.h',
    'workuser_combat.h': 'state_combat.h',
    'workuser_score.h': 'state_score.h',
    'workuser_select.h': 'state_select.h',
    'workuser_system.h': 'state_system.h',
}

# Discover plpat*.c and plpat*.h
plpat_files = glob.glob(os.path.join(ROOT_DIR, '**', 'plpat*.c'), recursive=True) + \
              glob.glob(os.path.join(ROOT_DIR, '**', 'plpat*.h'), recursive=True)

for p in plpat_files:
    basename = os.path.basename(p)
    new_name = basename.replace('plpat', 'player_pattern')
    file_mapping[basename] = new_name

# 1. Rename files using git mv
print("Starting file renaming...")
for root, dirs, files in os.walk(ROOT_DIR):
    for f in files:
        if f in file_mapping:
            old_path = os.path.join(root, f)
            new_path = os.path.join(root, file_mapping[f])
            print(f"git mv {old_path} {new_path}")
            subprocess.run(["git", "mv", old_path, new_path], cwd=ROOT_DIR)

# 2. Update contents
print("Updating file contents...")
for root, dirs, files in os.walk(ROOT_DIR):
    for f in files:
        if not (f.endswith('.c') or f.endswith('.cpp') or f.endswith('.h')):
            continue
            
        filepath = os.path.join(root, f)
        try:
            with open(filepath, 'r', encoding='utf-8') as file:
                content = file.read()
        except UnicodeDecodeError:
            continue
            
        original_content = content
        
        # Replace includes exactly
        for old_name, new_name in file_mapping.items():
            content = re.sub(r'(#include\s+["<].*?)' + re.escape(old_name) + r'([">])', r'\1' + new_name + r'\2', content)
            
        # Replace specific terms
        content = re.sub(r'\bWORK\b', 'State', content)
        content = re.sub(r'\bWORK_Other\b', 'State_Other', content)
        
        # Include guards for workuser
        content = re.sub(r'\bWORKUSER_H\b', 'STATE_USER_H', content)
        content = re.sub(r'\bWORKUSER_COMBAT_H\b', 'STATE_COMBAT_H', content)
        content = re.sub(r'\bWORKUSER_SCORE_H\b', 'STATE_SCORE_H', content)
        content = re.sub(r'\bWORKUSER_SELECT_H\b', 'STATE_SELECT_H', content)
        content = re.sub(r'\bWORKUSER_SYSTEM_H\b', 'STATE_SYSTEM_H', content)
        
        # And any other occurrences of workuser that might be loose
        # (Be careful, we already renamed includes, but maybe there's a comment)
        content = re.sub(r'\bworkuser\b', 'state_user', content)

        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as file:
                file.write(content)

print("Refactoring complete.")
