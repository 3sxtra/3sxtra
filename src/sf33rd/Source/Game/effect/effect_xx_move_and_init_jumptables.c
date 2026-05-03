/**
 * @file effxx.c
 * Effect Move and Init Jumptables
 */

// Uncomment once effmovejptbl is fully decompiled
// #include "sf33rd/Source/Game/effect/effect_xx_move_and_init_jumptables.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_00_judge_system.h"
#include "sf33rd/Source/Game/effect/effect_01_parts_overlap.h"
#include "sf33rd/Source/Game/effect/effect_02_hit_marks_sparks.h"
#include "sf33rd/Source/Game/effect/effect_03_player_effects_dust_impact.h"
#include "sf33rd/Source/Game/effect/effect_04_projectile_object.h"
#include "sf33rd/Source/Game/effect/effect_05_background.h"
#include "sf33rd/Source/Game/effect/effect_06_data_screen_object.h"
#include "sf33rd/Source/Game/effect/effect_07_water_liquid.h"
#include "sf33rd/Source/Game/effect/effect_08_color_palette_manipulation.h"
#include "sf33rd/Source/Game/effect/effect_09_appearance_entry.h"
#include "sf33rd/Source/Game/effect/effect_10_ui_screen_check_data.h"
#include "sf33rd/Source/Game/effect/effect_11_quake_directional.h"
#include "sf33rd/Source/Game/effect/effect_12_screen_object_flash.h"
#include "sf33rd/Source/Game/effect/effect_13_quake_shadow_homing.h"
#include "sf33rd/Source/Game/effect/effect_14_g_state_score_bonus_display_16x24_digits.h"
#include "sf33rd/Source/Game/effect/effect_15_g_state_score_bonus_display.h"
#include "sf33rd/Source/Game/effect/effect_16_decomposition_part_break_bunkai.h"
#include "sf33rd/Source/Game/effect/effect_17_dispatch_general.h"
#include "sf33rd/Source/Game/effect/effect_18_visual_misc.h"
#include "sf33rd/Source/Game/effect/effect_19_quake.h"
#include "sf33rd/Source/Game/effect/effect_20_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_21_speed_motion_blur.h"
#include "sf33rd/Source/Game/effect/effect_22_snow_weather.h"
#include "sf33rd/Source/Game/effect/effect_23_quake.h"
#include "sf33rd/Source/Game/effect/effect_24_quake_horizontal_vertical.h"
#include "sf33rd/Source/Game/effect/effect_25_background.h"
#include "sf33rd/Source/Game/effect/effect_26_background.h"
#include "sf33rd/Source/Game/effect/effect_27_screen_object_piece_data.h"
#include "sf33rd/Source/Game/effect/effect_29_vanish_timeout.h"
#include "sf33rd/Source/Game/effect/effect_30_object_etc3_character.h"
#include "sf33rd/Source/Game/effect/effect_31_object_etc3_character.h"
#include "sf33rd/Source/Game/effect/effect_32_object_etc3_character.h"
#include "sf33rd/Source/Game/effect/effect_33_win_lose_symbol.h"
#include "sf33rd/Source/Game/effect/effect_34_object_etc3_character.h"
#include "sf33rd/Source/Game/effect/effect_35_data_table_general.h"
#include "sf33rd/Source/Game/effect/effect_36_data_table_debug.h"
#include "sf33rd/Source/Game/effect/effect_37_panel_guide.h"
#include "sf33rd/Source/Game/effect/effect_38_quake_base_xy.h"
#include "sf33rd/Source/Game/effect/effect_39_quake.h"
#include "sf33rd/Source/Game/effect/effect_40_position_data.h"
#include "sf33rd/Source/Game/effect/effect_41_super_art_sign.h"
#include "sf33rd/Source/Game/effect/effect_42_quake.h"
#include "sf33rd/Source/Game/effect/effect_43_game_state.h"
#include "sf33rd/Source/Game/effect/effect_44_screen_object_multiple.h"
#include "sf33rd/Source/Game/effect/effect_45_debug_game_state.h"
#include "sf33rd/Source/Game/effect/effect_46_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_47_speed_data_table.h"
#include "sf33rd/Source/Game/effect/effect_48_numeric_counter.h"
#include "sf33rd/Source/Game/effect/effect_49_work_user_character_state.h"
#include "sf33rd/Source/Game/effect/effect_50_work_user_character_state.h"
#include "sf33rd/Source/Game/effect/effect_51_brief_system_direction_menu_selected_value_label_renderer.h"
#include "sf33rd/Source/Game/effect/effect_52_quake.h"
#include "sf33rd/Source/Game/effect/effect_53_vanish_timer.h"
#include "sf33rd/Source/Game/effect/effect_54_texture_cache.h"
#include "sf33rd/Source/Game/effect/effect_55_background_brz_table.h"
#include "sf33rd/Source/Game/effect/effect_56_color_bonus.h"
#include "sf33rd/Source/Game/effect/effect_57_header_for_menus.h"
#include "sf33rd/Source/Game/effect/effect_58_sound_se_request.h"
#include "sf33rd/Source/Game/effect/effect_59_correct_data_adjustment.h"
#include "sf33rd/Source/Game/effect/effect_60_flash_screen_flash.h"
#include "sf33rd/Source/Game/effect/effect_61_menu_options.h"
#include "sf33rd/Source/Game/effect/effect_62_correct_data_adjustment.h"
#include "sf33rd/Source/Game/effect/effect_63_quake.h"
#include "sf33rd/Source/Game/effect/effect_64_quake.h"
#include "sf33rd/Source/Game/effect/effect_65_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_66_quake_half_object_flash.h"
#include "sf33rd/Source/Game/effect/effect_67_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_68_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_69_quake.h"
#include "sf33rd/Source/Game/effect/effect_70_suicide_handler.h"
#include "sf33rd/Source/Game/effect/effect_71_time_table_slow.h"
#include "sf33rd/Source/Game/effect/effect_72_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_73_vanish_survive_table.h"
#include "sf33rd/Source/Game/effect/effect_74_position_data_quake_link.h"
#include "sf33rd/Source/Game/effect/effect_75_quake_link.h"
#include "sf33rd/Source/Game/effect/effect_76_quake.h"
#include "sf33rd/Source/Game/effect/effect_77_collision_slow.h"
#include "sf33rd/Source/Game/effect/effect_78_quake_crow.h"
#include "sf33rd/Source/Game/effect/effect_79_quake_z_axis.h"
#include "sf33rd/Source/Game/effect/effect_80_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_81_quake.h"
#include "sf33rd/Source/Game/effect/effect_82_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_83_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_84_time_data_slow.h"
#include "sf33rd/Source/Game/effect/effect_85_quake_character_index.h"
#include "sf33rd/Source/Game/effect/effect_86_data_table.h"
#include "sf33rd/Source/Game/effect/effect_90_debug_visual.h"
#include "sf33rd/Source/Game/effect/effect_91_position_data.h"
#include "sf33rd/Source/Game/effect/effect_92_mark_ui_rewrite.h"
#include "sf33rd/Source/Game/effect/effect_93_quake_jump_table.h"
#include "sf33rd/Source/Game/effect/effect_94_quake_multiple_tables.h"
#include "sf33rd/Source/Game/effect/effect_95_data_table.h"
#include "sf33rd/Source/Game/effect/effect_96_suicide_handler.h"
#include "sf33rd/Source/Game/effect/effect_97_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_98_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_99_position_data.h"
#include "sf33rd/Source/Game/effect/effect_a0_position_data.h"
#include "sf33rd/Source/Game/effect/effect_a1_visual_misc.h"
#include "sf33rd/Source/Game/effect/effect_a2_color_table.h"
#include "sf33rd/Source/Game/effect/effect_a3_content_check_system.h"
#include "sf33rd/Source/Game/effect/effect_a6_player_2_data_visual.h"
#include "sf33rd/Source/Game/effect/effect_a7_quake_hit_spark.h"
#include "sf33rd/Source/Game/effect/effect_a8_position_data.h"
#include "sf33rd/Source/Game/effect/effect_a9_suicide_handler.h"
#include "sf33rd/Source/Game/effect/effect_b0_timer_ending_data.h"
#include "sf33rd/Source/Game/effect/effect_b1_sound_wait_timer.h"
#include "sf33rd/Source/Game/effect/effect_b2_sound_collision.h"
#include "sf33rd/Source/Game/effect/effect_b3_quake.h"
#include "sf33rd/Source/Game/effect/effect_b4_mark_table.h"
#include "sf33rd/Source/Game/effect/effect_b5_game_state.h"
#include "sf33rd/Source/Game/effect/effect_b6_message_debug_text.h"
#include "sf33rd/Source/Game/effect/effect_b7_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_b8_quake.h"
#include "sf33rd/Source/Game/effect/effect_b9_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_c0_hok_table_player_adjust.h"
#include "sf33rd/Source/Game/effect/effect_c1_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_c2_quake_bs2_data.h"
#include "sf33rd/Source/Game/effect/effect_c3_car_parts_nsc.h"
#include "sf33rd/Source/Game/effect/effect_c4_menu_ex_data.h"
#include "sf33rd/Source/Game/effect/effect_c5_appear_entry.h"
#include "sf33rd/Source/Game/effect/effect_c6_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_c7_paring_mark.h"
#include "sf33rd/Source/Game/effect/effect_c8_data_table.h"
#include "sf33rd/Source/Game/effect/effect_c9_sound_judge_shadow.h"
#include "sf33rd/Source/Game/effect/effect_d0_counter_data_table.h"
#include "sf33rd/Source/Game/effect/effect_d1_fall_no_death_attack.h"
#include "sf33rd/Source/Game/effect/effect_d3_timer_color.h"
#include "sf33rd/Source/Game/effect/effect_d4_suction_swallow.h"
#include "sf33rd/Source/Game/effect/effect_d5_sound_range_check.h"
#include "sf33rd/Source/Game/effect/effect_d6_flower_hana.h"
#include "sf33rd/Source/Game/effect/effect_d7_sound_hit_box.h"
#include "sf33rd/Source/Game/effect/effect_d8_quake_priority.h"
#include "sf33rd/Source/Game/effect/effect_d9_collision_table_1p_2p.h"
#include "sf33rd/Source/Game/effect/effect_e0_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_e1_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_e2_flame_freeze_status.h"
#include "sf33rd/Source/Game/effect/effect_e3_gauge_player_control.h"
#include "sf33rd/Source/Game/effect/effect_e4_gauge_player_control.h"
#include "sf33rd/Source/Game/effect/effect_e5_after_image_illusion.h"
#include "sf33rd/Source/Game/effect/effect_e6_ending_scene_gill_general.h"
#include "sf33rd/Source/Game/effect/effect_e7_after_image_slow.h"
#include "sf33rd/Source/Game/effect/effect_e8_after_image_work_user.h"
#include "sf33rd/Source/Game/effect/effect_e9_ending_renderer.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effect_f0_texture_visual.h"
#include "sf33rd/Source/Game/effect/effect_f2_speed_timer.h"
#include "sf33rd/Source/Game/effect/effect_f5_dispatch_table.h"
#include "sf33rd/Source/Game/effect/effect_f6_move_data_table.h"
#include "sf33rd/Source/Game/effect/effect_f8_paring_mark_b.h"
#include "sf33rd/Source/Game/effect/effect_f9_text_message.h"
#include "sf33rd/Source/Game/effect/effect_g0_quake_g_state_score_result.h"
#include "sf33rd/Source/Game/effect/effect_g3_suicide_handler.h"
#include "sf33rd/Source/Game/effect/effect_g4_gill_data.h"
#include "sf33rd/Source/Game/effect/effect_g5_sweat_ase.h"
#include "sf33rd/Source/Game/effect/effect_g6_data_g6.h"
#include "sf33rd/Source/Game/effect/effect_g7_ending_data.h"
#include "sf33rd/Source/Game/effect/effect_g8_sp_table.h"
#include "sf33rd/Source/Game/effect/effect_g9_position_adjust.h"
#include "sf33rd/Source/Game/effect/effect_h0_difficulty_bbbs.h"
#include "sf33rd/Source/Game/effect/effect_h1_wait_timer_ending.h"
#include "sf33rd/Source/Game/effect/effect_h2_panel_guide.h"
#include "sf33rd/Source/Game/effect/effect_h6_code_table_ending.h"
#include "sf33rd/Source/Game/effect/effect_h9_ball_bbbs.h"
#include "sf33rd/Source/Game/effect/effect_i0_pebble_koishi.h"
#include "sf33rd/Source/Game/effect/effect_i3_background_data.h"
#include "sf33rd/Source/Game/effect/effect_i4_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_i6_game_state.h"
#include "sf33rd/Source/Game/effect/effect_i7_ex_sign_meter.h"
#include "sf33rd/Source/Game/effect/effect_i8_sound_hit_box_tall.h"
#include "sf33rd/Source/Game/effect/effect_i9_image_buffer_manager.h"
#include "sf33rd/Source/Game/effect/effect_j0_image_buffer_manager.h"
#include "sf33rd/Source/Game/effect/effect_j2_difficulty_large_bbbs.h"
#include "sf33rd/Source/Game/effect/effect_j4_suicide_handler.h"
#include "sf33rd/Source/Game/effect/effect_j6_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_j7_color_player_l.h"
#include "sf33rd/Source/Game/effect/effect_j8_timer_y_table.h"
#include "sf33rd/Source/Game/effect/effect_j9_quake_c2_table.h"
#include "sf33rd/Source/Game/effect/effect_k2_sound_debris.h"
#include "sf33rd/Source/Game/effect/effect_k3_isp_particle.h"
#include "sf33rd/Source/Game/effect/effect_k4_hit_spark_isp.h"
#include "sf33rd/Source/Game/effect/effect_k5_lookup_index_check.h"
#include "sf33rd/Source/Game/effect/effect_k6_quake.h"
#include "sf33rd/Source/Game/effect/effect_k7_player_common.h"
#include "sf33rd/Source/Game/effect/effect_k8_seraphic_wing.h"
#include "sf33rd/Source/Game/effect/effect_k9_color_transition.h"
#include "sf33rd/Source/Game/effect/effect_l0_suicide_handler.h"
#include "sf33rd/Source/Game/effect/effect_l1_decomposition_grade_g_state_score.h"
#include "sf33rd/Source/Game/effect/effect_l2_direction_table.h"
#include "sf33rd/Source/Game/effect/effect_l3_wait_timer_data_table.h"
#include "sf33rd/Source/Game/effect/effect_l4_data_table.h"
#include "sf33rd/Source/Game/effect/effect_l5_game_state.h"
#include "sf33rd/Source/Game/effect/effect_l6_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_l7_win_player_data_table.h"
#include "sf33rd/Source/Game/effect/effect_l8_color_player_17.h"
#include "sf33rd/Source/Game/effect/effect_l9_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_m0_animal_table.h"
#include "sf33rd/Source/Game/effect/effect_m1_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_m2_character_table_animal.h"
#include "sf33rd/Source/Game/effect/effect_m3_bahn_path_movement.h"
#include "sf33rd/Source/Game/effect/effect_m5_appear_entry.h"
#include "sf33rd/Source/Game/effect/effect_m6_simple_animation.h"
#include "sf33rd/Source/Game/effect/effect_m7_data_table.h"
#include "sf33rd/Source/Game/effect/effect_m8_random_timer.h"
#include "sf33rd/Source/Game/engine/player_pattern_oro.h"

