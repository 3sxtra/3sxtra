/**
 * @file appear.c
 * @brief Character entrance (appear) animations.
 *
 * Per-character entrance sequences dispatched by `appear_player()`,
 * including walk-on, jump-in, vehicle arrival, and boss intro animations.
 * Each `Appear_NNNNN` handler corresponds to a specific animation type
 * selected by `app_type_tbl`.
 *
 * Part of the animation module.
 */

#include "sf33rd/Source/Game/animation/appear.h"
#include "game_state.h"
#include "sf33rd/Source/Game/animation/appear_states.h"
#include "port/appear_registry.h"
#include "common.h"
#include "sf33rd/Source/Game/animation/app_data.h"
#include "sf33rd/Source/Game/effect/eff09.h"
#include "sf33rd/Source/Game/effect/eff15.h"
#include "sf33rd/Source/Game/effect/eff46.h"
#include "sf33rd/Source/Game/effect/eff86.h"
#include "sf33rd/Source/Game/effect/eff97.h"
#include "sf33rd/Source/Game/effect/effc5.h"
#include "sf33rd/Source/Game/effect/effc8.h"
#include "sf33rd/Source/Game/effect/effm0.h"
#include "sf33rd/Source/Game/effect/effm1.h"
#include "sf33rd/Source/Game/effect/effm5.h"
#include "sf33rd/Source/Game/effect/effm7.h"
#include "sf33rd/Source/Game/engine/calculate_direction.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/ta_sub.h"
#include "sf33rd/Source/Game/system/work_sys.h"

/* === Named Constants === */
/* APPEAR_TYPE_COUNT is provided by the AppearTypeId enum in appear_registry.h */
#define SMOKE_CHECK_COUNT 22   /**< Entries in smoke_check[] */
#define ANIMAL_DECIDE_COUNT 16 /**< Entries in animal_decide_tbl[] */

/** @brief Clear all appear work variables for a new round. */
void appear_work_clear() {
    g_state.Appear_end = 0;
    g_state.Appear_flag[0] = 0;
    g_state.Appear_flag[1] = 0;
    g_state.Appear_free[0] = 0;
    g_state.Appear_free[1] = 0;
    g_state.bg_stop = 0;
    g_state.bg_app_stop = 0;
}

/** @brief Determine if the player is on the home or visitor side. */
s32 home_visitor_check(PLW* wk) {
    s8 hv_type;
    s16 pl_num;

    hv_type = 0;

    if (wk->wu.id) {
        pl_num = g_state.plw[0].player_number;
    } else {
        pl_num = g_state.plw[1].player_number;
    }

    if (g_state.Play_Type) {
        if (g_state.Champion == wk->wu.id && wk->player_number == pl_num && pl_num != 8) {
            hv_type = 1;
        }
    } else if (wk->wu.pl_operator && wk->player_number == pl_num && pl_num != 8) {
        hv_type = 1;
    }

    return hv_type;
}

/** @brief Apply appear data (position, direction, state) to the player work. */
void appear_data_set(PLW* wk, APPEAR_DATA* dtbl) {
    if (wk->wu.id) {
        wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work - dtbl->hx;
        wk->wu.xyz[1].disp.pos = dtbl->hy;
        wk->wu.rl_flag = (s8)((dtbl->rl + 1) & 1);
        wk->wu.routine_no[APPEAR_RNO_TYPE] = dtbl->rno;
        g_state.Appear_flag[0] = dtbl->ixod;
        wk->wu.char_index = dtbl->char_index;

    } else {
        wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work + dtbl->hx;
        wk->wu.xyz[1].disp.pos = dtbl->hy;
        wk->wu.rl_flag = dtbl->rl;
        wk->wu.routine_no[APPEAR_RNO_TYPE] = dtbl->rno;
        g_state.Appear_flag[1] = dtbl->ixod;
        wk->wu.char_index = dtbl->char_index;
    }
}

/** @brief Select and apply initial appear data based on character/matchup. */
void appear_data_init_set(PLW* wk) {
    APPEAR_DATA* dtbl;
    s8 ap_work;
    s16 id_work;

    g_state.Appear_hv[wk->wu.id] = home_visitor_check(wk);

    id_work = wk->wu.id ^ 1;

    if (g_state.bg_w.area) {
        ap_work = 0;
    } else if (g_state.Appear_hv[wk->wu.id]) {
        ap_work = app_type_tbl2[wk->player_number][g_state.plw[id_work].player_number][g_state.bg_w.stage];
    } else {
        ap_work = app_type_tbl[wk->player_number][g_state.plw[id_work].player_number][g_state.bg_w.stage];
    }

    dtbl = (APPEAR_DATA*)&appear_data[ap_work];

    appear_data_set(wk, dtbl);
}

/** @brief Top-level appear dispatcher — select animation by type registry. */
void appear_player(PLW* wk) {
    s16 idx = (s16)wk->wu.routine_no[APPEAR_RNO_TYPE];
    if (idx < 0 || idx >= APPEAR_TYPE_COUNT) {
        return;
    }

    const AppearTypeCallbacks* cb = AppearType_Get((AppearTypeId)idx);
    if (cb && cb->on_tick) {
        cb->on_tick(wk);
    }
}

/** @brief Appear type 0 — standard walk-on entrance. */
void Appear_00000(PLW* wk) {
    g_state.Appear_end++;
    wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
    wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
}

/** @brief Appear type 1 — walk-on with initial pose. */
void Appear_01000(PLW* wk) {
    s16 work;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        g_state.bg_app_stop = 1;
        work = random_16();

        switch (wk->wu.routine_no[APPEAR_RNO_TYPE]) {
        case 1:
            work &= 3;
            set_char_move_init(&wk->wu, 9, work);
            break;

        case 2:
            work &= 3;
            set_char_move_init(&wk->wu, 9, work + 4);
            break;

        case 35:
            work &= 7;
            set_char_move_init(&wk->wu, 9, work);
            break;
        }
        break;

    case 1:
        if (g_state.Appear_flag[wk->wu.id]) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            char_move(&wk->wu);
            return;
        }
        wk->wu.routine_no[APPEAR_RNO_PHASE] = 3;
        set_char_move_init(&wk->wu, 9, wk->wu.char_index + 8);
        break;

    case 2:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 9 && g_state.Appear_flag[wk->wu.id] == 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init(&wk->wu, 9, wk->wu.char_index + 8);
        }
        break;

    case 3:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }
        break;
    }
}

