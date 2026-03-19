/**
 * @file sc_cockpit.h
 * @brief Public API for HUD cockpit gauges: health, stun, super-art bars,
 *        stun marks, and the Akaobi health-bar stripe.
 *
 * Part of the ui module. Split from sc_sub.c (task #21).
 */

#ifndef SC_COCKPIT_H
#define SC_COCKPIT_H

#include "types.h"

void vital_put(u8 Pl_Num, s8 atr, s16 vital, u8 kind, u16 priority);
void silver_vital_put(u8 Pl_Num);
void vital_base_put(u8 Pl_Num);
void spgauge_base_put(u8 Pl_Num, s16 len);
void stun_put(u8 Pl_Num, u8 stun);
void stun_base_put(u8 Pl_Num, s16 len);
void stun_mark_write(u8 Pl_Num, s16 Len);
void max_mark_write(s8 Pl_Num, u8 Gauge_Len, u8 Mchar, u8 Mass_Len);
void stun_gauge_waku_write(s16 p1len, s16 p2len);
void sa_stock_trans(s16 St_Num, s16 Spg_Col, s8 Stpl_Num);
void sa_fullstock_trans(s16 St_Num, s16 Spg_Col, s8 Stpl_Num);
void sa_number_write(s8 Stpl_Num, u16 x);
void Akaobi(void);

#endif
