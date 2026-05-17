#include "sf33rd/Source/Game/menu/menu_training.h"
#include "game_state.h"
#include "sf33rd/Source/Game/menu/menu_network.h"
#include "sf33rd/Source/Game/menu/menu_save.h"
#include "sf33rd/Source/Game/menu/menu_replay.h"
#include "port/init_task.h"
#include "port/menu_task.h"
#include "port/task_api.h"
#include "port/menu_screen.h"
#include "common.h"
#include "main.h"
#include "netplay/discovery.h"
#include "netplay/netplay.h"
#include "netplay/ping_probe.h"
#include "port/config/config.h"
#include "port/rendering/renderer.h"
#include "port/save/native_save.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/input/controller_image_overlay.h"
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "port/input.h"
#include "sf33rd/Source/Game/animation/appear.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect_04_projectile_object.h"
#include "sf33rd/Source/Game/effect/effect_10_ui_screen_check_data.h"
#include "sf33rd/Source/Game/effect/effect_18_visual_misc.h"
#include "sf33rd/Source/Game/effect/effect_23_quake.h"
#include "sf33rd/Source/Game/effect/effect_38_quake_base_xy.h"
#include "sf33rd/Source/Game/effect/effect_39_quake.h"
#include "sf33rd/Source/Game/effect/effect_40_position_data.h"
#include "sf33rd/Source/Game/effect/effect_43_game_state.h"
#include "sf33rd/Source/Game/effect/effect_45_debug_game_state.h"
#include "sf33rd/Source/Game/effect/effect_51_brief_system_direction_menu_selected_value_label_renderer.h"
#include "sf33rd/Source/Game/effect/effect_57_header_for_menus.h"
#include "sf33rd/Source/Game/effect/effect_58_sound_se_request.h"
#include "sf33rd/Source/Game/effect/effect_61_menu_options.h"
#include "sf33rd/Source/Game/effect/effect_63_quake.h"
#include "sf33rd/Source/Game/effect/effect_64_quake.h"
#include "sf33rd/Source/Game/effect/effect_66_quake_half_object_flash.h"
#include "sf33rd/Source/Game/effect/effect_75_quake_link.h"
#include "sf33rd/Source/Game/effect/effect_91_position_data.h"
#include "sf33rd/Source/Game/effect/effect_a0_position_data.h"
#include "sf33rd/Source/Game/effect/effect_a3_content_check_system.h"
#include "sf33rd/Source/Game/effect/effect_a8_position_data.h"
#include "sf33rd/Source/Game/effect/effect_c4_menu_ex_data.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effect_k6_quake.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/player_system_utilities.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/io/afs_loader.h"
#include "sf33rd/Source/Game/io/rumble.h"
#include "sf33rd/Source/Game/io/save_file_ops.h"
#include "sf33rd/Source/Game/menu/director_data.h"
#include "sf33rd/Source/Game/menu/extra_data.h"
#include "sf33rd/Source/Game/menu/menu_internal.h"
#include "sf33rd/Source/Game/message/en/msgtable_en.h"
#include "sf33rd/Source/Game/rendering/color_palette.h"
#include "sf33rd/Source/Game/rendering/memory_texture_control.h"
#include "sf33rd/Source/Game/rendering/rendering_transform.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/rendering/texture_group.h"
#include "sf33rd/Source/Game/screen/entry.h"
#include "sf33rd/Source/Game/sound/sound_effects.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"
#include "sf33rd/Source/Game/system/pause.h"
#include "sf33rd/Source/Game/system/ram_control.h"
#include "sf33rd/Source/Game/system/reset.h"
#include "sf33rd/Source/Game/system/save_manager.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
#include "sf33rd/Source/Game/system/system_subroutines_2.h"
#include "sf33rd/Source/Game/system/system_director.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/round_timer.h"
#include "sf33rd/Source/Game/ui/hud_data.h"
#include "sf33rd/Source/Game/ui/hud_subroutines.h"
#include "structs.h"
#include "port/sdl/rmlui/rmlui_button_config.h"
#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include "port/sdl/rmlui/rmlui_char_select.h"
#include "port/sdl/rmlui/rmlui_exit_confirm.h"
#include "port/sdl/rmlui/rmlui_extra_option.h"
#include "port/sdl/rmlui/rmlui_game_option.h"
#include "port/sdl/rmlui/rmlui_memory_card.h"
#include "port/sdl/rmlui/rmlui_mode_menu.h"
#include "port/sdl/rmlui/rmlui_leaderboard.h"
#include "port/sdl/rmlui/rmlui_network_lobby.h"
#include "port/sdl/rmlui/rmlui_option_menu.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui/rmlui_replay_picker.h"
#include "port/sdl/rmlui/rmlui_sound_menu.h"
#include "port/sdl/rmlui/rmlui_sysdir.h"
#include "port/sdl/rmlui/rmlui_training_menus.h"
#include "port/sdl/rmlui/rmlui_vs_result.h"
#include "port/sdl/rmlui/rmlui_vs_screen.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include "sf33rd/Source/Game/menu/menu_training_constants.h"
#include "sf33rd/Source/Game/menu/menu_input_constants.h"