/** @brief Appear type 3 — jump-in entrance. */
void Appear_03000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        set_char_move_init(&wk->wu, 9, wk->wu.char_index);
        g_state.bg_app_stop = 1;
        return;

    case 1:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }
    }
}

/** @brief Appear type 4 — walk-on with character-specific flourish. */
void Appear_04000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        g_state.bg_app_stop = 1;
        wk->wu.disp_flag = 1;
        set_char_move_init(&wk->wu, 9, 0x10);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 9) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            g_state.app_counter[wk->wu.id] = 0x1C;

            if (wk->wu.id) {
                cal_all_speed_data(
                    &wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work + 0x58, 0, 0, 1);
                return;
            }

            cal_all_speed_data(&wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work - 0x58, 0, 0, 1);
            return;
        }
        break;

    case 2:
        char_move(&wk->wu);
        g_state.app_counter[wk->wu.id]--;

        if (g_state.app_counter[wk->wu.id] <= 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init(&wk->wu, 9, 0x11);

            if (wk->wu.id) {
                wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work + 0x58;
            } else {
                wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work - 0x58;
            }

            wk->wu.xyz[0].disp.low = 0;
            wk->wu.xyz[1].cal = 0;
            return;
        }

        add_x_sub(&wk->wu);
        add_y_sub(&wk->wu);
        break;

    case 3:
        char_move(&wk->wu);

        if ((wk->wu.cg_type) == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }

        break;
    }
}

/** @brief Appear type 5 — dash-in entrance. */
void Appear_05000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        set_char_move_init(&wk->wu, 9, 0x17);
        g_state.bg_app_stop = 1;
        g_state.appear_work[wk->wu.id] = 0x1C;
        break;

    case 1:
        g_state.appear_work[wk->wu.id]--;

        if (g_state.appear_work[wk->wu.id] < 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            g_state.appear_work[wk->wu.id] = 0x1B;

            if (wk->wu.id) {
                cal_all_speed_data(
                    &wk->wu, g_state.appear_work[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work + 0x60, 0, 2, 0);
                return;
            }

            cal_all_speed_data(&wk->wu, g_state.appear_work[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work - 0x60, 0, 2, 0);
        }
        break;

    case 2:
        char_move(&wk->wu);
        g_state.appear_work[wk->wu.id]--;

        if (g_state.appear_work[wk->wu.id] <= 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init2(&wk->wu, 9, 0x17, 9, 0);

            if (wk->wu.id) {
                wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work + 0x58;
                return;
            }
            wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work - 0x58;
            return;
        }

        add_x_sub(&wk->wu);
        break;

    case 3:
        char_move(&wk->wu);

        if ((wk->wu.cg_type) == 9) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->wu.rl_flag ^= 1;
            return;
        }

        break;

    case 4:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }

        break;
    }
}

/** @brief Appear type 6 — flying/airborne entrance. */
void Appear_06000(PLW* wk) {
    s16 work;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        switch (wk->wu.routine_no[APPEAR_RNO_TYPE]) {
        case 6:
            effect_C5_init(wk, 0);
            break;
        case 27:
            effect_C5_init(wk, 1);
            break;
        case 40:
            effect_M5_init(wk);
            break;
        }
        wk->wu.disp_flag = 0;
        set_char_move_init(&wk->wu, 9, 0x13);
        g_state.bg_app_stop = 1;
        break;

    case 1:
        if (g_state.demo_car_flag[wk->wu.id]) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->wu.disp_flag = 1;
            wk->wu.mirror_flag = 0;
            set_char_move_init(&wk->wu, 9, 0x13);
            wk->wu.position_z = wk->wu.next_z = 0x6E;
            wk->wu.mvxy.d[0].sp = 0;
            wk->wu.mvxy.d[1].sp = -0x8000;

            if (wk->wu.routine_no[APPEAR_RNO_TYPE] == 0x1B) {
                g_state.appear_work[wk->wu.id] = 0x34;
            } else {
                g_state.appear_work[wk->wu.id] = 0x2A;
            }

            work = 88;
            if (wk->wu.id) {
                cal_initial_speed(&wk->wu, g_state.appear_work[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work + work, 0);
                break;
            }
            cal_initial_speed(&wk->wu, g_state.appear_work[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work - work, 0);
        }
        break;

    case 2:
        char_move(&wk->wu);
        g_state.appear_work[wk->wu.id]--;

        if (*&g_state.appear_work[wk->wu.id] <= 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->wu.xyz[1].cal = 0;
            set_char_move_init(&wk->wu, 9, 0x10);
            g_state.Appear_end++;
        } else {
            add_x_sub(&wk->wu);
            add_y_sub(&wk->wu);
        }
        break;

    case 3:
        char_move(&wk->wu);
        if (wk->wu.cg_type) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            if (wk->wu.routine_no[APPEAR_RNO_TYPE] == 0x1B) {
                wk->wu.rl_flag ^= 1;
                set_char_move_init(&wk->wu, 0, 1);
            } else {
                wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
                wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            }
        }
        break;

    case 4:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
        }
        break;
    }

    wk->wu.position_x = wk->wu.xyz[0].disp.pos;
    wk->wu.position_y = wk->wu.xyz[1].disp.pos;
}

