/**
 * @file spgauge.c
 * Super Art Gauge Controller
 */

#include "sf33rd/Source/Game/engine/spgauge.h"
#include "game_state.h"
#include "structs.h"

s16 col;
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/plmain.h"
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

/* Phase 3 RmlUi bypass */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include <stdbool.h>

// sbss - now declared in header as extern

const u16 spgauge_tbl[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };

const u16 spg1p_npos_tbl[16] = { 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6 };

const u16 spg2p_npos_tbl[16] = { 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41 };

const u16 sa_time_data_tbl[6][2] = { { 0, 16 }, { 4, 20 }, { 8, 24 }, { 12, 28 }, { 8, 24 }, { 4, 20 } };

const u16 sa_color_data2_tbl[3][2] = { { 13, 141 }, { 12, 140 }, { 13, 141 } };

const u16 sagauge_colchg_tbl[4][2] = { { 17, 145 }, { 18, 146 }, { 19, 147 }, { 18, 146 } };

const u16* spgauge_puttbl[2] = { spgauge_tbl, spgauge_tbl };
const u16* spgauge_postbl[2] = { spg1p_npos_tbl, spg2p_npos_tbl };

/** Per-player spgcol_number constants: [player][color_index].
 *  Index 0 = default/empty, 1 = filled/stock, 2 = timed-SA-active. */
enum { SPG_COL_DEFAULT = 0, SPG_COL_FILLED = 1, SPG_COL_TIMED = 2 };
static const s16 spg_player_colors[2][3] = {
    { 17, 18, 14 },    /* P1 */
    { 145, 146, 142 }, /* P2 */
};

static void spgauge_control(s8 Spg_Num);
static void wipe_check();
static void satime_ko_after_clear(s8 Stpl_Num);
static void sa_time_moji_send();
static void samoji_control(s8 Stpl_Num);
static void sast_control(s8 Stpl_Num);
static void sast_color_chenge(s8 Stpl_Num);
static void sagauge_color_chenge(s8 Stpl_Num);
static void sa_moji_trans(s8 Stpl_Num, s8 Kind, s8 OnOff);
static void sa_gauge_trans(s8 pl_kind);
static void spgauge_sound_request(s8 Stpl_Num);
static void spgauge_work_clear(s8 Stpl_Num);
static void spgauge_wipe_write(s8 Stpl_Num);
static void sa_waku_trans(s8 Stpl_Num, s8 Spg_Col);

/**
 * @brief Shared SA gauge initialization for a single player.
 *
 * Parameterized by @p full: when 0, gauge starts empty (match start);
 * when 1, gauge starts full (demo/training-full).
 *
 * Both spgauge_cont_init/spgauge_cont_demo_init (two-player loop) and
 * tr_spgauge_cont_init/tr_spgauge_cont_init2 (single-player) delegate here.
 */
static void spgauge_init_player(s8 pl, s8 full) {
    if (full) {
        demo_set_sa_full(&g_state.super_arts[pl]);
        g_state.spg_dat[pl].current_spg = g_state.super_arts[pl].gauge_len;
        g_state.spg_dat[pl].old_spg = g_state.super_arts[pl].gauge_len;
        g_state.spg_dat[pl].spg_level = g_state.super_arts[pl].store;
    } else {
        g_state.spg_dat[pl].current_spg = 0;
        g_state.spg_dat[pl].old_spg = 0;
        g_state.spg_dat[pl].spg_level = 0;
    }

    g_state.spg_dat[pl].spgtbl_ptr = spgauge_puttbl[pl];
    g_state.spg_dat[pl].spg_maxlevel = g_state.super_arts[pl].store_max;
    g_state.spg_dat[pl].spg_len = g_state.super_arts[pl].gauge_len / 8;
    g_state.spg_dat[pl].spg_dotlen = g_state.super_arts[pl].gauge_len;
    g_state.spg_dat[pl].flag = full ? 1 : 0;
    g_state.spg_dat[pl].flag2 = 0;
    g_state.spg_dat[pl].level_flag = 0;
    g_state.spg_dat[pl].timer = 51;
    g_state.spg_dat[pl].timer2 = 2;
    g_state.spg_dat[pl].kind = full ? 1 : 0;
    g_state.spg_dat[pl].max = 0;
    g_state.spg_dat[pl].max_old = full ? 1 : 0;
    g_state.spg_dat[pl].max_rno = full ? 2 : 0;
    g_state.spg_dat[pl].time_rno = full ? 5 : 0;
    g_state.spg_dat[pl].gauge_flash_time = 2;
    g_state.spg_dat[pl].gauge_flash_col = 0;
    g_state.spg_dat[pl].sa_flag = 0;
    g_state.spg_dat[pl].ex_flag = 0;
    g_state.spg_dat[pl].no_chgcol = 0;
    g_state.spg_dat[pl].time_no_clear = 0;
    g_state.spg_dat[pl].sa_mukou = 0;
    g_state.sa_gauge_flash[pl] = 0;
    g_state.spg_dat[pl].spgptbl_ptr = spgauge_postbl[pl];

    if (g_state.super_arts[pl].gauge_type == 1) {
        g_state.spg_dat[pl].time = 1;
        g_state.time_flag[pl] = 1;
    } else {
        g_state.spg_dat[pl].time = 0;
        g_state.time_flag[pl] = 0;
    }

    g_state.spg_dat[pl].mass_len = g_state.spg_dat[pl].spg_len - 5;

    if (g_state.spg_dat[pl].spg_len & 1) {
        g_state.spg_dat[pl].mchar = 5;
    } else {
        g_state.spg_dat[pl].mass_len = g_state.spg_dat[pl].mass_len - 1;
        g_state.spg_dat[pl].mchar = 6;
    }

    g_state.spg_dat[pl].mass_len /= 2;
    g_state.time_operate[pl] = 0;
    g_state.sast_now[pl] = 0;
    g_state.max2[pl] = 0;
    g_state.max_rno2[pl] = 0;
}

