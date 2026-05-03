/**
 * @file effb8.c
 * Effect: Quake Effect
 */

#include "sf33rd/Source/Game/effect/effect_b8_quake.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/effect/effect_b6_message_debug_text.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/system/country_region.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/stage/bg.h"

static u16 effb8_normal_or_senyou();
static u16 effb8_sel_1_by_8();
static void wk_set(EffectMultiSprite* ewk);

void effect_B8_move(EffectMultiSprite* ewk) {
    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.my_sprite_sheet = 12;
        g_state.mes_timer = g_state.mes_timer - 1;

        if (--g_state.mes_timer > 0) {
            break;
        }

        ewk->wu.routine_no[0]++;
        get_message_conn_data(ewk, 0, ewk->master_player, g_state.mes_already);
        ewk->wu.disp_flag = 1;
        ewk->wu.vitality = 240;
        g_state.mes_timer = 55;
        ewk->wu.mvxy.a[0].sp = -0x100000;
        ewk->wu.mvxy.d[0].sp = 0;
        ewk->wu.hit_quake = g_state.bg_w.bgw[ewk->wu.my_family - 1].wxy[0].disp.pos - 152;
        ewk->wu.mvxy.a[0].sp = -0x100000;
        ewk->wu.mvxy.d[0].sp = 0;
        break;

    case 1:
        if (g_state.Suicide[2] == 1) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0] = 3;
            break;
        }

        ewk->wu.xyz[0].cal += ewk->wu.mvxy.a[0].sp;
        ewk->wu.mvxy.a[0].sp += ewk->wu.mvxy.d[0].sp;

        if (ewk->wu.hit_quake >= ewk->wu.xyz[0].disp.pos) {
            ewk->wu.routine_no[0] = 2;
            ewk->wu.xyz[0].disp.pos = ewk->wu.hit_quake;
        }

        ewk->wu.position_x = ewk->wu.xyz[0].disp.pos;
        ewk->wu.cg_number++;
        ewk->wu.cg_number &= 0x7FFF;
        sort_push_request3(&ewk->wu);
        break;

    case 2:
        if (g_state.Suicide[2] == 1) {
            ewk->wu.disp_flag = 0;
            ewk->wu.routine_no[0]++;
            break;
        }

        sort_push_request3(&ewk->wu);
        break;

    case 3:
        ewk->wu.routine_no[0]++;
        break;

    default:
        Release_Effect(&ewk->wu);
        break;
    }
}

s32 effect_B8_init(s8 WIN_PL_NO, s16 timer) {
    PlayerEntity* wk;
    EffectMultiSprite* ewk;
    s16 ix;
    u16 mes_no;

    g_state.test_in = 0;
    wk = &g_state.plw[WIN_PL_NO];

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (EffectMultiSprite*)frw[ix];
    ewk->wu.routine_no[0] = 0;
    wk_set(ewk);
    g_state.mes_timer = timer;
    ewk->my_master = wk;
    ewk->master_work_id = wk->wu.work_id;
    ewk->master_id = wk->wu.id;
    ewk->master_player = g_state.My_char[wk->wu.id];
    g_state.test_pl_no = g_state.My_char[wk->wu.id];

    if (effb8_normal_or_senyou()) {
        mes_no = g_state.My_char[wk->wu.id ^ 1] + 0;

        if (g_state.old_mes_no_pl == mes_no) {
            mes_no = effb8_sel_1_by_8();
        }

        g_state.old_mes_no_pl = mes_no;
    } else {
        mes_no = effb8_sel_1_by_8();
    }

    g_state.mes_already = mes_no;
    g_state.test_mes_no = mes_no;
    return 0;
}

static u16 effb8_normal_or_senyou() {
    if (g_state.Country != COUNTRY_JAPAN) {
        return 0;
    }

    return random_16() & 1;
}

static u16 effb8_sel_1_by_8() {
    u16 mes_no;

    mes_no = random_16() & 7;
    g_state.old_mes_no2 = g_state.old_mes_no2 & 7;
    g_state.old_mes_no3 = g_state.old_mes_no3 & 7;

    if (g_state.old_mes_no2 == mes_no) {
        mes_no = (mes_no + 1) & 7;
    }

    if (g_state.old_mes_no3 == mes_no) {
        mes_no = (mes_no + 1) & 7;

        if (g_state.old_mes_no2 == mes_no) {
            mes_no = (mes_no + 1) & 7;
        }
    }

    g_state.old_mes_no3 = g_state.old_mes_no2;
    g_state.old_mes_no2 = mes_no;
    mes_no = mes_no + 20;
    return mes_no;
}

static void wk_set(EffectMultiSprite* ewk) {
    ewk->wu.active_flag = 1;
    ewk->wu.id = 118;
    ewk->wu.work_id = 16;
    ewk->wu.facing_flag = 0;
    ewk->wu.graphic_rom_type = 1;
    ewk->wu.sync_bg_strip = 0;
    ewk->wu.my_col_mode = 0x4200;
    ewk->wu.my_col_code = 0x37;
    ewk->wu.my_family = 1;
    ewk->wu.my_priority = 35;
    ewk->wu.position_x = g_state.bg_w.bgw[ewk->wu.my_family - 1].wxy[0].disp.pos + 168;
    ewk->wu.position_y = g_state.bg_w.bgw[ewk->wu.my_family - 1].wxy[1].disp.pos + 15;
    ewk->wu.xyz[0].disp.pos = g_state.bg_w.bgw[ewk->wu.my_family - 1].wxy[0].disp.pos + 168;
    ewk->wu.position_z = 35;
}