const APPEAR_DATA appear_data[] = {
    { -88, 0, -88, 0, 1, 0, 0 },    { -88, 0, -88, 0, 1, 1, 0 },     { -88, 0, -88, 0, 1, 2, 0 },
    { -124, 0, -124, 0, 1, 3, 12 }, { -256, 144, -256, 0, 1, 4, 0 }, { 288, 0, 288, 1, 0, 5, 0 },
    { -248, 0, -248, 0, 1, 6, 0 },  { -88, 0, -88, 0, 1, 7, 0 },     { -62, 0, -62, 0, 1, 8, 0 },
    { -88, 200, -88, 0, 1, 9, 16 }, { -216, 0, -224, 0, 1, 10, 16 }, { -102, 0, -102, 0, 1, 1, 0 },
    { -88, 0, -88, 0, 1, 11, 0 },   { -17, 0, -17, 0, 1, 1, 0 },     { -88, 384, -88, 0, 1, 13, 0 },
    { -88, 0, -88, 0, 1, 14, 0 },   { -128, 0, -128, 0, 1, 15, 0 },  { -272, 0, -304, 0, 1, 12, 0 },
    { -88, 0, -88, 0, 1, 16, 0 },   { -88, 0, -88, 0, 1, 17, 0 },    { -88, 0, -88, 0, 1, 21, 0 },
    { -48, 0, -48, 0, 1, 18, 0 },   { -120, 0, -120, 0, 1, 1, 0 },   { -88, 0, -88, 0, 1, 3, 23 },
    { 256, 0, 256, 0, 1, 20, 0 },   { -88, 0, -88, 0, 1, 22, 0 },    { -88, 0, -88, 0, 1, 23, 0 },
    { -96, 0, -96, 0, 1, 24, 0 },   { -96, 0, -96, 0, 1, 25, 0 },    { 168, 80, 168, 0, 0, 26, 0 },
    { 248, 0, 248, 0, 0, 27, 0 },   { -112, 0, -112, 0, 1, 28, 0 },  { -111, 0, -111, 0, 1, 1, 0 },
    { -48, 0, -48, 0, 1, 3, 18 },   { -96, 0, -96, 0, 1, 3, 23 },    { -53, 0, -53, 0, 1, 3, 21 },
    { -53, 0, -53, 0, 1, 3, 22 },   { -72, 0, -72, 0, 1, 38, 20 },   { -84, 0, -84, 0, 1, 3, 22 },
    { -84, 0, -84, 0, 1, 3, 22 },   { -256, 0, -256, 0, 1, 29, 0 },  { -88, 0, -88, 0, 1, 30, 0 },
    { -64, 0, -64, 0, 1, 31, 0 },   { -258, 0, -258, 0, 1, 32, 0 },  { -256, 0, -256, 0, 1, 33, 0 },
    { -88, 0, -88, 0, 1, 34, 0 },   { -88, 0, -88, 0, 1, 35, 0 },    { -88, 0, -88, 0, 1, 3, 16 },
    { -24, 0, -24, 0, 1, 36, 0 },   { -24, 0, -24, 0, 1, 37, 0 },    { -448, 0, -448, 0, 1, 39, 0 },
    { -72, 0, -72, 0, 1, 3, 17 },   { -248, 0, -248, 0, 1, 40, 0 },  { -88, 0, -88, 0, 1, 41, 0 },
    { -88, 0, -88, 0, 1, 3, 17 },
};

/** @brief Appear type 7 — vehicle/ride-in entrance. */
void Appear_07000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.disp_flag = 1;
        g_state.bg_app_stop = 1;

        if (g_state.plw[wk->wu.id ^ 1].player_number == 12 && g_state.bg_w.stage == 12 && g_state.bg_w.area == 0) {
            wk->wu.routine_no[APPEAR_RNO_TYPE] = 1;
            set_char_move_init(&wk->wu, 9, 17);
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 3;
        } else {
            set_char_move_init(&wk->wu, 9, 8);
            effect_C8_init(wk);

            if (g_state.Appear_flag[wk->wu.id]) {
                wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            } else {
                wk->wu.routine_no[APPEAR_RNO_PHASE] = 2;
            }
        }

        break;

    case 1:
        if (g_state.Appear_flag[wk->wu.id] != 0) {
            break;
        }
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        /* fallthrough */

    case 2:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 9) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->wu.cg_type = 0;
        }
        break;

    case 3:
    case 4:
        switch (wk->wu.cg_type) {
        case 1:
            wk->wu.mvxy.a[1].sp = 0x30000;
            wk->wu.mvxy.d[1].sp = 0xffffa000;
            wk->wu.cg_type = 0;
            char_move_z(&wk->wu);
            break;

        case 2:
        case 3:
            char_move(&wk->wu);
            add_y_sub(&wk->wu);
            if (wk->wu.xyz[1].disp.pos < 0) {
                wk->wu.xyz[1].disp.pos = wk->wu.position_y = 0;
                wk->wu.xyz[1].disp.low = 0;
                char_move_z(&wk->wu);
            }
            break;

        case 4:
            wk->wu.cg_type = 0;
            char_move_z(&wk->wu);
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            break;

        default:
            char_move(&wk->wu);
            break;
        }

        break;

    case 5:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }

        break;
    }

    wk->wu.position_x = wk->wu.xyz[0].disp.pos;
    wk->wu.position_y = wk->wu.xyz[1].disp.pos;
}

/** @brief Appear type 8 — charge-in entrance. */
void Appear_08000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;

        if (sean_appear_check(wk, wk->wu.id)) {
            set_char_move_init(&wk->wu, 9, 0x11);
            g_state.Appear_free[wk->wu.id] = 0;
            g_state.bg_app_stop = 1;
            break;
        }

        appear_data_set(wk, (APPEAR_DATA*)appear_data);
        wk->wu.routine_no[APPEAR_RNO_TYPE] = 2;
        break;

    case 1:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }
    }
}

/** @brief Check if Sean’s appear needs the basketball variant. */
s32 sean_appear_check(PLW* wk, s16 id) {
    if (g_state.plw[id].player_number == 12 && g_state.bg_w.stage == 12) {
        return 1;
    }

    return 0;
}

/** @brief Appear type 9 — Sean’s basketball entrance. */
void Appear_09000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        set_char_move_init(&wk->wu, 9, 0x10);
        g_state.bg_app_stop = 1;
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            g_state.Appear_free[wk->wu.id] = 1;
            g_state.app_counter[wk->wu.id] = 0x20;
            return;
        }

        break;

    case 2:
        g_state.app_counter[wk->wu.id]--;

        if (g_state.app_counter[wk->wu.id] < 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init(&wk->wu, 9, 0x11);
            wk->wu.mvxy.a[1].sp = -0xB0000;
            wk->wu.mvxy.d[1].sp = -0x6000;
            return;
        }

        break;

    case 3:
        char_move(&wk->wu);
        add_y_sub(&wk->wu);

        if (wk->wu.xyz[1].disp.pos <= 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->wu.xyz[1].cal = 0;
            set_char_move_init(&wk->wu, 9, 0x12);
            g_state.Appear_end++;
            return;
        }

        break;

    case 4:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
        }

        break;
    }
}

