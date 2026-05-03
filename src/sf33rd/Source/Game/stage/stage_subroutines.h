#ifndef STAGE_SUBROUTINES_H
#define STAGE_SUBROUTINES_H

#include "structs.h"
#include "types.h"

void check_cg_zoom();
void bg_chase_move();
void chase_start_check();
void chase_xy_move();
void Bg_mv_tw(s32 value_x, s32 value_y);
void x_right_check(s16 d1);
void x_left_check(s16 d0);
void scr_x_dummy();
void scr_10_20();
void scr_10_21();
void scr_10_22();
void scr_11_20();
void scr_11_21();
void scr_11_22();
void scr_12_20();
void scr_12_21();
void scr_12_22();
void bg_base_x_move_sub();
void bg_base_x_move_check();
void Bg_Y_Sitei(u8 on_off, s16 pos);
void bg_base_y_move_check();
void bg_x_move_check();
void bg_y_move_check();
void zoom_ud_check();
void suzi_offset_set(State_Other* ewk);
void sync_bg_strip_position(State_Other* ewk);
void Bg_Family_Set();
void Bg_Family_Set_appoint(s32 num_of_bg);
void Bg_Family_Set_2();
void Bg_Family_Set_2_appoint(s32 num_of_bg);
void ake_Family_Set2();
void bg_pos_adjust_sub2(s16 bg_no);
void bg_pos_adjust_sub3(s16 bg_no);
void bg_pos_adjust2();
s16 get_center_position();
s16 get_height_position();
void bg_work_clear();
void compel_bg_init_position();
void bg_base_move_common();
void bg_move_common();
void bg_initialize();
void akebono_initialize();
void bg_etc_write(s16 type);
s32 Ck_Range_Out_S(State_Other* ewk, s16 BG_No, s16 R);

#endif // BG_SUB_H