/** @brief Initializes the Super Art gauge controller for match start. */
void spgauge_cont_init() {
    s8 lpy;

    Sa_frame_Clear();

    for (lpy = 0; lpy < 2; lpy++) {
        spgauge_init_player(lpy, 0);
    }

    g_state.spg_dat[0].spgcol_number = spg_player_colors[0][SPG_COL_DEFAULT];
    g_state.spg_dat[1].spgcol_number = spg_player_colors[1][SPG_COL_DEFAULT];
    sa_stock_trans(0, 0, 0);
    sa_stock_trans(0, 0, 1);
    sa_waku_trans(0, 0);
    sa_waku_trans(1, 0);
    sa_gauge_trans(0);
    sa_gauge_trans(1);
    g_state.Old_Stop_SG = 0;
    g_state.Exec_Wipe_F = 0;
    g_state.time_clear[0] = 0;
    g_state.time_clear[1] = 0;
    g_state.spg_offset = 0;
    g_state.time_num = 0;
    g_state.time_timer = 3;
    col = 0;
}

/** @brief Initializes the Super Art gauge controller for demo playback. */
void spgauge_cont_demo_init() {
    s8 lpy;

    Sa_frame_Clear();

    for (lpy = 0; lpy < 2; lpy++) {
        spgauge_init_player(lpy, 1);
    }

    g_state.spg_dat[0].spgcol_number = spg_player_colors[0][SPG_COL_DEFAULT];
    g_state.spg_dat[1].spgcol_number = spg_player_colors[1][SPG_COL_DEFAULT];
    sa_stock_trans(g_state.spg_dat[0].spg_maxlevel, 1, 0);
    sa_stock_trans(g_state.spg_dat[1].spg_maxlevel, 1, 1);
    sa_waku_trans(0, 1);
    sa_waku_trans(1, 1);
    sa_gauge_trans(0);
    sa_gauge_trans(1);
    sa_moji_trans(0, 0, 1);
    sa_moji_trans(1, 0, 1);
    g_state.Old_Stop_SG = 0;
    g_state.Exec_Wipe_F = 0;
    g_state.time_clear[0] = 0;
    g_state.time_clear[1] = 0;
    g_state.spg_offset = 0;
    g_state.time_num = 0;
    g_state.time_timer = 3;
    col = 1;
}

/** @brief Per-frame Super Art gauge main update — runs gauge control for both sides. */
void spgauge_cont_main() {
    u8 i;

    for (i = 0; i < 2; i++) {
        if (!use_rmlui || !rmlui_hud_super)
            spgauge_base_put(i, g_state.spg_dat[i].spg_len);
    }

    if ((g_state.Game_pause & 0x80) || g_state.EXE_flag) {
        return;
    }

    if (g_state.Stop_SG) {
        wipe_check();
        g_state.Old_Stop_SG = g_state.Stop_SG;
        return;
    }

    if (g_state.Old_Stop_SG) {
        g_state.Old_Stop_SG = 0;
        g_state.Exec_Wipe_F = 0;
        g_state.time_clear[0] = 0;
        g_state.time_clear[1] = 0;
    }

    sa_time_moji_send();

    if (g_state.gauge_stop_flag[0] == 0) {
        spgauge_control(0);
    }

    if (g_state.gauge_stop_flag[1] == 0) {
        spgauge_control(1);
    }
}

/** @brief Fixes an SA gauge display bug for a specific side. */
void sag_bug_fix(s32 side) {
    g_state.spg_dat[side].max = g_state.spg_dat[side].max_old = g_state.spg_dat[side].max_rno = 0;
}

