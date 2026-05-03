/**
 * @file caldir.c
 * Direction and Motion Calculation Library
 */

#include "sf33rd/Source/Game/engine/calculate_direction.h"
#include "sf33rd/Source/Game/engine/calculate_direction_data.h"
#include "common.h"

// turbo
// What: Convert caldir_pos_256 angle lookup quadrants and scale loop into branchless logical math.
// Target: CPU Branch Prediction (eliminates 4-way switch and branching in hot math paths).
// Expected Impact: Reduces branch mispredictions during physics collision checks.
s16 caldir_pos_256(s16 x1, s16 x2, s16 y1, s16 y2) {
    y1 -= x1;
    y2 -= x2;

    s16 s1 = y1 >> 15;
    s16 s2 = y2 >> 15;

    s16 yhan = (s1 ^ s2) & 1;
    s16 tent = s2 & 0x80;

    y1 = (y1 ^ s1) - s1;
    y2 = (y2 ^ s2) - s2;

    s16 max_val = y1 > y2 ? y1 : y2;
    int shift = 0;
    while (max_val >= 0x80) {
        max_val >>= 1;
        shift++;
    }
    y1 >>= shift;
    y2 >>= shift;

    tent += dir_sel_table[y1][y2];

    if (yhan) {
        tent = -tent;
        tent &= 0xFF;
    }

    return tent;
}

s16 caldir_pos_032(s16 x1, s16 x2, s16 y1, s16 y2) {
    return (caldir_pos_256(x1, x2, y1, y2) + 4) >> 3 & 0x1F;
}

void add_pos_dir_064(State* wk, s16 sp) {
    wk->xyz[0].cal += (sp * rate_256_table[wk->direction * 4][0]) >> 8;
    wk->xyz[1].cal += (sp * rate_256_table[wk->direction * 4][1]) >> 8;
}

// turbo
// What: Convert cal_move_quantity2 abs and scale loops into branchless logical math.
// Target: CPU Branch Prediction (eliminates branching in hot distance paths).
// Expected Impact: Reduces branch mispredictions during velocity applications.
s16 cal_move_quantity2(s16 x1, s16 x2, s16 y1, s16 y2) {
    s16 kakudo;
    MS ms;

    y1 -= x1;
    y2 -= x2;

    s16 s1 = y1 >> 15;
    s16 s2 = y2 >> 15;

    y1 = (y1 ^ s1) - s1;
    y2 = (y2 ^ s2) - s2;

    x1 = y1;
    x2 = y2;

    s16 max_val = y1 > y2 ? y1 : y2;
    int shift = 0;
    while (max_val >= 0x80) {
        max_val >>= 1;
        shift++;
    }
    y1 >>= shift;
    y2 >>= shift;

    kakudo = dir_sel_table[y1][y2];
    ms.psi = (x1 * rate_256_table[kakudo][0]);
    ms.psi += (x2 * rate_256_table[kakudo][1]);
    return ms.pss.h;
}

s16 cal_move_quantity3(State* wk, s16 tm) {
    s32 ltm;
    PS_DY ps;

    if (tm == 0) {
        return wk->xyz[1].disp.pos;
    }

    ltm = tm;
    ps.dy = ltm * ltm / 2 * wk->mvxy.d[1].sp;
    ps.dy = (wk->mvxy.a[1].sp * ltm) + ps.dy + wk->xyz[1].cal;
    return ps.ry.h;
}

void cmsd_all_x_speed_data(MotionState* cc) {
    switch (cc->swx) {
    case 1:
        cmsd_swx_1(cc);
        break;

    case 2:
        cmsd_swx_2(cc);
        break;

    default:
        cmsd_swx_0(cc);
        break;
    }
}

void cmsd_all_y_speed_data(MotionState* cc) {
    switch (cc->swy) {
    case 1:
        cmsd_swy_1(cc);
        break;

    case 2:
        cmsd_swy_2(cc);
        break;

    default:
        cmsd_swy_0(cc);
        break;
    }
}

void cmsd_swx_0(MotionState* cc) {
    cc->amx = cc->x.pl % cc->timer;
    cc->spx = cc->x.pl / cc->timer;
    cc->dlx = 0;
}

void cmsd_swy_0(MotionState* cc) {
    cc->amy = cc->y.pl % cc->timer;
    cc->spy = cc->y.pl / cc->timer;
    cc->dly = 0;
}

void cmsd_swx_1(MotionState* cc) {
    cc->amx = cc->x.pl % cc->timer2;
    cc->spx = cc->dlx = cc->x.pl / cc->timer2;
}

