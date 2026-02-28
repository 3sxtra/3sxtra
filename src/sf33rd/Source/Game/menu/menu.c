/**
 * @file menu.c
 * @brief Game menus â€” mode select, options, training, replays, and VS results.
 *
 * Contains the full menu state machine driven by `Menu_Task()` and its
 * 14-entry jump table: title screen, mode select, game options, button
 * config, screen adjust, sound test, save/load, system direction,
 * extra options, training sub-menus, replay save/load, and VS results.
 *
 * Part of the menu module.
 */

#include "sf33rd/Source/Game/menu/menu.h"
#include "common.h"
#include "main.h"
#include "netplay/discovery.h"
#include "netplay/netplay.h"
#include "port/config.h"
#include "port/native_save.h"
#include "port/renderer.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_netplay_ui.h"
#include "port/ui/replay_picker.h"
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

/* RmlUi Phase 3 bypass — per-component toggles + menu replacements */
#include "port/sdl/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui_option_menu.h"
#include "port/sdl/rmlui_game_option.h"
#include "port/sdl/rmlui_vs_result.h"
#include "port/sdl/rmlui_memory_card.h"
#include "port/sdl/rmlui_sound_menu.h"
#include "port/sdl/rmlui_extra_option.h"
#include "port/sdl/rmlui_training_menus.h"
#include "port/sdl/rmlui_sysdir.h"
#include "port/sdl/rmlui_network_lobby.h"
#include "port/sdl/rmlui_button_config.h"
#include "port/sdl/rmlui_char_select.h"
#include "port/sdl/rmlui_vs_screen.h"
#include "port/sdl/rmlui_replay_picker.h"
#include "port/sdl/rmlui_mode_menu.h"
extern bool use_rmlui;

// forward decls
static void After_Title(struct _TASK* task_ptr);
static void In_Game(struct _TASK* task_ptr);
static void Wait_Load_Save(struct _TASK* task_ptr);
static void Wait_Replay_Check(struct _TASK* task_ptr);
static void Disp_Auto_Save(struct _TASK* task_ptr);
static void Suspend_Menu();
static void Wait_Replay_Load();
static void Training_Menu(struct _TASK* task_ptr);
static void After_Replay(struct _TASK* task_ptr);
static void Disp_Auto_Save2(struct _TASK* task_ptr);
static void Wait_Pause_in_Tr(struct _TASK* task_ptr);
static void Reset_Training(struct _TASK* task_ptr);
static void Reset_Replay(struct _TASK* task_ptr);
static void End_Replay_Menu(struct _TASK* task_ptr);
static void Mode_Select(struct _TASK* task_ptr);
static void Option_Select(struct _TASK* task_ptr);
static void Training_Mode(struct _TASK* task_ptr);
static void System_Direction(struct _TASK* task_ptr);
static void Load_Replay(struct _TASK* task_ptr);
static void toSelectGame(struct _TASK* task_ptr);
static void Game_Option(struct _TASK* task_ptr);
static void Button_Config(struct _TASK* task_ptr);
static void Sound_Test(struct _TASK* task_ptr);
static void Memory_Card(struct _TASK* task_ptr);
static void Extra_Option(struct _TASK* task_ptr);
static void VS_Result(struct _TASK* task_ptr);
static void Save_Replay(struct _TASK* task_ptr);
static void Direction_Menu(struct _TASK* task_ptr);
static void Setup_VS_Mode(struct _TASK* task_ptr);
static void Network_Lobby(struct _TASK* task_ptr);

static void bg_etc_write_ex(s16 type);
void jmpRebootProgram();

static void Menu_in_Sub(struct _TASK* task_ptr);
s32 Exit_Sub(struct _TASK* task_ptr, s16 cursor_ix, s16 next_routine);
s32 Menu_Sub_case1(struct _TASK* task_ptr);
static void DAS_1st(struct _TASK* task_ptr);
static void DAS_2nd(struct _TASK* task_ptr);
static void DAS_3rd(struct _TASK* task_ptr);
static void DAS_4th(struct _TASK* task_ptr);
static void DAS2_4th(struct _TASK* task_ptr);
static void Training_Init(struct _TASK* task_ptr);
static void Character_Change(struct _TASK* task_ptr);
static void Normal_Training(struct _TASK* task_ptr);
static void Blocking_Training(struct _TASK* task_ptr);
static void Dummy_Setting(struct _TASK* task_ptr);
static void Training_Option(struct _TASK* task_ptr);
static void Blocking_Tr_Option(struct _TASK* task_ptr);

const MenuFunc Menu_Jmp_Tbl[MENU_JMP_COUNT] = {
    After_Title,   In_Game,      Wait_Load_Save,  Wait_Replay_Check, Disp_Auto_Save, Suspend_Menu, Wait_Replay_Load,
    Training_Menu, After_Replay, Disp_Auto_Save2, Wait_Pause_in_Tr,  Reset_Training, Reset_Replay, End_Replay_Menu,
};

// sbss
u8 r_no_plus;
u8 control_player;
u8 control_pl_rno;

// rodata

/** @brief Top-level menu task â€” pad setup, then dispatch via jump table. */
void Menu_Task(struct _TASK* task_ptr) {
    if (nowSoftReset()) {
        return;
    }

    if (Interface_Type[0] == 0 || Interface_Type[1] == 0) {
        Connect_Status = 0;
    } else {
        Connect_Status = 1;
    }

    Setup_Pad_or_Stick();
    IO_Result = 0;

    if (task_ptr->r_no[0] >= MENU_JMP_COUNT) {
        return;
    }

    Menu_Jmp_Tbl[task_ptr->r_no[0]](task_ptr);
}

/** @brief Read controller type (pad vs. stick) for both players. */
void Setup_Pad_or_Stick() {
    plsw_00[0] = PLsw[0][0];
    plsw_01[0] = PLsw[0][1];
    plsw_00[1] = PLsw[1][0];
    plsw_01[1] = PLsw[1][1];
}

/** @brief After-title state â€” dispatch to sub-menu by r_no[1]. */
static void After_Title(struct _TASK* task_ptr) {
    void (*AT_Jmp_Tbl[AT_JMP_COUNT])() = { Menu_Init,      Mode_Select,      Option_Select,  Option_Select,
                                           Training_Mode,  System_Direction, Load_Replay,    Option_Select,
                                           toSelectGame,   Game_Option,      Button_Config,  System_Direction,
                                           Sound_Test,     Memory_Card,      Extra_Option,   Option_Select,
                                           VS_Result,      Save_Replay,      Direction_Menu, Save_Direction,
                                           Load_Direction, Network_Lobby };

    if (task_ptr->r_no[1] >= AT_JMP_COUNT) {
        return;
    }

    AT_Jmp_Tbl[task_ptr->r_no[1]](task_ptr);
}

/** @brief One-time menu initialisation (fade, BG, saver task). */
void Menu_Init(struct _TASK* task_ptr) {
    s16 ix;
    s16 fade_on;

    if (Pause_Type == 2) {
        task_ptr->r_no[1] = 4;
    } else {
        task_ptr->r_no[1] = 1;
    }

    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    Menu_Cursor_Y[0] = 0;
    Menu_Cursor_Y[1] = 0;

    for (ix = 0; ix < 4; ix++) {
        Menu_Suicide[ix] = 0;
        Unsubstantial_BG[ix] = 0;
        Cursor_Y_Pos[0][ix] = 0;
    }

    All_Clear_Suicide();
    pulpul_stop();

    if (task_ptr->r_no[0] == 0) {
        FadeOut(1, 0xFF, 8);
        bg_etc_write_ex(2);
        Setup_Virtual_BG(0, 0x200, 0);
        Setup_BG(1, 0x200, 0);
        Setup_BG(2, 0x200, 0);
        base_y_pos = 0;

        if (task_ptr->r_no[1] != 0x12) {
            fade_on = 0;
        } else {
            fade_on = 1;
        }

        Order[0x4E] = 5;
        Order_Timer[0x4E] = 1;
        effect_57_init(0x4E, 0, 0, 0x45, fade_on);
        load_any_texture_patnum(0x7F30, 0xC, 0);
    }

    cpReadyTask(TASK_SAVER, Saver_Task);
}

/** @brief Mode-select screen (Arcade / VS / Training / Options / Exit). */
static void Mode_Select(struct _TASK* task_ptr) {
    s16 ix;
    s16 PL_id;
    s16 loop_counter = 7;

    switch (task_ptr->r_no[2]) {
    case 0:
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->timer = 5;
        Mode_Type = MODE_ARCADE;
        Present_Mode = 1;

        if (task[TASK_ENTRY].condition != 1) {
            E_No[0] = 1;
            E_No[1] = 2;
            E_No[2] = 2;
            E_No[3] = 0;
            cpReadyTask(TASK_ENTRY, Entry_Task);
        }

        Menu_Common_Init();

        for (ix = 0; ix < 4; ix++) {
            Menu_Suicide[ix] = 0;
        }

        Clear_Personal_Data(0);
        Clear_Personal_Data(1);
        Menu_Cursor_Y[0] = Cursor_Y_Pos[0][0];
        Cursor_Y_Pos[0][1] = 0;
        Cursor_Y_Pos[0][2] = 0;
        Cursor_Y_Pos[0][3] = 0;

        for (ix = 0; ix < 4; ix++) {
            Vital_Handicap[ix][0] = 7;
            Vital_Handicap[ix][1] = 7;
        }

        VS_Stage = 0x14;
        Order[0x8A] = 4;
        Order_Timer[0x8A] = 1;

        for (ix = 0; ix < 4; ix++) {
            Message_Data[ix].order = 3;
        }

        effect_57_init(0x64, 0, 0, 0x3F, 2);
        Order[0x64] = 1;
        Order_Dir[0x64] = 8;
        Order_Timer[0x64] = 1;
        Menu_Suicide[0] = 0;

        if (use_rmlui && rmlui_menu_mode) {
            rmlui_mode_menu_show();
        } else {
            effect_04_init(0, 0, 0, 0x48);

            for (ix = 0; ix < loop_counter; ix++) {
                effect_61_init(0, ix + 0x50, 0, 0, (u32)ix, ix, 0x7047);
                Order[ix + 0x50] = 1;
                Order_Dir[ix + 0x50] = 4;
                Order_Timer[ix + 0x50] = ix + 0x14;
            }
        }

        Menu_Cursor_Move = loop_counter;
        break;

    case 1:
        if (Menu_Sub_case1(task_ptr) != 0) {
            Order[0x4E] = 2;
            Order_Dir[0x4E] = 0;
            Order_Timer[0x4E] = 1;
            checkAdxFileLoaded();
            checkSelObjFileLoaded();
        }

        break;

    case 2:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
            Suicide[3] = 0;
        }

        break;

    case 3:
        if (Connect_Status == 0 && Menu_Cursor_Y[0] == 1) {
            Menu_Cursor_Y[0] = 2;
        } else {
            PL_id = 0;

            if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, loop_counter - 1, 1) == 0) {
                PL_id = 1;
                MC_Move_Sub(Check_Menu_Lever(1, 0), 0, loop_counter - 1, 1);
            }
        }

        switch (IO_Result) {
        case 0x100:
            switch (Menu_Cursor_Y[0]) {
            case 0:
                G_No[2] += 1;
                Mode_Type = MODE_ARCADE;
                task_ptr->r_no[0] = 5;
                cpExitTask(TASK_SAVER);
                Decide_PL(PL_id);
                break;

            case 1:
                Setup_VS_Mode(task_ptr);
                G_No[1] = 12;
                G_No[2] = 1;
                Mode_Type = MODE_VERSUS;
                cpExitTask(TASK_MENU);
                break;

            case 3:
                task_ptr->r_no[2] += 1;
                task_ptr->free[0] = 0;
                task_ptr->free[1] = 21; /* AT index for Network_Lobby */
                break;

            case 2:
            case 4:
            case 5:
            case 6:
                task_ptr->r_no[2] += 1;
                task_ptr->free[0] = 0;
                task_ptr->free[1] = Menu_Cursor_Y[0] + 2;
                break;

            default:
                break;
            }

            SE_selected();
            break;
        }

        break;

    default:
        Exit_Sub(task_ptr, 0, task_ptr->free[1]);
        break;
    }
}

/** @brief Prepare VS mode â€” enable both operators and init grades. */
static void Setup_VS_Mode(struct _TASK* task_ptr) {
    task_ptr->r_no[0] = 5;
    cpExitTask(TASK_SAVER);
    plw[0].wu.pl_operator = 1;
    plw[1].wu.pl_operator = 1;
    Operator_Status[0] = 1;
    Operator_Status[1] = 1;
    grade_check_work_1st_init(0, 0);
    grade_check_work_1st_init(0, 1);
    grade_check_work_1st_init(1, 0);
    grade_check_work_1st_init(1, 1);
    Setup_Training_Difficulty();
}

/** @brief Common sub-menu entry â€” fade out, reset cursors, show header. */
static void Menu_in_Sub(struct _TASK* task_ptr) {
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] += 1;
    task_ptr->timer = 5;
    Menu_Common_Init();
    Menu_Cursor_Y[0] = Cursor_Y_Pos[0][1];
    Menu_Suicide[0] = 1;
    Menu_Suicide[1] = 0;
    Order[0x64] = 4;
    Order_Timer[0x64] = 1;
}

/* ── Popup draw helpers (called from lobby and font_test) ──────── */

void NetLobby_DrawIncomingPopup(const char* name, const char* region, int ping) {
    /* Dark semi-transparent overlay covering the whole screen */
    {
        PAL_CURSOR_P op[4];
        PAL_CURSOR_COL ocol[4];
        op[0].x = -2;
        op[0].y = -2;
        op[1].x = 386;
        op[1].y = -2;
        op[2].x = -2;
        op[2].y = 226;
        op[3].x = 386;
        op[3].y = 226;
        ocol[0].color = ocol[1].color = ocol[2].color = ocol[3].color = 0xA0000000;
        Renderer_Queue2DPrimitive((f32*)op, PrioBase[3], (uintptr_t)ocol[0].color, 0);
    }
    /* Centered popup box */
    {
        PAL_CURSOR_P bp[4];
        PAL_CURSOR_COL bcol[4];
        bp[0].x = 60;
        bp[0].y = 56;
        bp[1].x = 324;
        bp[1].y = 56;
        bp[2].x = 60;
        bp[2].y = 168;
        bp[3].x = 324;
        bp[3].y = 168;
        bcol[0].color = bcol[1].color = bcol[2].color = bcol[3].color = 0xE0181818;
        Renderer_Queue2DPrimitive((f32*)bp, PrioBase[3], (uintptr_t)bcol[0].color, 0);

        /* Red border - top */
        PAL_CURSOR_P bb[4];
        PAL_CURSOR_COL bbcol[4];
        bb[0].x = 60;
        bb[0].y = 55;
        bb[1].x = 324;
        bb[1].y = 55;
        bb[2].x = 60;
        bb[2].y = 57;
        bb[3].x = 324;
        bb[3].y = 57;
        bbcol[0].color = bbcol[1].color = bbcol[2].color = bbcol[3].color = 0xFFCC0000;
        Renderer_Queue2DPrimitive((f32*)bb, PrioBase[3], (uintptr_t)bbcol[0].color, 0);
        /* Red border - bottom */
        bb[0].y = 167;
        bb[1].y = 167;
        bb[2].y = 169;
        bb[3].y = 169;
        Renderer_Queue2DPrimitive((f32*)bb, PrioBase[3], (uintptr_t)bbcol[0].color, 0);
    }

    /* Title */
    SSPutStr2(15, 9, 9, "INCOMING CHALLENGE!");

    /* Challenger name + region (centered dynamically) */
    {
        char name_buf[64];
        if (region && region[0])
            SDL_snprintf(name_buf, sizeof(name_buf), "> %s  [%s]", name, region);
        else
            SDL_snprintf(name_buf, sizeof(name_buf), "> %s", name);
        int name_len = (int)SDL_strlen(name_buf);
        u16 name_x = (u16)((192 - name_len * 4) / 8);
        SSPutStr2(name_x, 12, 6, (const s8*)name_buf);
    }

    /* Ping (centered dynamically: label=pal5, value=pal8) */
    {
        char val_buf[32];
        if (ping < 0)
            SDL_snprintf(val_buf, sizeof(val_buf), "...");
        else
            SDL_snprintf(val_buf, sizeof(val_buf), "~%dms", ping);
        /* Center the full "PING: <value>" string */
        int full_len = 6 + (int)SDL_strlen(val_buf); /* "PING: " = 6 chars */
        u16 ping_x = (u16)((192 - full_len * 4) / 8);
        SSPutStr2(ping_x, 15, 5, "PING: ");
        SSPutStr2((u16)(ping_x + 6), 15, 8, (const s8*)val_buf);
    }

    /* Accept/Decline with game button images */
    dispButtonImage2(0x6C, 0x8E, 0, 0x13, 0xF, 0, 4); /* A button */
    SSPutStrPro(0, 128, 144, 4, 0xFF00FF00, (s8*)"ACCEPT");
    dispButtonImage2(0xC0, 0x8E, 0, 0x13, 0xF, 0, 5); /* B button */
    SSPutStrPro(0, 216, 144, 4, 0xFFFF0000, (s8*)"DECLINE");
}

