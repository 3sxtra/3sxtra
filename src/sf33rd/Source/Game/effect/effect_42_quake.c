/**
 * @file eff42.c
 * Effect: Quake Effect
 */

#include "sf33rd/Source/Game/effect/effect_42_quake.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "port/sdl/rmlui/rmlui_char_select.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/screen/sel_data.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"

static void EFF42_SUDDENLY(State_Other* ewk);
static void EFF42_SLIDE_IN(State_Other* ewk);
static void EFF42_SLIDE_OUT(State_Other* ewk);
static void EFF42_KILL(State_Other* ewk);
static void EFF42_MOVE(State_Other* ewk);
static void Setup_Char_Index(State_Other* ewk);

void (*const EFF42_Jmp_Tbl[5])();

/* eff42 draws the timer/counter display on the char select screen.
 * Suppress rendering when the RmlUI overlay provides the same UI. */
void effect_42_move(State_Other* ewk) {
    EFF42_Jmp_Tbl[g_state.Order[ewk->wu.dir_old]](ewk);

    if (ewk->wu.be_flag != 0) {
        ewk->wu.position_x = ewk->wu.xyz[0].disp.pos & 0xFFFF;
        ewk->wu.position_y = ewk->wu.xyz[1].disp.pos & 0xFFFF;
        if (!rmlui_char_select_visible)
            sort_push_request4(&ewk->wu);
    }
}

static void EFF42_SUDDENLY(State_Other* ewk) {
    switch (ewk->wu.routine_no[6]) {
    case 0:
        if (--g_state.Order_Timer[ewk->wu.dir_old] != 0) {
            break;
        }

        if (ewk->wu.my_family == 4) {
            ewk->wu.routine_no[6]++;
            ewk->wu.xyz[0].disp.pos = g_state.Target_BG_X[3] + g_state.Offset_BG_X[3] + Pos_Data_69[ewk->wu.dir_old][0];
            ewk->wu.xyz[1].disp.pos = g_state.bg_w.bgw[3].wxy[1].disp.pos + Pos_Data_69[ewk->wu.dir_old][1];
        } else {
            ewk->wu.disp_flag = 1;
            g_state.Order[ewk->wu.dir_old] = 3;
            ewk->wu.routine_no[6] = 0;
            ewk->wu.xyz[0].disp.pos = Pos_Data_69[ewk->wu.dir_old][0] + 512;
            ewk->wu.xyz[1].disp.pos = Pos_Data_69[ewk->wu.dir_old][1] + 0;
        }

        set_char_move_init2(&ewk->wu, 0, ewk->wu.char_index, ewk->wu.dir_step + 1, 0);
        break;

    case 1:
        if (!Ck_Range_Out_S(ewk, ewk->wu.my_family - 1, 32)) {
            ewk->wu.disp_flag = 1;
            ewk->wu.routine_no[6] = 0;
            g_state.Order[ewk->wu.dir_old] = 3;
        }

        break;
    }
}

static void EFF42_SLIDE_IN(State_Other* ewk) {
    if (g_state.Order[ewk->wu.dir_old] != 1) {
        ewk->wu.routine_no[0] = g_state.Order[ewk->wu.dir_old];
        ewk->wu.routine_no[1] = 0;
        return;
    }

    switch (ewk->wu.routine_no[6]) {
    case 0:
        if (--g_state.Order_Timer[ewk->wu.dir_old] != 0) {
            break;
        }

        ewk->wu.routine_no[6]++;
        ewk->wu.disp_flag = 1;
        ewk->wu.hit_quake = Pos_Data_69[ewk->wu.dir_old][0] + 512;
        ewk->wu.xyz[1].disp.pos = Pos_Data_69[ewk->wu.dir_old][1] + 0;

        if (g_state.Order_Dir[ewk->wu.dir_old] == 4) {
            ewk->wu.xyz[0].disp.pos = 800;
            ewk->wu.mvxy.a[0].sp = -0x100000;
            ewk->wu.mvxy.d[0].sp = 0;
        } else {
            ewk->wu.xyz[0].disp.pos = 224;
            ewk->wu.mvxy.a[0].sp = 0x100000;
            ewk->wu.mvxy.d[0].sp = 0x8000;
        }

        set_char_move_init2(&ewk->wu, 0, ewk->wu.char_index, ewk->wu.dir_step + 1, 0);
        break;

    default:
        ewk->wu.xyz[0].cal += ewk->wu.mvxy.a[0].sp;
        ewk->wu.mvxy.a[0].sp += ewk->wu.mvxy.d[0].sp;

        if (0 < ewk->wu.mvxy.a[0].sp) {
            if (ewk->wu.hit_quake <= ewk->wu.xyz[0].disp.pos) {
                g_state.Order[ewk->wu.dir_old] = 3;
                ewk->wu.routine_no[6] = 0;
                ewk->wu.xyz[0].disp.pos = ewk->wu.hit_quake;
            }
        } else if (ewk->wu.hit_quake >= ewk->wu.xyz[0].disp.pos) {
            g_state.Order[ewk->wu.dir_old] = 3;
            ewk->wu.routine_no[6] = 0;
            ewk->wu.xyz[0].disp.pos = ewk->wu.hit_quake;
        }

        break;
    }
}