/** @brief Appear type 10 — dramatic pose entrance. */
void Appear_10000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        set_char_move_init(&wk->wu, 9, 0x13);
        g_state.bg_app_stop = 1;
        wk->wu.mvxy.d[0].sp = 0;
        if (wk->wu.id) {
            wk->wu.mvxy.a[0].sp = -0x18000;
            break;
        }
        wk->wu.mvxy.a[0].sp = 0x18000;
        break;

    case 1:
        char_move(&wk->wu);
        add_x_sub(&wk->wu);

        if (wk->wu.id) {
            if (!(wk->wu.xyz[0].disp.pos <= g_state.bg_w.bgw[1].pos_x_work + 0x58)) {
                return;
            }
        } else if (!(wk->wu.xyz[0].disp.pos >= g_state.bg_w.bgw[1].pos_x_work - 0x58)) {
            return;
        }
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        set_char_move_init(&wk->wu, 9, 0x14);
        g_state.Appear_end++;
        break;

    case 2:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
        }
        break;
    }
}

/** @brief Appear type 11 — casual walk-on variant. */
void Appear_11000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        g_state.bg_app_stop = 1;
        g_state.app_counter[wk->wu.id] = 0x50;
        set_char_move_init(&wk->wu, 0, 0);
        break;

    case 1:
        char_move(&wk->wu);
        g_state.app_counter[wk->wu.id]--;

        if (g_state.app_counter[wk->wu.id] < 0) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 1;
            g_state.Appear_end++;

            if (g_state.Demo_Flag != 0) {
                SsRequestPan(0x2A9, 0x40, 0x40, 0, 2);
            }
        }
    }
}

/** @brief Appear type 12 — multi-phase entrance animation. */
void Appear_12000(PLW* wk) {
    s16 work;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        g_state.bg_app_stop = 1;
        set_char_move_init(&wk->wu, 9, 12);
        effect_46_init(&wk->wu, 0);
        work = 88;
        wk->wu.mvxy.d[0].sp = 0;
        wk->wu.mvxy.d[1].sp = -0x8000;
        g_state.app_counter[wk->wu.id] = 0x30;

        if (wk->wu.id) {
            cal_initial_speed(&wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work + work, 0);
            return;
        }
        cal_initial_speed(&wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work - work, 0);
        return;

    case 1:
        char_move(&wk->wu);
        g_state.app_counter[wk->wu.id]--;

        if (g_state.app_counter[wk->wu.id] <= 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->wu.xyz[1].cal = 0;
            set_char_move_init2(&wk->wu, 9, 12, 19, 0);
            g_state.Appear_end++;
            return;
        }

        add_x_sub(&wk->wu);
        add_y_sub(&wk->wu);
        return;

    case 2:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.cg_type = 0;
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
        }
    }
}

/** @brief Appear type 13 — character taunt entrance. */
void Appear_13000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        g_state.bg_app_stop = 1;
        set_char_move_init2(&wk->wu, 9, 0x3D, 4, 0);
        wk->wu.mvxy.a[1].sp = 0x78000;
        wk->wu.mvxy.d[1].sp = -0x3000;
        wk->wu.shadow_flag = 0;
        break;

    case 1:
        // Do nothing

    case 2:
        char_move(&wk->wu);
        add_y_sub(&wk->wu);

        if (wk->wu.xyz[1].disp.pos < 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init(&wk->wu, 9, 0x3E);
            wk->wu.xyz[1].cal = 0;
            g_state.Appear_end++;
            return;
        }

        break;

    case 3:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
        }

        break;
    }
}

/** @brief Appear type 14 — teleport/materialise entrance. */
void Appear_14000(PLW* wk) {
    s16 work;
    s16 id_w = wk->wu.id ^ 1;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE] += 1;
        wk->wu.disp_flag = 1;
        wk->gill_ccch_go = 1;

        if (sean_appear_check(wk, id_w)) {
            set_char_move_init(&wk->wu, 9, 0x3C);
            return;
        }

        wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
        wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
        g_state.Appear_end++;
        break;

    case 1:
        switch (g_state.Appear_free[id_w]) {
        case 0:
            char_move(&wk->wu);
            break;

        case 1:
            work = wk->wu.graphic_index / wk->wu.char_graphic_data_type;
            set_char_move_init2(&wk->wu, 0, 0, work + 1, 0);
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 1;
            g_state.Appear_end += 1;
            break;

        case 2:
            char_move(&wk->wu);
            if (wk->wu.cg_type == 0xFF) {
                wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
                wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
                g_state.Appear_end += 1;
            }
        }
    }
}

/** @brief Appear type 15 — off-screen approach entrance. */
void Appear_15000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        g_state.bg_app_stop = 1;
        wk->wu.disp_flag = 1;
        set_char_move_init(&wk->wu, 9, 8);
        effect_97_init(wk);
        break;

    case 1:
        char_move(&wk->wu);

        switch (wk->wu.cg_type) {
        case 0x2:
            wk->wu.cg_type = 0;
            Sound_SE(0x10A);
            break;

        case 0x3:
            wk->wu.cg_type = 0;
            Sound_SE(0x10B);
            break;

        case 0xFF:
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
            break;
        }
    }
}

/** @brief Appear type 16 — backflip/acrobatic entrance. */
void Appear_16000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        g_state.bg_app_stop = 1;

        if (smoke_check[g_state.bg_w.bg_index]) {
            set_char_move_init(&wk->wu, 9, 0xE);
            return;
        }

        set_char_move_init(&wk->wu, 9, 0xC);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }
    }
}

/** @brief Check if Gill should use the special boss-intro appear. */
s16 gill_appear_check() {
    s16 id_w;

    if (g_state.bg_w.stage && g_state.bg_w.area) {
        return 1;
    }

    id_w = -1;

    if (g_state.plw[0].player_number == 0) {
        id_w = 1;
    }

    if (g_state.plw[1].player_number == 0) {
        id_w = 0;
    }

    if (id_w < 0) {
        return 1;
    }

    if (g_state.Play_Type == 1) {
        return 1;
    }

    if (g_state.Introduce_Boss[g_state.Player_id][1] & 0x80) {
        return 1;
    }

    return 0;
}

