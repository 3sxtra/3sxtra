#ifndef PLS03_H
#define PLS03_H

#include "structs.h"
#include "types.h"

void hissatsu_setup_union(PlayerEntity* wk, s16 rno);
s16 cmdixconv(s16 ix);
s32 check_full_gauge_attack(PlayerEntity* wk, s8 always);
s32 check_full_gauge_attack2(PlayerEntity* wk, s8 always);
s16 check_super_arts_attack(PlayerEntity* wk);
s32 check_super_arts_attack_dc(PlayerEntity* wk);
s32 execute_super_arts(PlayerEntity* wk);
s32 check_special_attack(PlayerEntity* wk);
void chainex_spat_cancel_trajectory(State* wk);
s32 check_leap_attack(PlayerEntity* wk);
s32 check_nm_attack(PlayerEntity* wk);
s16 hikusugi_check(State* wk);
s32 check_chouhatsu(PlayerEntity* wk);
s32 Check_Throw_Escape_Command(PlayerEntity* wk);
s32 check_catch_attack(PlayerEntity* wk);
void set_attack_routine_number(PlayerEntity* wk);
u16 get_nearing_range(s16 pnum, s16 kos);
s32 move_select(PlayerEntity* wk, s16 kos, s16 sf);
u16 decode_wst_data(PlayerEntity* wk, u16 cmd, s16 cmd_ex);
s16 get_em_body_range(State* wk);
s32 cmd_ex_check(s16 px, s16 cx);
s16 shot_data_convert(u16 sw);
s16 shot_data_refresh(s16 sw);
s16 renbanshot_conpaneshot(const s16* dadr, s16 pow);
s16 datacmd_conpanecmd(s16 dat);
s32 check_renda_cancel(PlayerEntity* wk);
s32 check_target_combo_cancel(PlayerEntity* wk);
s16 get_tc_input_dir(s16 data);
s16 get_tc_input_button(s16 data);

#endif
