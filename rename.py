import os
import re

renames = {
    'cg_meoshi': 'cg_tc_state',
    'check_meoshi_cancel': 'check_target_combo_cancel',
    'get_meoshi_lever': 'get_tc_input_dir',
    'get_meoshi_shot': 'get_tc_input_button',
    'setup_meoshi_hit_flag': 'setup_tc_hit_flag',
    'Check_Meoshi_Attack': 'Check_Target_Combo_Attack',
    'Get_Meoshi_Data': 'Get_Target_Combo_Data',
    'get_saikinnno_idouryou': 'get_recent_movement_delta',
    'chainex_spat_cancel_kidou': 'chainex_spat_cancel_trajectory',
    'chain_hidou_nm_ground_table': 'ground_knockback_table',
    'chain_hidou_nm_air_table': 'air_knockback_table',
    'hahen_dummy': 'debris_dummy',
    'hahen_data': 'debris_data',
    'ill_hahen_data': 'ill_debris_data',
    'HAHEN': 'DEBRIS_DEF',
    'hahen': 'debris',
    'k2_kidou': 'k2_trajectory',
    'grade_t_bougyoritsu2': 'grade_defense_multiplier2',
    'grade_t_bougyoritsu3': 'grade_defense_multiplier3',
    'PiyoriType': 'StunState',
    'stun_type': 'stun_state',
    'genkai': 'stun_threshold',
    'saishin_lvdir': 'latest_stick_dir',
    'setup_saishin_lvdir': 'setup_latest_stick_dir',
    'saishin_bs2_area_car': 'latest_bs2_area_car',
    'saishin_bs2_on_car': 'latest_bs2_on_car',
    'convert_saishin_lvdir': 'convert_latest_stick_dir',
    'waku_work_index': 'bbox_work_index',
    'waku_ram_index': 'bbox_ram_index',
    'stun_gauge_waku_write': 'render_stun_gauge_frame',
    'sa_waku_trans': 'render_sa_gauge_frame',
    'sync_suzi': 'sync_bg_strip',
    'suzi_offset': 'bg_strip_offset',
    'suzi_sync_pos_set': 'sync_bg_strip_position',
    'no_suzi_line': 'disable_bg_strip',
    'start_suzi': 'start_bg_strip',
    'suzi_c_no': 'bg_strip_c_no',
    'suzi_base_flag': 'bg_strip_base_flag',
    'suzi_adrs': 'bg_strip_adrs',
    'suzi_adrs2': 'bg_strip_adrs2',
    'start_suzi2': 'start_bg_strip2',
    'suzi_c_no2': 'bg_strip_c_no2',
    'suzi_pos': 'bg_strip_pos',
    'micchaku_wall_time': 'corner_stuck_timer',
    'olc_ix': 'overlap_col_index',
    'cg_olc_ix': 'anim_overlap_col_index',
    'cg_hit_ix': 'anim_hurtbox_index',
    'cg_att_ix': 'anim_hitbox_index',
    'cmoa': 'cmd_roa_state',
    'cmsw': 'cmd_subroutine_return',
    'cmlp': 'cmd_loop_counter_1',
    'cml2': 'cmd_loop_counter_2',
    'cmja': 'cmd_jump_addr_1',
    'cmj2': 'cmd_jump_addr_2',
    'cmj3': 'cmd_jump_addr_3',
    'cmj4': 'cmd_jump_addr_4',
    'cmj5': 'cmd_jump_addr_5',
    'cmj6': 'cmd_jump_addr_6',
    'cmj7': 'cmd_jump_addr_7',
    'cmms': 'cmd_move_jump_addr',
    'cmmd': 'cmd_move_data',
    'cmyd': 'cmd_y_axis_data',
    'cmcf': 'cmd_catch_frame',
    'cmcr': 'cmd_catch_release',
    'boix': 'body_hurtbox_index',
    'haix': 'hand_hurtbox_index',
    'caix': 'catch_box_index',
    'cuix': 'caught_box_index',
    'atix': 'attack_box_index',
    'hoix': 'pushbox_index',
    'bhix': 'behind_hurtbox_index',
    'koc': 'kind_of_char',
    'cgd_type': 'char_graphic_data_type',
    'kohm': 'kind_of_hit_mark',
    'piyo': 'stun_effect',
    'mkh_ix': 'hit_mark_index',
    'but_ix': 'button_index',
    'hs_me': 'hitstop_me',
    'hs_you': 'hitstop_you',
    'cmwk': 'script_register_bank',
    'now_koc': 'current_char_type',
    'my_mr_flag': 'mirror_flag',
    'my_mr': 'mirror_scale',
    'ccoff': 'collision_center_offset',
    'at_koa': 'attack_art_type',
    'kop': 'physics_curve_type'
}

def process_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8', newline='') as f:
            content = f.read()
    except UnicodeDecodeError:
        return

    original_content = content
    for old, new in renames.items():
        content = re.sub(rf'\b{old}\b', new, content)

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8', newline='') as f:
            f.write(content)
        print(f"Updated {filepath}")

def main():
    root_dir = r"d:\3sxtra\src"
    for subdir, dirs, files in os.walk(root_dir):
        for file in files:
            if file.endswith('.c') or file.endswith('.h') or file.endswith('.cpp'):
                process_file(os.path.join(subdir, file))

if __name__ == "__main__":
    main()