/** @brief Appear type 17 — Gill’s resurrection/boss intro. */
void Appear_17000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;

        if (g_state.Gill_Appear_Flag) {
            appear_data_set(wk, (APPEAR_DATA*)appear_data);
            Appear_00000(wk);
            return;
        }

        set_char_move_init(&wk->wu, 9, 0);
        g_state.bg_app_stop = 1;
        gSeqStatus[0] = 0;
        SsRequest(0x3C);
        set_char_move_init(&wk->wu, 9, 8);
        break;

    case 1:
        if (!g_state.bg_app) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        }
        break;

    case 2:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 2) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->gill_ccch_go = 1;
            return;
        }

        break;

    case 3:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 3) {
            wk->wu.cg_type = 0;
            SsBgmFadeOut(0xAA);
        }

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
            Standby_BGM(0x2E);
        }

        break;
    }
}

/** @brief Appear type 18 — extended character-specific entrance. */
void Appear_18000(PLW* wk) {
    s16 work;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;

        if (g_state.plw[0].player_number == 8 && g_state.plw[1].player_number == 8) {
            g_state.Appear_free[wk->wu.id] = 0;

            if (wk->wu.id) {
                wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work - 0x3B;
            } else {
                wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work + 0x3B;
            }

            set_char_move_init(&wk->wu, 9, 0x10);
            goto one;
        }

        g_state.Appear_free[wk->wu.id] = 1;
        work = random_16();
        work &= 3;
        set_char_move_init(&wk->wu, 9, work + 8);

        wk->wu.mvxy.a[0].sp = 0;
        wk->wu.mvxy.a[1].sp = 0x80000;

        g_state.appear_work[wk->wu.id] = 0x1F;

        if (wk->wu.id) {
            cal_delta_speed(&wk->wu, g_state.appear_work[wk->wu.id], (g_state.bg_w.bgw[1].pos_x_work + 0x58), 0, 0, 1);
            goto one;
        }
        cal_delta_speed(&wk->wu, g_state.appear_work[wk->wu.id], (g_state.bg_w.bgw[1].pos_x_work - 0x58), 0, 0, 1);

    one:
        g_state.bg_app_stop = 1;
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 9) {
            if (!g_state.Appear_free[wk->wu.id]) {
                wk->wu.mvxy.a[0].sp = 0;
                wk->wu.mvxy.a[1].sp = 0x80000;

                g_state.appear_work[wk->wu.id] = 0x1F;

                if (wk->wu.id) {
                    cal_delta_speed(
                        &wk->wu, g_state.appear_work[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work + 0x58, 0, 0, 1);
                } else {
                    cal_delta_speed(
                        &wk->wu, g_state.appear_work[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work - 0x58, 0, 0, 1);
                }
            }

            wk->wu.cg_type = 0;
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        }
        break;

    case 2:
        char_move(&wk->wu);

        g_state.appear_work[wk->wu.id]--;

        if (g_state.appear_work[wk->wu.id] <= 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->wu.xyz[1].cal = 0;
            return;
        }

        add_x_sub(&wk->wu);
        add_y_sub(&wk->wu);
        return;

    case 3:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }

        break;
    }
    return;
}

/** @brief Appear type 19 — car/vehicle arrival entrance. */
void Appear_19000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        g_state.bg_app_stop = 1;
        wk->wu.disp_flag = 1;
        set_char_move_init(&wk->wu, 9, 0);
        wk->wu.rl_flag = 0;
        wk->wu.xyz[0].disp.pos = 0x275;
        wk->wu.xyz[1].disp.pos = 0x154;
        wk->wu.next_z = 0x56;
        wk->wu.mirror_flag = 1;
        wk->wu.mirror_scale.size.x = 0x35;
        wk->wu.mirror_scale.size.y = 0x35;
        g_state.appear_work[wk->wu.id] = 0x82;
        break;

    case 1:
        g_state.appear_work[wk->wu.id]--;

        wk->wu.next_z = 0x56;

        if (g_state.appear_work[wk->wu.id] < 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->wu.mirror_flag = 0;
            wk->wu.mirror_scale.size.x = 0x3F;
            wk->wu.mirror_scale.size.y = 0x3F;

            set_char_move_init2(&wk->wu, 9, 0x3D, 4, 0);

            g_state.appear_work[wk->wu.id] = 0x20;

            if (wk->wu.id) {
                cal_all_speed_data(
                    &wk->wu, g_state.appear_work[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work + 0x58, 0, 1, 1);
            } else {
                cal_all_speed_data(
                    &wk->wu, g_state.appear_work[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work - 0x58, 0, 1, 1);
            }

            if (wk->wu.id == 0) {
                wk->wu.rl_flag = 1;
            }

            wk->wu.next_z = wk->wu.my_priority;

            effect_15_init(&wk->wu, 0);
            effect_15_init(&wk->wu, 1);
            effect_15_init(&wk->wu, 2);
        }
        break;

    case 2:
        g_state.appear_work[wk->wu.id]--;

        if (g_state.appear_work[wk->wu.id] <= 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init(&wk->wu, 9, 0x3E);
            wk->wu.xyz[1].cal = 0;

            if (wk->wu.id) {
                g_state.Appear_flag[0] = 0;
                return;
            }

            g_state.Appear_flag[1] = 0;
            return;
        }

        add_x_sub(&wk->wu);
        add_y_sub(&wk->wu);
        break;

    case 3:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }

        break;
    }
}

/** @brief Appear type 20 — helicopter/airborne drop entrance. */
void Appear_20000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        set_char_move_init(&wk->wu, 9, 0x15);
        g_state.bg_app_stop = 1;
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }
    }
}

/** @brief Appear type 21 — train arrival entrance. */
void Appear_21000(PLW* wk) {
    s16 work;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE] += 1;
        wk->wu.disp_flag = 1;
        work = random_16();
        work &= 7;
        if (work == 6 || work == 7) {
            appear_data_set(wk, (APPEAR_DATA*)appear_data + 24);
            set_char_move_init(&wk->wu, 9, 0xE);
        } else {
            set_char_move_init(&wk->wu, 9, work + 8);
        }
        g_state.bg_app_stop = 1;
        break;

    case 1:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end += 1;
        }
    }
}