void NetLobby_DrawOutgoingPopup(const char* name, int ping) {
    /* Dark semi-transparent overlay covering the whole screen */
    {
        PAL_CURSOR_P op[4];
        PAL_CURSOR_COL ocol[4];
        op[0].x = -2;
        op[0].y = -2;
        op[1].x = 386;
        op[1].y = -2;
        op[2].x = -2;
        op[2].y = 226;
        op[3].x = 386;
        op[3].y = 226;
        ocol[0].color = ocol[1].color = ocol[2].color = ocol[3].color = 0xA0000000;
        Renderer_Queue2DPrimitive((f32*)op, PrioBase[3], (uintptr_t)ocol[0].color, 0);
    }
    /* Centered popup box */
    {
        PAL_CURSOR_P bp[4];
        PAL_CURSOR_COL bcol[4];
        bp[0].x = 60;
        bp[0].y = 56;
        bp[1].x = 324;
        bp[1].y = 56;
        bp[2].x = 60;
        bp[2].y = 168;
        bp[3].x = 324;
        bp[3].y = 168;
        bcol[0].color = bcol[1].color = bcol[2].color = bcol[3].color = 0xE0181818;
        Renderer_Queue2DPrimitive((f32*)bp, PrioBase[3], (uintptr_t)bcol[0].color, 0);

        /* Red border - top */
        PAL_CURSOR_P bb[4];
        PAL_CURSOR_COL bbcol[4];
        bb[0].x = 60;
        bb[0].y = 55;
        bb[1].x = 324;
        bb[1].y = 55;
        bb[2].x = 60;
        bb[2].y = 57;
        bb[3].x = 324;
        bb[3].y = 57;
        bbcol[0].color = bbcol[1].color = bbcol[2].color = bbcol[3].color = 0xFFCC0000;
        Renderer_Queue2DPrimitive((f32*)bb, PrioBase[3], (uintptr_t)bbcol[0].color, 0);
        /* Red border - bottom */
        bb[0].y = 167;
        bb[1].y = 167;
        bb[2].y = 169;
        bb[3].y = 169;
        Renderer_Queue2DPrimitive((f32*)bb, PrioBase[3], (uintptr_t)bbcol[0].color, 0);
    }

    /* Title */
    SSPutStr2(17, 9, 9, "CONNECTING...");

    /* Peer name (centered dynamically) */
    {
        char out_buf[64];
        SDL_snprintf(out_buf, sizeof(out_buf), "> %s", name);
        int out_len = (int)SDL_strlen(out_buf);
        u16 out_x = (u16)((192 - out_len * 4) / 8);
        SSPutStr2(out_x, 12, 6, (const s8*)out_buf);
    }

    /* Ping estimate (centered dynamically: label=pal5, value=pal8) */
    {
        char val_buf[32];
        if (ping < 0)
            SDL_snprintf(val_buf, sizeof(val_buf), "...");
        else
            SDL_snprintf(val_buf, sizeof(val_buf), "~%dms", ping);
        int full_len = 6 + (int)SDL_strlen(val_buf);
        u16 out_px = (u16)((192 - full_len * 4) / 8);
        SSPutStr2(out_px, 15, 5, "PING: ");
        SSPutStr2((u16)(out_px + 6), 15, 8, (const s8*)val_buf);
    }

    /* Cancel button */
    dispButtonImage2(0x98, 0x8E, 0, 0x13, 0xF, 0, 5); /* B button */
    SSPutStrPro(0, 176, 144, 4, 0xFFFF0000, (s8*)"CANCEL");
}

/** @brief Network Lobby screen — options-screen style with toggles and peer list.
 *  Uses effect_61 brightness for cursor indication (no effect_04 cursor bar). */
