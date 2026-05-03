/**
 * @file eff50.c
 * Effect: Work User / Character State Effect
 */

#include "sf33rd/Source/Game/effect/eff50.h"
#include "game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "port/sdl/rmlui/rmlui_char_select.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/screen/sel_data.h"
#include "sf33rd/Source/Game/system/work_sys.h"

void effect_50_move(WORK_Other* ewk) {
    WORK_Other* pwk;
    u16 sw;

    if (ewk->master_id) {
        sw = p2sw_0 & 3;
    } else {
        sw = p1sw_0 & 3;
    }

    if (g_state.Sel_Arts_Complete[ewk->master_id] < 0) {
        ewk->wu.routine_no[0] = 3;
        ewk->wu.dir_timer = 1;
        return;
    }

    pwk = (WORK_Other*)Synchro_Address[ewk->master_id][(ewk->wu.direction - 1) ^ 1];

    switch (ewk->wu.routine_no[0]) {
    case 0:
        if (g_state.Select_Arts[ewk->master_id] == 0) {
            ewk->wu.routine_no[0]++;
            ewk->wu.disp_flag = 1;
        }

        break;

    case 1:
        if (g_state.Sel_Arts_Complete[ewk->master_id]) {
            ewk->wu.routine_no[0] = 3;
            ewk->wu.dir_timer = 5;
        } else if (g_state.Moving_Plate[ewk->master_id] == ewk->wu.direction && ewk->wu.damage_vitality == 0) {
            ewk->wu.routine_no[0]++;
            ewk->wu.char_index++;
            ewk->wu.damage_calc_multiplier += 3;
            ewk->wu.damage_calc_divider--;
            set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
        }

        if (ewk->wu.damage_vitality == 0) {
            char_move(&ewk->wu);
        }

        break;

    case 2:
        if (ewk->wu.cg_type != 0 && sw != ewk->wu.direction) {
            ewk->wu.routine_no[0] = 1;
            ewk->wu.char_index--;
            set_char_move_init(&ewk->wu, 0, ewk->wu.char_index);
            ewk->wu.graphic_index = pwk->wu.graphic_index - ewk->wu.char_graphic_data_type;
            char_move_z(&ewk->wu);
            ewk->wu.cg_ctr = pwk->wu.cg_ctr;
            ewk->wu.damage_calc_multiplier -= 3;
            ewk->wu.damage_calc_divider++;

            if (ewk->wu.direction != 1) {
                break;
            }
        }

        char_move(&ewk->wu);
        break;

    case 3:
        if (--ewk->wu.dir_timer != 0) {
            break;
        }

        ewk->wu.disp_flag = 0;
        ewk->wu.routine_no[0]++;
        return;

    default:
        Release_Effect(&ewk->wu);
        return;
    }

    ewk->wu.xyz[0].disp.pos = ewk->wu.damage_calc_multiplier + g_state.Plate_X[ewk->master_id][0];
    ewk->wu.xyz[1].disp.pos = ewk->wu.damage_calc_divider + g_state.Plate_Y[ewk->master_id][0];
    ewk->wu.position_x = ewk->wu.xyz[0].disp.pos & 0xFFFF;
    ewk->wu.position_y = ewk->wu.xyz[1].disp.pos & 0xFFFF;
    if (!rmlui_char_select_visible)
        sort_push_request4(&ewk->wu);
}

s32 effect_50_init(s16 PL_id, s16 Direction, s16 damage_vitality) {
    WORK_Other* ewk;
    s16 ix;

    if ((ix = Acquire_Effect(4)) == -1) {
        return -1;
    }

    ewk = (WORK_Other*)frw[ix];
    ewk->wu.be_flag = 1;
    ewk->wu.id = 50;
    ewk->wu.work_id = 16;
    ewk->wu.my_col_code = 0x2090;
    ewk->wu.my_family = 3;
    ewk->master_id = PL_id;
    *ewk->wu.char_table = _sel_pl_char_table;
    ewk->wu.damage_vitality = damage_vitality;
    ewk->wu.direction = Direction;
    ewk->wu.my_mts = 13;
    ewk->wu.my_trans_mode = get_my_trans_mode(ewk->wu.my_mts);

    if (damage_vitality == 0) {
        Synchro_Address[ewk->master_id][ewk->wu.direction - 1] = ewk;
    }

    ewk->wu.xyz[0].disp.pos = Plate_Pos_Data_79[g_state.Play_Type][ewk->master_id][0][0];
    ewk->wu.xyz[1].disp.pos = Plate_Pos_Data_79[g_state.Play_Type][ewk->master_id][0][1];

    if (damage_vitality == 1) {
        ewk->wu.char_index = 31;
        ewk->wu.dir_step = Direction - 1;
    } else {
        ewk->wu.char_index = ((Direction - 1) * 2) + 27;
        ewk->wu.dir_step = 0;
    }

    ewk->wu.damage_calc_multiplier = EFF50_Correct_Data[Direction - 1][damage_vitality][0];
    ewk->wu.damage_calc_divider = EFF50_Correct_Data[Direction - 1][damage_vitality][1];
    ewk->wu.position_z = 30;
    set_char_move_init2(&ewk->wu, 0, ewk->wu.char_index, ewk->wu.dir_step + 1, 0);
    return 0;
}
