/**
 * @file effb5.c
 * Effect: Game State Effect
 */

#include "sf33rd/Source/Game/effect/effect_b5_game_state.h"
#include "bin2obj/char_table.h"
#include "common.h"
#include "game_state.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/rendering/sprite_utilities.h"
#include "sf33rd/Source/Game/screen/name_input.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/target_subroutines.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"

static void current_name_move(State_Other* ewk, NAME_WK* np);

void effect_B5_move(State_Other* ewk) {
    NAME_WK* np = (NAME_WK*)ewk->my_master;

    switch (ewk->wu.routine_no[0]) {
    case 0:
        ewk->wu.routine_no[0]++;
        ewk->wu.hit_stop = 0;
        ewk->wu.old_routine_no[3] = 0;
        ewk->wu.old_routine_no[1] = 47;
        ewk->wu.disp_flag = 1;
        set_char_move_init2(&ewk->wu, 0, 6, np->code[ewk->wu.old_routine_no[2]] + 1, 0);
        /* fallthrough */

    case 1:
    case_1:
        if (np->end_flag[ewk->wu.type]) {
            ewk->wu.disp_flag = 1;
            set_char_move_init2(&ewk->wu, 0, 6, np->code[ewk->wu.old_routine_no[2]] + 1, 0);
            ewk->wu.routine_no[0]++;
        } else {
            current_name_move(ewk, (NAME_WK*)np);
        }

        break;

    case 2:
        if (np->r_no_0 == 7) {
            ewk->wu.routine_no[0] = 3;
            ewk->wu.routine_no[2] = 1;
            ewk->wu.routine_no[3] = 3;
            ewk->wu.routine_no[4] = 0;
            ewk->wu.vitality = 0;
            ewk->wu.vital_new = 66;
            ewk->wu.vital_old = 67;
        }

        if (np->end_flag[ewk->wu.type] == 0) {
            ewk->wu.old_routine_no[0] = 0;
            ewk->wu.old_routine_no[4] = 0;
            ewk->wu.routine_no[0] = 1;
            set_char_move_init2(&ewk->wu, 0, 6, np->code[ewk->wu.old_routine_no[2]] + 1, 0);
            ewk->wu.old_routine_no[3] = np->code[np->index];
            goto case_1;
        }

        if (ewk->wu.old_routine_no[3] != np->code[ewk->wu.type]) {
            ewk->wu.old_routine_no[0] = 0;
            ewk->wu.old_routine_no[4] = 1;
            set_char_move_init2(&ewk->wu, 0, 6, np->code[ewk->wu.old_routine_no[2]] + 1, 0);
            ewk->wu.old_routine_no[3] = np->code[ewk->wu.type];
        }

        break;

    case 3:
        if (Flash_Violent(ewk, 9)) {
            ewk->wu.routine_no[0]++;
        }

        if (ewk->wu.vitality) {
            set_char_move_init2(&ewk->wu, 0, 6, np->code[ewk->wu.old_routine_no[2]] + 1, 0);
        }

        break;

    case 4:
        break;

    default:
        all_cgps_put_back(&ewk->wu);
        Release_Effect(&ewk->wu);
        break;
    }

    if (ewk->wu.old_routine_no[3] != np->code[ewk->wu.type]) {
        ewk->wu.old_routine_no[4] = 0;
        ewk->wu.old_routine_no[0] = 0;
        set_char_move_init2(&ewk->wu, 0, 6, np->code[ewk->wu.type] + 1, 0);
        ewk->wu.old_routine_no[3] = np->code[ewk->wu.type];
    }

    disp_pos_trans_entry(ewk);
}

static void current_name_move(State_Other* ewk, NAME_WK* np) {
    if (np->index != ewk->wu.old_routine_no[2]) {
        return;
    }

    switch (ewk->wu.hit_stop) {
    case 0:
        ewk->wu.hit_stop++;
        ewk->wu.old_routine_no[4] = 0;
        ewk->wu.old_routine_no[0] = 0;
        set_char_move_init2(&ewk->wu, 0, 6, np->code[ewk->wu.old_routine_no[2]] + 1, 0);
        ewk->wu.old_routine_no[3] = np->code[np->index];
        break;

    case 1:
        if (np->r_no_0 > 5) {
            ewk->wu.routine_no[0]++;
            break;
        }

        ewk->wu.old_routine_no[0]++;

        if (ewk->wu.old_routine_no[0] <= 16) {
            break;
        }

        ewk->wu.old_routine_no[0] = 0;
        ewk->wu.old_routine_no[4]++;

        if (ewk->wu.old_routine_no[4] > 2) {
            ewk->wu.old_routine_no[4] = 0;
        }

        if (ewk->wu.old_routine_no[4] != 2) {
            set_char_move_init2(&ewk->wu, 0, 6, np->code[np->index] + 1, 0);
        } else {
            set_char_move_init2(&ewk->wu, 0, 6, 48, 0);
        }

        break;
    }
}
