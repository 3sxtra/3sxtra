import os
import re

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
    # patterns
    'plpatuni.c': 'player_patternuni.c',
    'plpatuni.h': 'player_patternuni.h',
    'plpat_akuma.c': 'player_pattern_akuma.c',
    'plpat_akuma.h': 'player_pattern_akuma.h',
    'plpat_alex.c': 'player_pattern_alex.c',
    'plpat_alex.h': 'player_pattern_alex.h',
    'plpat_chun_li.c': 'player_pattern_chun_li.c',
    'plpat_chun_li.h': 'player_pattern_chun_li.h',
    'plpat_dudley.c': 'player_pattern_dudley.c',
    'plpat_dudley.h': 'player_pattern_dudley.h',
    'plpat_elena.c': 'player_pattern_elena.c',
    'plpat_elena.h': 'player_pattern_elena.h',
    'plpat_gill.c': 'player_pattern_gill.c',
    'plpat_gill.h': 'player_pattern_gill.h',
    'plpat_hugo.c': 'player_pattern_hugo.c',
    'plpat_hugo.h': 'player_pattern_hugo.h',
    'plpat_ibuki.c': 'player_pattern_ibuki.c',
    'plpat_ibuki.h': 'player_pattern_ibuki.h',
    'plpat_ken.c': 'player_pattern_ken.c',
    'plpat_ken.h': 'player_pattern_ken.h',
    'plpat_makoto.c': 'player_pattern_makoto.c',
    'plpat_makoto.h': 'player_pattern_makoto.h',
    'plpat_necro.c': 'player_pattern_necro.c',
    'plpat_necro.h': 'player_pattern_necro.h',
    'plpat_oro.c': 'player_pattern_oro.c',
    'plpat_oro.h': 'player_pattern_oro.h',
    'plpat_q.c': 'player_pattern_q.c',
    'plpat_q.h': 'player_pattern_q.h',
    'plpat_remy.c': 'player_pattern_remy.c',
    'plpat_remy.h': 'player_pattern_remy.h',
    'plpat_ryu.c': 'player_pattern_ryu.c',
    'plpat_ryu.h': 'player_pattern_ryu.h',
    'plpat_sean.c': 'player_pattern_sean.c',
    'plpat_sean.h': 'player_pattern_sean.h',
    'plpat_twelve.c': 'player_pattern_twelve.c',
    'plpat_twelve.h': 'player_pattern_twelve.h',
    'plpat_urien.c': 'player_pattern_urien.c',
    'plpat_urien.h': 'player_pattern_urien.h',
    'plpat_yang.c': 'player_pattern_yang.c',
    'plpat_yang.h': 'player_pattern_yang.h',
    'plpat_yun.c': 'player_pattern_yun.c',
    'plpat_yun.h': 'player_pattern_yun.h',
}

print("Updating file contents with cp932 fallback...")
for root, dirs, files in os.walk(ROOT_DIR):
    for f in files:
        if not (f.endswith('.c') or f.endswith('.cpp') or f.endswith('.h')):
            continue
            
        filepath = os.path.join(root, f)
        
        encoding_used = 'utf-8'
        try:
            with open(filepath, 'r', encoding='utf-8') as file:
                content = file.read()
        except UnicodeDecodeError:
            try:
                with open(filepath, 'r', encoding='cp932') as file:
                    content = file.read()
                    encoding_used = 'cp932'
            except UnicodeDecodeError:
                print(f"Failed to read {filepath} with both utf-8 and cp932")
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
        content = re.sub(r'\bworkuser\b', 'state_user', content)

        if content != original_content:
            with open(filepath, 'w', encoding=encoding_used) as file:
                file.write(content)
            print(f"Updated {filepath} (encoding: {encoding_used})")

print("Refactoring complete.")