/** @brief Controls a single SA gauge stock: fill level, flash, and stock transitions. */
static void spgauge_control(s8 Spg_Num) {
    if (g_state.sa_gauge_flash[Spg_Num] != 0 && g_state.plw[Spg_Num].sa->ex == -1) {
        spgauge_sound_request(Spg_Num);

        if (g_state.Conclusion_Flag != 0) {
            g_state.spg_dat[Spg_Num].sa_mukou = 1;
        }

        g_state.spg_dat[Spg_Num].max = 0;
        g_state.spg_dat[Spg_Num].max_rno = 0;
        g_state.spg_dat[Spg_Num].flag = 1;
        g_state.spg_dat[Spg_Num].flag2 = 1;
        g_state.spg_dat[Spg_Num].time_rno = 0;
        g_state.spg_dat[Spg_Num].timer2 = 2;
        g_state.spg_dat[Spg_Num].kind = 0;
        g_state.spg_dat[Spg_Num].no_chgcol = 0;
        g_state.spg_dat[Spg_Num].ex_flag = 1;
        g_state.spg_dat[Spg_Num].timer = 16;
        g_state.sa_gauge_flash[Spg_Num] &= ~2;
    } else if (g_state.sast_now[Spg_Num] == 0 && g_state.spg_dat[Spg_Num].flag2 == 0 && g_state.sa_gauge_flash[Spg_Num] != 0) {
        spgauge_sound_request(Spg_Num);

        if (g_state.super_arts[Spg_Num].gt2 == 1) {
            g_state.spg_dat[Spg_Num].time = 1;
            g_state.time_flag[Spg_Num] = 1;
        } else {
            g_state.spg_dat[Spg_Num].time = 0;
            g_state.time_flag[Spg_Num] = 0;
        }

        if (g_state.plw[Spg_Num].sa->store == g_state.plw[Spg_Num].sa->store_max && g_state.spg_dat[Spg_Num].max_old == 0 &&
            g_state.spg_dat[Spg_Num].max == 0) {
            g_state.spg_dat[Spg_Num].max = 1;
        } else {
            g_state.spg_dat[Spg_Num].max = 0;
        }

        g_state.spg_dat[Spg_Num].flag = 1;
        g_state.spg_dat[Spg_Num].flag2 = 1;
        g_state.spg_dat[Spg_Num].max_rno = 0;
        g_state.spg_dat[Spg_Num].time_rno = 0;
        g_state.spg_dat[Spg_Num].timer2 = 2;
        g_state.spg_dat[Spg_Num].kind = 0;
        g_state.spg_dat[Spg_Num].no_chgcol = 0;

        if (g_state.plw[Spg_Num].sa->ok == -1 || g_state.plw[Spg_Num].sa->mp == -1) {
            g_state.spg_dat[Spg_Num].sa_flag = 1;
            g_state.spg_dat[Spg_Num].timer = 51;

            if (g_state.Conclusion_Flag != 0) {
                if ((g_state.spg_dat[Spg_Num].time) == 1) {
                    g_state.spg_dat[Spg_Num].time_no_clear = 1;
                }

                if (g_state.My_char[Spg_Num] == 0 && ((g_state.plw[Spg_Num].sa->ok) == -1)) {
                    g_state.spg_dat[Spg_Num].sa_mukou = 0;
                } else {
                    g_state.spg_dat[Spg_Num].sa_mukou = 1;
                }
            }

            g_state.sa_gauge_flash[Spg_Num] &= ~4;
        } else {
            g_state.spg_dat[Spg_Num].timer = 25;
            g_state.sa_gauge_flash[Spg_Num] &= ~1;
        }
    }

    if (g_state.spg_dat[Spg_Num].max) {
        samoji_control(Spg_Num);
    } else if (g_state.spg_dat[Spg_Num].flag) {
        sast_control(Spg_Num);
    }

    if ((g_state.plw[Spg_Num].sa->ex != 0 || g_state.spg_dat[Spg_Num].ex_flag == 1 || g_state.spg_dat[Spg_Num].sa_flag == 1) && !g_state.Game_pause) {
        sagauge_color_chenge(Spg_Num);
    }

    if (g_state.spg_dat[Spg_Num].current_spg != g_state.plw[Spg_Num].sa->gauge.s.h || g_state.spg_dat[Spg_Num].max != 0) {
        if (g_state.spg_dat[Spg_Num].max) {
            g_state.spg_dat[Spg_Num].current_spg = g_state.spg_dat[Spg_Num].spg_dotlen;
        } else {
            g_state.spg_dat[Spg_Num].current_spg = g_state.plw[Spg_Num].sa->gauge.s.h;
        }

        if (g_state.max2[Spg_Num] != 1 && g_state.spg_dat[Spg_Num].max == 0 && g_state.spg_dat[Spg_Num].flag == 0 &&
            g_state.spg_dat[Spg_Num].max_old == 0 && g_state.spg_dat[Spg_Num].time_no_clear == 0) {
            sa_gauge_trans(Spg_Num);
        }
    }
}

/** @brief Checks and processes gauge wipe transitions (round start/end). */
static void wipe_check() {
    /* Per-player sc_clear coordinates: {x1, y1, x2, y2}. */
    static const s8 wipe_clear_coords[2][4] = { { 1, 25, 4, 26 }, { 43, 25, 46, 26 } };

    if (g_state.Old_Stop_SG) {
        if (g_state.Exec_Wipe != 0) {
            return;
        }

        if (g_state.Exec_Wipe_F != 0) {
            return;
        }

        g_state.Exec_Wipe_F = 1;

        for (s8 pl = 0; pl < 2; pl++) {
            if (g_state.spg_dat[pl].time == 1 && g_state.time_clear[pl] == 1) {
                if (g_state.spg_dat[pl].time_no_clear == 0) {
                    spgauge_work_clear(pl);
                    sc_clear(wipe_clear_coords[pl][0],
                             wipe_clear_coords[pl][1],
                             wipe_clear_coords[pl][2],
                             wipe_clear_coords[pl][3]);
                    sast_color_chenge(pl);
                    spgauge_wipe_write(pl);
                } else {
                    satime_ko_after_clear(pl);
                }
            }
        }

    } else {
        for (s8 pl = 0; pl < 2; pl++) {
            if (g_state.spg_dat[pl].time_no_clear == 1 || g_state.plw[pl].sa->ok == -1) {
                g_state.plw[pl].sa->ok = 0;
                g_state.time_clear[pl] = 1;
                g_state.spg_dat[pl].spg_level = g_state.plw[pl].sa->store;
            }
        }
    }
}

/** @brief Clears SA time display after a KO for a given stock. */
static void satime_ko_after_clear(s8 Stpl_Num) {
    g_state.spg_dat[Stpl_Num].max = 0;

    if (g_state.plw[Stpl_Num].sa->store == g_state.spg_dat[Stpl_Num].spg_maxlevel) {
        g_state.spg_dat[Stpl_Num].max_old = 1;
        g_state.spg_dat[Stpl_Num].max_rno = 2;
    } else {
        g_state.spg_dat[Stpl_Num].max_old = 0;
        g_state.spg_dat[Stpl_Num].max_rno = 0;
    }

    g_state.spg_dat[Stpl_Num].kind = 1;
    g_state.spg_dat[Stpl_Num].timer2 = 2;
    g_state.spg_dat[Stpl_Num].time_rno = 5;
    g_state.spg_dat[Stpl_Num].time_no_clear = 0;
    g_state.plw[Stpl_Num].sa->gauge.s.h = g_state.spg_dat[Stpl_Num].current_spg = g_state.plw[Stpl_Num].sa->bacckup_g_h;
    g_state.plw[Stpl_Num].sa->bacckup_g_h = 0;
}