static void EFF42_SLIDE_OUT(State_Other* ewk) {
    switch (ewk->wu.routine_no[6]) {
    case 0:
        if (--g_state.Order_Timer[ewk->wu.dir_old] != 0) {
            break;
        }

        ewk->wu.routine_no[6]++;

        if (g_state.Order_Dir[ewk->wu.dir_old] == 4) {
            ewk->wu.mvxy.a[0].sp = -0x100000;
            ewk->wu.mvxy.d[0].sp = -0x8000;
        } else {
            ewk->wu.mvxy.a[0].sp = 0x100000;
            ewk->wu.mvxy.d[0].sp = 0x8000;
        }

        break;

    case 1:
        ewk->wu.xyz[0].cal += ewk->wu.mvxy.a[0].sp;
        ewk->wu.mvxy.a[0].sp += ewk->wu.mvxy.d[0].sp;

        if (Ck_Range_Out_S(ewk, 2, 16)) {
            ewk->wu.routine_no[6] += 1;
            ewk->wu.disp_flag = 0;
        }

        break;

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

static void EFF42_KILL(State_Other* ewk) {
    switch (ewk->wu.routine_no[1]) {
    case 0:
        if (--g_state.Order_Timer[ewk->wu.dir_old] == 0) {
            ewk->wu.routine_no[1] += 1;
            ewk->wu.disp_flag = 0;
        }

        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }
}

static void EFF42_MOVE(State_Other* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        if (--ewk->wu.dir_timer == 0) {
            ewk->wu.routine_no[0]++;
            g_state.Time_Stop = 0;
        }

        break;

    case 1:
        if (ewk->wu.active_move != g_state.Select_Timer) {
            ewk->wu.active_move = g_state.Select_Timer;
            Setup_Char_Index(ewk);
            set_char_move_init2(&ewk->wu, 0, ewk->wu.char_index, ewk->wu.dir_step + 1, 0);
        }

        break;
    }
}

static void Setup_Char_Index(State_Other* ewk) {
    s16 xx = g_state.Select_Timer & (s8)ewk->wu.routine_no[7];

    xx &= 0xFF;

    if (ewk->wu.routine_no[7] == 240) {
        xx >>= 4;
    }

    if (ewk->wu.dir_old >= 7) {
        ewk->wu.dir_step = xx + 10;
    } else {
        ewk->wu.dir_step = xx;
    }
}

s32 effect_42_init(s16 type) {
    State_Other* ewk;
    s16 ix;

    if (g_state.Present_Mode == 4 || g_state.Present_Mode == 5) {
        return 0;
    }

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (State_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 42;
    ewk->wu.work_id = 16;
    ewk->wu.my_col_code = 0x2090;
    ewk->wu.my_family = 3;
    ewk->wu.dir_timer = 10;
    ewk->wu.active_move = g_state.Select_Timer;
    *ewk->wu.char_table = _sel_pl_char_table;
    ewk->wu.dir_old = type;
    ewk->wu.my_mts = 13;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_mts);

    if (type & 1) {
        ewk->wu.direction = 4;
    } else {
        ewk->wu.direction = 8;
    }

    ewk->wu.char_index = 3;
    ewk->wu.position_z = 14;

    switch (type) {
    case 5:
        ewk->wu.routine_no[7] = 240;
        ix = g_state.Select_Timer & 0xF0;
        ix >>= 4;
        ewk->wu.dir_step = ix;
        break;

    case 6:
        ewk->wu.routine_no[7] = 15;
        ewk->wu.dir_step = 0;
        break;

    case 7:
        ewk->wu.routine_no[7] = 240;
        ix = g_state.Select_Timer & 0xF0;
        ix >>= 4;
        ewk->wu.dir_step = ix + 10;
        break;

    case 8:
        ewk->wu.routine_no[7] = 15;
        ewk->wu.dir_step = 10;
        break;

    case 9:
        ewk->wu.routine_no[7] = 240;
        ix = g_state.Select_Timer & 0xF0;
        ix >>= 4;
        ewk->wu.dir_step = ix + 10;
        ewk->wu.my_family = 4;
        break;

    case 10:
        ewk->wu.routine_no[7] = 15;
        ewk->wu.dir_step = 10;
        ewk->wu.my_family = 4;
        break;
    }

    return 0;
}

void (*const EFF42_Jmp_Tbl[5])() = { EFF42_SUDDENLY, EFF42_SLIDE_IN, EFF42_SLIDE_OUT, EFF42_MOVE, EFF42_KILL };
