#ifndef PLS01_H
#define PLS01_H

#include "structs.h"
#include "types.h"

s32 sa_stop_check();
void check_my_tk_power_off(PlayerEntity* wk, PlayerEntity* /* unused */);
void check_em_tk_power_off(PlayerEntity* wk, PlayerEntity* tk);
s16 check_ukemi_flag(PlayerEntity* wk);
s32 check_facing_flag(State* wk);
void set_rl_move(PlayerEntity* wk);
s16 check_rl_on_car(PlayerEntity* wk);
s32 latest_bs2_area_car(PlayerEntity* wk);
s8 latest_bs2_on_car(PlayerEntity* wk);
s32 check_air_jump(PlayerEntity* wk);
s32 check_sankaku_tobi(PlayerEntity* wk);
void check_extra_jump_timer(PlayerEntity* wk);
void remake_sankaku_tobi_mvxy(State* wk, u8 kabe);
s16 check_F_R_dash(PlayerEntity* wk);
s32 check_jump_ready(PlayerEntity* wk);
s32 check_hijump_only(PlayerEntity* wk);
s32 check_bend_myself(PlayerEntity* wk);
s16 check_F_R_walk(PlayerEntity* wk);
s32 check_turn_to_back(PlayerEntity* wk);
s32 check_hurimuki(State* wk);
s16 check_walking_lv_dir(PlayerEntity* wk);
s32 check_stand_up(PlayerEntity* wk);
s32 check_defense_lever(PlayerEntity* wk);
s32 check_em_catt(PlayerEntity* wk);
s16 check_attbox_dir(PlayerEntity* wk);
u16 check_defense_kind(PlayerEntity* wk);
void jumping_union_process(State* wk, s16 num);
s32 check_floor(PlayerEntity* wk);
s32 check_ashimoto(PlayerEntity* wk);
s32 check_floor_2(PlayerEntity* wk);
s32 check_ashimoto_ex(PlayerEntity* wk);

#endif