/** @brief Sends the SA time text sprites to the display system. */
static void sa_time_moji_send() {
    if (g_state.time_flag[0] == 0 && g_state.time_flag[1] == 0) {
        return;
    }

    g_state.time_timer--;

    if (g_state.time_timer != 0) {
        return;
    }

    if (g_state.time_flag[0] == 1 && g_state.time_operate[0] == 1) {
        scfont_sqput2(1, 25, 11, 0, 2, sa_time_data_tbl[g_state.time_num][0], 0, 4, 2);
    }

    if (g_state.time_flag[1] == 1 && g_state.time_operate[1] == 1) {
        scfont_sqput2(43, 25, 11, 0, 2, sa_time_data_tbl[g_state.time_num][1], 0, 4, 2);
    }

    g_state.time_timer = 3;

    if (g_state.time_num == 5) {
        g_state.time_num = 0;
        return;
    }

    g_state.time_num++;
}

/** @brief Controls the SA label text sprite animation (flash/blink cycles). */
static void samoji_control(s8 Stpl_Num) {
    switch (g_state.spg_dat[Stpl_Num].max_rno) {
    case 0:
        g_state.max2[Stpl_Num] = 0;
        g_state.max_rno2[Stpl_Num] = 1;
        sa_moji_trans(Stpl_Num, 0, 1);
        g_state.spg_dat[Stpl_Num].spg_level = g_state.plw[Stpl_Num].sa->store;
        sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, 1, Stpl_Num);
        sa_waku_trans(Stpl_Num, 1);
        sagauge_color_chenge(Stpl_Num);
        g_state.spg_dat[Stpl_Num].max_rno = 1;
        break;

    case 1:
        g_state.spg_dat[Stpl_Num].timer--;

        if (g_state.spg_dat[Stpl_Num].timer) {
            g_state.spg_dat[Stpl_Num].timer2--;

            if (g_state.spg_dat[Stpl_Num].timer2 != 0) {
                break;
            }

            g_state.spg_dat[Stpl_Num].kind++;

            if (g_state.spg_dat[Stpl_Num].kind == 3) {
                g_state.spg_dat[Stpl_Num].kind = 0;
            }

            sa_waku_trans(Stpl_Num, g_state.spg_dat[Stpl_Num].kind);
            sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, g_state.spg_dat[Stpl_Num].kind, Stpl_Num);
            g_state.spg_dat[Stpl_Num].timer2 = 1;
        } else {
            sa_waku_trans(Stpl_Num, 1);
            sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, 1, Stpl_Num);
            g_state.spg_dat[Stpl_Num].max = 0;
            g_state.spg_dat[Stpl_Num].max_old = 1;
            g_state.spg_dat[Stpl_Num].kind = 1;
            g_state.spg_dat[Stpl_Num].timer2 = 2;
            g_state.spg_dat[Stpl_Num].max_rno = 2;
            g_state.spg_dat[Stpl_Num].time_rno = 5;
            g_state.spg_dat[Stpl_Num].flag2 = 0;
            g_state.max_rno2[Stpl_Num] = 0;
        }

        break;

    case 2:
        break;
    }
}