void Wait_Pause_in_Tr(struct _TASK* task_ptr) {
    u16 ans;
    u16 ix;

    Training_Data_Disp();
    Control_Player_Tr();

    if (g_state.End_Training) {
        Next_Be_Tr_Menu(task_ptr);
        return;
    }

    switch (task_ptr->r_no[1]) {
    case 0:
        if (g_state.Allow_a_battle_f) {
            task_ptr->r_no[1]++;

            if (g_state.Present_Mode == 4) {
                g_state.Disp_Attack_Data = Training->contents[0][1][1];
            } else {
                g_state.Disp_Attack_Data = 0;
            }
        } else {
            g_state.Disp_Attack_Data = 0;
        }

        /* fallthrough */

    case 1:
        if (g_state.Allow_a_battle_f == 0 || g_state.Extra_Break != 0) {
            return;
        }

        ans = 0;

        if (Check_Pause_Term_Tr(0)) {
            ans = Pause_Check_Tr(0);
        }

        if (ans == 0 && Check_Pause_Term_Tr(1)) {
            ans = Pause_Check_Tr(1);
        }

        switch (ans) {
        case 1:
            Setup_Tr_Pause(task_ptr);
            break;

        case 2:
            Setup_Tr_Pause(task_ptr);
            task_ptr->r_no[1] = 3;
            break;
        }

        break;

    case 2:
        if (Interface_Type[g_state.Pause_ID] == 0) {
            Setup_Tr_Pause(task_ptr);
            task_ptr->r_no[1] = 3;
            break;
        }

        if (g_state.Pause_Down) {
            Flash_1P_or_2P(task_ptr);
        }

        switch (Pause_in_Normal_Tr(task_ptr)) {
        case 1:
            task_ptr->r_no[1] = 0;
            SE_selected();
            g_state.Game_pause = 0;
            g_state.Pause = 0;
            g_state.Pause_Down = 0;
            g_state.Disp_Attack_Data = Training->contents[0][1][1];

            for (ix = 0; ix < 4; ix++) {
                g_state.Menu_Suicide[ix] = 1;
            }

            pulpul_request_again();
            SsBgmHalfVolume(0);
            break;

        case 2:
            Next_Be_Tr_Menu(task_ptr);
            break;
        }

        break;

    case 3:
        if (Interface_Type[g_state.Pause_ID] == 0) {
            dispControllerWasRemovedMessage(
                TRAINING_CTRL_REMOVED_MSG_X, TRAINING_CTRL_REMOVED_MSG_Y, TRAINING_CTRL_REMOVED_MSG_COLOR);
            break;
        }

        Setup_Tr_Pause(task_ptr);
        break;
    }
}