static void Network_Lobby(struct _TASK* task_ptr) {
    s16 ix;
    static int s_lobby_peer_idx = 0;
    static int s_net_peer_idx = 0;
    static int s_slide_offset = 384; /* slide-in offset for SSPutStr elements */

    switch (task_ptr->r_no[2]) {
    case 0:
        /* Phase 1: Start fade, set suicide, request blue BG mode */
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->r_no[3] = 0;
        task_ptr->timer = 5;
        Menu_Suicide[0] = 1;        /* kill Mode_Select items (master_player=0) */
        Menu_Suicide[1] = 0;        /* enable our items (master_player=1) */
        Message_Data->kind_req = 4; /* blue-BG background mode */
        break;

    case 1:
        /* Phase 2: Destroy old effects, rebuild from scratch */
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;

        effect_work_init();
        Menu_Common_Init();
        s_slide_offset = 384;
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Order[0x4E] = 5;
        Order_Timer[0x4E] = 1;

        /* Red slide-in header bar */
        Order_Dir[0x4E] = 1;

        if (use_rmlui && rmlui_menu_lobby) {
            rmlui_network_lobby_show();
        } else {
            effect_57_init(0x4E, 0, 0, 0x45, 0);

            /* Right-side grey overlay boxes (LAN and Internet peer areas) */
            effect_66_init(0x8A, 42, 1, 0, -1, -1, -0x7FF0); /* cg_type=16: LAN peer box (top, small) */
            Order[0x8A] = 3;
            Order_Timer[0x8A] = 1;
            effect_66_init(0x8B, 42, 1, 0, -1, -1, -0x7FF1); /* cg_type=15: Internet peer box (bottom, big) */
            Order[0x8B] = 3;
            Order_Timer[0x8B] = 1;

            /* No effect_04_init — effect_61 brightness handles cursor indication */

            /* Menu items: 6 items, 0x70A7 = compact 8px font, master_player=1
             * 68=LAN AUTO-CONN, 69=NET AUTO-CONN, 70=AUTO-SEARCH,
             * 71=CONNECT PEER, 72=SEARCH MATCH, 73=EXIT */
            {
                static const s16 lobby_strings[] = { 68, 69, 70, 71, 72, 73 };
                for (ix = 0; ix < 6; ix++) {
                    effect_61_init(0, ix + 0x50, 0, 1, lobby_strings[ix], ix, 0x70A7);
                    Order[ix + 0x50] = 1;
                    Order_Dir[ix + 0x50] = 4;
                    Order_Timer[ix + 0x50] = ix + 0x14;
                }
            }

            /* Title: "NETWORK LOBBY" in big CG font (0x7047), string index 67 */
            effect_61_init(0, 0x5F, 0, 1, 67, -1, 0x7047);
            Order[0x5F] = 1;
            Order_Dir[0x5F] = 4;
            Order_Timer[0x5F] = 0x12;

            /* Message system for description text (same font as Extra Option) */
            Message_Data->pos_x = 0;
            Message_Data->pos_y = 0x3E;
            Message_Data->pos_z = 0x44;
            Message_Data->request = 35; /* lobby msg index 0 = LAN AUTO-CONN */
            Message_Data->order = 0;
            Message_Data->timer = 1;
            effect_45_init(0, 0, 1);
        }

        Menu_Cursor_Move = 6;

        /* Enter lobby state — set native flag BEFORE changing session state
         * to prevent ImGui lobby from rendering a frame before the flag is set */
        SDLNetplayUI_SetNativeLobbyActive(true);
        Netplay_EnterLobby();
        /* fallthrough to case 2 */

    case 2:
        /* Wait for fade-out timer */
        FadeOut(1, 0xFF, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2] += 1;
            task_ptr->r_no[3] = 1;
            FadeInit();
        }
        break;

    case 3:
        /* Fade in */
        if (FadeIn(1, 25, 8)) {
            task_ptr->r_no[2] += 1;
        }
        break;

    case 4: {
        /* --- Input loop --- */

        /* Decelerate slide-in offset */
        if (s_slide_offset > 0) {
            s_slide_offset = (int)(s_slide_offset / 1.18f);
            if (s_slide_offset < 2)
                s_slide_offset = 0;
        }
        const s16 sl = (s16)s_slide_offset;

        /* Custom red banner (brighter than default Akaobi 0xA0D00000) */
        if (!use_rmlui || !rmlui_menu_lobby)
        {
            PAL_CURSOR_P ap[4];
            PAL_CURSOR_COL acol[4];
            u8 ci;
            for (ci = 0; ci < 4; ci++) {
                ap[ci].x = Akaobi_Pos_tbl[ci * 2];
                ap[ci].y = Akaobi_Pos_tbl[(ci * 2) + 1];
                acol[ci].color = 0xFFCC0000; /* fully opaque vibrant red */
            }
            Renderer_Queue2DPrimitive((f32*)ap, PrioBase[69], (uintptr_t)acol[0].color, 0);

            /* White top border (1px, 1px gap above red) */
            ap[0].x = -2;
            ap[0].y = 14;
            ap[1].x = 386;
            ap[1].y = 14;
            ap[2].x = -2;
            ap[2].y = 15;
            ap[3].x = 386;
            ap[3].y = 15;
            acol[0].color = acol[1].color = acol[2].color = acol[3].color = 0xFFFFFFFF;
            Renderer_Queue2DPrimitive((f32*)ap, PrioBase[67], (uintptr_t)acol[0].color, 0);

            /* White bottom border (1px, 1px gap below red) */
            ap[0].y = 41;
            ap[1].y = 41;
            ap[2].y = 42;
            ap[3].y = 42;
            Renderer_Queue2DPrimitive((f32*)ap, PrioBase[67], (uintptr_t)acol[0].color, 0);
        }

        /* Compute popup_active once — shared by cursor, toggle, and text suppression */
        bool lan_incoming = false;
        bool lan_outgoing = (Discovery_GetChallengeTarget() != 0);
        {
            NetplayDiscoveredPeer pp[16];
            int pp_n = Discovery_GetPeers(pp, 16);
            for (int i = 0; i < pp_n; i++) {
                if (pp[i].is_challenging_me && !lan_outgoing) {
                    lan_incoming = true;
                    break;
                }
            }
        }
        bool popup_active =
            SDLNetplayUI_HasPendingInvite() || SDLNetplayUI_HasOutgoingChallenge() || lan_incoming || lan_outgoing;

        /* Handle cursor movement (6 items: 0..5)
         * MC_Move_Sub MUST always run because it sets IO_Result from button presses.
         * When the popup is active, we save/restore the cursor to suppress movement
         * while still capturing confirm/cancel button events for the popup handler. */
        {
            s16 prev_cursor = Menu_Cursor_Y[0];
            if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, 5, 0xFF) == 0) {
                MC_Move_Sub(Check_Menu_Lever(1, 0), 0, 5, 0xFF);
            }
            if (popup_active) {
                /* Restore cursor — suppress movement while popup is showing */
                Menu_Cursor_Y[0] = prev_cursor;
            } else if (prev_cursor != Menu_Cursor_Y[0]) {
                /* Update description message on cursor change */
                Message_Data->order = 1;
                Message_Data->request = 35 + Menu_Cursor_Y[0];
                Message_Data->timer = 2;
                Message_Data->pos_y = 0x3E;
            }
        }

        /* === Left/right toggle handling for toggle items === */
        if (!popup_active) {
            u16 click = (~plsw_01[0] & plsw_00[0]) | (~plsw_01[1] & plsw_00[1]);

            if (click & 12) { // SWK_LEFT is 4, SWK_RIGHT is 8
                switch (Menu_Cursor_Y[0]) {
                case 0: { /* LAN AUTO-CONN */
                    bool v = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
                    Config_SetBool(CFG_KEY_NETPLAY_AUTO_CONNECT, !v);
                    Config_Save();
                    SE_dir_cursor_move();
                    break;
                }
                case 1: { /* LAN CONNECT (peee toggling) */
                    NetplayDiscoveredPeer tg_peers[16];
                    int tg_count = Discovery_GetPeers(tg_peers, 16);
                    if (tg_count > 0) {
                        if (click & 4) { // Left
                            s_lobby_peer_idx--;
                            if (s_lobby_peer_idx < 0)
                                s_lobby_peer_idx = tg_count - 1;
                        } else { // Right
                            s_lobby_peer_idx++;
                            if (s_lobby_peer_idx >= tg_count)
                                s_lobby_peer_idx = 0;
                        }
                        SE_dir_cursor_move();
                        if (Discovery_GetChallengeTarget() != 0) {
                            Discovery_SetChallengeTarget(0);
                        }
                    } else {
                        // ignore empty
                    }
                    break;
                }
                case 2: { /* NET AUTO-CONN */
                    bool v = Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT);
                    Config_SetBool(CFG_KEY_LOBBY_AUTO_CONNECT, !v);
                    Config_Save();
                    SE_dir_cursor_move();
                    break;
                }
                case 3: { /* NET AUTO-SEARCH */
                    bool v = Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH);
                    Config_SetBool(CFG_KEY_LOBBY_AUTO_SEARCH, !v);
                    Config_Save();
                    SE_dir_cursor_move();
                    break;
                }
                case 4: { /* NET CONNECT */
                    if (SDLNetplayUI_IsSearching()) {
                        int p_count = SDLNetplayUI_GetOnlinePlayerCount();
                        if (p_count > 0) {
                            if (click & 4) { // Left
                                s_net_peer_idx--;
                                if (s_net_peer_idx < 0)
                                    s_net_peer_idx = p_count - 1;
                            } else { // Right
                                s_net_peer_idx++;
                                if (s_net_peer_idx >= p_count)
                                    s_net_peer_idx = 0;
                            }
                            SE_dir_cursor_move();
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }

        /* === Background text — hide when popup covers the screen === */
        if ((!use_rmlui || !rmlui_menu_lobby) && !popup_active) {
            /* === Display toggle values (right of labels) === */
            {
                /* LAN toggle values (left column) */
                bool lan_ac = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
                SSPutStr_Bigger(136 + sl, 63, 5, lan_ac ? (s8*)"ON" : (s8*)"OFF", 1.0f, lan_ac ? 9 : 1, 1.0f);

                /* NET toggle values (left column, below LAN) */
                bool net_ac = Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT);
                SSPutStr_Bigger(136 + sl, 115, 5, net_ac ? (s8*)"ON" : (s8*)"OFF", 1.0f, net_ac ? 9 : 1, 1.0f);

                bool auto_s = Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH);
                SSPutStr_Bigger(136 + sl, 129, 5, auto_s ? (s8*)"ON" : (s8*)"OFF", 1.0f, auto_s ? 9 : 1, 1.0f);
            }

            /* === LAN / NET Headers (centered on screen, 384px wide) === */
            {
                const char* lan_hdr = "----- LAN -----";
                int lan_hdr_px = (int)SDL_strlen(lan_hdr) * 8;
                s16 lan_hdr_x = (s16)((384 - lan_hdr_px) / 2);
                SSPutStr_Bigger(lan_hdr_x + sl, 50, 5, (s8*)lan_hdr, 1.0f, 0, 1.0f);
            }
            {
                const char* net_hdr = "-- INTERNET --";
                int net_hdr_px = (int)SDL_strlen(net_hdr) * 8;
                s16 net_hdr_x = (s16)((384 - net_hdr_px) / 2);
                SSPutStr_Bigger(net_hdr_x + sl, 102, 5, (s8*)net_hdr, 1.0f, 0, 1.0f);
            }

            /* === Peer / Online Info (Right Side, inside grey boxes) === */
            {
                s16 peer_x = 200;
                s16 lan_peer_y = 63;
                s16 net_peer_y = 115;

                /* LAN Selected Peer (right side, top) — styled like Internet */
                NetplayDiscoveredPeer d_peers[16];
                int d_count = Discovery_GetPeers(d_peers, 16);
                if (d_count > 0) {
                    if (s_lobby_peer_idx >= d_count)
                        s_lobby_peer_idx = d_count - 1;
                    if (s_lobby_peer_idx < 0)
                        s_lobby_peer_idx = 0;
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "%d FOUND", d_count);
                    SSPutStr_Bigger(peer_x + sl, lan_peer_y, 5, (s8*)buf, 1.0f, 9, 1.0f);

                    // Show selected peer name
                    SDL_snprintf(buf, sizeof(buf), "> %s", d_peers[s_lobby_peer_idx].name);
                    SSPutStr_Bigger(peer_x + sl, (u16)(lan_peer_y + 15), 5, (s8*)buf, 1.0f, 0, 1.0f);
                } else {
                    s_lobby_peer_idx = 0;
                    SSPutStr_Bigger(peer_x + sl, lan_peer_y, 5, (s8*)"NONE", 1.0f, 1, 1.0f);
                }

                /* Internet Online Players (right side, below LAN peer) */
                if (SDLNetplayUI_IsSearching()) {
                    int online_count = SDLNetplayUI_GetOnlinePlayerCount();
                    char s_buf[64];
                    if (online_count > 0) {
                        if (s_net_peer_idx >= online_count)
                            s_net_peer_idx = online_count - 1;
                        if (s_net_peer_idx < 0)
                            s_net_peer_idx = 0;

                        SDL_snprintf(s_buf, sizeof(s_buf), "%d ONLINE", online_count);
                        SSPutStr_Bigger(peer_x + sl, net_peer_y, 5, (s8*)s_buf, 1.0f, 9, 1.0f);

                        // Show selected peer
                        SDL_snprintf(s_buf, sizeof(s_buf), "> %s", SDLNetplayUI_GetOnlinePlayerName(s_net_peer_idx));
                        SSPutStr_Bigger(peer_x + sl, (u16)(net_peer_y + 15), 5, (s8*)s_buf, 1.0f, 0, 1.0f);
                    } else {
                        s_net_peer_idx = 0;
                        SSPutStr_Bigger(peer_x + sl, net_peer_y, 5, (s8*)"SEARCHING", 1.0f, 9, 1.0f);
                    }
                } else {
                    SSPutStr_Bigger(peer_x + sl, net_peer_y, 5, (s8*)"IDLE", 1.0f, 1, 1.0f);
                }
            }

            /* === Grey description banner (between EXIT and status) === */
            {
                PAL_CURSOR_P dp[4];
                PAL_CURSOR_COL dcol[4];
                dp[0].x = -2;
                dp[0].y = 175;
                dp[1].x = 386;
                dp[1].y = 175;
                dp[2].x = -2;
                dp[2].y = 213;
                dp[3].x = 386;
                dp[3].y = 213;
                dcol[0].color = dcol[1].color = dcol[2].color = dcol[3].color = 0x80202020;
                Renderer_Queue2DPrimitive((f32*)dp, PrioBase[70], (uintptr_t)dcol[0].color, 0);

                /* Description text rendered by effect_45 message system */
            }

            /* === Status line (bottom area) === */
            {
                const char* status = SDLNetplayUI_GetStatusMsg();
                if (status[0]) {
                    SSPutStr_Bigger(40 + sl, 215, 5, (s8*)status, 1.0f, 9, 1.0f);
                } else {
                    NetplayDiscoveredPeer c_peers[16];
                    int c_count = Discovery_GetPeers(c_peers, 16);
                    int current_target = Discovery_GetChallengeTarget();
                    bool showing_status = false;

                    for (int i = 0; i < c_count; i++) {
                        if (c_peers[i].is_challenging_me) {
                            char c_buf[64];
                            SDL_snprintf(c_buf, sizeof(c_buf), "CHALLENGED BY %s!", c_peers[i].name);
                            SSPutStr_Bigger(40 + sl, 215, 5, (s8*)c_buf, 1.0f, 9, 1.0f);
                            showing_status = true;
                            break;
                        }
                    }

                    if (!showing_status && current_target != 0) {
                        for (int i = 0; i < c_count; i++) {
                            if ((int)c_peers[i].instance_id == current_target) {
                                char c_buf[64];
                                SDL_snprintf(c_buf, sizeof(c_buf), "CHALLENGING %s...", c_peers[i].name);
                                SSPutStr_Bigger(40 + sl, 215, 5, (s8*)c_buf, 1.0f, 9, 1.0f);
                                showing_status = true;
                                break;
                            }
                        }
                    }

                    if (!showing_status && SDLNetplayUI_IsDiscovering()) {
                        SSPutStr_Bigger(40 + sl, 215, 5, (s8*)"DISCOVERING...", 1.0f, 9, 1.0f);
                    }
                }
            }
        } /* end !popup_active background text */

        /* === Incoming Challenge Popup (Internet) === */
        if (SDLNetplayUI_HasPendingInvite()) {
            if (!use_rmlui || !rmlui_menu_lobby)
                NetLobby_DrawIncomingPopup(SDLNetplayUI_GetPendingInviteName(),
                                           SDLNetplayUI_GetPendingInviteRegion(),
                                           SDLNetplayUI_GetPendingInvitePing());

            /* Override input: accept on Confirm, decline on Cancel */
            switch (IO_Result) {
            case 0x100: /* Confirm = Accept */
                SDLNetplayUI_AcceptPendingInvite();
                SE_selected();
                break;
            case 0x200: /* Cancel = Decline */
                SDLNetplayUI_DeclinePendingInvite();
                SE_selected();
                break;
            default:
                break;
            }
        } else if (SDLNetplayUI_HasOutgoingChallenge()) {
            /* === Outgoing Challenge Popup (Internet) === */
            if (!use_rmlui || !rmlui_menu_lobby)
                NetLobby_DrawOutgoingPopup(SDLNetplayUI_GetOutgoingChallengeName(),
                                           SDLNetplayUI_GetOutgoingChallengePing());

            /* Cancel on either Confirm or Cancel button press */
            if (IO_Result == 0x100 || IO_Result == 0x200) {
                SDLNetplayUI_CancelOutgoingChallenge();
                Discovery_SetChallengeTarget(0);
                SE_selected();
            }
        } else if (Discovery_GetChallengeTarget() != 0) {
            /* === Outgoing Challenge Popup (LAN) === */
            {
                NetplayDiscoveredPeer op[16];
                int op_n = Discovery_GetPeers(op, 16);
                int tgt = Discovery_GetChallengeTarget();
                const char* tgt_name = "...";
                for (int i = 0; i < op_n; i++) {
                    if ((int)op[i].instance_id == tgt) {
                        tgt_name = op[i].name;
                        break;
                    }
                }
                if (!use_rmlui || !rmlui_menu_lobby)
                    NetLobby_DrawOutgoingPopup(tgt_name, -1);
            }

            /* Cancel on either Confirm or Cancel button press */
            if (IO_Result == 0x100 || IO_Result == 0x200) {
                Discovery_SetChallengeTarget(0);
                SE_selected();
            }
        } else {
            /* === Incoming Challenge Popup (LAN) === */
            NetplayDiscoveredPeer ip_peers[16];
            int ip_n = Discovery_GetPeers(ip_peers, 16);
            int lan_challenger = -1;
            for (int i = 0; i < ip_n; i++) {
                if (ip_peers[i].is_challenging_me) {
                    lan_challenger = i;
                    break;
                }
            }

            if (lan_challenger >= 0) {
                if (!use_rmlui || !rmlui_menu_lobby)
                    NetLobby_DrawIncomingPopup(ip_peers[lan_challenger].name, "", -1);

                switch (IO_Result) {
                case 0x100: /* Confirm = Accept (challenge them back) */
                    Discovery_SetChallengeTarget(ip_peers[lan_challenger].instance_id);
                    SE_selected();
                    break;
                case 0x200: /* Cancel = Decline (no-op, just dismiss) */
                    SE_selected();
                    break;
                default:
                    break;
                }
            } else {
                /* === Handle confirm/cancel (normal lobby input) === */
                switch (IO_Result) {
                case 0x100: /* Confirm */
                    switch (Menu_Cursor_Y[0]) {
                    case 0: { /* LAN AUTO-CONN toggle */
                        bool v = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
                        Config_SetBool(CFG_KEY_NETPLAY_AUTO_CONNECT, !v);
                        Config_Save();
                        SE_selected();
                        break;
                    }
                    case 1: { /* LAN CONNECT */
                        NetplayDiscoveredPeer cp_peers[16];
                        int cp_count = Discovery_GetPeers(cp_peers, 16);
                        if (cp_count > 0 && s_lobby_peer_idx >= 0 && s_lobby_peer_idx < cp_count) {
                            NetplayDiscoveredPeer* p = &cp_peers[s_lobby_peer_idx];

                            Discovery_SetChallengeTarget(p->instance_id);
                            SE_selected();
                        } else {
                            // No peer found
                            SE_selected();
                        }
                        break;
                    }
                    case 2: { /* NET AUTO-CONN toggle */
                        bool v = Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT);
                        Config_SetBool(CFG_KEY_LOBBY_AUTO_CONNECT, !v);
                        Config_Save();
                        SE_selected();
                        break;
                    }
                    case 3: { /* NET AUTO-SEARCH toggle */
                        bool v = Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH);
                        Config_SetBool(CFG_KEY_LOBBY_AUTO_SEARCH, !v);
                        Config_Save();
                        SE_selected();
                        break;
                    }
                    case 4: /* NET CONNECT */
                        if (SDLNetplayUI_IsSearching()) {
                            int p_count = SDLNetplayUI_GetOnlinePlayerCount();
                            if (p_count > 0 && s_net_peer_idx >= 0 && s_net_peer_idx < p_count) {
                                SDLNetplayUI_ConnectToPlayer(s_net_peer_idx);
                                SE_selected();
                            } else {
                                SDLNetplayUI_StopSearch();
                                SE_selected();
                            }
                        } else {
                            SDLNetplayUI_StartSearch();
                            SE_selected();
                        }
                        break;

                    case 5:
                        /* EXIT */
                        goto lobby_exit;
                    }
                    break;

                case 0x200: /* Cancel */
                    if (Discovery_GetChallengeTarget() != 0) {
                        Discovery_SetChallengeTarget(0);
                        SE_selected();
                        break;
                    }
                lobby_exit:
                    SE_selected();
                    SDLNetplayUI_SetNativeLobbyActive(false);
                    if (use_rmlui && rmlui_menu_lobby)
                        rmlui_network_lobby_hide();
                    Netplay_HandleMenuExit();
                    Menu_Suicide[0] = 0;
                    Menu_Suicide[1] = 1;   /* kill our items + blue BG */
                    task_ptr->r_no[1] = 1; /* Mode_Select */
                    task_ptr->r_no[2] = 0;
                    task_ptr->r_no[3] = 0;
                    task_ptr->free[0] = 0;
                    break;

                default:
                    break;
                }
            }
        } /* end LAN incoming / normal input */
        break;
    }
    }
}

/** @brief â€œSelect Gameâ€ (3S vs 2I) screen with exit-to-desktop option. */
static void toSelectGame(struct _TASK* task_ptr) {
    u16 sw;

    switch (task_ptr->r_no[2]) {
    case 0:
        Forbid_Reset = 1;
        Menu_in_Sub(task_ptr);
        Setup_BG(1, 0x200, 0);
        effect_66_init(0x8A, 8, 1, 0, -1, -1, -0x7FF2);
        Order[0x8A] = 3;
        Order_Timer[0x8A] = 1;
        task_ptr->free[0] = 0;
        task_ptr->timer = 0x10;
        break;

    case 1:
        if (Menu_Sub_case1(task_ptr) != 0) {
            Message_Data->kind_req = 5;
            Message_Data->request = 0;
            Message_Data->order = 1;
            Message_Data->timer = 2;
            Message_Data->pos_x = 0;
            Message_Data->pos_y = 0xA0;
            Message_Data->pos_z = 0x18;
            effect_45_init(0, 0, 2);
        }

        break;

    case 2:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
        }

        imgSelectGameButton();
        break;

    case 3:
        imgSelectGameButton();
        sw = (~plsw_01[0] & plsw_00[0]) | (~plsw_01[1] & plsw_00[1]); // potential macro
        sw &= (SWK_SOUTH | SWK_EAST);

        if (sw != 0) {
            if (sw != (SWK_SOUTH | SWK_EAST)) {
                if (sw & SWK_SOUTH) {
                    task_ptr->free[0] = 1;
                }

                SE_selected();
                FadeInit();
                task_ptr->r_no[2] = 8;
                break;
            }
        }

        break;

    case 8:
        imgSelectGameButton();

        if (FadeOut(1, 0x19, 8) != 0) {
            if (task_ptr->free[0]) {
                task_ptr->r_no[2] = 0xA;
                sound_all_off();
                break;
            }

            task_ptr->r_no[2] = 9;
            break;
        }

        break;

    case 9:
        Menu_Suicide[0] = 0;
        Menu_Suicide[1] = 1;
        task_ptr->r_no[1] = 1;
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        task_ptr->free[0] = 0;
        FadeOut(1, 0xFF, 8);
        Forbid_Reset = 0;
        break;

    case 10:
        Exit_sound_system();
        task_ptr->r_no[2] += 1;
        break;

    default:
        SDLApp_Exit();
        break;
    }
}

/** @brief Draw the two game-select button images. */