/** @brief Appear type 22 — boat/water entrance. */
void Appear_22000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        wk->wu.script_register_bank[1] = 0;
        set_char_move_init(&wk->wu, 9, 0);
        g_state.bg_app_stop = 1;
        break;

    case 1:
        char_move(&wk->wu);
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        set_char_move_init(&wk->wu, 9, 8);
        break;

    case 2:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }
    }
}

/** @brief Appear type 23 — motorcycle entrance. */
void Appear_23000(PLW* wk) {
    s16 work;
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE] += 1;
        wk->wu.disp_flag = 1;
        work = random_16();
        work &= 3;
        wk->wu.script_register_bank[1] = 0;
        set_char_move_init(&wk->wu, 9, work + 4);
        g_state.bg_app_stop = 1;
        break;

    case 1:
        char_move(&wk->wu);
        if ((wk->wu.script_register_bank[1]) && wk->wu.cg_type == 9) {
            wk->wu.routine_no[APPEAR_RNO_PHASE] += 1;
            set_char_move_init(&wk->wu, 9, wk->wu.char_index + 8);
            return;
        } else {
            break;
        }
    case 2:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end += 1;
        }
        break;
    }
}

/** @brief Appear type 24 — skateboard entrance. */
void Appear_24000(PLW* wk) {
    if (!wk->wu.pl_operator) {
        if (wk->wu.id) {
            wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work + 0xA8;
        } else {
            wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work - 0x90;
        }
    }

    wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
    wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
}

/** @brief Appear type 25 — minimal entrance (direct set). */
void Appear_25000(PLW* wk) {
    if (!wk->wu.pl_operator) {
        wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work;
    }

    wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
    wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
}

const s16 smoke_check[SMOKE_CHECK_COUNT] = { 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0 };

/** @brief Appear type 26 — smoke/cloud entrance with per-character variants. */
void Appear_26000(PLW* wk) {
    // s32 effect_86_init(s16 type86);

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        g_state.bg_app_stop = 1;
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        set_char_move_init(&wk->wu, 9, 0x10);
        g_state.appear_work[wk->wu.id] = 0x14;
        g_state.Appear_free[wk->wu.id] = 0;
        break;

    case 1:
        g_state.appear_work[wk->wu.id]--;

        if (g_state.appear_work[wk->wu.id] < 1) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            g_state.appear_work[wk->wu.id] = 0x14;

            if (wk->wu.id) {
                cal_all_speed_data(
                    &wk->wu, g_state.appear_work[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work + 88, 0, 0, 1);
            } else {
                cal_all_speed_data(
                    &wk->wu, g_state.appear_work[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work - 88, 0, 0, 1);
            }
        }

        break;

    case 2:
        char_move(&wk->wu);
        g_state.appear_work[wk->wu.id]--;
        add_x_sub(&wk->wu);
        add_y_sub(&wk->wu);

        if (wk->wu.xyz[1].disp.pos < 0x41) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->wu.hit_quake = 0x18;

            if (wk->wu.id == 0) {
                effect_86_init(0);
            }
            Sound_SE(0x109);
        }

        break;

    case 3:
        char_move(&wk->wu);

        if (wk->wu.hit_quake < 1) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            g_state.Appear_free[wk->wu.id] = 1;
        }

        break;

    case 4:
        char_move(&wk->wu);
        g_state.appear_work[wk->wu.id]--;

        if (g_state.appear_work[wk->wu.id] < 1) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init2(&wk->wu, 9, 0x10, 3, 0);
            wk->wu.xyz[1].cal = 0;
        } else {
            add_x_sub(&wk->wu);
            add_y_sub(&wk->wu);
        }

        break;

    case 5:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 9) {
            wk->wu.rl_flag ^= 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        }

        break;

    case 6:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }

        break;
    }
}

/** @brief Appear type 28 — round-2+ re-entrance animation. */
void Appear_28000(PLW* wk) {
    s16 id_w = wk->wu.id ^ 1;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        g_state.Appear_car_stop[id_w] = 0;
        set_char_move_init(&wk->wu, 9, 17);
        g_state.bg_app_stop = 1;
        break;

    case 1:
        if (g_state.Appear_car_stop[id_w]) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init2(&wk->wu, 9, 17, 2, 0);
        }

        break;

    case 2:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 9) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        }

        break;

    case 3:
        if (g_state.plw[id_w].wu.routine_no[APPEAR_RNO_PHASE] >= 3) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            g_state.appear_work[wk->wu.id] = 20;
        }

        break;

    case 4:
        g_state.appear_work[wk->wu.id]--;

        if (g_state.appear_work[wk->wu.id] < 1) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init2(&wk->wu, 9, 17, 15, 0);
        }

        break;

    case 5:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }

        break;
    }
}