void Reset_Training(struct _TASK* task_ptr) {
    s16 ix;

    switch (task_ptr->r_no[1]) {
    case 0:
        task_ptr->r_no[1]++;
        task_ptr->timer = TRAINING_CHARACTER_CHANGE_TIMER;
        g_state.Game_pause = GAME_PAUSE_ACTIVE;
        break;

    case 1:
        if (--task_ptr->timer != 0) {
            break;
        }

        if (Check_LDREQ_Break() == 0) {
            task_ptr->r_no[1]++;
            Switch_Screen_Init(0);
            break;
        }

        task_ptr->timer = 1;
        break;

    case 2:
        if (!Switch_Screen(0)) {
            break;
        }

        task_ptr->r_no[1]++;
        task_ptr->timer = TRAINING_RESET_WAIT_TIMER;
        effect_work_kill(6, -1);
        move_effect_work(6);

        for (ix = 0; ix < 4; ix++) {
            g_state.manage_phase[ix] = 0;
        }

        g_state.manage_phase[0] = 1;
        g_state.fsm[2] = GAME_SUBMODE_TRAINING;
        g_state.fsm[3] = 0;
        g_state.seraph_flag = 0;
        g_state.BGmessage_phase[0] = 1;
        g_state.BGM_Timer[0] = 1;
        g_state.fsm_timer = TRAINING_RESET_G_TIMER;
        g_state.Cover_Timer = TRAINING_RESET_COVER_TIMER;
        g_state.Suicide[0] = 1;
        g_state.Suicide[6] = 1;
        g_state.judge_flag = 0;
        g_state.Lever_LR[0] = 0;
        g_state.Lever_LR[1] = 0;
        break;

    default:
        Switch_Screen(0);

        if (--task_ptr->timer != 0) {
            break;
        }

        for (ix = 0; ix < 4; ix++) {
            task_ptr->r_no[ix] = 0;
        }

        task_ptr->r_no[0] = 7;
        break;
    }
}

void Training_Menu(struct _TASK* task_ptr) {
    if (task_ptr->r_no[1] >= TRAINING_JMP_COUNT) {
        return;
    }

    /* ── MenuScreen registry integration hook (Task 18) ──
     * If the registry is already driving a training sub-screen, tick it.
     * Otherwise, try to map the legacy r_no[1] index to a MenuScreenId;
     * if a migrated (and enabled) screen is found, hand off to the
     * registry.  Un-migrated indices fall through to the legacy table.
     * CRITICAL: Akaobi/ToneDown/SSPutStr_Bigger MUST run after BOTH paths. */
    if (MenuScreen_IsTrainingActive()) {
        MenuScreen_TrainingTick(task_ptr);
    } else {
        MenuScreenId mapped = MenuScreen_FromTrainingIndex(task_ptr->r_no[1]);
        if (mapped != MENU_SCREEN_NONE) {
            MenuScreen_Goto(mapped);
            MenuScreen_TrainingTick(task_ptr);
        } else {
            /* Legacy dispatch (un-migrated training screens only).
             * All indices 1–7 are intercepted by MenuScreen_FromTrainingIndex()
             * above.  Only index 0 (Training_Init) is still dispatched here. */
            switch (task_ptr->r_no[1]) {
            case 0:
                Training_Init(task_ptr);
                break;
            default:
                break;
            }
        }
    }

    /* Post-dispatch rendering — runs after BOTH registry and legacy paths */
    Akaobi();
    ToneDown(TRAINING_TONE_NORMAL, 2);

    if ((!use_rmlui || !rmlui_menu_training) && g_state.Training_Index < TRAINING_LETTER_COUNT) {
        SSPutStr_Bigger(training_letter_data[g_state.Training_Index].pos_x,
                        TRAINING_HEADER_POS_Y,
                        9,
                        training_letter_data[g_state.Training_Index].menu,
                        1,
                        2,
                        1);
    }
}

void Training_Init(struct _TASK* task_ptr) {
    ToneDown(TRAINING_TONE_DARK, 2);
    Menu_Init(task_ptr);
    task_ptr->r_no[1] = g_state.Mode_Type - 2;
    g_state.Pause_Down = 1;
    g_state.End_Training = 0;
    g_state.Demo_Time_Stop = 0;
    g_state.Disp_Cockpit = 0;

    if (g_state.Mode_Type == MODE_NORMAL_TRAINING) {
        control_player = g_state.Champion;
        control_pl_rno = TRAINING_CONTROL_NONE;
    } else {
        control_player = g_state.Champion;
        control_pl_rno = 0;
    }

    g_state.Round_num = 0;
    g_state.PL_Wins[0] = 0;
    g_state.PL_Wins[1] = 0;
    g_state.Play_Mode = 0;
    g_state.Replay_Status[0] = 0;
    g_state.Replay_Status[1] = 0;
}

