#include "sf33rd/Source/Game/menu/menu_save.h"
#include "game_state.h"
#include "sf33rd/Source/Game/menu/menu_network.h"
#include "sf33rd/Source/Game/menu/menu_replay.h"
#include "sf33rd/Source/Game/menu/menu_training.h"
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
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/animation/appear.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/eff04.h"
#include "sf33rd/Source/Game/effect/eff10.h"
#include "sf33rd/Source/Game/effect/eff18.h"
#include "sf33rd/Source/Game/effect/eff23.h"
#include "sf33rd/Source/Game/effect/eff38.h"
#include "sf33rd/Source/Game/effect/eff39.h"
#include "sf33rd/Source/Game/effect/eff40.h"
#include "sf33rd/Source/Game/effect/eff43.h"
#include "sf33rd/Source/Game/effect/eff45.h"
#include "sf33rd/Source/Game/effect/eff51.h"
#include "sf33rd/Source/Game/effect/eff57.h"
#include "sf33rd/Source/Game/effect/eff58.h"
#include "sf33rd/Source/Game/effect/eff61.h"
#include "sf33rd/Source/Game/effect/eff63.h"
#include "sf33rd/Source/Game/effect/eff64.h"
#include "sf33rd/Source/Game/effect/eff66.h"
#include "sf33rd/Source/Game/effect/eff75.h"
#include "sf33rd/Source/Game/effect/eff91.h"
#include "sf33rd/Source/Game/effect/effa0.h"
#include "sf33rd/Source/Game/effect/effa3.h"
#include "sf33rd/Source/Game/effect/effa8.h"
#include "sf33rd/Source/Game/effect/effc4.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/effk6.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/io/vm_sub.h"
#include "sf33rd/Source/Game/menu/dir_data.h"
#include "sf33rd/Source/Game/menu/ex_data.h"
#include "sf33rd/Source/Game/menu/menu_internal.h"
#include "sf33rd/Source/Game/message/en/msgtable_en.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/rendering/mmtmcnt.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/rendering/texgroup.h"
#include "sf33rd/Source/Game/screen/entry.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/system/pause.h"
#include "sf33rd/Source/Game/system/ramcnt.h"
#include "sf33rd/Source/Game/system/reset.h"
#include "sf33rd/Source/Game/system/saver.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sys_sub2.h"
#include "sf33rd/Source/Game/system/sysdir.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/ui/count.h"
#include "sf33rd/Source/Game/ui/sc_data.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"
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

#include "sf33rd/Source/Game/menu/menu_save_constants.h"
#include "sf33rd/Source/Game/menu/menu_input_constants.h"

void Wait_Load_Save(struct _TASK* task_ptr) {
    s16 ix;

    switch (task_ptr->free[1]) {
    case 0:
        if (g_state.vm_w.Request != 0) {
            break;
        }

        task_ptr->free[0] = 0;
        task_ptr->free[1]++;

        if (task_ptr->r_no[1] == MENU_RNO1_SYS_DIR) {
            task_ptr->free[2] = MENU_RNO1_DIR_MENU;
        } else {
            task_ptr->free[2] = task_ptr->r_no[1];
        }

        Exit_Sub(task_ptr, 2, task_ptr->free[2]);
        break;

    case 1:
        if (!Exit_Sub(task_ptr, 2, task_ptr->free[2])) {
            break;
        }

        task_ptr->free[1]++;
        task_ptr->timer = 1;

        for (ix = 0; ix < 4; ix++) {
            g_state.Menu_Suicide[ix] = 1;
        }

        switch (task_ptr->r_no[1]) {
        case MENU_RNO1_MEM_CARD:
            ix = EFF_SLOT_MEM_CARD;
            break;

        case MENU_RNO1_SAVE_REPLAY:
            task_ptr->r_no[2] = MENU_RNO2_PAUSE_EXIT;
            /* fallthrough */

        case MENU_RNO1_LOAD_REPLAY:
            ix = EFF_SLOT_REPLAY_HDR;
            break;

        case MENU_AT_SAVE_DIRECTION:
        case MENU_AT_LOAD_DIRECTION:
            ix = EFF_SLOT_DIR_HDR;
            break;

        case MENU_RNO1_SYS_SAVE:
            ix = EFF_SLOT_MEM_CARD;
            task_ptr->r_no[0] = 0;
            task_ptr->r_no[2] = MENU_RNO2_PAUSE_EXIT;
            task_ptr->free[0] = 1;
            task_ptr->free[1] = 8;
            break;
        }

        g_state.Order[ix] = 4;
        g_state.Order_Timer[ix] = 1;
        break;

    case 2:
        FadeOut(1, FADE_OPAQUE, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[0] = 0;
        }

        break;
    }
}

