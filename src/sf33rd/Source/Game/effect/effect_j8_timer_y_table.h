#ifndef EFFECT_J8_TIMER_Y_TABLE_H
#define EFFECT_J8_TIMER_Y_TABLE_H

#include "structs.h"
#include "types.h"

extern const s16 effj8_timer_tbl[8];
extern const s16 effj8_y_tbl[8];
extern const s32 effj8_sp_tbl[8][4];

void effect_J8_move(State_Other* ewk);
void dragonfly_move(State_Other* ewk);
void dragonfly_l_move_0(State_Other* ewk);
void dragonfly_l_move_1(State_Other* ewk);
s16 dragonfly_l_move_2(State_Other* ewk);
s16 dragonfly_l_move_3(State_Other* ewk);
s16 dragonfly_l_move_4(State_Other* ewk);
void dragonfly_l_move(State_Other* ewk);
void dragonfly_r_move_0(State_Other* ewk);
void dragonfly_r_move_1(State_Other* ewk);
s16 dragonfly_r_move_2(State_Other* ewk);
s16 dragonfly_r_move_3(State_Other* ewk);
s16 dragonfly_r_move_4(State_Other* ewk);
void dragonfly_r_move(State_Other* ewk);
void dragonfly_move_0000(State_Other* ewk);
void dragonfly_move_0001(State_Other* ewk);
void dragonfly_move_0004(State_Other* ewk);
void dragonfly_move_0005(State_Other* ewk);
void dragonfly_stop_timer(State_Other* ewk);
void dragonfly_line_set(State_Other* ewk, s16 dir_type);
void dragonfly_move_next(State_Other* ewk);
s32 effect_J8_init();

#endif