void Normal_Training(struct _TASK* task_ptr) {
    s16 ix;
    s16 x;
    s16 y;

    s16 s2;

    g_state.Menu_Cursor_Y[1] = g_state.Menu_Cursor_Y[0];

    switch (task_ptr->r_no[2]) {
    case 0:
        Training_Init_Sub(task_ptr);
        g_state.Training_Index = 0;
        x = TRAINING_NORMAL_START_X;
        y = TRAINING_NORMAL_START_Y;
        Training[0] = Training[2];

        for (ix = 0; ix < 8; ix++, s2 = y += TRAINING_SPACING_Y) {
            (void)s2;

            effect_A3_init(0, 0, ix, ix, 0, x, y, 0);
        }

        break;

    case 1:
        if (g_state.Appear_end < 2) {
            break;
        }

        if (g_state.Exec_Wipe) {
            break;
        }

        MC_Move_Sub(Check_Menu_Lever(g_state.Decide_ID, 0), 0, 7, TRAINING_RESULT_MASK);
        Check_Skip_Recording();
        Check_Skip_Replay(2);

        switch (g_state.IO_Result) {
        case SWK_SOUTH:
            switch (g_state.Menu_Cursor_Y[0]) {
            case 0:
            case 1:
            case 2:
                if (Interface_Type[g_state.Champion ^ 1] == 0 && Training[2].contents[0][0][0] == 4) {
                    Training[2].contents[0][0][0] = 0;
                }

                task_ptr->r_no[0] = 10;
                task_ptr->r_no[1] = 0;
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                g_state.Menu_Suicide[0] = 1;
                g_state.Game_pause = 0;
                g_state.Pause_Down = 0;
                Training_Disp_Work_Clear();
                g_state.CP_No[0][0] = 0;
                g_state.CP_No[1][0] = 0;
                g_state.plw[g_state.New_Challenger].wu.pl_operator = 1;
                g_state.Operator_Status[g_state.New_Challenger] = 1;
                Setup_NTr_Data(g_state.Menu_Cursor_Y[0]);
                count_cont_init(0);

                switch (Training[0].contents[0][0][0]) {
                case 0:
                    control_pl_rno = 0;
                    control_player = g_state.New_Challenger;
                    break;

                case 1:
                    control_pl_rno = 1;
                    control_player = g_state.New_Challenger;
                    break;

                case 2:
                    control_pl_rno = 2;
                    control_player = g_state.New_Challenger;
                    break;

                case 3:
                    control_pl_rno = 99;
                    g_state.plw[g_state.New_Challenger].wu.pl_operator = 0;
                    g_state.Operator_Status[g_state.New_Challenger] = 0;
                    break;

                case 4:
                    control_pl_rno = 99;
                    break;
                }

                All_Clear_Timer();
                Check_Replay();
                Training[0].contents[0][1][3] = g_state.Menu_Cursor_Y[0];
                init_omop();
                set_init_A4_flag();
                setup_vitality(&g_state.plw[0].wu, g_state.My_char[0] + 0);
                setup_vitality(&g_state.plw[1].wu, g_state.My_char[1] + 0);
                Setup_Training_Difficulty();
                g_state.Training_Cursor = g_state.Menu_Cursor_Y[0];
                break;

            case 3:
            case 4:
            case 5:
            case 6:
                task_ptr->r_no[1] = g_state.Menu_Cursor_Y[0];
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                g_state.Training_Cursor = g_state.Menu_Cursor_Y[0];
                break;

            case 7:
                g_state.Training_Cursor = 7;
                Training_Exit_Sub(task_ptr);
            }

            SsBgmHalfVolume(0);
            SE_selected();
        }

        break;

    case 2:
        Yes_No_Cursor_Exit_Training(task_ptr, 7);
        break;

    default:
        Exit_Sub(task_ptr, 0, g_state.Menu_Cursor_Y[0] + 1);
        break;
    }
}