/** @brief Appear type 29 — extended re-entrance with sound cues. */
void Appear_29000(PLW* wk) {
    s16 work;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        g_state.bg_app_stop = 1;
        wk->wu.disp_flag = 1;
        wk->wu.script_register_bank[1] = 0;
        wk->wu.script_register_bank[2] = 0;
        work = random_16();
        work &= 3;
        wk->wu.script_register_bank[2] = work;
        set_char_move_init(&wk->wu, 9, 0);
        work = random_16();

        if (work & 1) {
            effect_09_init2(&wk->wu, 0x19);
        }
        if (8 < work) {
            effect_09_init2(&wk->wu, 0x1b);
        }

        animal_decide(wk);
        break;

    case 1:
        if (wk->wu.script_register_bank[1]) {
            switch (wk->wu.script_register_bank[2]) {
            case 0:
            case 1:
                wk->wu.routine_no[APPEAR_RNO_PHASE] = 2;

                if (wk->wu.id) {
                    wk->wu.mvxy.a[0].sp = 0xffff0000;
                } else {
                    wk->wu.mvxy.a[0].sp = 0x10000;
                }

                set_char_move_init(&wk->wu, 9, 8);
                break;

            case 2:
                wk->wu.routine_no[APPEAR_RNO_PHASE] = 3;

                if (wk->wu.id) {
                    wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work + 0xd8;
                } else {
                    wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work + -0xd8;
                }

                set_char_move_init(&wk->wu, 9, 10);
                break;

            case 3:
                wk->wu.routine_no[APPEAR_RNO_PHASE] = 4;
                set_char_move_init(&wk->wu, 9, 0xb);
                wk->wu.mvxy.d[0].sp = 0;
                wk->wu.mvxy.d[1].sp = 0xffff8000;
                wk->wu.xyz[1].disp.pos = 0xb0;
                g_state.app_counter[wk->wu.id] = 0x20;

                if (wk->wu.id) {
                    cal_initial_speed(
                        &wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work + 0x58, 0);
                } else {
                    cal_initial_speed(
                        &wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work - 0x58, 0);
                }
            }
        }

        break;

    case 2:
        char_move(&wk->wu);
        add_x_sub(&wk->wu);

        if (wk->wu.id) {
            if (!(wk->wu.xyz[0].disp.pos <= (g_state.bg_w.bgw[1].pos_x_work + 0x58))) {
                return;
            }
        } else if (wk->wu.xyz[0].disp.pos < (g_state.bg_w.bgw[1].pos_x_work - 0x58)) {
            return;
        }

        wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
        wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
        g_state.Appear_end++;
        break;

    case 3:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }
        break;

    case 4:
        g_state.app_counter[wk->wu.id]--;

        if (g_state.app_counter[wk->wu.id] < 1) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init2(&wk->wu, 9, 0xb, 5, 0);
            wk->wu.xyz[1].disp.pos = 0;
        } else {
            add_x_sub(&wk->wu);
            add_y_sub(&wk->wu);
        }

        break;

    case 5:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }

        break;
    }
}

/** @brief Select which companion animal appears with the character. */
void animal_decide(PLW* wk) {
    u8 work2;
    s16 work = random_16();

    work2 = animal_decide_tbl[work & (ANIMAL_DECIDE_COUNT - 1)];
    don_appear_check(wk);

    switch (work2) {
    case 0:
        break;

    case 1:
        effect_M0_init(wk->wu.rl_flag, 0);
        effect_M0_init(wk->wu.rl_flag, 1);
        break;

    default:
        effect_M0_init(wk->wu.rl_flag, work2);
        break;
    }

    return;
}

/** @brief Check if Don’s appear triggers a special crowd reaction. */
void don_appear_check(PLW* wk) {
    s16 id_w = wk->wu.id ^ 1;

    if (g_state.plw[id_w].player_number == 7) {
        effect_M0_init(wk->wu.rl_flag, 6);
    }
}

/** @brief Appear type 30 — standing idle entrance. */
void Appear_30000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        g_state.bg_app_stop = 1;
        random_16(); // Although the result is unused, I'm keeping the call, because it changes RNG state
        appear_data_set(wk, (APPEAR_DATA*)appear_data + 24);
        set_char_move_init(&wk->wu, 9, 0xE);
        break;

    case 1:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end += 1;
        }
    }
}

/** @brief Appear type 31 — walk-on with taunt. */
void Appear_31000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;

        if (g_state.Appear_flag[wk->wu.id]) {
            appear_data_set(wk, (APPEAR_DATA*)appear_data);
            Appear_00000(wk);
        } else {
            set_char_move_init(&wk->wu, 9, 8);
        }

        g_state.bg_app_stop = 1;
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }

        break;
    }
}

/** @brief Appear type 32 — power-up entrance animation. */
void Appear_32000(PLW* wk) {
    s16 work;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE] += 1;
        wk->wu.disp_flag = 1;
        work = random_16();
        work &= 7;
        set_char_move_init(&wk->wu, 9, work + 8);
        g_state.bg_app_stop = 1;
        break;

    case 1:
        char_move(&wk->wu);
        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end += 1;
        }
    }
}

/** @brief Appear type 33 — leaping entrance. */
void Appear_33000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        set_char_move_init(&wk->wu, 9, 0xC);
        wk->wu.mvxy.d[0].sp = 0;
        wk->wu.mvxy.d[1].sp = -0x8000;
        wk->wu.xyz[1].disp.pos = 0x50;
        g_state.app_counter[wk->wu.id] = 0x2A;

        if (wk->wu.id) {
            cal_initial_speed(&wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work + 0x58, 0);
        } else {
            cal_initial_speed(&wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work - 0x58, 0);
        }

        g_state.bg_app_stop = 1;
        don_appear_check(wk);
        break;

    case 1:
        g_state.app_counter[wk->wu.id]--;

        if (g_state.app_counter[wk->wu.id] <= 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init2(&wk->wu, 9, 0xC, 2, 0);
            wk->wu.xyz[1].disp.pos = 0;
            return;
        }

        add_x_sub(&wk->wu);
        add_y_sub(&wk->wu);
        break;

    case 2:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }
    }
}

/** @brief Appear type 34 — landing from above entrance. */
void Appear_34000(PLW* wk) {
    s16 work;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE] += 1;
        wk->wu.disp_flag = 1;

        work = random_16();
        work &= 7;
        set_char_move_init(&wk->wu, 9, work);

        switch (work) {
        case 0:
        case 2:
        case 6:
        case 7:
            if (wk->wu.id) {
                wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work + 0x71;
            } else {
                wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work - 0x71;
            }
        }
        g_state.bg_app_stop = 1;
        break;

    default:
        Appear_01000(wk);
    }
}

/** @brief Appear type 36 — complex multi-phase entrance. */
void Appear_36000(PLW* wk) {
    s16 id_w = wk->wu.id ^ 1;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        set_char_move_init(&wk->wu, 9, 0x10);
        g_state.bg_app_stop = 1;
        break;

    case 1:
        char_move(&wk->wu);

        if (g_state.plw[id_w].wu.script_register_bank[0] == 3) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init(&wk->wu, 9, 0x11);
            g_state.app_counter[wk->wu.id] = 0x10;
            wk->wu.next_z = g_state.plw[id_w].wu.my_priority;
        }

        break;

    case 2:
        char_move(&wk->wu);
        g_state.app_counter[wk->wu.id]--;

        if (g_state.app_counter[wk->wu.id] <= 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            g_state.app_counter[wk->wu.id] = 0x16;

            if (wk->wu.id) {
                cal_all_speed_data(
                    &wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work + 0x58, 0, 2, 0);
            } else {
                cal_all_speed_data(
                    &wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work - 0x58, 0, 2, 0);
            }
        }

        wk->wu.next_z = g_state.plw[id_w].wu.my_priority;
        break;

    case 3:
        char_move(&wk->wu);
        g_state.app_counter[wk->wu.id]--;

        if (g_state.app_counter[wk->wu.id] <= 0) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init2(&wk->wu, 9, 0x11, 0x0A, 0);
            wk->wu.next_z = wk->wu.my_priority;
        } else {
            add_x_sub(&wk->wu);
        }

        break;

    case 4:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }

        break;
    }
}