static s32 effect_dummy_init() {
    return -1;
}

static void effect_dummy_move() {
    // Do nothing
}

const void (*effmovejptbl[229])() = {
    effect_00_move,    effect_01_move,    effect_02_move,    effect_03_move,    effect_04_move,    effect_05_move,
    effect_06_move,    effect_07_move,    effect_08_move,    effect_09_move,    effect_10_move,    effect_11_move,
    effect_12_move,    effect_13_move,    effect_14_move,    effect_15_move,    effect_16_move,    effect_17_move,
    effect_18_move,    effect_19_move,    effect_20_move,    effect_21_move,    effect_22_move,    effect_23_move,
    effect_24_move,    effect_25_move,    effect_26_move,    effect_27_move,    effect_dummy_move, effect_29_move,
    effect_30_move,    effect_31_move,    effect_32_move,    effect_33_move,    effect_34_move,    effect_35_move,
    effect_36_move,    effect_37_move,    effect_38_move,    effect_39_move,    effect_40_move,    effect_41_move,
    effect_42_move,    effect_43_move,    effect_44_move,    effect_45_move,    effect_46_move,    effect_47_move,
    effect_48_move,    effect_49_move,    effect_50_move,    effect_51_move,    effect_52_move,    effect_53_move,
    effect_54_move,    effect_55_move,    effect_56_move,    effect_57_move,    effect_58_move,    effect_59_move,
    effect_60_move,    effect_61_move,    effect_62_move,    effect_63_move,    effect_64_move,    effect_65_move,
    effect_66_move,    effect_67_move,    effect_68_move,    effect_69_move,    effect_70_move,    effect_71_move,
    effect_72_move,    effect_73_move,    effect_74_move,    effect_75_move,    effect_76_move,    effect_77_move,
    effect_78_move,    effect_79_move,    effect_80_move,    effect_81_move,    effect_82_move,    effect_83_move,
    effect_84_move,    effect_85_move,    effect_86_move,    effect_dummy_move, effect_dummy_move, effect_dummy_move,
    effect_90_move,    effect_91_move,    effect_92_move,    effect_93_move,    effect_94_move,    effect_95_move,
    effect_96_move,    effect_97_move,    effect_98_move,    effect_99_move,    effect_A0_move,    effect_A1_move,
    effect_A2_move,    effect_A3_move,    effect_dummy_move, effect_dummy_move, effect_A6_move,    effect_A7_move,
    effect_A8_move,    effect_A9_move,    effect_B0_move,    effect_B1_move,    effect_B2_move,    effect_B3_move,
    effect_B4_move,    effect_B5_move,    effect_B6_move,    effect_B7_move,    effect_B8_move,    effect_B9_move,
    effect_C0_move,    effect_C1_move,    effect_C2_move,    effect_C3_move,    effect_C4_move,    effect_C5_move,
    effect_C6_move,    effect_C7_move,    effect_C8_move,    effect_C9_move,    effect_D0_move,    effect_D1_move,
    effect_dummy_move, effect_D3_move,    effect_D4_move,    effect_D5_move,    effect_D6_move,    effect_D7_move,
    effect_D8_move,    effect_D9_move,    effect_E0_move,    effect_E1_move,    effect_E2_move,    effect_e3_move,
    effect_E4_move,    effect_E5_move,    effect_E6_move,    effect_E7_move,    effect_E8_move,    effect_E9_move,
    effect_F0_move,    effect_dummy_move, effect_F2_move,    effect_dummy_move, effect_dummy_move, effect_F5_move,
    effect_F6_move,    effect_dummy_move, effect_F8_move,    effect_F9_move,    effect_G0_move,    effect_dummy_move,
    effect_dummy_move, effect_G3_move,    effect_G4_move,    effect_G5_move,    effect_G6_move,    effect_G7_move,
    effect_G8_move,    effect_G9_move,    effect_H0_move,    effect_H1_move,    effect_H2_move,    effect_dummy_move,
    effect_dummy_move, effect_dummy_move, effect_H6_move,    effect_dummy_move, effect_dummy_move, effect_H9_move,
    effect_I0_move,    effect_dummy_move, effect_dummy_move, effect_I3_move,    effect_I4_move,    effect_dummy_move,
    effect_I6_move,    effect_I7_move,    effect_I8_move,    effect_I9_move,    effect_J0_move,    effect_dummy_move,
    effect_J2_move,    effect_dummy_move, effect_J4_move,    effect_dummy_move, effect_J6_move,    effect_J7_move,
    effect_J8_move,    effect_J9_move,    effect_dummy_move, effect_dummy_move, effect_K2_move,    effect_K3_move,
    effect_K4_move,    effect_k5_move,    effect_K6_move,    effect_K7_move,    effect_K8_move,    effect_K9_move,
    effect_L0_move,    effect_L1_move,    effect_L2_move,    effect_L3_move,    effect_L4_move,    effect_L5_move,
    effect_L6_move,    effect_L7_move,    effect_L8_move,    effect_L9_move,    effect_M0_move,    effect_M1_move,
    effect_M2_move,    effect_M3_move,    effect_dummy_move, effect_M5_move,    effect_M6_move,    effect_M7_move,
    effect_M8_move,
};