void Dummy_Setting(struct _TASK* task_ptr) {
    s16 ix;
    s16 group;
    s16 y;

    s16 s6;
    s16 s5;
    s16 s4;
    s16 s3;

    switch (task_ptr->r_no[2]) {
    case 0:
        task_ptr->r_no[2]++;
        Menu_Common_Init();
        g_state.Menu_Cursor_Y[0] = 0;
        g_state.Menu_Cursor_Y[1] = 0;
        g_state.Menu_Suicide[0] = 1;
        g_state.Training_Index = 2;

        for (ix = 0, s6 = y = TRAINING_DUMMY_SETTING_START_Y; ix < 7; ix++, s5 = y += TRAINING_SPACING_Y) {
            effect_A3_init(0, 1, ix, ix, 1, TRAINING_DUMMY_SETTING_LBL_X, y, 0);
        }

        for (ix = 0, y = TRAINING_DUMMY_SETTING_START_Y, s4 = group = 2; ix < 5;
             ix++, group++, s3 = y += TRAINING_SPACING_Y) {
            effect_A3_init(0, group, ix, ix, 1, TRAINING_DUMMY_SETTING_X, y, 0);
        }

        break;

    case 1:
        Dummy_Move_Sub(task_ptr, g_state.Champion, 0, 0, 6);

        if (g_state.Menu_Cursor_Y[0] == 5 && (g_state.IO_Result & SWK_SOUTH)) {
            Training[2].contents[0][0][0] = 0;
            Training[2].contents[0][0][1] = 0;
            Training[2].contents[0][0][2] = 0;
            Training[2].contents[0][0][3] = 0;
            Training[2].contents[0][0][4] = 0;
            SE_selected();
        }

        break;

    case 2:
        SE_selected();
        g_state.Menu_Suicide[0] = 0;
        g_state.Menu_Suicide[1] = 1;
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        Training_Disp_Sub(task_ptr);
        break;
    }
}

void Training_Option(struct _TASK* task_ptr) {
    s16 ix;
    s16 group;
    s16 y;

    s16 s6;
    s16 s5;
    s16 s4;
    s16 s3;

    switch (task_ptr->r_no[2]) {
    case 0:
        task_ptr->r_no[2]++;
        Menu_Common_Init();
        g_state.Menu_Cursor_Y[0] = 0;
        g_state.Menu_Cursor_Y[1] = 0;
        g_state.Menu_Suicide[0] = 1;
        g_state.Training_Index = 3;

        for (ix = 0, s6 = y = TRAINING_OPTION_START_Y; ix < 6; ix++, s5 = y += TRAINING_SPACING_Y) {
            effect_A3_init(0, 9, ix, ix, 1, TRAINING_OPTION_LBL_X, y, 1);
        }

        for (ix = 0, y = TRAINING_OPTION_START_Y, s4 = group = 10; ix < 4;
             ix++, group++, s3 = y += TRAINING_SPACING_Y) {
            effect_A3_init(0, group, ix, ix, 1, TRAINING_OPTION_VAL_X, y, 1);
        }

        break;

    case 1:
        Dummy_Move_Sub(task_ptr, g_state.Champion, 0, 1, 5);

        if (g_state.Menu_Cursor_Y[0] == 4 && (g_state.IO_Result & SWK_SOUTH)) {
            Default_Training_Option();
            SE_selected();
            break;
        }

        CurrentSave()->Damage_Level = Training[2].contents[0][1][2];
        CurrentSave()->Difficulty = Training[2].contents[0][1][3];
        break;

    case 2:
        SE_selected();
        g_state.Menu_Suicide[0] = 0;
        g_state.Menu_Suicide[1] = 1;
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        Training_Disp_Sub(task_ptr);
        Training[0] = Training[2];
        break;
    }
}