void Disp_Auto_Save(struct _TASK* task_ptr) {
    if (task_ptr->r_no[1] >= AUTO_SAVE_JMP_COUNT) {
        return;
    }

    switch (task_ptr->r_no[1]) {
    case AUTO_SAVE_1ST:
        DAS_1st(task_ptr);
        break;
    case AUTO_SAVE_2ND:
        DAS_2nd(task_ptr);
        break;
    case AUTO_SAVE_3RD:
        DAS_3rd(task_ptr);
        break;
    case AUTO_SAVE_4TH:
        DAS_4th(task_ptr);
        break;
    }
}

void DAS_1st(struct _TASK* task_ptr) {
    FadeOut(1, FADE_OPAQUE, 8);
    task_ptr->r_no[1]++;
    task_ptr->timer = 5;
    g_state.Order[EFF_SLOT_HEADER] = 2;
    g_state.Order_Dir[EFF_SLOT_HEADER] = 0;
    g_state.Order_Timer[EFF_SLOT_HEADER] = 1;
    effect_66_init(EFF_SLOT_CURSOR_BG, 8, 0, 0, -1, -1, SAVE_Z_DEPTH_CURSOR);
    g_state.Order[EFF_SLOT_CURSOR_BG] = 3;
    g_state.Order_Timer[EFF_SLOT_CURSOR_BG] = 1;
}

void DAS_2nd(struct _TASK* task_ptr) {
    FadeOut(1, FADE_OPAQUE, 8);

    if ((task_ptr->timer -= 1) == 0) {
        task_ptr->r_no[1]++;
        FadeInit();
        NativeSave_SaveOptions();
    }
}

void DAS_3rd(struct _TASK* task_ptr) {
    if (FadeIn(1, FADE_SPEED_SLOW, 8) != 0) {
        task_ptr->r_no[1]++;
    }
}

void DAS_4th(struct _TASK* task_ptr) {
    /* NativeSave_SaveOptions() is synchronous, so always proceed */
    task_ptr->r_no[0] = 0;
    task_ptr->r_no[1] = 1;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    g_state.Forbid_Reset = 0;
}

void Disp_Auto_Save2(struct _TASK* task_ptr) {
    if (task_ptr->r_no[1] >= AUTO_SAVE_JMP_COUNT) {
        return;
    }

    switch (task_ptr->r_no[1]) {
    case AUTO_SAVE_1ST:
        DAS_1st(task_ptr);
        break;
    case AUTO_SAVE_2ND:
        DAS_2nd(task_ptr);
        break;
    case AUTO_SAVE_3RD:
        DAS_3rd(task_ptr);
        break;
    case AUTO_SAVE_4TH:
        DAS2_4th(task_ptr);
        break;
    }
}

void DAS2_4th(struct _TASK* task_ptr) {
    /* NativeSave_SaveOptions() is synchronous, so always proceed */
    g_state.fsm[2] = GAME_SUBMODE_SAVE;
    cpExitTask(TASK_MENU);
    Task_Activate(TASK_ENTRY);
}