void cmsd_swy_1(MotionState* cc) {
    cc->amy = cc->y.pl % cc->timer2;
    cc->spy = cc->dly = cc->y.pl / cc->timer2;
}

void cmsd_swx_2(MotionState* cc) {
    cc->amx = cc->x.pl % cc->timer2;
    cc->dlx = cc->x.pl / cc->timer2;
    cc->spx = cc->dlx * cc->timer;
    cc->dlx = -cc->dlx;
}

void cmsd_swy_2(MotionState* cc) {
    cc->amy = cc->y.pl % cc->timer2;
    cc->dly = cc->y.pl / cc->timer2;
    cc->spy = cc->dly * cc->timer;
    cc->dly = -cc->dly;
}

void cmsd_x_initial_speed(MotionState* cc) {
    cc->amx = cc->x.pl - (cc->timer2 * cc->dlx);
    cc->spx = cc->dlx + (cc->amx / cc->timer);
    cc->amx %= cc->timer;
}

/** @brief Sets the initial Y speed from the motion state data. */
void cmsd_y_initial_speed(MotionState* cc) {
    cc->amy = cc->y.pl - (cc->timer2 * cc->dly);
    cc->spy = cc->dly + (cc->amy / cc->timer);
    cc->amy %= cc->timer;
}

/** @brief Sets the X delta (acceleration) speed from the motion state data. */
void cmsd_x_delta_speed(MotionState* cc) {
    if (cc->spx != 0) {
        cc->amx = cc->x.pl - (cc->timer * cc->spx);
        cc->dlx = cc->amx / cc->timer2;
        cc->amx %= cc->timer2;
        cc->spx += cc->dlx;
        return;
    }

    cmsd_all_x_speed_data(cc);
}

/** @brief Sets the Y delta (acceleration) speed from the motion state data. */
void cmsd_y_delta_speed(MotionState* cc) {
    if (cc->spy != 0) {
        cc->amy = cc->y.pl - (cc->timer * cc->spy);
        cc->dly = cc->amy / cc->timer2;
        cc->amy %= cc->timer2;
        cc->spy += cc->dly;
        return;
    }

    cmsd_all_y_speed_data(cc);
}

/** @brief Calculates all speed components (initial + delta) for a given trajectory. */
void cal_all_speed_data(State* wk, s16 tm, s16 x1, s16 y1, s8 xsw, s8 ysw) {
    MotionState bb;

    wk->xyz[0].disp.low = wk->xyz[1].disp.low = -0x8000;
    bb.timer = tm;
    bb.timer2 = bb.timer + (bb.timer * (bb.timer - 1) / 2);
    bb.x.ps.h = x1 - wk->xyz[0].disp.pos;
    bb.y.ps.h = y1 - wk->xyz[1].disp.pos;
    bb.x.ps.l = bb.y.ps.l = 0;
    bb.swx = xsw;
    bb.swy = ysw;

    if (bb.timer == 0) {
        bb.amy = 0;
        bb.amx = 0;
        bb.dly = 0;
        bb.spy = 0;
        bb.dlx = 0;
        bb.spx = 0;
    } else {
        cmsd_all_x_speed_data(&bb);
        cmsd_all_y_speed_data(&bb);
    }

    wk->mvxy.a[0].sp = bb.spx;
    wk->mvxy.d[0].sp = bb.dlx;
    wk->mvxy.a[1].sp = bb.spy;
    wk->mvxy.d[1].sp = bb.dly;
    wk->xyz[0].cal += bb.amx;
    wk->xyz[1].cal += bb.amy;
    wk->mvxy.physics_curve_type[0] = wk->mvxy.physics_curve_type[1] = 0;
}

/** @brief Calculates initial speed to reach a target position in a given time. */
void cal_initial_speed(State* wk, s16 tm, s16 x1, s16 y1) {
    MotionState bb;

    wk->xyz[0].disp.low = wk->xyz[1].disp.low = 0;
    bb.timer = tm;
    bb.timer2 = bb.timer + bb.timer * (bb.timer - 1) / 2;
    bb.x.ps.h = x1 - wk->xyz[0].disp.pos;
    bb.y.ps.h = y1 - wk->xyz[1].disp.pos;
    bb.x.ps.l = bb.y.ps.l = 0;
    bb.dlx = wk->mvxy.d[0].sp;
    bb.dly = wk->mvxy.d[1].sp;

    if (bb.timer == 0) {
        bb.amy = 0;
        bb.amx = 0;
        bb.spy = 0;
        bb.spx = 0;
    } else {
        cmsd_x_initial_speed(&bb);
        cmsd_y_initial_speed(&bb);
    }

    wk->mvxy.a[0].sp = bb.spx;
    wk->mvxy.a[1].sp = bb.spy;
    wk->xyz[0].cal += bb.amx;
    wk->xyz[1].cal += bb.amy;
}