void Blocking_Training(struct _TASK* task_ptr) {
    s16 ix;
    s16 x;
    s16 y;
    s16 s2;

    g_state.Menu_Cursor_Y[1] = g_state.Menu_Cursor_Y[0];

    switch (task_ptr->r_no[2]) {
    case 0:
        Training_Init_Sub(task_ptr);
        g_state.Training_Index = 1;
        x = TRAINING_BLOCKING_START_X;
        y = TRAINING_BLOCKING_START_Y;
        g_state.plw[0].wu.pl_operator = 1;
        g_state.Operator_Status[0] = 1;
        g_state.plw[1].wu.pl_operator = 1;
        g_state.Operator_Status[1] = 1;

        for (ix = 0; ix < 6; ix++, s2 = y += TRAINING_SPACING_Y) {
            (void)s2;

            effect_A3_init(1, 14, ix, ix, 0, x, y, 0);
        }

        break;

    case 1:
        if (g_state.Appear_end < 2) {
            break;
        }

        if (g_state.Exec_Wipe) {
            break;
        }

        MC_Move_Sub(Check_Menu_Lever(g_state.Decide_ID, 0), 0, 5, TRAINING_RESULT_MASK);
        Check_Skip_Replay(1);

        switch (g_state.IO_Result) {
        case SWK_SOUTH:
            switch (g_state.Menu_Cursor_Y[0]) {
            case 0:
                g_state.Record_Data_Tr = 1;
                Training[0] = Training[2];
                Training[0].contents[1][0][2] = 1;
                Training[1] = Training[2];

                switch (Training[0].contents[1][0][0]) {
                case 0:
                    control_pl_rno = 0;
                    break;

                case 1:
                    control_pl_rno = 1;
                    break;

                case 2:
                    control_pl_rno = 2;
                    break;
                }

                /* fallthrough */

            case 1:
                if (g_state.Menu_Cursor_Y[0] == 0) {
                    g_state.Play_Mode = 1;
                } else {
                    g_state.Play_Mode = 3;
                }

                All_Clear_Timer();
                Check_Replay();

                if (g_state.Menu_Cursor_Y[0] == 1) {
                    g_state.Replay_Status[g_state.Training_ID] = 0;
                    g_state.Replay_Status[g_state.Training_ID ^ 1] = 3;
                    Training[0] = Training[1];
                    Training[0].contents[1][0][2] = Training[2].contents[1][0][2];
                    Training[0].contents[1][0][3] = Training[2].contents[1][0][3];
                    control_pl_rno = 99;
                }

                task_ptr->r_no[0] = 10;
                task_ptr->r_no[1] = 0;
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                g_state.Menu_Suicide[0] = 1;
                g_state.Game_pause = 0;
                g_state.Pause_Down = 0;
                CurrentSave()->Time_Limit = 60;
                count_cont_init(0);
                Training[0].contents[1][1][3] = g_state.Menu_Cursor_Y[0];
                init_omop();
                set_init_A4_flag();
                g_state.Training_Cursor = g_state.Menu_Cursor_Y[0];
                break;

            case 2:
                task_ptr->r_no[1] = 7;
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                g_state.Training_Cursor = 2;
                break;

            case 3:
                g_state.Training_Cursor = 3;
                /* fallthrough */

            case 4:
                task_ptr->r_no[1] = g_state.Menu_Cursor_Y[0] + 2;
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                break;

            case 5:
                g_state.Training_Cursor = 5;
                Training_Exit_Sub(task_ptr);
                break;
            }

            SsBgmHalfVolume(0);
            SE_selected();
            break;
        }

        break;

    case 2:
        Yes_No_Cursor_Exit_Training(task_ptr, 5);
        break;

    default:
        Exit_Sub(task_ptr, 0, g_state.Menu_Cursor_Y[0] + 1);
        break;
    }
}