/** @brief Training-mode sub-menu (Normal / Parrying / Exit). */
static void Training_Mode(struct _TASK* task_ptr) {
    s16 ix;
    s16 PL_id;

    switch (task_ptr->r_no[2]) {
    case 0:
        Menu_in_Sub(task_ptr);
        mpp_w.initTrainingData = true;
        effect_57_init(0x6F, 0xB, 0, 0x3F, 2);
        Order[0x6F] = 1;
        Order_Dir[0x6F] = 8;
        Order_Timer[0x6F] = 1;
        if (use_rmlui && rmlui_menu_training) {
            rmlui_training_mode_show();
        } else {
            effect_04_init(1, 5, 0, 0x48);

            static const s16 menu_strings[] = { 0x35, 0x36, 66, 0x37 };
            for (ix = 0; ix < 4; ix++) {
                effect_61_init(0, ix + 0x50, 0, 1, menu_strings[ix], ix, 0x7047);
                Order[ix + 0x50] = 1;
                Order_Dir[ix + 0x50] = 4;
                Order_Timer[ix + 0x50] = ix + 0x14;
            }
        }

        Menu_Cursor_Move = 4;
        system_dir[4] = system_dir[1];
        system_dir[5] = system_dir[1];
        break;

    case 1:
        Menu_Sub_case1(task_ptr);
        break;

    case 2:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
            Suicide[3] = 0;
        }

        break;

    case 3:
        PL_id = 0;

        if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, 3, 0xFF) == 0) {
            PL_id = 1;
            MC_Move_Sub(Check_Menu_Lever(1, 0), 0, 3, 0xFF);
        }

        switch (IO_Result) {
        case 0x100:
        case 0x200:
            break;

        default:
            return;
        }

        SE_selected();

        if (Menu_Cursor_Y[0] == 3 || IO_Result == 0x200) {
            Menu_Suicide[0] = 0;
            Menu_Suicide[1] = 1;
            task_ptr->r_no[1] = 1;
            task_ptr->r_no[2] = 0;
            task_ptr->r_no[3] = 0;
            task_ptr->free[0] = 0;
            Order[0x6F] = 4;
            Order_Timer[0x6F] = 4;
            if (use_rmlui && rmlui_menu_training)
                rmlui_training_mode_hide();
            break;
        }

        Decide_ID = PL_id;

        if (Menu_Cursor_Y[0] == 0) {
            Mode_Type = MODE_NORMAL_TRAINING;
            Present_Mode = 4;
        } else if (Menu_Cursor_Y[0] == 1) {
            Mode_Type = MODE_PARRY_TRAINING;
            Present_Mode = 5;
        } else {
            Mode_Type = MODE_TRIALS;
            Present_Mode = 4; // Reuse normal training data
        }

        Setup_VS_Mode(task_ptr);
        G_No[2] += 1;
        task_ptr->r_no[0] = 5;
        cpExitTask(TASK_SAVER);
        Champion = PL_id;
        Pause_ID = PL_id;
        Training_ID = PL_id;
        New_Challenger = PL_id ^ 1;
        cpExitTask(TASK_ENTRY);

        break;
    }
}

/** @brief Options sub-menu (Game Options â€¦ Extra Option â€¦ Exit). */
static void Option_Select(struct _TASK* task_ptr) {
    s16 ix;
    s16 char_index;

    switch (task_ptr->r_no[2]) {
    case 0:
        Menu_in_Sub(task_ptr);
        Order[0x4E] = 2;
        Order_Dir[0x4E] = 0;
        Order_Timer[0x4E] = 1;
        effect_57_init(0x4F, 1, 0, 0x3F, 2);
        Order[0x4F] = 1;
        Order_Dir[0x4F] = 8;
        Order_Timer[0x4F] = 1;

        if (save_w[Present_Mode].Extra_Option == 0 && save_w[Present_Mode].Unlock_All == 0) {
            if (use_rmlui && rmlui_menu_option) {
                rmlui_option_menu_show();
            } else {
                effect_04_init(1, 4, 0, 0x48);

                ix = 0;
                char_index = 0x2F;

                while (ix < 6) {
                    effect_61_init(0, ix + 0x50, 0, 1, char_index, ix, 0x70A7);
                    Order[ix + 0x50] = 1;
                    Order_Dir[ix + 0x50] = 4;
                    Order_Timer[ix + 0x50] = ix + 0x14;
                    ix++;
                    char_index++;
                }
            }

            Menu_Cursor_Move = 6;
            break;
        }

        if (use_rmlui && rmlui_menu_option) {
            rmlui_option_menu_show();
        } else {
            effect_04_init(1, 1, 0, 0x48);

            ix = 0;
            char_index = 7;

            while (ix < 7) {
                effect_61_init(0, ix + 0x50, 0, 1, char_index, ix, 0x70A7);
                Order[ix + 0x50] = 1;
                Order_Dir[ix + 0x50] = 4;
                Order_Timer[ix + 0x50] = ix + 0x14;
                ix++;
                char_index++;
            }
        }

        Menu_Cursor_Move = 7;
        break;

    case 1:
        if (Menu_Sub_case1(task_ptr) != 0) {
            checkSelObjFileLoaded();
        }

        break;

    case 2:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
            Suicide[3] = 0;
        }

        break;

    case 3:
        if (save_w[Present_Mode].Extra_Option || save_w[Present_Mode].Unlock_All) {
            ix = 1;
        } else {
            ix = 0;
        }

        if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, ix + 5, 0xFF) == 0) {
            MC_Move_Sub(Check_Menu_Lever(1, 0), 0, ix + 5, 0xFF);
        }

        switch (IO_Result) {
        case 0x100:
        case 0x200:
            break;

        default:
            return;
        }

        SE_selected();

        if (Menu_Cursor_Y[0] == ix + 5 || IO_Result == 0x200) {
            Menu_Suicide[0] = 0;
            Menu_Suicide[1] = 1;
            task_ptr->r_no[1] = 1;
            task_ptr->r_no[2] = 0;
            task_ptr->r_no[3] = 0;
            task_ptr->free[0] = 0;
            Order[0x4F] = 4;
            Order_Timer[0x4F] = 4;

            if (Check_Change_Contents()) {
                if (save_w[Present_Mode].Auto_Save) {
                    task_ptr->r_no[0] = 4;
                    task_ptr->r_no[1] = 0;
                    Forbid_Reset = 1;
                    Copy_Check_w();
                    break;
                }
            }

            break;
        }

        task_ptr->r_no[2] += 1;
        task_ptr->free[0] = 0;
        X_Adjust_Buff[0] = X_Adjust;
        X_Adjust_Buff[1] = X_Adjust;
        X_Adjust_Buff[2] = X_Adjust;
        Y_Adjust_Buff[0] = Y_Adjust;
        Y_Adjust_Buff[1] = Y_Adjust;
        Y_Adjust_Buff[2] = Y_Adjust;
        break;

    default:
        Exit_Sub(task_ptr, 1, Menu_Cursor_Y[0] + 9);
        break;
    }
}

/** @brief System Direction (dipswitch) page-based settings menu.
 *  Context-aware: when entered from Option_Select (r_no[1]==11), uses
 *  master_player 2 and returns via Return_Option_Mode_Sub; when entered
 *  from Mode_Select (r_no[1]==5), uses the original master_player 1 path. */
static void System_Direction(struct _TASK* task_ptr) {
    s16 ix;
    s16 char_index;
    const int from_option = (task_ptr->r_no[1] == 11); /* AT index 11 = from Option_Select */

    switch (task_ptr->r_no[2]) {
    case 0:
        if (from_option) {
            /* Option_Select context: kill option items, enable sub-menu items */
            FadeOut(1, 0xFF, 8);
            task_ptr->r_no[2] += 1;
            task_ptr->timer = 5;
            Menu_Common_Init();
            Menu_Cursor_Y[0] = 0;
            Menu_Suicide[1] = 1; /* kill Option items (master_player=1) */
            Menu_Suicide[2] = 0; /* enable our items (master_player=2) */
            Order[0x4F] = 4;
            Order_Timer[0x4F] = 1;
            Order[0x4E] = 2;
            Order_Dir[0x4E] = 2;
            Order_Timer[0x4E] = 1;
        } else {
            /* Mode_Select context: original path */
            Menu_in_Sub(task_ptr);
            Order[0x4E] = 2;
            Order_Dir[0x4E] = 3;
            Order_Timer[0x4E] = 1;
        }
        Convert_Buff[3][0][0] = Direction_Working[1];

        if (use_rmlui && rmlui_menu_sysdir) {
            rmlui_sysdir_show();
        } else {
            effect_57_init(0x6D, 0xA, 0, 0x3F, 2);
            Order[0x6D] = 1;
            Order_Dir[0x6D] = 8;
            Order_Timer[0x6D] = 1;
            effect_04_init(1, 3, 0, 0x48);
            effect_64_init(0x61U, 0, from_option ? 2 : 1, 0xA, 0, 0x7047, 0xB, 3, 0);
            Order[0x61] = 1;
            Order_Dir[0x61] = 4;
            Order_Timer[0x61] = 0x14;

            ix = 0;
            char_index = 0x2B;

            while (ix < 4) {
                effect_61_init(0, ix + 0x50, 0, from_option ? 2 : 1, char_index, ix + 1, 0x7047);
                Order[ix + 0x50] = 1;
                Order_Dir[ix + 0x50] = 4;
                Order_Timer[ix + 0x50] = ix + 0x15;
                ix++;
                char_index++;
            }
        }

        Menu_Cursor_Move = 4;
        Page_Max = Check_SysDir_Page();
        break;

    case 1:
        Menu_Sub_case1(task_ptr);
        break;

    case 2:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
            Suicide[3] = 0;
        }

        break;

    case 3:
        System_Dir_Move_Sub(0);

        if (IO_Result == 0) {
            System_Dir_Move_Sub(1);
        }

        switch (IO_Result) {
        case 0x100:
            if (Menu_Cursor_Y[0] == 0) {
                break;
            }

            // fallthrough

        case 0x200:
            SE_selected();
            Order[0x6D] = 4;
            Order_Timer[0x6D] = 4;

            if (Menu_Cursor_Y[0] == 4 || IO_Result == 0x200) {
                if (from_option) {
                    /* Return to Option_Select */
                    Return_Option_Mode_Sub(task_ptr);
                } else {
                    /* Return to Mode_Select */
                    Menu_Suicide[0] = 0;
                    Menu_Suicide[1] = 1;
                    task_ptr->r_no[1] = 1;
                    task_ptr->r_no[2] = 0;
                    task_ptr->r_no[3] = 0;
                    task_ptr->free[0] = 0;
                }
                break;
            }

            task_ptr->r_no[2] += 1;
            task_ptr->free[0] = 0;

            break;
        }

        break;

    default:
        Exit_Sub(task_ptr, 1, Menu_Cursor_Y[0] + 0x11);
        break;
    }
}

/** @brief Direction Menu page â€” per-character dipswitch sub-pages. */
static void Direction_Menu(struct _TASK* task_ptr) {
    Menu_Cursor_Y[1] = Menu_Cursor_Y[0];

    switch (task_ptr->r_no[2]) {
    case 0:
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->timer = 5;
        Menu_Suicide[1] = 1;
        Menu_Suicide[2] = 0;
        Menu_Page = 0;
        Menu_Page_Buff = Menu_Page;
        Message_Data->kind_req = 3;
        if (use_rmlui && rmlui_menu_sysdir)
            rmlui_sysdir_enter_subpage();
        break;

    case 1:
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;
        if (!use_rmlui || !rmlui_menu_sysdir)
            Setup_Next_Page(task_ptr, 0);
        else {
            /* RmlUi mode: do data setup only — skip effect spawning */
            Menu_Page_Buff = Menu_Page;
            effect_work_init();
            Menu_Common_Init();
            Menu_Cursor_Y[0] = 0;
            Menu_Max = Page_Data[Menu_Page];
            system_dir[1].contents[Menu_Page][Menu_Max] = 1;
        }
        /* fallthrough */

    case 2:
        FadeOut(1, 0xFF, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2] += 1;
            FadeInit();
        }

        break;

    case 3:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
        }

        break;

    case 4:
        Pause_ID = 0;

        Dir_Move_Sub(task_ptr, 0);

        if (IO_Result == 0) {
            Pause_ID = 1;
            Dir_Move_Sub(task_ptr, 1);
        }

        if (Menu_Cursor_Y[1] != Menu_Cursor_Y[0]) {
            SE_cursor_move();
            system_dir[1].contents[Menu_Page][Menu_Max] = 1;

            if (Menu_Cursor_Y[0] < Menu_Max) {
                Message_Data->order = 1;
                Message_Data->request = Menu_Page * 0xC + Menu_Cursor_Y[0] * 2 + 1;
                Message_Data->timer = 2;

                if (msgSysDirTbl[0]->msgNum[Menu_Page * 0xC + Menu_Cursor_Y[0] * 2 + 1] == 1) {
                    Message_Data->pos_y = 0x36;
                } else {
                    Message_Data->pos_y = 0x3E;
                }
            } else {
                Message_Data->order = 1;
                Message_Data->request = system_dir[1].contents[Menu_Page][Menu_Max] + 0x74;
                Message_Data->timer = 2;
                Message_Data->pos_y = 0x36;
            }
        }

        switch (IO_Result) {
        case 0x200:
            task_ptr->r_no[2] += 1;
            Menu_Suicide[0] = 0;
            Menu_Suicide[1] = 0;
            Menu_Suicide[2] = 1;
            SE_dir_selected();
            if (use_rmlui && rmlui_menu_sysdir)
                rmlui_sysdir_exit_subpage();
            break;

        case 0x80:
        case 0x800:
            task_ptr->r_no[2] = 1;
            task_ptr->timer = 5;

            if (--Menu_Page < 0) {
                Menu_Page = (s8)Page_Max;
            }

            SE_dir_selected();
            break;

        case 0x40:
        case 0x400:
            task_ptr->r_no[2] = 1;
            task_ptr->timer = 5;

            if (++Menu_Page > Page_Max) {
                Menu_Page = 0;
            }

            SE_dir_selected();
            break;

        case 0x100:
            if (Menu_Cursor_Y[0] == Menu_Max) {
                switch (system_dir[1].contents[Menu_Page][Menu_Max]) {
                case 0:
                    task_ptr->r_no[2] = 1;
                    task_ptr->timer = 5;

                    if (--Menu_Page < 0) {
                        Menu_Page = (s8)Page_Max;
                    }

                    break;

                case 2:
                    task_ptr->r_no[2] = 1;
                    task_ptr->timer = 5;

                    if (++Menu_Page > Page_Max) {
                        Menu_Page = 0;
                    }

                    break;

                default:
                    task_ptr->r_no[2] += 1;
                    Menu_Suicide[0] = 0;
                    Menu_Suicide[1] = 0;
                    Menu_Suicide[2] = 1;
                    break;
                }

                SE_selected();
                break;
            }

            break;
        }

        break;

    default:
        Exit_Sub(task_ptr, 2, 5);
        break;
    }
}