const u8 animal_decide_tbl[ANIMAL_DECIDE_COUNT] = { 0, 1, 2, 3, 4, 5, 0, 2, 0, 1, 2, 3, 4, 5, 0, 0 };

/** @brief Appear type 37 — animal companion entrance. */
void Appear_37000(PLW* wk) {
    s16 id_w = wk->wu.id ^ 1;

    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        set_char_move_init(&wk->wu, 9, 0x11);
        g_state.bg_app_stop = 1;
        wk->wu.script_register_bank[0] = 0;
        effect_M1_init(&wk->wu);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->wu.script_register_bank[0] = 1;
        }
        break;

    case 2:
        char_move(&wk->wu);

        if (wk->wu.script_register_bank[0] == 2) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init(&wk->wu, 9, 0x12);
        }

        break;

    case 3:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init(&wk->wu, 9, 0x13);
        }

        break;

    case 4:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 9) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            wk->wu.script_register_bank[0] = 3;
            wk->wu.next_z = g_state.plw[id_w].wu.my_priority;
        }

        break;

    case 5:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_PHASE]++;
            set_char_move_init(&wk->wu, 0, 3);
            g_state.app_counter[wk->wu.id] = 0x2a;

            if (wk->wu.id) {
                cal_all_speed_data(
                    &wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work + 0x58, 0, 0, 0);
            } else {
                cal_all_speed_data(
                    &wk->wu, g_state.app_counter[wk->wu.id], g_state.bg_w.bgw[1].pos_x_work - 0x58, 0, 0, 0);
            }

            wk->wu.next_z = wk->wu.my_priority;
        } else {
            wk->wu.next_z = g_state.plw[id_w].wu.my_priority;
        }

        break;

    case 6:
        char_move(&wk->wu);
        g_state.app_counter[wk->wu.id]--;

        if (g_state.app_counter[wk->wu.id] < 1) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        } else {
            add_x_sub(&wk->wu);
        }

        break;
    }
}

/** @brief Appear type 38 — meta-character walk-on entrance. */
void Appear_38000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        g_state.bg_app_stop = 1;
        wk->wu.disp_flag = 1;

        if (wk->wu.id) {
            set_char_move_init(&wk->wu, 9, 0x14);
            return;
        }

        set_char_move_init(&wk->wu, 9, 0x13);
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }
    }
}

/** @brief Appear type 39 — elevator/platform rise entrance. */
void Appear_39000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;

        if (g_state.Gill_Appear_Flag) {
            appear_data_set(wk, (APPEAR_DATA*)appear_data);
            Appear_00000(wk);
            return;
        }

        g_state.bg_app_stop = 1;
        set_char_move_init(&wk->wu, 0, 2);

        if (wk->wu.id) {
            wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work + 0x200;
        }

        setup_mvxy_data(&wk->wu, 0);
        wk->wu.mvxy.a[0].sp >>= 1;
        add_mvxy_speed(&wk->wu);
        wk->wu.mvxy.a[0].sp *= 2;
        break;

    case 1:
        cal_mvxy_speed(&wk->wu);
        add_mvxy_speed(&wk->wu);
        char_move(&wk->wu);

        if (wk->wu.id) {
            if (wk->wu.xyz[0].disp.pos < (g_state.bg_w.bgw[1].pos_x_work + 88)) {
                wk->wu.routine_no[APPEAR_RNO_PHASE]++;
                wk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[1].pos_x_work + 88;
                return;
            }
        } else {
            if (wk->wu.xyz[0].disp.pos > (g_state.bg_w.bgw[1].pos_x_work - 88)) {
                wk->wu.routine_no[APPEAR_RNO_PHASE] += 1;
                wk->wu.xyz[0].disp.pos = (g_state.bg_w.bgw[1].pos_x_work - 88);
                return;
            }
        }

        break;

    case 2:
        wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
        wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
        g_state.Appear_end += 1;
        break;
    }
}

/** @brief Appear type 41 — delayed entrance (Q-specific). */
void Appear_41000(PLW* wk) {
    switch (wk->wu.routine_no[APPEAR_RNO_PHASE]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_PHASE]++;
        wk->wu.disp_flag = 1;
        g_state.bg_app_stop = 1;
        set_char_move_init(&wk->wu, 0, 0);
        g_state.app_counter[wk->wu.id] = 0x78;
        effect_M7_init(wk);
        break;

    case 1:
        char_move(&wk->wu);
        g_state.app_counter[wk->wu.id]--;

        if (g_state.app_counter[wk->wu.id] < 0) {
            wk->wu.routine_no[APPEAR_RNO_COMPLETE] = 1;
            wk->wu.routine_no[APPEAR_RNO_PHASE] = 0;
            g_state.Appear_end++;
        }

        break;
    }
}

/** @brief Gouki (Akuma) teleport entrance animation. */
void gouki_appear(PLW* wk) {
    if (!wk->wu.script_register_bank[0]) {
        char_move(&wk->wu);
        return;
    }

    switch (wk->wu.routine_no[APPEAR_RNO_GOUKI]) {
    case 0:
        wk->wu.routine_no[APPEAR_RNO_GOUKI]++;
        set_char_move_init(&wk->wu, 1, 0x3C);
        char_move_z(&wk->wu);
        wk->wu.xyz[1].disp.pos = -6;
        break;

    case 1:
        char_move(&wk->wu);

        if (wk->wu.cg_type == 0xFF) {
            wk->wu.routine_no[APPEAR_RNO_GOUKI]++;
        }

        break;

    case 2:
        break;
    }
}
