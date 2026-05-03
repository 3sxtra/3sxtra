#ifndef PLMAIN_H
#define PLMAIN_H

#include "structs.h"
#include "types.h"

extern void (*const plmain_lv_00[5])(PlayerEntity* wk);
extern void (*const plmain_lv_02[5])(PlayerEntity* wk);

void Player_move(PlayerEntity* wk, u16 lv_data);
u16 check_illegal_lever_data(u16 data);
void get_recent_movement_delta(PlayerEntity* wk);
void demo_set_sa_full(SuperArtGauge* sa);
void clear_attack_num(State* wk);
void clear_tk_flags(PlayerEntity* wk);
void about_gauge_process(PlayerEntity* wk);
s16 check_hit_stop(PlayerEntity* wk);
void look_after_timers(PlayerEntity* wk);

#endif