/** @brief Controls the SA stock indicator state machine. */
static void sast_control(s8 Stpl_Num) {
    g_state.sast_now[Stpl_Num] = 1;

    if (g_state.spg_dat[Stpl_Num].time) {
        switch (g_state.spg_dat[Stpl_Num].time_rno) {
        case 0:
            if (g_state.plw[Stpl_Num].sa->ok == -1) {
                g_state.spg_dat[Stpl_Num].time_rno = 1;
            } else {
                if (g_state.plw[Stpl_Num].sa->store > g_state.spg_dat[Stpl_Num].spg_level) {
                    g_state.spg_dat[Stpl_Num].current_spg = g_state.spg_dat[Stpl_Num].spg_dotlen;
                    sa_gauge_trans(Stpl_Num);
                    g_state.spg_dat[Stpl_Num].spg_level = g_state.plw[Stpl_Num].sa->store;
                    sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, col, Stpl_Num);
                }

                g_state.spg_dat[Stpl_Num].time_rno = 3;
                goto case_3;
            }

            /* fallthrough */

        case 1:
            g_state.spg_dat[Stpl_Num].timer--;

            if ((!g_state.spg_dat[Stpl_Num].sa_mukou || g_state.spg_dat[Stpl_Num].timer != 0) &&
                (g_state.spg_dat[Stpl_Num].spg_level == g_state.plw[Stpl_Num].sa->store)) {
                g_state.spg_dat[Stpl_Num].timer2--;

                if (g_state.spg_dat[Stpl_Num].kind == 0) {
                    if (g_state.spg_dat[Stpl_Num].timer2 == 0) {
                        sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, 0, Stpl_Num);
                        sa_waku_trans(Stpl_Num, 0);
                        g_state.spg_dat[Stpl_Num].kind = 1;
                        g_state.spg_dat[Stpl_Num].timer2 = 2;
                    }
                } else if (g_state.spg_dat[Stpl_Num].timer2 == 0) {
                    sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, 1, Stpl_Num);
                    sa_waku_trans(Stpl_Num, 1);
                    g_state.spg_dat[Stpl_Num].kind = 0;
                    g_state.spg_dat[Stpl_Num].timer2 = 2;
                }

                return;
            }

            if (g_state.spg_dat[Stpl_Num].sa_flag != 0 && g_state.spg_dat[Stpl_Num].sa_mukou == 0) {
                sa_waku_trans(Stpl_Num, 1);
                sa_moji_trans(Stpl_Num, 1, 1);
                g_state.time_operate[Stpl_Num] = 1;
                sast_color_chenge(Stpl_Num);
                g_state.spg_dat[Stpl_Num].time_rno = 2;
                g_state.spg_dat[Stpl_Num].no_chgcol = 1;

                g_state.spg_dat[Stpl_Num].current_spg = g_state.spg_dat[Stpl_Num].spg_dotlen;
                sa_gauge_trans(Stpl_Num);
                return;
            }

            goto jump;

        case 2:
            if (g_state.spg_dat[Stpl_Num].current_spg > 0 && g_state.plw[Stpl_Num].sa->ok == -1) {
                if (g_state.spg_dat[Stpl_Num].current_spg != g_state.spg_dat[Stpl_Num].old_spg) {
                    sa_gauge_trans(Stpl_Num);
                }

                g_state.spg_dat[Stpl_Num].old_spg = g_state.spg_dat[Stpl_Num].current_spg;
            } else {
                g_state.spg_dat[Stpl_Num].time_rno = 4;
            }

            return;

        case 3:
        case_3:
            g_state.spg_dat[Stpl_Num].timer--;

            if (g_state.spg_dat[Stpl_Num].timer) {
                g_state.spg_dat[Stpl_Num].timer2--;

                if (g_state.spg_dat[Stpl_Num].kind == 0) {
                    if (g_state.spg_dat[Stpl_Num].timer2 == 0) {
                        sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, 0, Stpl_Num);
                        sa_waku_trans(Stpl_Num, 0);
                        g_state.spg_dat[Stpl_Num].kind = 1;
                        g_state.spg_dat[Stpl_Num].timer2 = 2;
                    }
                } else {
                    if (g_state.spg_dat[Stpl_Num].timer2 == 0) {
                        sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, 1, Stpl_Num);
                        sa_waku_trans(Stpl_Num, 1);
                        g_state.spg_dat[Stpl_Num].kind = 0;
                        g_state.spg_dat[Stpl_Num].timer2 = 2;
                    }
                }

                return;
            }

            if ((g_state.spg_dat[Stpl_Num].ex_flag == 1 && omop_use_ex_gauge_ix[Stpl_Num] == 0) &&
                (g_state.spg_dat[Stpl_Num].max_old == 1 || g_state.max_rno2[Stpl_Num] == 1)) {
                break;
            }

            if (g_state.plw[Stpl_Num].sa->store == g_state.plw[Stpl_Num].sa->store_max) {
                g_state.max2[Stpl_Num] = 1;
            }

            if (g_state.spg_dat[Stpl_Num].sa_mukou == 1) {
                g_state.max2[Stpl_Num] = 0;
            }

            g_state.spg_dat[Stpl_Num].time_rno = 4;
            /* fallthrough */

        case 4:
            if (g_state.spg_dat[Stpl_Num].sa_mukou == 0) {
                sa_moji_trans(Stpl_Num, 1, 0);
                g_state.spg_dat[Stpl_Num].max_old = 0;
            }

        jump:
            g_state.time_operate[Stpl_Num] = 0;
            sast_color_chenge(Stpl_Num);
            g_state.spg_dat[Stpl_Num].spg_level = g_state.plw[Stpl_Num].sa->store;
            sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, col, Stpl_Num);
            sa_waku_trans(Stpl_Num, col);

            if (g_state.max2[Stpl_Num] == 0 && g_state.spg_dat[Stpl_Num].sa_mukou == 0) {
                sa_gauge_trans(Stpl_Num);
            }

            g_state.spg_dat[Stpl_Num].flag = 0;
            g_state.spg_dat[Stpl_Num].time_rno = 5;
            g_state.spg_dat[Stpl_Num].max_rno = 0;
            g_state.spg_dat[Stpl_Num].sa_flag = 0;
            g_state.spg_dat[Stpl_Num].ex_flag = 0;
            g_state.spg_dat[Stpl_Num].no_chgcol = 0;
            g_state.spg_dat[Stpl_Num].sa_mukou = 0;

            g_state.spg_dat[Stpl_Num].flag2 = 0;
            g_state.sast_now[Stpl_Num] = 0;
            return;

        default:
        case 5:
            g_state.sast_now[Stpl_Num] = 0;
            return;
        }

        sast_color_chenge(Stpl_Num);
        sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, col, Stpl_Num);
        sa_waku_trans(Stpl_Num, col);

        g_state.spg_dat[Stpl_Num].flag = 1;
        g_state.spg_dat[Stpl_Num].time_rno = 5;
        g_state.spg_dat[Stpl_Num].max_old = 1;
        g_state.spg_dat[Stpl_Num].max_rno = 2;
        g_state.spg_dat[Stpl_Num].sa_flag = 0;
        g_state.spg_dat[Stpl_Num].ex_flag = 0;
        g_state.spg_dat[Stpl_Num].no_chgcol = 0;
        g_state.spg_dat[Stpl_Num].sa_mukou = 0;
        g_state.spg_dat[Stpl_Num].flag2 = 0;
        g_state.max2[Stpl_Num] = 0;
        g_state.max_rno2[Stpl_Num] = 0;
        g_state.sast_now[Stpl_Num] = 0;
        return;
    }

    switch (g_state.spg_dat[Stpl_Num].max_rno) {
    case 0:
        if (g_state.plw[Stpl_Num].sa->store > g_state.spg_dat[Stpl_Num].spg_level) {
            g_state.spg_dat[Stpl_Num].current_spg = g_state.spg_dat[Stpl_Num].spg_dotlen;
            sa_gauge_trans(Stpl_Num);
            g_state.spg_dat[Stpl_Num].spg_level = g_state.plw[Stpl_Num].sa->store;
            sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, col, Stpl_Num);
        }

        g_state.spg_dat[Stpl_Num].max_rno = 1;
        /* fallthrough */

    case 1:
        g_state.spg_dat[Stpl_Num].timer--;

        if (g_state.spg_dat[Stpl_Num].timer) {
            g_state.spg_dat[Stpl_Num].timer2--;

            if (g_state.spg_dat[Stpl_Num].kind == 0) {
                if (g_state.spg_dat[Stpl_Num].timer2 == 0) {
                    sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, 0, Stpl_Num);
                    sa_waku_trans(Stpl_Num, 0);
                    g_state.spg_dat[Stpl_Num].kind = 1;
                    g_state.spg_dat[Stpl_Num].timer2 = 2;
                }
            } else if (g_state.spg_dat[Stpl_Num].timer2 == 0) {
                sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, 1, Stpl_Num);
                sa_waku_trans(Stpl_Num, 1);
                g_state.spg_dat[Stpl_Num].kind = 0;
                g_state.spg_dat[Stpl_Num].timer2 = 2;
            }

            return;
        }

        if (g_state.plw[Stpl_Num].sa->store == g_state.plw[Stpl_Num].sa->store_max) {
            g_state.max2[Stpl_Num] = 1;
        }

        if (g_state.spg_dat[Stpl_Num].sa_mukou == 1) {
            g_state.max2[Stpl_Num] = 0;
        }

        if ((g_state.spg_dat[Stpl_Num].ex_flag == 1 && omop_use_ex_gauge_ix[Stpl_Num] == 0) &&
            (g_state.spg_dat[Stpl_Num].max_old == 1 || g_state.max_rno2[Stpl_Num] == 1)) {
            break;
        }

        if (g_state.spg_dat[Stpl_Num].max_old != 0 && g_state.spg_dat[Stpl_Num].sa_mukou == 0) {
            sa_moji_trans(Stpl_Num, 0, 0);
            g_state.spg_dat[Stpl_Num].max_old = 0;
        }

        sast_color_chenge(Stpl_Num);
        g_state.spg_dat[Stpl_Num].spg_level = g_state.plw[Stpl_Num].sa->store;
        sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, col, Stpl_Num);
        sa_waku_trans(Stpl_Num, col);

        if (g_state.max2[Stpl_Num] == 0 && g_state.spg_dat[Stpl_Num].max_old == 0 && g_state.spg_dat[Stpl_Num].sa_mukou == 0) {
            sa_gauge_trans(Stpl_Num);
        }

        g_state.spg_dat[Stpl_Num].flag = 0;
        g_state.spg_dat[Stpl_Num].max_rno = 2;
        g_state.spg_dat[Stpl_Num].sa_flag = 0;
        g_state.spg_dat[Stpl_Num].ex_flag = 0;
        g_state.spg_dat[Stpl_Num].sa_mukou = 0;

        g_state.spg_dat[Stpl_Num].flag2 = 0;
        g_state.sast_now[Stpl_Num] = 0;
        /* fallthrough */

    default:
    case 2:
        g_state.sast_now[Stpl_Num] = 0;
        return;
    }

    sast_color_chenge(Stpl_Num);
    sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, col, Stpl_Num);
    sa_waku_trans(Stpl_Num, col);

    g_state.spg_dat[Stpl_Num].flag = 1;
    g_state.spg_dat[Stpl_Num].max_old = 1;
    g_state.spg_dat[Stpl_Num].max_rno = 2;
    g_state.spg_dat[Stpl_Num].sa_flag = 0;
    g_state.spg_dat[Stpl_Num].ex_flag = 0;
    g_state.spg_dat[Stpl_Num].sa_mukou = 0;
    g_state.spg_dat[Stpl_Num].flag2 = 0;

    g_state.max2[Stpl_Num] = 0;
    g_state.max_rno2[Stpl_Num] = 0;
    g_state.sast_now[Stpl_Num] = 0;
}