/** @brief Load Replay data screen â€” file select and playback setup. */
static void Load_Replay(struct _TASK* task_ptr) {
    Menu_Cursor_X[1] = Menu_Cursor_X[0];
    Clear_Flash_Sub();

    switch (task_ptr->r_no[2]) {
    case 0:
        Menu_in_Sub(task_ptr);
        Menu_Cursor_X[0] = 0;
        Setup_BG(1, 0x200, 0);
        Setup_Replay_Sub(1, 0x6E, 9, 1);
        Clear_Flash_Init(4);
        Message_Data->kind_req = 5;
        break;

    case 1:
        if (Menu_Sub_case1(task_ptr) != 0) {
            ReplayPicker_Open(0); /* load mode */
        }

        break;

    case 2:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
            task_ptr->free[3] = 0;
            Menu_Cursor_X[0] = Setup_Final_Cursor_Pos(0, 8);
        }

        break;

    case 3: {
        int pick_result = ReplayPicker_Update();
        if (pick_result == 0) {
            int slot = ReplayPicker_GetSelectedSlot();
            if (NativeSave_LoadReplay(slot) == 0) {
                Decide_ID = 0;
                if (Interface_Type[0] == 0) {
                    Decide_ID = 1;
                }
                task_ptr->r_no[2] += 1;
                task_ptr->r_no[3] = 0;
            } else {
                IO_Result = 0x200;
                Load_Replay_MC_Sub(task_ptr, 0);
            }
        } else if (pick_result == -1) {
            IO_Result = 0x200;
            Load_Replay_MC_Sub(task_ptr, 0);
        }
        break;
    }

    case 4:
        Load_Replay_Sub(task_ptr);
        break;
    }
}

const u8 Setup_Index_64[10] = { 1, 2, 3, 3, 4, 5, 6, 7, 8, 8 };

/** @brief Game Options screen (difficulty, time, rounds, etc). */
static void Game_Option(struct _TASK* task_ptr) {
    s16 char_index;
    s16 ix;

    s16 unused_s3;
    s16 unused_s2;

    switch (task_ptr->r_no[2]) {
    case 0:
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->timer = 5;
        Menu_Common_Init();
        Menu_Cursor_Y[0] = 0;
        Menu_Suicide[1] = 1;
        Menu_Suicide[2] = 0;
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Order[0x4F] = 4;
        Order_Timer[0x4F] = 1;
        Order[0x4E] = 2;
        Order_Dir[0x4E] = 2;
        Order_Timer[0x4E] = 1;
        effect_57_init(0x6A, 7, 0, 0x3F, 2);
        Order[0x6A] = 1;
        Order_Dir[0x6A] = 8;
        Order_Timer[0x6A] = 1;

        if (use_rmlui && rmlui_menu_game_option) {
            rmlui_game_option_show();
        } else {
            for (ix = 0, unused_s3 = char_index = 0x19; ix < 0xC; ix++, unused_s2 = char_index++) {
                effect_61_init(0, ix + 0x50, 0, 2, char_index, ix, 0x70A7);
                Order[ix + 0x50] = 1;
                Order_Dir[ix + 0x50] = 4;
                Order_Timer[ix + 0x50] = ix + 0x14;
            }

            for (ix = 0; ix < 0xA; ix++) {
                effect_64_init(ix + 0x5D, 0, 2, Setup_Index_64[ix], ix, 0x70A7, ix + 1, 0, 0);
                Order[ix + 0x5D] = 1;
                Order_Dir[ix + 0x5D] = 4;
                Order_Timer[ix + 0x5D] = ix + 0x14;
            }
        }

        Menu_Cursor_Move = 0xA;

        break;

    case 1:
        Menu_Sub_case1(task_ptr);
        break;

    case 2:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
            Suicide[3] = 0;
        }

        break;

    case 3:
        Game_Option_Sub(0);
        Button_Exit_Check(task_ptr, 0);
        Game_Option_Sub(1);
        Button_Exit_Check(task_ptr, 1);
        Save_Game_Data();
        break;

    default:
        Exit_Sub(task_ptr, 2, 5);
        break;
    }
}

/** @brief Button Config screen â€” remap controller buttons. */
static void Button_Config(struct _TASK* task_ptr) {
    s16 ix;
    s16 disp_index;

    switch (task_ptr->r_no[2]) {
    case 0:
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->timer = 5;
        Menu_Common_Init();
        pp_operator_check_flag(0);
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Menu_Suicide[1] = 1;
        Menu_Suicide[2] = 0;
        Copy_Key_Disp_Work();
        Order[0x4F] = 4;
        Order_Timer[0x4F] = 1;
        Order[0x4E] = 2;
        Order_Dir[0x4E] = 2;
        Order_Timer[0x4E] = 1;
        effect_57_init(0x6B, 2, 0, 0x3F, 2);
        Order[0x6B] = 1;
        Order_Dir[0x6B] = 8;
        Order_Timer[0x6B] = 1;

        if (use_rmlui && rmlui_menu_button_config) {
            rmlui_button_config_show();
        } else {
            for (ix = 0; ix < 12; ix++) {
                effect_23_init(0, ix + 0x50, 0, 2, 2, ix, 0x70A7, ix + 9, 1);
                Order[ix + 0x50] = 1;
                Order_Dir[ix + 0x50] = 4;
                Order_Timer[ix + 0x50] = ix + 0x14;
                effect_23_init(1, ix + 0x5C, 0, 2, 3, ix, 0x70A7, ix + 9, 1);
                Order[ix + 0x5C] = 1;
                Order_Dir[ix + 0x5C] = 4;
                Order_Timer[ix + 0x5C] = ix + 0x14;
            }

            for (ix = 0; ix < 9; ix++) {
                if (ix == 8) {
                    disp_index = 1;
                } else {
                    disp_index = 0;
                }

                effect_23_init(0, ix + 0x78, 0, 2, disp_index, ix, 0x70A7, ix, 0);
                Order[ix + 0x78] = 1;
                Order_Dir[ix + 0x78] = 4;
                Order_Timer[ix + 0x78] = ix + 0x14;
                effect_23_init(1, ix + 0x81, 0, 2, disp_index, ix, 0x70A7, ix, 0);
                Order[ix + 0x81] = 1;
                Order_Dir[ix + 0x81] = 4;
                Order_Timer[ix + 0x81] = ix + 0x14;
            }

            Menu_Cursor_Move = 0x22;
            effect_66_init(0x8A, 7, 2, 0, -1, -1, -0x7FFF);
            Order[0x8A] = 1;
            Order_Dir[0x8A] = 4;
            Order_Timer[0x8A] = 0x14;
            effect_66_init(0x8B, 8, 2, 0, -1, -1, -0x7FFF);
            Order[0x8B] = 1;
            Order_Dir[0x8B] = 4;
            Order_Timer[0x8B] = 0x14;
        }
        break;

    case 1:
        Menu_Sub_case1(task_ptr);
        break;

    case 2:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
            Suicide[3] = 0;
        }

        break;

    case 3:
        Button_Config_Sub(0);
        Button_Exit_Check(task_ptr, 0);
        Button_Config_Sub(1);
        Button_Exit_Check(task_ptr, 1);
        Save_Game_Data();
        break;
    }
}

/** @brief Sound Test menu â€” BGM / SE / voice playback. */
static void Sound_Test(struct _TASK* task_ptr) {
    s16 char_index;
    s16 ix;
    u8 last_mode;

    Clear_Flash_Sub();

    switch (task_ptr->r_no[2]) {
    case 0:
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->timer = 5;
        setupAlwaysSeamlessFlag(((plsw_00[0] | plsw_00[1]) & 0x4000) != 0);
        Clear_Flash_Init(4);
        Menu_Common_Init();
        Menu_Cursor_Y[0] = 0;
        Menu_Suicide[1] = 1;
        Menu_Suicide[2] = 0;
        Convert_Buff[3][1][5] = 0;

        if (sys_w.sound_mode == 0) {
            Convert_Buff[3][1][0] = 0;
        } else {
            Convert_Buff[3][1][0] = 1;
        }

        if (sys_w.bgm_type == BGM_ARRANGED) {
            Convert_Buff[3][1][3] = 0;
        } else {
            Convert_Buff[3][1][3] = 1;
        }

        Convert_Buff[3][1][7] = 1;
        Order[0x4F] = 4;
        Order_Timer[0x4F] = 1;
        Order[0x4E] = 2;
        Order_Dir[0x4E] = 2;
        Order_Timer[0x4E] = 1;

        if (use_rmlui && rmlui_menu_sound) {
            rmlui_sound_menu_show();
        } else {
            effect_57_init(0x72, 4, 0, 0x3F, 2);
            Order[0x72] = 1;
            Order_Dir[0x72] = 8;
            Order_Timer[0x72] = 1;
            effect_04_init(2, 6, 2, 0x48);

            {
                s32 ixSoundMenuItem[4] = { 10, 11, 11, 12 };

                for (ix = 0; ix < 4; ix++) {
                    Order[ix + 0x57] = 1;
                    Order_Dir[ix + 0x57] = 4;
                    Order_Timer[ix + 0x57] = ix + 0x14;
                    effect_64_init(ix + 0x57, 0, 2, ixSoundMenuItem[ix] + 1, ix, 0x7047, ix + 0xC, 3, 1);
                }
            }

            Order_Dir[0x78] = 0;
            effect_A8_init(0, 0x78, 0, 2, 5, 0x70A7, 0);
            Order_Dir[0x79] = 1;
            effect_A8_init(0, 0x79, 0, 2, 5, 0x70A7, 1);
            effect_A8_init(3, 0x7A, 0, 2, 5, 0x70A7, 3);
            Convert_Buff[3][1][5] = 0;
            Order_Dir[0x7B] = 0;
            effect_A8_init(2, 0x7B, 0, 2, 5, 0x70A7, 2);

            {
                s16 unused_s2;
                s16 unused_s3;

                for (ix = 0, unused_s3 = char_index = 0x3B; ix < 7; ix++, unused_s2 = char_index++) {
                    effect_61_init(0, ix + 0x50, 0, 2, char_index, ix, 0x7047);
                    Order[ix + 0x50] = 1;
                    Order_Dir[ix + 0x50] = 4;
                    Order_Timer[ix + 0x50] = ix + 0x14;
                }
            }
        }

        Menu_Cursor_Move = 5;
        break;

    case 1:
        Menu_Sub_case1(task_ptr);
        break;

    case 2:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
            Suicide[3] = 0;
        }

        break;

    case 3:
        last_mode = Convert_Buff[3][1][0];
        Sound_Cursor_Sub(0);

        if (IO_Result == 0) {
            Sound_Cursor_Sub(1);
        }

        if ((Menu_Cursor_Y[0] == 4) && (IO_Result == 0x100)) {
            SE_selected();
            Convert_Buff[3][1][0] = 0;
            Convert_Buff[3][1][1] = 0xF;
            Convert_Buff[3][1][2] = 0xF;
            Convert_Buff[3][1][3] = 0;
        }

        if (bgm_level != (s16)Convert_Buff[3][1][1]) {
            bgm_level = Convert_Buff[3][1][1];
            save_w[Present_Mode].BGM_Level = Convert_Buff[3][1][1];
            SsBgmHalfVolume(0);
        }

        if (se_level != (s16)Convert_Buff[3][1][2]) {
            se_level = Convert_Buff[3][1][2];
            setSeVolume(save_w[Present_Mode].SE_Level = Convert_Buff[3][1][2]);
        }

        save_w[Present_Mode].BgmType = Convert_Buff[3][1][3];

        if (sys_w.bgm_type != Convert_Buff[3][1][3]) {
            sys_w.bgm_type = Convert_Buff[3][1][3];
            Convert_Buff[3][1][5] = 0;
            BGM_Request_Code_Check(0x41);
        }

        Order_Dir[0x7B] = Convert_Buff[3][1][5];
        Setup_Sound_Mode(last_mode);
        Save_Game_Data();

        if (Menu_Cursor_Y[0] == 5) {
            if (IO_Result == 0x100) {
                SsRequest((u16)Order_Dir[0x7B] + 1);
                Convert_Buff[3][1][7] = 1;
                return;
            }

            if ((IO_Result == 0x200) && Convert_Buff[3][1][7]) {
                Convert_Buff[3][1][7] = 0;
                BGM_Stop();
                return;
            }
        }

        if (IO_Result == 0x200 || ((Menu_Cursor_Y[0] == 6) && (IO_Result == 0x100 || IO_Result == 0x4000))) {
            SE_selected();
            Return_Option_Mode_Sub(task_ptr);
            setupAlwaysSeamlessFlag(0);
            Order[0x72] = 4;
            Order_Timer[0x72] = 4;
            BGM_Request_Code_Check(0x41);
        }

        break;
    }
}

/** @brief Memory Card management menu. */
static void Memory_Card(struct _TASK* task_ptr) {
    s16 ix;
    s16 char_index;

    s16 unused_s3;
    s16 unused_s2;

    switch (task_ptr->r_no[2]) {
    case 0:
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->timer = 5;
        Menu_Common_Init();
        Menu_Cursor_Y[0] = 0;
        Menu_Suicide[1] = 1;
        Menu_Suicide[2] = 0;
        Order[0x4F] = 4;
        Order_Timer[0x4F] = 1;
        Order[0x4E] = 2;
        Order_Dir[0x4E] = 4;
        Order_Timer[0x4E] = 1;

        if (use_rmlui && rmlui_menu_memory_card) {
            rmlui_memory_card_show();
        } else {
            effect_57_init(0x69, 5, 0, 0x3F, 2);
            Order[0x69] = 1;
            Order_Dir[0x69] = 8;
            Order_Timer[0x69] = 1;

            for (ix = 0, unused_s3 = char_index = 0x15; ix < 4; ix++, unused_s2 = char_index++) {
                effect_61_init(0, ix + 0x50, 1, 2, char_index, ix, 0x7047);
                Order[ix + 0x50] = 1;
                Order_Dir[ix + 0x50] = 4;
                Order_Timer[ix + 0x50] = ix + 0x14;
            }

            Menu_Cursor_Move = 4;
            effect_64_init(0x61, 1, 2, 0, 2, 0x7047, 0, 3, 0);
            Order[0x61] = 1;
            Order_Dir[0x61] = 4;
            Order_Timer[0x61] = 0x18;
            effect_66_init(0x8A, 8, 2, 1, -1, -1, -0x7FF5);
            Order[0x8A] = 3;
            Order_Timer[0x8A] = 1;
            effect_04_init(2, 2, 2, 0x48);
        }
        Setup_File_Property(0, 0xFF);
        break;

    case 1:
        Menu_Sub_case1(task_ptr);
        break;

    case 2:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
            Suicide[3] = 0;
        }

        break;

    case 3:
        Memory_Card_Sub(0);
        Button_Exit_Check(task_ptr, 0);

        if (IO_Result == 0) {
            Memory_Card_Sub(1);
            Button_Exit_Check(task_ptr, 0);
        }

        break;

    case 4:
    case 5:
    case 6:
        Save_Load_Menu(task_ptr);
        break;
    }
}

/** @brief Compute final cursor position for multi-column menus. */
s32 Setup_Final_Cursor_Pos(s8 cursor_x, s16 dir) {
    s16 ix;
    s16 check_x[2];
    s16 next_dir;

    if (cursor_x == -1) {
        cursor_x = 0;
    }

    if (vm_w.Connect[cursor_x]) {
        return cursor_x;
    }

    check_x[0] = cursor_x ^ 1;

    if (vm_w.Connect[check_x[0]]) {
        return check_x[0];
    }

    if (dir == 4) {
        next_dir = -2;
    } else {
        next_dir = 2;
    }

    check_x[0] = cursor_x;

    for (ix = 0; ix < 4; ix++) {
        check_x[0] += next_dir;

        if (check_x[0] < 0) {
            if (IO_Result == 0) {
                check_x[0] += 8;
            } else {
                return Menu_Cursor_X[1];
            }
        }

        if (check_x[0] > 7) {
            if (IO_Result == 0) {
                check_x[0] -= 8;
            } else {
                return Menu_Cursor_X[1];
            }
        }

        if (vm_w.Connect[check_x[0]]) {
            return check_x[0];
        }

        check_x[1] = check_x[0] ^ 1;

        if (vm_w.Connect[check_x[1]]) {
            return check_x[1];
        }
    }

    return -1;
}