void Blocking_Tr_Option(struct _TASK* task_ptr) {
    s16 ix;
    s16 group;
    s16 y;

    s16 s6;
    s16 s5;
    s16 s4;
    s16 s3;

    switch (task_ptr->r_no[2]) {
    case 0:
        task_ptr->r_no[2]++;
        Menu_Common_Init();
        g_state.Menu_Cursor_Y[0] = 0;
        g_state.Menu_Cursor_Y[1] = 0;
        g_state.Menu_Suicide[0] = 1;
        g_state.Training_Index = 3;
        effect_A3_init(1, 24, 99, 0, 1, TRAINING_BLOCKING_OPT_HDR_X, TRAINING_BLOCKING_OPT_HDR_1_Y, 1);
        effect_A3_init(1, 24, 99, 1, 1, TRAINING_BLOCKING_OPT_HDR_X, TRAINING_BLOCKING_OPT_HDR_2_Y, 1);

        for (ix = 0, s6 = y = TRAINING_OPTION_START_Y; ix < 6; ix++, s5 = y += TRAINING_SPACING_Y) {
            if (ix == 2) {
                y += 20;
            }

            if (ix == 4) {
                y += 8;
            }

            effect_A3_init(1, 19, ix, ix, 1, TRAINING_BLOCKING_OPT_LBL_X, y, 0);
        }

        for (ix = 0, y = TRAINING_OPTION_START_Y, s4 = group = 18; ix < 4;
             ix++, group++, s3 = y += TRAINING_SPACING_Y) {
            if (ix == 2) {
                y += 20;
            }

            effect_A3_init(1, group + 2, ix, ix, 1, TRAINING_BLOCKING_OPT_VAL_X, y, 0);
        }

        break;

    case 1:
        Dummy_Move_Sub(task_ptr, g_state.Champion, 1, 0, 5);

        if (g_state.Menu_Cursor_Y[0] == 4 && (g_state.IO_Result & SWK_SOUTH)) {
            Default_Training_Data(1);
            SE_selected();
        }

        break;

    case 2:
        SE_selected();
        g_state.Menu_Suicide[0] = 0;
        g_state.Menu_Suicide[1] = 1;
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        Training[0] = Training[2];

        g_state.plw[g_state.New_Challenger].wu.pl_operator = 1;
        g_state.Operator_Status[g_state.New_Challenger] = 1;

        switch (Training[0].contents[1][0][0]) {
        case 0:
            control_pl_rno = 0;
            control_player = g_state.Champion;
            break;
        case 1:
            control_pl_rno = 1;
            control_player = g_state.Champion;
            break;
        case 2:
            control_pl_rno = 2;
            control_player = g_state.Champion;
            break;
        }

        Training_Disp_Sub(task_ptr);
        break;
    }
}

void Character_Change(struct _TASK* task_ptr) {
    s16 ix;

    if (Check_Pad_in_Pause(task_ptr) == 0) {
        switch (task_ptr->r_no[2]) {
        case 0:
            task_ptr->r_no[2]++;
            task_ptr->timer = TRAINING_CHARACTER_CHANGE_TIMER;
            g_state.Game_pause = GAME_PAUSE_ACTIVE;
            break;

        case 1:
            if ((task_ptr->timer -= 1) == 0) {
                if ((Check_LDREQ_Break() == 0)) {
                    task_ptr->r_no[2]++;
                    Switch_Screen_Init(0);
                    return;
                }

                task_ptr->timer = 1;
                return;
            }
            break;

        case 2:
            if (Switch_Screen(0) != 0) {
                task_ptr->r_no[2]++;
                g_state.Cover_Timer = TRAINING_COVER_TIMER;
                g_state.fsm[1] = GAME_MODE_IN_GAME;
                g_state.fsm[2] = 0;
                g_state.fsm[3] = 0;

                for (ix = 0; ix < 2; ix++) {
                    g_state.Sel_PL_Complete[ix] = 0;
                    g_state.Sel_Arts_Complete[ix] = 0;
                    g_state.plw[ix].wu.pl_operator = 1;
                    g_state.Operator_Status[ix] = 1;
                }

                cpExitTask(TASK_MENU);
            }
            break;
        }
    }
}

void Default_Training_Data(s32 flag) {
    s16 ix;
    s16 ix2;
    s16 ix3;

    if (flag == 0) {
        if (!mpp_w.initTrainingData) {
            return;
        }

        mpp_w.initTrainingData = false;
    }

    for (ix = 0; ix < 2; ix++) {
        for (ix2 = 0; ix2 < 2; ix2++) {
            for (ix3 = 0; ix3 < 8; ix3++) {
                Training[0].contents[ix][ix2][ix3] = 0;
            }
        }
    }

    Training[0].contents[0][1][2] = save_w->Damage_Level;
    Training[0].contents[0][1][3] = save_w->Difficulty;
    CurrentSave()->Damage_Level = save_w->Damage_Level;
    CurrentSave()->Difficulty = save_w->Difficulty;
    Training[2] = Training[0];
    g_state.Disp_Attack_Data = 0;
}