/** @brief Cycles colors for the SA stock indicator sprites. */
static void sast_color_chenge(s8 Stpl_Num) {
    if (g_state.plw[Stpl_Num].sa->gauge_type == 1 && g_state.plw[Stpl_Num].sa->ok == -1) {
        col = 1;
        g_state.spg_dat[Stpl_Num].spgcol_number = spg_player_colors[Stpl_Num][SPG_COL_TIMED];
        return;
    } else if (g_state.plw[Stpl_Num].sa->store) {
        col = 1;
        g_state.spg_dat[Stpl_Num].spgcol_number = spg_player_colors[Stpl_Num][SPG_COL_FILLED];
    } else {
        col = 0;
        g_state.spg_dat[Stpl_Num].spgcol_number = spg_player_colors[Stpl_Num][SPG_COL_DEFAULT];
    }
}

/** @brief General SA color change handler (delegates to gauge or stock). */
static void sa_color_chenge(s8 Stpl_Num) {
    g_state.spg_dat[Stpl_Num].spgcol_number = g_state.spg_dat[Stpl_Num].kind ? spg_player_colors[Stpl_Num][SPG_COL_FILLED]
                                                             : spg_player_colors[Stpl_Num][SPG_COL_DEFAULT];
}

/** @brief Cycles colors for the SA gauge bar fill sprites. */
static void sagauge_color_chenge(s8 Stpl_Num) {
    if (g_state.spg_dat[Stpl_Num].no_chgcol) {
        return;
    }

    g_state.spg_dat[Stpl_Num].gauge_flash_time--;

    if (g_state.spg_dat[Stpl_Num].gauge_flash_time != 0) {
        return;
    }

    g_state.spg_dat[Stpl_Num].gauge_flash_time = 2;

    if (Stpl_Num == 0) {
        sq_paint_chenge(6, 26, g_state.spg_dat[0].spg_len, 1, sagauge_colchg_tbl[g_state.spg_dat[0].gauge_flash_col][0]);
    } else if (g_state.spg_dat[1].max == 1 || g_state.spg_dat[1].max_old == 1 || g_state.spg_dat[1].spg_level == g_state.spg_dat[1].spg_maxlevel) {
        sq_paint_chenge(
            42 - g_state.spg_dat[1].spg_len, 26, g_state.spg_dat[1].mass_len, 1, sagauge_colchg_tbl[g_state.spg_dat[1].gauge_flash_col][1]);
        sq_paint_chenge(42 - g_state.spg_dat[1].spg_len + g_state.spg_dat[1].mass_len,
                        26,
                        g_state.spg_dat[1].mchar,
                        1,
                        sagauge_colchg_tbl[g_state.spg_dat[1].gauge_flash_col][0]);
        sq_paint_chenge(42 - g_state.spg_dat[1].spg_len + g_state.spg_dat[1].mass_len + g_state.spg_dat[1].mchar,
                        26,
                        g_state.spg_dat[1].mass_len,
                        1,
                        sagauge_colchg_tbl[g_state.spg_dat[1].gauge_flash_col][1]);
    } else {
        sq_paint_chenge(
            42 - g_state.spg_dat[1].spg_len, 26, g_state.spg_dat[1].spg_len, 1, sagauge_colchg_tbl[g_state.spg_dat[1].gauge_flash_col][1]);
    }

    if (g_state.spg_dat[Stpl_Num].gauge_flash_col == 3) {
        g_state.spg_dat[Stpl_Num].gauge_flash_col = 0;
    } else {
        g_state.spg_dat[Stpl_Num].gauge_flash_col++;
    }

    g_state.spg_dat[Stpl_Num].spgcol_number = sagauge_colchg_tbl[g_state.spg_dat[Stpl_Num].gauge_flash_col][Stpl_Num];
}