/** @brief Generic exit sub-routine â€” fade and transition to next routine. */
s32 Exit_Sub(struct _TASK* task_ptr, s16 cursor_ix, s16 next_routine) {
    switch (task_ptr->free[0]) {
    case 0:
        task_ptr->free[0] += 1;
        FadeInit();
        /* fallthrough */

    case 1:
        if (FadeOut(1, 0x19, 8) != 0) {
            task_ptr->r_no[1] = next_routine;
            task_ptr->r_no[2] = 0;
            task_ptr->r_no[3] = 0;
            task_ptr->free[0] = 0;
            Cursor_Y_Pos[0][cursor_ix] = Menu_Cursor_Y[0];
            Cursor_Y_Pos[1][cursor_ix] = Menu_Cursor_Y[1];
            pulpul_stop();
            return 1;
        }

    default:
        return 0;
    }
}

const u8 Menu_Deley_Time[6] = { 15, 10, 6, 15, 15, 15 };

/** @brief Common menu init â€” reset cursors, clear effects, set timers. */
void Menu_Common_Init() {
    s16 ix;

    for (ix = 0; ix < 2; ix++) {
        Deley_Shot_No[ix] = 0;
        Deley_Shot_Timer[ix] = Menu_Deley_Time[Deley_Shot_No[ix]];
    }

    Menu_Cursor_Move = 0;
    r_no_plus = 0;
}

/** @brief Read and debounce menu lever input for a player. */
u16 Check_Menu_Lever(u8 PL_id, s16 type) {
    u16 sw;
    u16 lever;
    u16 ix;

    sw = ~plsw_01[PL_id] & plsw_00[PL_id];

    if (type) {
        sw = ~PLsw[PL_id][1] & PLsw[PL_id][0];
    }

    lever = plsw_00[PL_id] & SWK_DIRECTIONS;

    if (sw & (SWK_ATTACKS | SWK_START)) {
        return sw;
    }

    sw &= SWK_DIRECTIONS;

    if (sw) {
        return sw;
    }

    if (lever == 0) {
        Deley_Shot_No[PL_id] = 0;
        Deley_Shot_Timer[PL_id] = Menu_Deley_Time[Deley_Shot_No[PL_id]];
        return 0;
    }

    if (--Deley_Shot_Timer[PL_id] == 0) {
        if (++Deley_Shot_No[PL_id] > 2) {
            Deley_Shot_No[PL_id] = 2;
        }

        if (lever & (SWK_UP | SWK_DOWN)) {
            ix = 0;
        } else {
            ix = 3;
        }

        if (Deley_Shot_No[PL_id] + ix >= MENU_DELAY_COUNT) {
            return 0;
        }

        Deley_Shot_Timer[PL_id] = Menu_Deley_Time[Deley_Shot_No[PL_id] + ix];
        return lever;
    }

    return 0;
}

/** @brief Suspend-menu stub (no-op). */
static void Suspend_Menu(struct _TASK* /* unused */) {
    // Do nothing
}

/** @brief In-game state â€” delegate to game task. */
static void In_Game(struct _TASK* task_ptr) {
    void (*In_Game_Jmp_Tbl[IN_GAME_JMP_COUNT])() = {
        Menu_Init, Menu_Select, Button_Config_in_Game, Character_Change, Pad_Come_Out
    };

    if (task_ptr->r_no[1] >= IN_GAME_JMP_COUNT) {
        return;
    }

    In_Game_Jmp_Tbl[task_ptr->r_no[1]](task_ptr);
}

/** @brief Write BG extras for menu backgrounds (type-based). */
static void bg_etc_write_ex(s16 type) {
    u8 i;

    Family_Init();
    Scrn_Pos_Init();
    Zoomf_Init();
    scr_sc = 1.0f;
    bg_w.bg_opaque = 224;
    bg_w.pos_offset = 192;

    for (i = 0; i < 7; i++) {
        bg_w.bgw[i].pos_x_work = 0;
        bg_w.bgw[i].pos_y_work = 0;
        bg_w.bgw[i].zuubun = 0;
        bg_w.bgw[i].xy[0].cal = 0;
        bg_w.bgw[i].xy[1].cal = 0;
        bg_w.bgw[i].wxy[0].cal = 0;
        bg_w.bgw[i].wxy[1].cal = 0;
        bg_w.bgw[i].hos_xy[0].cal = 0;
        bg_w.bgw[i].hos_xy[1].cal = 0;
        bg_w.bgw[i].rewrite_flag = 0;
        bg_w.bgw[i].fam_no = i;
        bg_w.bgw[i].speed_x = 0;
        bg_w.bgw[i].speed_y = 0;
        bg_w.bgw[i].r_no_1 = bg_w.bgw[i].r_no_2 = 0;
    }

    bg_w.scr_stop = 0;
    bg_w.frame_flag = 0;
    bg_w.old_chase_flag = bg_w.chase_flag = 0;
    bg_w.bg_f_x = 64;
    bg_w.bg_f_y = 64;
    bg_w.bg2_sp_x2 = bg_w.bg2_sp_x = 0;
    bg_w.max_x = 8;
    bg_w.quake_x_index = 0;
    bg_w.quake_y_index = 0;

    for (i = 0; i <= 0; i++) {
        bg_w.bgw[i].hos_xy[0].cal = bg_w.bgw[i].wxy[0].cal = bg_w.bgw[i].xy[0].cal = bg_pos_tbl2[type][i][0];
        bg_w.bgw[i].hos_xy[1].cal = bg_w.bgw[i].wxy[1].cal = bg_w.bgw[i].xy[1].cal = bg_pos_tbl2[type][i][1];
        bg_w.bgw[i].pos_y_work = bg_w.bgw[i].xy[1].disp.pos;
        bg_w.bgw[i].old_pos_x = bg_w.bgw[i].pos_x_work = bg_w.bgw[i].xy[0].disp.pos;
        bg_w.bgw[i].speed_x = msp2[type][i][0];
        bg_w.bgw[i].speed_y = msp2[type][i][1];
        bg_w.bgw[i].rewrite_flag = 0;
        bg_w.bgw[i].zuubun = 0;
        bg_w.bgw[i].frame_deff = 64;
        bg_w.bgw[i].max_x_limit = bg_w.bgw[i].speed_x * bg_w.max_x;
    }

    base_y_pos = 40;
}

/** @brief Wait for save/load I/O completion before proceeding. */
static void Wait_Load_Save(struct _TASK* task_ptr) {
    s16 ix;

    switch (task_ptr->free[1]) {
    case 0:
        if (vm_w.Request != 0) {
            break;
        }

        task_ptr->free[0] = 0;
        task_ptr->free[1]++;

        if (task_ptr->r_no[1] == 5) {
            task_ptr->free[2] = 18;
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
            Menu_Suicide[ix] = 1;
        }

        switch (task_ptr->r_no[1]) {
        case 13:
            ix = 105;
            break;

        case 17:
            task_ptr->r_no[2] = 99;
            /* fallthrough */

        case 6:
            ix = 110;
            break;

        case 19:
        case 20:
            ix = 112;
            break;

        case 23:
            ix = 105;
            task_ptr->r_no[0] = 0;
            task_ptr->r_no[2] = 99;
            task_ptr->free[0] = 1;
            task_ptr->free[1] = 8;
            break;
        }

        Order[ix] = 4;
        Order_Timer[ix] = 1;
        break;

    case 2:
        FadeOut(1, 0xFF, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[0] = 0;
        }

        break;
    }
}

/** @brief Display auto-save notification. */
static void Disp_Auto_Save(struct _TASK* task_ptr) {
    void (*Auto_Save_Jmp_Tbl[AUTO_SAVE_JMP_COUNT])() = { DAS_1st, DAS_2nd, DAS_3rd, DAS_4th };

    if (task_ptr->r_no[1] >= AUTO_SAVE_JMP_COUNT) {
        return;
    }

    Auto_Save_Jmp_Tbl[task_ptr->r_no[1]](task_ptr);
}

/** @brief Auto-save step 1 â€” initiate save process. */
static void DAS_1st(struct _TASK* task_ptr) {
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[1]++;
    task_ptr->timer = 5;
    Order[0x4E] = 2;
    Order_Dir[0x4E] = 0;
    Order_Timer[0x4E] = 1;
    effect_66_init(0x8A, 8, 0, 0, -1, -1, -0x7FFD);
    Order[0x8A] = 3;
    Order_Timer[0x8A] = 1;
}

/** @brief Auto-save step 2 â€” wait for I/O completion. */
static void DAS_2nd(struct _TASK* task_ptr) {
    FadeOut(1, 0xFF, 8);

    if ((task_ptr->timer -= 1) == 0) {
        task_ptr->r_no[1]++;
        FadeInit();
        NativeSave_SaveOptions();
    }
}

/** @brief Auto-save step 3 â€” display completion message. */
static void DAS_3rd(struct _TASK* task_ptr) {
    if (FadeIn(1, 0x19, 8) != 0) {
        task_ptr->r_no[1]++;
    }
}

/** @brief Auto-save step 4 â€” fade and return. */
static void DAS_4th(struct _TASK* task_ptr) {
    /* NativeSave_SaveOptions() is synchronous, so always proceed */
    task_ptr->r_no[0] = 0;
    task_ptr->r_no[1] = 1;
    task_ptr->r_no[2] = 0;
    task_ptr->r_no[3] = 0;
    Forbid_Reset = 0;
}

/** @brief Display auto-save notification (variant 2). */
static void Disp_Auto_Save2(struct _TASK* task_ptr) {
    void (*Auto_Save2_Jmp_Tbl[AUTO_SAVE_JMP_COUNT])() = { DAS_1st, DAS_2nd, DAS_3rd, DAS2_4th };

    if (task_ptr->r_no[1] >= AUTO_SAVE_JMP_COUNT) {
        return;
    }

    Auto_Save2_Jmp_Tbl[task_ptr->r_no[1]](task_ptr);
}

/** @brief Auto-save variant 2 step 4 â€” fade and return. */
static void DAS2_4th(struct _TASK* task_ptr) {
    /* NativeSave_SaveOptions() is synchronous, so always proceed */
    G_No[2] = 6;
    cpExitTask(TASK_MENU);
    task[TASK_ENTRY].condition = 1;
}

/** @brief Wait for replay check result before proceeding. */
static void Wait_Replay_Check(struct _TASK* task_ptr) {
    switch (task_ptr->free[1]) {
    case 0:
        if (vm_w.Request != 0) {
            break;
        }

        task_ptr->r_no[0] = 0;
        task_ptr->r_no[3] = 0;

        if (vm_w.Number == 0 && vm_w.New_File == 0) {
            task_ptr->r_no[2] = 3;
            break;
        }

        task_ptr->r_no[2] = 5;
        break;
    }
}

/** @brief VS Result screen â€” show match outcome and next-action choices. */
static void VS_Result(struct _TASK* task_ptr) {
    s16 ix;
    s16 char_ix2;
    s16 total_battle;
    u16 ave[2];

    s16 s4;
    s16 s3;

    Clear_Flash_Sub();

    switch (task_ptr->r_no[2]) {
    case 0:
        System_all_clear_Level_B();
        Menu_Init(task_ptr);
        task_ptr->r_no[1] = 16;
        task_ptr->r_no[2] = 1;
        task_ptr->r_no[3] = 0;
        Sel_PL_Complete[0] = 0;
        Sel_Arts_Complete[0] = 0;
        Sel_PL_Complete[1] = 0;
        Sel_Arts_Complete[1] = 0;
        Clear_Flash_Init(4);
        break;

    case 1:
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2]++;
        task_ptr->timer = 5;
        Menu_Common_Init();
        Menu_Cursor_Y[0] = Cursor_Y_Pos[0][0];
        Menu_Cursor_Y[1] = Cursor_Y_Pos[1][0];
        Menu_Suicide[0] = 0;
        Menu_Suicide[1] = 1;
        Menu_Cursor_X[0] = 0;
        Menu_Cursor_X[1] = 0;
        Order[78] = 2;
        Order_Dir[78] = 0;
        Order_Timer[78] = 1;

        /* Compute percentages — always needed */
        total_battle = VS_Win_Record[0] + VS_Win_Record[1];
        if (total_battle == 0) {
            total_battle = 1;
        }
        if (VS_Win_Record[0] >= VS_Win_Record[1]) {
            ave[1] = (VS_Win_Record[1] * 100) / total_battle;
            if (ave[1] == 0 && VS_Win_Record[1] > 0) {
                ave[1] = 1;
            }
            ave[0] = 100 - ave[1];
        } else {
            ave[0] = (VS_Win_Record[0] * 100) / total_battle;
            if (ave[0] == 0 && VS_Win_Record[0] > 0) {
                ave[0] = 1;
            }
            ave[1] = 100 - ave[0];
        }

        if (use_rmlui && rmlui_screen_vs_result) {
            rmlui_vs_result_show(VS_Win_Record[0], VS_Win_Record[1], ave[0], ave[1]);
        } else {
            effect_66_init(91, 12, 0, 0, 71, 9, 0);
            Order[91] = 3;
            Order_Timer[91] = 1;
            effect_66_init(138, 24, 0, 0, -1, -1, -0x7FF9);
            Order[138] = 3;
            Order_Timer[138] = 1;
            effect_66_init(139, 25, 0, 0, -1, -1, -0x7FF9);
            Order[139] = 3;
            Order_Timer[139] = 1;
            effect_A0_init(0, VS_Win_Record[0], 0, 3, 0, 0, 0);
            effect_A0_init(0, VS_Win_Record[1], 1, 3, 0, 0, 0);
            effect_A0_init(0, ave[0], 2, 3, 0, 0, 0);
            effect_A0_init(0, ave[1], 3, 3, 0, 0, 0);

            for (ix = 0, s4 = char_ix2 = 22; ix < 3; ix++, s3 = char_ix2++) {
                effect_91_init(0, ix, 0, 71, char_ix2, 0);
                effect_91_init(1, ix, 0, 71, char_ix2, 0);
            }

            Setup_Win_Lose_OBJ();
        }
        Menu_Cursor_Move = 0;
        break;

    case 2:
        FadeOut(1, 0xFF, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2]++;
            FadeInit();
        }

        break;

    case 3:
        if (FadeIn(1, 25, 8)) {
            task_ptr->r_no[2]++;
            Suicide[3] = 0;
        }

        break;

    case 4:
        if (VS_Result_Select_Sub(task_ptr, 0) == 0) {
            VS_Result_Select_Sub(task_ptr, 1);
        }

        break;

    case 5:
        if (task_ptr->r_no[3] == 0) {
            if (--task_ptr->timer == 0) {
                task_ptr->r_no[3]++;
            }

            break;
        }

        Exit_Sub(task_ptr, 0, 17);
        break;

    case 6:
        switch (task_ptr->r_no[3]) {
        case 0:
            task_ptr->r_no[3]++;
            /* fallthrough */

        case 1:
            if (--task_ptr->timer) {
                break;
            }

            Setup_VS_Mode(task_ptr);
            G_No[1] = 12;
            G_No[2] = 1;
            Mode_Type = MODE_VERSUS;
            break;
        }

        break;

    case 7:
    default:
        Netplay_HandleMenuExit();

        if (Exit_Sub(task_ptr, 0, 0)) {
            System_all_clear_Level_B();
            BGM_Request_Code_Check(65);
        }

        break;
    }
}