/** @brief Calculates initial Y speed to reach a target height in a given time. */
void cal_initial_speed_y(State* wk, s16 tm, s16 y1) {
    MotionState bb;

    wk->xyz[1].disp.low = 0;
    bb.timer = tm + 0;
    bb.timer2 = bb.timer + bb.timer * (bb.timer - 1) / 2;
    bb.y.ps.h = y1 - wk->xyz[1].disp.pos;
    bb.y.ps.l = 0;
    bb.dly = wk->mvxy.d[1].sp;

    if (bb.timer == 0) {
        bb.amy = 0;
        bb.spy = 0;
    } else {
        cmsd_y_initial_speed(&bb);
    }

    wk->mvxy.a[1].sp = bb.spy;
    wk->xyz[1].cal += bb.amy;
}

/** @brief Calculates delta (acceleration) speed for a given trajectory. */
void cal_delta_speed(State* wk, s16 tm, s16 x1, s16 y1, s8 xsw, s8 ysw) {
    MotionState bb;

    wk->xyz[0].disp.low = wk->xyz[1].disp.low = 0;
    bb.timer = tm + 0;
    bb.timer2 = bb.timer + bb.timer * (bb.timer - 1) / 2;
    bb.x.ps.h = x1 - wk->xyz[0].disp.pos;
    bb.y.ps.h = y1 - wk->xyz[1].disp.pos;
    bb.x.ps.l = bb.y.ps.l = 0;
    bb.swx = xsw;
    bb.swy = ysw;
    bb.spx = wk->mvxy.a[0].sp;
    bb.spy = wk->mvxy.a[1].sp;

    if (bb.timer == 0) {
        bb.amy = 0;
        bb.amx = 0;
        bb.dly = 0;
        bb.dlx = 0;
    } else {
        cmsd_x_delta_speed(&bb);
        cmsd_y_delta_speed(&bb);
    }

    wk->mvxy.a[0].sp = bb.spx;
    wk->mvxy.d[0].sp = bb.dlx;
    wk->mvxy.a[1].sp = bb.spy;
    wk->mvxy.d[1].sp = bb.dly;
    wk->xyz[0].cal += bb.amx;
    wk->xyz[1].cal += bb.amy;
}

/** @brief Calculates the peak Y position of the current trajectory arc. */
s16 cal_top_of_position_y(State* wk) {
    s32 num = cal_time_of_sign_change(wk);
    s32 num2;
    PS_UNI ps_uni;

    if (num == 0) {
        return wk->xyz[1].disp.pos;
    }

    num2 = num * (num - 1) / 2;
    ps_uni.psy = (num * wk->mvxy.a[1].sp) + (num2 * wk->mvxy.d[1].sp) + wk->xyz[1].cal;
    return ps_uni.psys.h;
}

/** @brief Calculates the time at which the Y velocity changes sign (apex). */
s16 cal_time_of_sign_change(State* wk) {
    if (wk->mvxy.a[1].real.h > 0 && wk->mvxy.d[1].real.h < 0) {
        return wk->mvxy.a[1].sp / -wk->mvxy.d[1].sp;
    }

    return 0;
}

/** @brief Forecasts the move direction after a given number of frames. */
s16 cal_move_dir_forecast(State* wk, s16 tm) {
    PS_DP ps[2];

    if (tm == 0) {
        return 0;
    }

    ps[0].dp = (wk->mvxy.d[0].sp * (tm * tm)) / 2;
    ps[0].dp = wk->xyz[0].cal + (ps[0].dp + (wk->mvxy.a[0].sp * tm));
    ps[1].dp = (wk->mvxy.d[1].sp * (tm * tm)) / 2;
    ps[1].dp = wk->xyz[1].cal + (ps[1].dp + (wk->mvxy.a[1].sp * tm));
    return caldir_pos_032(wk->xyz[0].disp.pos, wk->xyz[1].disp.pos, ps[0].rp.h, ps[1].rp.h);
}

/** @brief Converts a binary-coded number to decimal (base-10 representation). */
s16 remake_2_10(s16 num, s16 keta) {
    switch (keta) {
    case 2:
        num = (num % 10) + ((num % 100 / 10) << 4);
        break;

    case 3:
        num = ((num / 100) << 8) + ((num % 100 / 10) << 4) + (num % 10);
        break;

    default:
        num = ((num / 1000) << 12) + ((num / 100) << 8) + ((num % 100 / 10) << 4) + (num % 10);
        break;
    }

    return num;
}