/** @brief Transfers SA label text sprite data to the rendering system. */
static void sa_moji_trans(s8 Stpl_Num, s8 Kind, s8 OnOff) {
    switch (Kind) {
    case 0:
        if (OnOff) {
            g_state.spg_dat[Stpl_Num].current_spg = g_state.spg_dat[Stpl_Num].spg_dotlen;
            sa_gauge_trans(Stpl_Num);
            max_mark_write(Stpl_Num, g_state.spg_dat[Stpl_Num].spg_len, g_state.spg_dat[Stpl_Num].mchar, g_state.spg_dat[Stpl_Num].mass_len);
            break;
        }

        if (g_state.max2[Stpl_Num] == 0) {
            sa_gauge_trans(Stpl_Num);
        }

        break;

    default:
    case 1:
        if (Stpl_Num == 0) {
            if (OnOff) {
                scfont_sqput2(1, 25, 11, 0, 2, sa_time_data_tbl[g_state.time_num][0], 0, 4, 2);
                scfont_sqput2(1, 27, 11, 0, 0, 13, 12, 2, 1);
                break;
            }

            sc_clear(1, 25, 4, 26);
            sc_clear(1, 27, 2, 27);
            break;
        }

        if (OnOff) {
            scfont_sqput2(43, 25, 11, 0, 2, sa_time_data_tbl[g_state.time_num][1], 0, 4, 2);
            scfont_sqput2(45, 27, 11, 0, 0, 14, 8, 2, 1);
            break;
        }

        sc_clear(43, 25, 46, 26);
        sc_clear(45, 27, 46, 27);
        break;
    }
}

/** @brief Transfers SA gauge bar sprite data to the rendering system. */
static void sa_gauge_trans(s8 pl_kind) {
    s8 i;
    s16 len;
    const u16* sa_char_ptr;

    g_state.spg_work = 0;
    g_state.spg_number = 0;
    sa_char_ptr = *spgauge_puttbl;
    len = g_state.super_arts[pl_kind].gauge_len / 8;

    for (i = 0; i < len; i++) {
        g_state.spg_work += 8;

        if (g_state.spg_work >= g_state.spg_dat[pl_kind].current_spg) {
            if (g_state.spg_dat[pl_kind].current_spg >= (g_state.spg_work - 8)) {
                g_state.spg_offset = g_state.spg_dat[pl_kind].current_spg - (i * 8);
                scfont_put2((&g_state.spg_dat[pl_kind].spgptbl_ptr[g_state.spg_number])[16 - len],
                            26,
                            g_state.spg_dat[pl_kind].spgcol_number,
                            0,
                            sa_char_ptr[g_state.spg_offset],
                            11);
            } else {
                scfont_put2((&g_state.spg_dat[pl_kind].spgptbl_ptr[g_state.spg_number])[16 - len],
                            26,
                            g_state.spg_dat[pl_kind].spgcol_number,
                            0,
                            0,
                            11);
            }
        } else {
            scfont_put2(
                (&g_state.spg_dat[pl_kind].spgptbl_ptr[g_state.spg_number])[16 - len], 26, g_state.spg_dat[pl_kind].spgcol_number, 0, 8, 11);
        }

        g_state.spg_number++;
    }
}

/** @brief Requests the SA gauge fill sound effect when a stock fills up. */
static void spgauge_sound_request(s8 Stpl_Num) {
    if (g_state.plw[Stpl_Num].sa->store > g_state.spg_dat[Stpl_Num].spg_level) {
        Sound_SE(Stpl_Num + 107);
    }
}