/** @brief Save Replay screen â€” write match replay to card. */
static void Save_Replay(struct _TASK* task_ptr) {
    Menu_Cursor_X[1] = Menu_Cursor_X[0];
    Clear_Flash_Sub();

    switch (task_ptr->r_no[2]) {
    case 0:
        Setup_Save_Replay_1st(task_ptr);
        break;

    case 1:
        if (Menu_Sub_case1(task_ptr) != 0) {
            ReplayPicker_Open(1); /* save mode */
        }
        Order[0x4E] = 2;
        Order_Dir[0x4E] = 0;
        Order_Timer[0x4E] = 1;
        break;

    case 2:
        Setup_Save_Replay_2nd(task_ptr, 1);
        break;

    case 3: {
        int pick_result = ReplayPicker_Update();
        if (pick_result == 0) {
            int slot = ReplayPicker_GetSelectedSlot();
            NativeSave_SaveReplay(slot);
        }
        if (pick_result != 1) { /* done or cancelled */
            IO_Result = 0x200;
            Save_Replay_MC_Sub(task_ptr, 0);
        }
        break;
    }
    }
}

/** @brief Save Replay step 2 â€” execute memory-card write. */
void Setup_Save_Replay_2nd(struct _TASK* task_ptr, s16 arg1) {
    if (FadeIn(1, 25, 8)) {
        task_ptr->r_no[2]++;
        task_ptr->free[3] = 0;
        Menu_Cursor_X[0] = Setup_Final_Cursor_Pos(Menu_Cursor_X[0], 8);
    }
}

/** @brief Set up replay parameters (type, character, master player). */
void Setup_Replay_Sub(s16 /* unused */, s16 type, s16 char_type, s16 master_player) {
    effect_57_init(type, char_type, 0, 63, 2);
    Order[type] = 1;
    Order_Dir[type] = 8;
    Order_Timer[type] = 1;
    effect_66_init(138, 8, master_player, 0, -1, -1, -0x7FF4);
    Order[138] = 3;
    Order_Timer[138] = 1;
}

/** @brief Wait-in-pause state for training mode. */
static void Wait_Pause_in_Tr(struct _TASK* task_ptr) {
    u16 ans;
    u16 ix;

    Training_Data_Disp();
    Control_Player_Tr();

    if (End_Training) {
        Next_Be_Tr_Menu(task_ptr);
        return;
    }

    switch (task_ptr->r_no[1]) {
    case 0:
        if (Allow_a_battle_f) {
            task_ptr->r_no[1]++;

            if (Present_Mode == 4) {
                Disp_Attack_Data = Training->contents[0][1][1];
            } else {
                Disp_Attack_Data = 0;
            }
        } else {
            Disp_Attack_Data = 0;
        }

        /* fallthrough */

    case 1:
        if (Allow_a_battle_f == 0 || Extra_Break != 0) {
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
        if (Interface_Type[Pause_ID] == 0) {
            Setup_Tr_Pause(task_ptr);
            task_ptr->r_no[1] = 3;
            break;
        }

        if (Pause_Down) {
            Flash_1P_or_2P(task_ptr);
        }

        switch (Pause_in_Normal_Tr(task_ptr)) {
        case 1:
            task_ptr->r_no[1] = 0;
            SE_selected();
            Game_pause = 0;
            Pause = 0;
            Pause_Down = 0;
            Disp_Attack_Data = Training->contents[0][1][1];

            for (ix = 0; ix < 4; ix++) {
                Menu_Suicide[ix] = 1;
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
        if (Interface_Type[Pause_ID] == 0) {
            dispControllerWasRemovedMessage(132, 82, 16);
            break;
        }

        Setup_Tr_Pause(task_ptr);
        break;
    }
}

/** @brief Reset training session (reinitialise state). */
static void Reset_Training(struct _TASK* task_ptr) {
    s16 ix;

    switch (task_ptr->r_no[1]) {
    case 0:
        task_ptr->r_no[1]++;
        task_ptr->timer = 10;
        Game_pause = 0x81;
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
        task_ptr->timer = 2;
        effect_work_kill(6, -1);
        move_effect_work(6);

        for (ix = 0; ix < 4; ix++) {
            C_No[ix] = 0;
        }

        C_No[0] = 1;
        G_No[2] = 5;
        G_No[3] = 0;
        seraph_flag = 0;
        BGM_No[0] = 1;
        BGM_Timer[0] = 1;
        G_Timer = 10;
        Cover_Timer = 5;
        Suicide[0] = 1;
        Suicide[6] = 1;
        judge_flag = 0;
        Lever_LR[0] = 0;
        Lever_LR[1] = 0;
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

/** @brief Reset replay session (reinitialise state). */
static void Reset_Replay(struct _TASK* task_ptr) {
    switch (task_ptr->r_no[1]) {
    case 0:
        task_ptr->r_no[1]++;
        task_ptr->timer = 10;
        Game_pause = 0x81;
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
        task_ptr->timer = 2;
        G_No[2] = 2;
        G_No[3] = 0;
        seraph_flag = 0;
        G_Timer = 10;
        Cover_Timer = 5;
        effect_work_kill_mod_plcol();
        move_effect_work(6);
        Suicide[0] = 1;
        Suicide[6] = 1;
        judge_flag = 0;
        cpExitTask(TASK_PAUSE);
        break;

    default:
        Switch_Screen(0);

        if (--task_ptr->timer == 0) {
            cpExitTask(TASK_MENU);
        }

        break;
    }
}

/** @brief Training Menu dispatch â€” jump to selected training sub-screen. */
static void Training_Menu(struct _TASK* task_ptr) {
    void (*Training_Jmp_Tbl[TRAINING_JMP_COUNT])() = { Training_Init,    Normal_Training,   Blocking_Training,
                                                       Dummy_Setting,    Training_Option,   Button_Config_Tr,
                                                       Character_Change, Blocking_Tr_Option };

    if (task_ptr->r_no[1] >= TRAINING_JMP_COUNT) {
        return;
    }

    Training_Jmp_Tbl[task_ptr->r_no[1]](task_ptr);
    Akaobi();
    ToneDown(0xAA, 2);

    if (Training_Index >= TRAINING_LETTER_COUNT) {
        return;
    }

    SSPutStr_Bigger(
        training_letter_data[Training_Index].pos_x, 0x18, 9, training_letter_data[Training_Index].menu, 1, 2, 1);
}

/** @brief Training initialisation â€” set up menu items and effects. */
static void Training_Init(struct _TASK* task_ptr) {
    ToneDown(0x80, 2);
    Menu_Init(task_ptr);
    task_ptr->r_no[1] = Mode_Type - 2;
    Pause_Down = 1;
    End_Training = 0;
    Demo_Time_Stop = 0;
    Disp_Cockpit = 0;

    if (Mode_Type == MODE_NORMAL_TRAINING) {
        control_player = Champion;
        control_pl_rno = 0x63;
    } else {
        control_player = Champion;
        control_pl_rno = 0;
    }

    Round_num = 0;
    PL_Wins[0] = 0;
    PL_Wins[1] = 0;
    Play_Mode = 0;
    Replay_Status[0] = 0;
    Replay_Status[1] = 0;
}

/** @brief Normal Training sub-menu â€” recording, playback, and settings. */
static void Normal_Training(struct _TASK* task_ptr) {
    s16 ix;
    s16 x;
    s16 y;

    s16 s2;

    Menu_Cursor_Y[1] = Menu_Cursor_Y[0];

    switch (task_ptr->r_no[2]) {
    case 0:
        Training_Init_Sub(task_ptr);
        Training_Index = 0;
        x = 120;
        y = 56;
        Training[0] = Training[2];

        for (ix = 0; ix < 8; ix++, s2 = y += 16) {
            (void)s2;

            effect_A3_init(0, 0, ix, ix, 0, x, y, 0);
        }

        break;

    case 1:
        if (Appear_end < 2) {
            break;
        }

        if (Exec_Wipe) {
            break;
        }

        MC_Move_Sub(Check_Menu_Lever(Decide_ID, 0), 0, 7, 0xFF);
        Check_Skip_Recording();
        Check_Skip_Replay(2);

        switch (IO_Result) {
        case 0x100:
            switch (Menu_Cursor_Y[0]) {
            case 0:
            case 1:
            case 2:
                if (Interface_Type[Champion ^ 1] == 0 && Training[2].contents[0][0][0] == 4) {
                    Training[2].contents[0][0][0] = 0;
                }

                task_ptr->r_no[0] = 10;
                task_ptr->r_no[1] = 0;
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                Menu_Suicide[0] = 1;
                Game_pause = 0;
                Pause_Down = 0;
                Training_Disp_Work_Clear();
                CP_No[0][0] = 0;
                CP_No[1][0] = 0;
                plw[New_Challenger].wu.pl_operator = 1;
                Operator_Status[New_Challenger] = 1;
                Setup_NTr_Data(Menu_Cursor_Y[0]);
                count_cont_init(0);

                switch (Training[0].contents[0][0][0]) {
                case 0:
                    control_pl_rno = 0;
                    control_player = New_Challenger;
                    break;

                case 1:
                    control_pl_rno = 1;
                    control_player = New_Challenger;
                    break;

                case 2:
                    control_pl_rno = 2;
                    control_player = New_Challenger;
                    break;

                case 3:
                    control_pl_rno = 99;
                    plw[New_Challenger].wu.pl_operator = 0;
                    Operator_Status[New_Challenger] = 0;
                    break;

                case 4:
                    control_pl_rno = 99;
                    break;
                }

                All_Clear_Timer();
                Check_Replay();
                Training[0].contents[0][1][3] = Menu_Cursor_Y[0];
                init_omop();
                set_init_A4_flag();
                setup_vitality(&plw[0].wu, My_char[0] + 0);
                setup_vitality(&plw[1].wu, My_char[1] + 0);
                Setup_Training_Difficulty();
                Training_Cursor = Menu_Cursor_Y[0];
                break;

            case 3:
            case 4:
            case 5:
            case 6:
                task_ptr->r_no[1] = Menu_Cursor_Y[0];
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                Training_Cursor = Menu_Cursor_Y[0];
                break;

            case 7:
                Training_Cursor = 7;
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
        Exit_Sub(task_ptr, 0, Menu_Cursor_Y[0] + 1);
        break;
    }
}

/** @brief Dummy Setting sub-menu â€” configure training dummy. */
static void Dummy_Setting(struct _TASK* task_ptr) {
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
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Menu_Suicide[0] = 1;
        Training_Index = 2;

        for (ix = 0, s6 = y = 80; ix < 6; ix++, s5 = y += 16) {
            effect_A3_init(0, 1, ix, ix, 1, 48, y, 0);
        }

        for (ix = 0, y = 80, s4 = group = 2; ix < 5; ix++, group++, s3 = y += 16) {
            effect_A3_init(0, group, ix, ix, 1, 0xE6, y, 0);
        }

        break;

    case 1:
        Dummy_Move_Sub(task_ptr, Champion, 0, 0, 5);

        if (Menu_Cursor_Y[0] == 4 && IO_Result & 0x100) {
            Training[2].contents[0][0][0] = 0;
            Training[2].contents[0][0][1] = 0;
            Training[2].contents[0][0][2] = 0;
            Training[2].contents[0][0][3] = 0;
            SE_selected();
        }

        break;

    case 2:
        SE_selected();
        Menu_Suicide[0] = 0;
        Menu_Suicide[1] = 1;
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        Training_Disp_Sub(task_ptr);
        break;
    }
}

/** @brief Training Option sub-menu â€” configure training parameters. */
static void Training_Option(struct _TASK* task_ptr) {
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
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Menu_Suicide[0] = 1;
        Training_Index = 3;

        for (ix = 0, s6 = y = 72; ix < 6; ix++, s5 = y += 16) {
            effect_A3_init(0, 7, ix, ix, 1, 48, y, 1);
        }

        for (ix = 0, y = 72, s4 = group = 8; ix < 4; ix++, group++, s3 = y += 16) {
            effect_A3_init(0, group, ix, ix, 1, 230, y, 1);
        }

        break;

    case 1:
        Dummy_Move_Sub(task_ptr, Champion, 0, 1, 5);

        if (Menu_Cursor_Y[0] == 4 && IO_Result & 0x100) {
            Default_Training_Option();
            SE_selected();
            break;
        }

        save_w[Present_Mode].Damage_Level = Training[2].contents[0][1][2];
        save_w[Present_Mode].Difficulty = Training[2].contents[0][1][3];
        break;

    case 2:
        SE_selected();
        Menu_Suicide[0] = 0;
        Menu_Suicide[1] = 1;
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        Training_Disp_Sub(task_ptr);
        Training[0] = Training[2];
        break;
    }
}

/** @brief Blocking (parrying) Training sub-menu. */
static void Blocking_Training(struct _TASK* task_ptr) {
    s16 ix;
    s16 x;
    s16 y;
    s16 s2;

    Menu_Cursor_Y[1] = Menu_Cursor_Y[0];

    switch (task_ptr->r_no[2]) {
    case 0:
        Training_Init_Sub(task_ptr);
        Training_Index = 1;
        x = 112;
        y = 72;
        plw[0].wu.pl_operator = 1;
        Operator_Status[0] = 1;
        plw[1].wu.pl_operator = 1;
        Operator_Status[1] = 1;

        for (ix = 0; ix < 6; ix++, s2 = y += 16) {
            (void)s2;

            effect_A3_init(1, 12, ix, ix, 0, x, y, 0);
        }

        break;

    case 1:
        if (Appear_end < 2) {
            break;
        }

        if (Exec_Wipe) {
            break;
        }

        MC_Move_Sub(Check_Menu_Lever(Decide_ID, 0), 0, 5, 0xFF);
        Check_Skip_Replay(1);

        switch (IO_Result) {
        case 0x100:
            switch (Menu_Cursor_Y[0]) {
            case 0:
                Record_Data_Tr = 1;
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
                if (Menu_Cursor_Y[0] == 0) {
                    Play_Mode = 1;
                } else {
                    Play_Mode = 3;
                }

                All_Clear_Timer();
                Check_Replay();

                if (Menu_Cursor_Y[0] == 1) {
                    Replay_Status[Training_ID] = 0;
                    Replay_Status[Training_ID ^ 1] = 3;
                    Training[0] = Training[1];
                    Training[0].contents[1][0][2] = Training[2].contents[1][0][2];
                    Training[0].contents[1][0][3] = Training[2].contents[1][0][3];
                    control_pl_rno = 99;
                }

                task_ptr->r_no[0] = 10;
                task_ptr->r_no[1] = 0;
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                Menu_Suicide[0] = 1;
                Game_pause = 0;
                Pause_Down = 0;
                save_w[Present_Mode].Time_Limit = 60;
                count_cont_init(0);
                Training[0].contents[1][1][3] = Menu_Cursor_Y[0];
                init_omop();
                set_init_A4_flag();
                Training_Cursor = Menu_Cursor_Y[0];
                break;

            case 2:
                task_ptr->r_no[1] = 7;
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                Training_Cursor = 2;
                break;

            case 3:
                Training_Cursor = 3;
                /* fallthrough */

            case 4:
                task_ptr->r_no[1] = Menu_Cursor_Y[0] + 2;
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                break;

            case 5:
                Training_Cursor = 5;
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
        Exit_Sub(task_ptr, 0, Menu_Cursor_Y[0] + 1);
        break;
    }
}

const LetterData training_letter_data[6] = { { 0x82, "NORMAL TRAINING" },   { 0x73, "PARRYING TRAINING" },
                                             { 0x7C, "DUMMY SETTING" },     { 0x87, "TRAINING OPTION" },
                                             { 0x7D, "RECORDING SETTING" }, { 0x8F, "BUTTON CONFIG." } };

/** @brief Blocking Training option screen. */
static void Blocking_Tr_Option(struct _TASK* task_ptr) {
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
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Menu_Suicide[0] = 1;
        Training_Index = 3;
        effect_A3_init(1, 22, 99, 0, 1, 51, 56, 1);
        effect_A3_init(1, 22, 99, 1, 1, 51, 106, 1);

        for (ix = 0, s6 = y = 72; ix < 6; ix++, s5 = y += 16) {
            if (ix == 2) {
                y += 20;
            }

            if (ix == 4) {
                y += 8;
            }

            effect_A3_init(1, 17, ix, ix, 1, 64, y, 0);
        }

        for (ix = 0, y = 72, s4 = group = 18; ix < 4; ix++, group++, s3 = y += 16) {
            if (ix == 2) {
                y += 20;
            }

            effect_A3_init(1, group, ix, ix, 1, 264, y, 0);
        }

        break;

    case 1:
        Dummy_Move_Sub(task_ptr, Champion, 1, 0, 5);

        if (Menu_Cursor_Y[0] == 4 && IO_Result & 0x100) {
            Default_Training_Data(1);
            SE_selected();
        }

        break;

    case 2:
        SE_selected();
        Menu_Suicide[0] = 0;
        Menu_Suicide[1] = 1;
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        Training[0] = Training[2];

        plw[New_Challenger].wu.pl_operator = 1;
        Operator_Status[New_Challenger] = 1;

        switch (Training[0].contents[1][0][0]) {
        case 0:
            control_pl_rno = 0;
            control_player = Champion;
            break;
        case 1:
            control_pl_rno = 1;
            control_player = Champion;
            break;
        case 2:
            control_pl_rno = 2;
            control_player = Champion;
            break;
        }

        Training_Disp_Sub(task_ptr);
        break;
    }
}

/** @brief Character Change screen in training mode. */
static void Character_Change(struct _TASK* task_ptr) {
    s16 ix;

    if (Check_Pad_in_Pause(task_ptr) == 0) {
        switch (task_ptr->r_no[2]) {
        case 0:
            task_ptr->r_no[2]++;
            task_ptr->timer = 0xA;
            Game_pause = 0x81;
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
                Cover_Timer = 0x17;
                G_No[1] = 1;
                G_No[2] = 0;
                G_No[3] = 0;

                for (ix = 0; ix < 2; ix++) {
                    Sel_PL_Complete[ix] = 0;
                    Sel_Arts_Complete[ix] = 0;
                    plw[ix].wu.pl_operator = 1;
                    Operator_Status[ix] = 1;
                }

                cpExitTask(TASK_MENU);
            }
            break;
        }
    }
}

/** @brief Reset training data to defaults (optionally full or partial). */
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
            for (ix3 = 0; ix3 < 4; ix3++) {
                Training[0].contents[ix][ix2][ix3] = 0;
            }
        }
    }

    Training[0].contents[0][1][2] = save_w->Damage_Level;
    Training[0].contents[0][1][3] = save_w->Difficulty;
    save_w[Present_Mode].Damage_Level = save_w->Damage_Level;
    save_w[Present_Mode].Difficulty = save_w->Difficulty;
    Training[2] = Training[0];
    Disp_Attack_Data = 0;
}

/** @brief Wait for replay data to finish loading. */
static void Wait_Replay_Load(struct _TASK* task_ptr) {}

/** @brief After-replay results screen and menu. */
static void After_Replay(struct _TASK* task_ptr) {
    s16 ix;
    s16 char_ix;

    s16 s5;
    s16 s4;
    s16 s3;
    s16 s2;

    switch (task_ptr->r_no[1]) {
    case 0:
        task_ptr->r_no[1]++;
        ToneDown(192, 32);
        Menu_Common_Init();
        Menu_Suicide[0] = 0;
        Menu_Cursor_Y[0] = 0;

        for (ix = 0, s5 = char_ix = '8'; ix < 3; ix++, s4 = char_ix++) {
            effect_61_init(0, ix + 80, 0, 0, char_ix, ix, 0x7047);
            Order[ix + 80] = 3;
            Order_Timer[ix + 80] = 1;
        }

        effect_66_init(138, 38, 0, 0, -1, -1, -0x7FF7);
        Order[138] = 3;
        Order_Timer[138] = 1;
        break;

    case 1:
        ToneDown(192, 32);
        Pause_ID = 0;

        if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, 2, 0xFF) == 0) {
            Pause_ID = 1;
            MC_Move_Sub(Check_Menu_Lever(1, 0), 0, 2, 0xFF);
        }

        switch (IO_Result) {
        case 0x100:
            SE_selected();
            task_ptr->r_no[1] = Menu_Cursor_Y[0] + 2;
            break;

        case 0x200:
            SE_selected();
            task_ptr->r_no[1] = 4;
            break;
        }

        break;

    case 4:
        ToneDown(192, 32);
        Back_to_Mode_Select(task_ptr);
        break;

    case 2:
        ToneDown(192, 32);
        task_ptr->r_no[1] = 12;
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;

    case 12:
        Load_Replay_Sub(task_ptr);
        break;

    case 3:
        task_ptr->free[0] = 0;
        task_ptr->r_no[1] = 5;
        task_ptr->r_no[2] = 0;

    case 5:
        ToneDown(192, 32);

        if (Exit_Sub(task_ptr, 0, 6)) {
            Menu_Suicide[0] = 1;
            Menu_Suicide[1] = Menu_Suicide[2] = Menu_Suicide[3] = 0;
        }

        break;

    case 6:
        ToneDown(232, 32);
        switch (task_ptr->r_no[2]) {
        case 0:
            FadeOut(1, 0xFF, 8);
            task_ptr->r_no[2]++;
            task_ptr->timer = 5;
            Menu_Suicide[0] = 0;
            Menu_Common_Init();
            Menu_Cursor_X[0] = 0;
            Setup_BG(1, 512, 0);
            effect_57_init(110, 9, 0, 63, 999);
            Order[110] = 3;
            Order_Dir[110] = 8;
            Order_Timer[110] = 1;
            Setup_File_Property(1, 0xFF);
            ReplayPicker_Open(1); /* save mode */
            effect_66_init(138, 41, 0, 0, -1, -1, -0x7FF3);
            Order[138] = 3;
            Order_Timer[138] = 1;
            break;

        case 1:
            Menu_Sub_case1(task_ptr);
            break;

        case 2:
            Setup_Save_Replay_2nd(task_ptr, 1);
            break;

        case 3: {
            int pick_result = ReplayPicker_Update();
            if (pick_result == 0) {
                NativeSave_SaveReplay(ReplayPicker_GetSelectedSlot());
            }
            if (pick_result == 1)
                break; /* still active */
        }

            task_ptr->r_no[2]++;
            /* fallthrough */

        case 4:
            Exit_Sub(task_ptr, 0, 7);
            break;
        }

        break;

    case 7:
        FadeOut(1, 0xFF, 8);
        Order[110] = 4;
        Order_Timer[110] = 1;
        Menu_Suicide[0] = 1;
        task_ptr->r_no[1]++;
        break;

    case 8:
        FadeOut(1, 0xFF, 8);
        Menu_Suicide[0] = 0;

        for (ix = 0, s3 = char_ix = '8'; ix < 3; ix++, s2 = char_ix++) {
            effect_61_init(0, ix + 80, 0, 0, char_ix, ix, 0x7047);
            Order[ix + 80] = 3;
            Order_Timer[ix + 80] = 1;
        }

        effect_66_init(138, 38, 0, 0, -1, -1, -0x7FF7);
        Order[138] = 3;
        Order_Timer[138] = 1;
        task_ptr->r_no[1]++;
        FadeInit();

    case 9:
        ToneDown(192, 32);

        if (FadeIn(1, 25, 8)) {
            task_ptr->r_no[2] = 0;
            task_ptr->r_no[1] = 1;
        }
    }
}

