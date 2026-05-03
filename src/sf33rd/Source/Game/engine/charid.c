/**
 * @file charid.c
 * Data tables and functions for initializing character properties
 */

#include "sf33rd/Source/Game/engine/charid.h"
#include "bin2obj/char_table.h"
#include "common.h"

// bss
CharInitData char_init_data[23];

// sbss
ParabolaData* parabora_own_table[20];

const CharInitData2 char_init_data2[23] = {
    { 0x0000, 0x0000, 0x0000, 0x0000 }, { 0x4200, 0x2000, 0x0026, 0x0002 }, { 0x4200, 0x2000, 0x0032, 0x0002 },
    { 0x4200, 0x2000, 0x0034, 0x0002 }, { 0x4200, 0x2000, 0x002a, 0x0002 }, { 0x4200, 0x2000, 0x0030, 0x0002 },
    { 0x4200, 0x2000, 0x0036, 0x0002 }, { 0x4200, 0x2000, 0x0038, 0x0002 }, { 0x4200, 0x2000, 0x0028, 0x0002 },
    { 0x4200, 0x2000, 0x002e, 0x0002 }, { 0x4200, 0x2000, 0x002c, 0x0002 }, { 0x4200, 0x2000, 0x001c, 0x0002 },
    { 0x4200, 0x2000, 0x0026, 0x0002 }, { 0x4200, 0x2000, 0x0030, 0x0002 }, { 0x4200, 0x2000, 0x0030, 0x0002 },
    { 0x4200, 0x2000, 0x0036, 0x0002 }, { 0x4200, 0x2000, 0x0030, 0x0002 }, { 0x4200, 0x2000, 0x001c, 0x0002 },
    { 0x4200, 0x2000, 0x0032, 0x0002 }, { 0x4200, 0x2000, 0x0032, 0x0002 }, { 0x4200, 0x2000, 0x0036, 0x0002 },
    { 0x4200, 0x2000, 0x0038, 0x0002 }, { 0x4200, 0x2000, 0x0028, 0x0002 }
};

const CharInitData char_init_data_ex[2] = { { _ef13_char_table,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              (s16*)_ef13_stepxy_table,
                                              (s16*)_ef13_movexy_table,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              (HitIndexTable*)_ef13_hit_ix_table,
                                              (CollisionHurtbox*)_ef13_body_box,
                                              (CollisionHurtboxExt*)_ef13_hand_box,
                                              (CollisionCatchbox*)_ef13_cat_box,
                                              (CollisionThrowable*)_ef13_cau_box,
                                              (CollisionHitbox*)_ef13_att_box,
                                              (CollisionPushbox*)_ef13_hos_box,
                                              (AttackIndexTable*)_ef13_catt_table,
                                              NULL },
                                            { (u32*)_bonus_char_table,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              (s16*)_ef13_stepxy_table,
                                              (s16*)_ef13_movexy_table,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              (HitIndexTable*)_ef13_hit_ix_table,
                                              (CollisionHurtbox*)_ef13_body_box,
                                              (CollisionHurtboxExt*)_ef13_hand_box,
                                              (CollisionCatchbox*)_ef13_cat_box,
                                              (CollisionThrowable*)_ef13_cau_box,
                                              (CollisionHitbox*)_ef13_att_box,
                                              (CollisionPushbox*)_ef13_hos_box,
                                              (AttackIndexTable*)_ef13_catt_table,
                                              NULL } };

/** @brief Initializes character work data from the char init tables. */
void set_char_base_data(State* wk) {
    CharInitData* cdat = &char_init_data[wk->charset_id];
    const CharInitData2* cdat2 = &char_init_data2[wk->charset_id];

    wk->char_table[0] = cdat->nmca;
    wk->char_table[1] = cdat->dmca;
    wk->char_table[6] = cdat->btca;
    wk->char_table[2] = cdat->caca;
    wk->char_table[3] = cdat->cuca;
    wk->char_table[4] = cdat->atca;
    wk->char_table[5] = cdat->saca;
    wk->char_table[7] = cdat->exca;
    wk->char_table[8] = cdat->cbca;
    wk->char_table[9] = cdat->yuca;
    wk->step_xy_table = cdat->stxy;
    wk->move_xy_table = cdat->mvxy;
    wk->se_random_table = cdat->sernd;
    wk->overlap_char_tbl = cdat->ovct;
    wk->olc_ix_table = cdat->ovix;
    wk->rival_catch_tbl = cdat->rict;
    wk->hit_ix_table = cdat->hiit;
    wk->body_adrs = cdat->boda;
    wk->hand_adrs = cdat->hana;
    wk->catch_adrs = cdat->cata;
    wk->caught_adrs = cdat->caua;
    wk->attack_adrs = cdat->atta;
    wk->adjust_adrs = cdat->hosa;
    wk->att_ix_table = cdat->atit;
    wk->my_col_mode = cdat2->my_cm;
    wk->my_col_code = cdat2->my_cc;
    wk->my_priority = cdat2->my_pr;
    wk->my_family = cdat2->my_fm;
    wk->position_z = wk->my_priority;
}

/** @brief Copies extended character init data to the main init array. */
void copy_char_base_data() {
    char_init_data[11] = char_init_data_ex[0];
    char_init_data[17] = char_init_data_ex[1];
}