/** @brief Clears all SA gauge work variables for a given stock. */
static void spgauge_work_clear(s8 Stpl_Num) {
    g_state.plw[Stpl_Num].sa->gauge.s.h = g_state.spg_dat[Stpl_Num].current_spg = g_state.plw[Stpl_Num].sa->bacckup_g_h;
    g_state.plw[Stpl_Num].sa->bacckup_g_h = 0;
    g_state.spg_dat[Stpl_Num].old_spg = g_state.spg_dat[Stpl_Num].current_spg;
    g_state.spg_dat[Stpl_Num].flag = 0;
    g_state.spg_dat[Stpl_Num].flag2 = 0;
    g_state.spg_dat[Stpl_Num].level_flag = 0;

    if (g_state.plw[Stpl_Num].sa->store) {
        g_state.spg_dat[Stpl_Num].timer = 0;
        g_state.spg_dat[Stpl_Num].time_rno = 5;
    } else {
        g_state.spg_dat[Stpl_Num].timer = 51;
        g_state.spg_dat[Stpl_Num].time_rno = 0;
    }

    g_state.spg_dat[Stpl_Num].timer2 = 2;
    g_state.spg_dat[Stpl_Num].kind = 0;
    g_state.spg_dat[Stpl_Num].max = 0;
    g_state.spg_dat[Stpl_Num].max_old = 0;
    g_state.spg_dat[Stpl_Num].max_rno = 0;
    g_state.spg_dat[Stpl_Num].gauge_flash_time = 2;
    g_state.spg_dat[Stpl_Num].gauge_flash_col = 0;
    g_state.spg_dat[Stpl_Num].sa_flag = 0;
    g_state.spg_dat[Stpl_Num].ex_flag = 0;
    g_state.spg_dat[Stpl_Num].no_chgcol = 0;
    g_state.spg_dat[Stpl_Num].time_no_clear = 0;
    g_state.spg_dat[Stpl_Num].sa_mukou = 0;
    g_state.sa_gauge_flash[Stpl_Num] = 0;
    sa_color_chenge(Stpl_Num);
    g_state.time_operate[Stpl_Num] = 0;
    g_state.sast_now[Stpl_Num] = 0;
    g_state.max2[Stpl_Num] = 0;
}

/** @brief Writes the SA gauge wipe transition sprites. */
static void spgauge_wipe_write(s8 Stpl_Num) {
    sa_stock_trans(g_state.spg_dat[Stpl_Num].spg_level, col, Stpl_Num);
    sa_gauge_trans(Stpl_Num);
    sa_waku_trans(Stpl_Num, col);
}

/** @brief Transfers the SA gauge frame (waku) sprites with color setting. */
static void sa_waku_trans(s8 Stpl_Num, s8 Spg_Col) {
    s8 lpy;

    sc_ram_to_vram(Stpl_Num + (Spg_Col * 2));

    if (Stpl_Num == 0) {
        for (lpy = 0; lpy < g_state.spg_dat[0].spg_len; lpy++) {
            scfont_put2(lpy + 6, 27, sa_color_data2_tbl[Spg_Col][0], 0, 15, 12);
        }

        if (g_state.cmd_sel[0] == 0 && g_state.no_sa[0] == 0) {
            sa_number_write(0, lpy + 6);
        } else {
            scfont_sqput2(lpy + 6, 26, 14, 0, 2, 27, 2, 1, 2);
        }

        sa_fullstock_trans(g_state.spg_dat[0].spg_maxlevel, Spg_Col, 0);
        return;
    }

    for (lpy = 0; lpy < g_state.spg_dat[1].spg_len; lpy++) {
        scfont_put2(41 - lpy, 27, sa_color_data2_tbl[Spg_Col][1], 0, 15, 12);
    }

    if (g_state.cmd_sel[1] == 0 && g_state.no_sa[1] == 0) {
        sa_number_write(1, 40 - lpy);
    } else {
        scfont_sqput2(41 - lpy, 26, 142, 1, 2, 27, 2, 1, 2);
    }

    sa_fullstock_trans(g_state.spg_dat[1].spg_maxlevel, Spg_Col, 1);
}

/** @brief Initializes SA gauge for training mode (empty gauge). */
void tr_spgauge_cont_init(s8 pl) {
    Sa_frame_Clear2(pl);
    spgauge_init_player(pl, 0);

    g_state.spg_dat[pl].spgcol_number = spg_player_colors[pl][SPG_COL_DEFAULT];

    sa_stock_trans(0, 0, pl);
    sa_waku_trans(pl, 0);
    sa_gauge_trans(pl);
    g_state.Old_Stop_SG = 0;
    g_state.Exec_Wipe_F = 0;
    g_state.time_clear[pl] = 0;
    g_state.spg_offset = 0;
    g_state.time_num = 0;
    g_state.time_timer = 3;
    col = 0;
}

/** @brief Initializes SA gauge for training mode (full gauge). */
void tr_spgauge_cont_init2(s8 pl) {
    Sa_frame_Clear2(pl);
    spgauge_init_player(pl, 1);

    g_state.spg_dat[pl].spgcol_number = spg_player_colors[pl][SPG_COL_DEFAULT];

    sa_stock_trans(g_state.spg_dat[pl].spg_maxlevel, 1, pl);
    sa_waku_trans(pl, 1);
    sa_gauge_trans(pl);
    sa_moji_trans(pl, 0, 1);
    g_state.Old_Stop_SG = 0;
    g_state.Exec_Wipe_F = 0;
    g_state.time_clear[pl] = 0;
    g_state.spg_offset = 0;
    g_state.time_num = 0;
    g_state.time_timer = 3;
    col = 1;
}