/** @brief Menu-sub case 1 â€” wait for fade and timer. */
s32 Menu_Sub_case1(struct _TASK* task_ptr) {
    FadeOut(1, 0xFF, 8);

    if ((task_ptr->timer -= 1) == 0) {
        task_ptr->r_no[2] += 1;
        FadeInit();
        return 1;
    }

    return 0;
}

/** @brief Extra Option screen â€” advanced training parameters. */
static void Extra_Option(struct _TASK* task_ptr) {
    Menu_Cursor_Y[1] = Menu_Cursor_Y[0];

    switch (task_ptr->r_no[2]) {
    case 0:
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2]++;
        task_ptr->r_no[3] = 0;
        task_ptr->timer = 5;
        Menu_Suicide[1] = 1;
        Menu_Suicide[2] = 0;
        Menu_Page = 0;
        Page_Max = 3;
        Menu_Page_Buff = Menu_Page;
        Message_Data->kind_req = 4;
        if (use_rmlui && rmlui_menu_extra_option)
            rmlui_extra_option_show();
        break;

    case 1:
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2]++;
        Setup_Next_Page(task_ptr, task_ptr->r_no[3]);
        /* fallthrough */

    case 2:
        FadeOut(1, 0xFF, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2]++;
            task_ptr->r_no[3] = 1;
            FadeInit();
        }

        break;

    case 3:
        if (FadeIn(1, 25, 8)) {
            task_ptr->r_no[2]++;
            break;
        }

        break;

    case 4:
        Pause_ID = 0;
        Dir_Move_Sub(task_ptr, 0);

        if (IO_Result == 0) {
            Pause_ID = 1;
            Dir_Move_Sub(task_ptr, 1);
        }

        if (Menu_Cursor_Y[1] != Menu_Cursor_Y[0]) {
            SE_cursor_move();
            save_w[Present_Mode].extra_option.contents[Menu_Page][Menu_Max] = 1;

            if (Menu_Cursor_Y[0] < Menu_Max) {
                Message_Data->order = 1;
                Message_Data->request = Ex_Account_Data[Menu_Page] + Menu_Cursor_Y[0];
                Message_Data->timer = 2;

                if (msgExtraTbl[0]->msgNum[Menu_Cursor_Y[0] + (Menu_Page * 8)] == 1) {
                    Message_Data->pos_y = 54;
                } else {
                    Message_Data->pos_y = 62;
                }
            } else {
                Message_Data->order = 1;
                Message_Data->request = save_w[Present_Mode].extra_option.contents[Menu_Page][Menu_Max] + 32;
                Message_Data->timer = 2;
                Message_Data->pos_y = 54;
            }
        }

        switch (IO_Result) {
        case 0x200:
            if (use_rmlui && rmlui_menu_extra_option)
                rmlui_extra_option_hide();
            Return_Option_Mode_Sub(task_ptr);
            Order[115] = 4;
            Order_Timer[115] = 4;
            save_w[4].extra_option = save_w[1].extra_option;
            save_w[5].extra_option = save_w[1].extra_option;
            SE_dir_selected();
            break;

        case 0x80:
        case 0x800:
            task_ptr->r_no[2] = 1;
            task_ptr->timer = 5;

            if (--Menu_Page < 0) {
                Menu_Page = Page_Max;
            }

            SE_dir_selected();
            break;

        case 0x40:
        case 0x400:
            task_ptr->r_no[2] = 1;
            task_ptr->timer = 5;

            if (++Menu_Page > Page_Max) {
                Menu_Page = 0;
            }

            SE_dir_selected();
            break;

        case 0x100:
            if (Menu_Page == 0 && Menu_Cursor_Y[0] == 6) {
                save_w[Present_Mode].extra_option = save_w[0].extra_option;
                SE_selected();
                break;
            }

            if (Menu_Cursor_Y[0] != Menu_Max) {
                break;
            }

            switch (save_w[Present_Mode].extra_option.contents[Menu_Page][Menu_Max]) {
            case 0:
                task_ptr->r_no[2] = 1;
                task_ptr->timer = 5;

                if (--Menu_Page < 0) {
                    Menu_Page = Page_Max;
                }

                break;

            case 2:
                task_ptr->r_no[2] = 1;
                task_ptr->timer = 5;

                if (++Menu_Page > Page_Max) {
                    Menu_Page = 0;
                }

                break;

            default:
                if (use_rmlui && rmlui_menu_extra_option)
                    rmlui_extra_option_hide();
                Return_Option_Mode_Sub(task_ptr);
                save_w[4].extra_option = save_w[1].extra_option;
                save_w[5].extra_option = save_w[1].extra_option;
                Order[115] = 4;
                Order_Timer[115] = 4;
                break;
            }

            SE_selected();

            break;
        }

        break;
    }
}

/** @brief End Replay Menu â€” post-replay choices (retry / exit). */
static void End_Replay_Menu(struct _TASK* task_ptr) {
    s16 ix;
    s16 ans;

    switch (task_ptr->r_no[1]) {
    case 0:
        if (Allow_a_battle_f == 0) {
            break;
        }

        task_ptr->r_no[1] += 1;
        Pause_ID = Decide_ID;
        Pause_Down = 1;
        Game_pause = 0x81;
        effect_A3_init(1, 0x17, 0x63, 0, 3, 0x82, 0x48, 1);
        effect_A3_init(1, 0x17, 0x63, 1, 3, 0x88, 0x58, 1);
        Order[0x8A] = 3;
        Order_Timer[0x8A] = 1;
        effect_66_init(0x8A, 0xA, 2, 7, -1, -1, -0x3FF6);
        /* fallthrough */

    case 1:
        task_ptr->r_no[1] += 1;
        Menu_Common_Init();
        Menu_Cursor_Y[0] = 0;

        for (ix = 0; ix < 4; ix++) {
            Menu_Suicide[ix] = 0;
        }

        effect_10_init(0, 0, 0, 4, 0, 0x14, 0xE);
        effect_10_init(0, 6, 1, 2, 0, 0x16, 0x10);
        break;

    case 2:
        MC_Move_Sub(Check_Menu_Lever(Pause_ID, 0), 0, 1, 0xFF);

        switch (IO_Result) {
        case 0x100:
            switch (Menu_Cursor_Y[0]) {
            case 0:
                task_ptr->r_no[0] = 0xC;
                task_ptr->r_no[1] = 0;

                for (ix = 0; ix < 4; ix++) {
                    Menu_Suicide[ix] = 1;
                }

                SE_selected();
                break;

            case 1:
                task_ptr->r_no[1] += 1;
                SE_selected();
                Menu_Suicide[0] = 1;
                Menu_Cursor_Y[0] = 1;
                effect_10_init(0, 0, 3, 3, 1, 0x13, 0xE);
                effect_10_init(0, 1, 0, 0, 1, 0x14, 0x10);
                effect_10_init(0, 1, 1, 1, 1, 0x1A, 0x10);
                break;
            }

            break;
        }

        break;

    case 3:
        ans = Yes_No_Cursor_Move_Sub(task_ptr);

        switch (ans) {
        case 1:
            task_ptr->r_no[1] = 1;
            break;

        case -1:
            Menu_Suicide[3] = 1;
            break;
        }

        break;
    }
}
