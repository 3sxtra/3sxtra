/**
 * @file appear.h
 * @brief Public API for character entrance (appear) animations.
 *
 * Part of the animation module.
 */

#ifndef APPEAR_H
#define APPEAR_H

#include "structs.h"
#include "types.h"

typedef struct {
    s16 hx;
    s16 hy;
    s16 whx;
    s8 ixod;
    s8 rl;
    s16 rno;
    s16 char_index;
} APPEAR_DATA;

extern const APPEAR_DATA appear_data[55];
extern const s16 smoke_check[22];
extern const u8 animal_decide_tbl[16];

extern s8 Appear_car_stop[2];
extern s8 Appear_hv[2];
extern s8 Appear_free[2];
extern s8 Appear_flag[2];
extern s16 app_counter[2];
extern s16 appear_work[2];
extern s16 Appear_end;

void appear_work_clear();
s32 home_visitor_check(PlayerEntity* wk);
void appear_data_set(PlayerEntity* wk, APPEAR_DATA* dtbl);
void appear_data_init_set(PlayerEntity* wk);
void appear_player(PlayerEntity* wk);
void Appear_00000(PlayerEntity* wk);
void Appear_01000(PlayerEntity* wk);
void Appear_03000(PlayerEntity* wk);
void Appear_04000(PlayerEntity* wk);
void Appear_05000(PlayerEntity* wk);
void Appear_06000(PlayerEntity* wk);
void Appear_07000(PlayerEntity* wk);
void Appear_08000(PlayerEntity* wk);
s32 sean_appear_check(PlayerEntity* wk, s16 id);
void Appear_09000(PlayerEntity* wk);
void Appear_10000(PlayerEntity* wk);
void Appear_11000(PlayerEntity* wk);
void Appear_12000(PlayerEntity* wk);
void Appear_13000(PlayerEntity* wk);
void Appear_14000(PlayerEntity* wk);
void Appear_15000(PlayerEntity* wk);
void Appear_16000(PlayerEntity* wk);
s16 gill_appear_check();
void Appear_17000(PlayerEntity* wk);
void Appear_18000(PlayerEntity* wk);
void Appear_19000(PlayerEntity* wk);
void Appear_20000(PlayerEntity* wk);
void Appear_21000(PlayerEntity* wk);
void Appear_22000(PlayerEntity* wk);
void Appear_23000(PlayerEntity* wk);
void Appear_24000(PlayerEntity* wk);
void Appear_25000(PlayerEntity* wk);
void Appear_26000(PlayerEntity* wk);
void Appear_28000(PlayerEntity* wk);
void Appear_29000(PlayerEntity* wk);
void animal_decide(PlayerEntity* wk);
void don_appear_check(PlayerEntity* wk);
void Appear_30000(PlayerEntity* wk);
void Appear_31000(PlayerEntity* wk);
void Appear_32000(PlayerEntity* wk);
void Appear_33000(PlayerEntity* wk);
void Appear_34000(PlayerEntity* wk);
void Appear_36000(PlayerEntity* wk);
void Appear_37000(PlayerEntity* wk);
void Appear_38000(PlayerEntity* wk);
void Appear_39000(PlayerEntity* wk);
void Appear_41000(PlayerEntity* wk);
void gouki_appear(PlayerEntity* wk);

#endif