const s32 (*effinitjptbl[59])() = {
    NULL,
    effect_03_init,
    effect_13_init,
    effect_09_init,
    effect_G7_init,
    effect_C0_init,
    effect_C7_init,
    effect_D0_init,
    effect_D1_init,
    effect_dummy_init,
    effect_34_init,
    effect_37_init,
    effect_09_init2,
    effect_41_init,
    effect_D4_init,
    set_tenguiwa,
    effect_D9_init,
    setup_accessories,
    setup_after_images,
    erase_after_images,
    effect_dummy_init,
    clear_caution_flag,
    setup_status_flag,
    reset_extra_bg_flag,
    flip_my_facing_flag,
    effect_F8_init,
    clear_caution_flag,
    effect_G3_init,
    effect_G4_init,
    setup_sweat_extra,
    effect_G6_init,
    setup_tc_hit_flag,
    exec_char_asxy,
    set_caution_flag,
    setup_my_clear_level,
    setup_my_bright_level,
    effect_dummy_init,
    setup_free_program,
    setup_bg_quake_x,
    setup_bg_quake_y,
    effect_47_init,
    setup_pebble_extra,
    effect_77_init,
    setup_exdamage_index,
    setup_dmv_use_flag,
    effect_D5_init,
    effect_D6_init,
    setup_disp_flag,
    setup_command_number,
    effect_I7_init,
    effect_dummy_init,
    setup_sa_shadow,
    effect_73_init,
    effect_K8_init,
    effect_K9_init,
    effect_L7_init,
    effect_M2_init,
    effect_M8_init,
    effect_F0_init,
};
