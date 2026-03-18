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
#include "port/init_task.h"
#include "port/menu_task.h"
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

/* RmlUi Phase 3 bypass — per-component toggles + menu replacements */
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
/* Legacy screen functions removed — migrated to MenuScreen registry (ms_*.c):
 *   Mode_Select, Option_Select, Training_Mode, System_Direction,
 *   Load_Replay, toSelectGame, Game_Option, Button_Config,
 *   Sound_Test, Memory_Card, Extra_Option, VS_Result,
 *   Save_Replay, Direction_Menu */
void Setup_VS_Mode(struct _TASK* task_ptr);
void Network_Lobby(struct _TASK* task_ptr);

static void bg_etc_write_ex(s16 type);
void jmpRebootProgram();

void Menu_in_Sub(struct _TASK* task_ptr);
s32 Exit_Sub(struct _TASK* task_ptr, s16 cursor_ix, s16 next_routine);
s32 Menu_Sub_case1(struct _TASK* task_ptr);
static void DAS_1st(struct _TASK* task_ptr);
static void DAS_2nd(struct _TASK* task_ptr);
static void DAS_3rd(struct _TASK* task_ptr);
static void DAS_4th(struct _TASK* task_ptr);
static void DAS2_4th(struct _TASK* task_ptr);
static void Training_Init(struct _TASK* task_ptr);
void Character_Change(struct _TASK* task_ptr);
void Normal_Training(struct _TASK* task_ptr);
void Blocking_Training(struct _TASK* task_ptr);
void Dummy_Setting(struct _TASK* task_ptr);
void Training_Option(struct _TASK* task_ptr);
void Blocking_Tr_Option(struct _TASK* task_ptr);

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
    /* ── MenuScreen registry integration hook (Task 4) ──
     * If the registry is already driving a screen, tick it and return.
     * Otherwise, try to map the legacy r_no[1] index to a MenuScreenId;
     * if a migrated (and enabled) screen is found, hand off to the
     * registry.  Un-migrated indices fall through to the legacy table. */
    if (MenuScreen_IsActive()) {
        MenuScreen_Tick(task_ptr);
        return;
    }

    {
        MenuScreenId mapped = MenuScreen_FromLegacyIndex(task_ptr->r_no[1]);
        if (mapped != MENU_SCREEN_NONE) {
            MenuScreen_Goto(mapped);
            MenuScreen_Tick(task_ptr);
            return;
        }
    }

    /* ── Legacy dispatch (un-migrated screens only) ──
     * All migrated screens are intercepted by MenuScreen_FromLegacyIndex()
     * above and will never reach this table.  Migrated entries use Menu_Init
     * as a safe fallback in case of regression.  Only indices 0 (Menu_Init),
     * 19 (Save_Direction), and 20 (Load_Direction) are still actively
     * dispatched through this table. */
    void (*AT_Jmp_Tbl[AT_JMP_COUNT])() = {
        Menu_Init,      /* [ 0] Menu_Init — bootstrap (un-migrated) */
        Menu_Init,      /* [ 1] DEAD: migrated to MENU_SCREEN_MODE_SELECT */
        Menu_Init,      /* [ 2] DEAD: migrated to MENU_SCREEN_OPTION_SELECT */
        Menu_Init,      /* [ 3] DEAD: OPTION_SELECT alias */
        Menu_Init,      /* [ 4] DEAD: migrated to MENU_SCREEN_TRAINING_MODE */
        Menu_Init,      /* [ 5] DEAD: migrated to MENU_SCREEN_SYSTEM_DIRECTION */
        Menu_Init,      /* [ 6] DEAD: migrated to MENU_SCREEN_LOAD_REPLAY */
        Menu_Init,      /* [ 7] DEAD: OPTION_SELECT alias */
        Menu_Init,      /* [ 8] DEAD: migrated to MENU_SCREEN_EXIT_CONFIRM */
        Menu_Init,      /* [ 9] DEAD: migrated to MENU_SCREEN_GAME_OPTION */
        Menu_Init,      /* [10] DEAD: migrated to MENU_SCREEN_BUTTON_CONFIG */
        Menu_Init,      /* [11] DEAD: SYSTEM_DIRECTION alias */
        Menu_Init,      /* [12] DEAD: migrated to MENU_SCREEN_SOUND_TEST */
        Menu_Init,      /* [13] DEAD: migrated to MENU_SCREEN_MEMORY_CARD */
        Menu_Init,      /* [14] DEAD: migrated to MENU_SCREEN_EXTRA_OPTION */
        Menu_Init,      /* [15] DEAD: OPTION_SELECT alias */
        Menu_Init,      /* [16] DEAD: migrated to MENU_SCREEN_VS_RESULT */
        Menu_Init,      /* [17] DEAD: migrated to MENU_SCREEN_SAVE_REPLAY */
        Menu_Init,      /* [18] DEAD: migrated to MENU_SCREEN_DIRECTION_MENU */
        Save_Direction, /* [19] Save_Direction (un-migrated) */
        Load_Direction, /* [20] Load_Direction (un-migrated) */
        Menu_Init,      /* [21] DEAD: migrated to MENU_SCREEN_NETWORK_LOBBY */
    };

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
        effect_57_init(0x4E, MENU_HEADER_MODE_MENU, 0, 0x45, fade_on);
        load_any_texture_patnum(0x7F30, 0xC, 0);
    }

    cpReadyTask(TASK_SAVER, Saver_Task);
}

/* Mode_Select() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/** @brief Prepare VS mode â€” enable both operators and init grades. */
void Setup_VS_Mode(struct _TASK* task_ptr) {
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
void Menu_in_Sub(struct _TASK* task_ptr) {
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] += 1;
    task_ptr->timer = 5;
    Menu_Common_Init();

    /* Hide all Phase 3 game documents when entering a new sub-menu.
     * Each sub-menu's case 0 will show its own doc afterwards. */
    if (use_rmlui)
        rmlui_wrapper_hide_all_game_documents();

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
/* Peer selection indices — exposed as globals for RmlUI bindings */
int g_lobby_peer_idx = 0;
int g_net_peer_idx = 0;

void Network_Lobby(struct _TASK* task_ptr) {
    s16 ix;
    static int s_slide_offset = 384; /* slide-in offset for SSPutStr elements */

    switch (task_ptr->r_no[2]) {
    /* ================================================================
     * GATEWAY PHASE (cases 0–3): LOBBY MODE / LOCAL NETWORK / EXIT
     * ================================================================ */
    case 0:
        /* Phase 0: Fade out, kill Mode_Select items, init gateway submenu */
        Menu_in_Sub(task_ptr);
        effect_57_init(0x70, MENU_HEADER_NETWORK, 0, 0x3F, 2);
        Order[0x70] = 1;
        Order_Dir[0x70] = 8;
        Order_Timer[0x70] = 1;
        effect_04_init(1, 7, 0, 0x48); /* cursor type 7 = 4-item gateway */
        {
            s16 char_index = 74; /* 74=LOBBY MODE, 75=LOCAL NETWORK, 76=LEADERBOARD, 77=EXIT */
            for (ix = 0; ix < 4; ix++) {
                effect_61_init(0, ix + 0x50, 0, 1, char_index, ix, 0x7047);
                Order[ix + 0x50] = 1;
                Order_Dir[ix + 0x50] = 4;
                Order_Timer[ix + 0x50] = ix + 0x14;
                char_index++;
            }
        }
        Menu_Cursor_Move = 4;
        break;

    case 1:
        Menu_Sub_case1(task_ptr);
        break;

    case 2:
        if (FadeIn(1, 0x19, 8) != 0) {
            task_ptr->r_no[2] += 1;
        }
        break;

    case 3:
        /* Gateway input: pick LOBBY MODE / LOCAL NETWORK / LEADERBOARD / EXIT */
        if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, 3, 0xFF) == 0) {
            MC_Move_Sub(Check_Menu_Lever(1, 0), 0, 3, 0xFF);
        }

        if (IO_Result == SWK_SOUTH || IO_Result == SWK_EAST) {
            SE_selected();

            if (Menu_Cursor_Y[0] == 3 || IO_Result == SWK_EAST) {
                /* EXIT — back to Mode_Select */
                Menu_Suicide[0] = 0;
                Menu_Suicide[1] = 1;
                task_ptr->r_no[1] = 1; /* Mode_Select */
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                task_ptr->free[0] = 0;
                Order[0x70] = 4;
                Order_Timer[0x70] = 4;
                break;
            }

            if (Menu_Cursor_Y[0] == 2) {
                /* LEADERBOARD — show RmlUI leaderboard overlay */
                rmlui_leaderboard_show();
                task_ptr->r_no[2] = 4; /* jump to leaderboard phase */
            } else if (Menu_Cursor_Y[0] == 1) {
                /* LOCAL NETWORK — jump to LAN-only lobby phase */
                task_ptr->free[2] = 2;  /* 2=lan-only */
                task_ptr->r_no[2] = 20; /* jump to LAN-only lobby phase */
            } else {
                /* LOBBY MODE (0) — always use RmlUI lobby */
                task_ptr->free[2] = 1;  /* 1=rmlui */
                task_ptr->r_no[2] = 10; /* jump to lobby phase */
            }
        }
        break;

    /* ================================================================
     * LEADERBOARD PHASE (cases 4–8): full-screen RmlUI leaderboard
     * ================================================================ */
    case 4:
        /* Phase 4: Fade out, kill gateway items, request blue BG */
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->r_no[3] = 0;
        task_ptr->timer = 5;
        Menu_Suicide[0] = 1; /* kill gateway items (master_player=0) */
        Menu_Suicide[1] = 0;
        Message_Data->kind_req = 4; /* blue-BG background mode */
        break;

    case 5:
        /* Phase 5: Destroy old effects, show blue BG + RmlUI leaderboard */
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;

        effect_work_init();
        Menu_Common_Init();
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Order[0x4E] = 5;
        Order_Timer[0x4E] = 1;
        Order_Dir[0x4E] = 1;

        /* Blue background banner */
        effect_57_init(0x4E, MENU_HEADER_MODE_MENU, 0, 0x45, 0);

        /* Show RmlUI leaderboard (auto-fetches page 0) */
        rmlui_leaderboard_show();
        /* fallthrough to case 6 */

    case 6:
        /* Wait for fade-out timer */
        FadeOut(1, 0xFF, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2] += 1;
            task_ptr->r_no[3] = 1;
            FadeInit();
        }
        break;

    case 7:
        /* Fade in */
        if (FadeIn(1, 25, 8)) {
            task_ptr->r_no[2] += 1;
        }
        break;

    case 8:
        /* Leaderboard input loop — B / Cancel exits back to gateway */
        {
            u16 trigger = 0;
            for (int i = 0; i < 2; i++) {
                trigger |= (~plsw_01[i] & plsw_00[i]);
            }

            /* Left D-pad: previous page */
            if (trigger & 0x0004) {
                SE_selected();
                rmlui_leaderboard_prev_page();
            }

            /* Right D-pad: next page */
            if (trigger & 0x0008) {
                SE_selected();
                rmlui_leaderboard_next_page();
            }

            if (trigger & 0x0200) { /* Cancel / B */
                SE_selected();
                rmlui_leaderboard_hide();
                Menu_Suicide[0] = 0;
                Menu_Suicide[1] = 1; /* kill blue BG items */
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                task_ptr->free[0] = 0;
            }
        }
        break;

    /* ================================================================
     * LOBBY PHASE (cases 10–13): full lobby with peer lists & popups
     * ================================================================ */
    case 10:
        /* Phase 10: Start fade, set suicide, request blue BG mode */
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->r_no[3] = 0;
        task_ptr->timer = 5;
        Menu_Suicide[0] = 1;        /* kill gateway items (master_player=0) */
        Menu_Suicide[1] = 0;        /* enable lobby items (master_player=1) */
        Message_Data->kind_req = 4; /* blue-BG background mode */
        break;

    case 11:
        /* Phase 11: Destroy old effects, rebuild lobby from scratch */
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

        if (task_ptr->free[2] == 1) {
            /* RMLUI lobby */
            rmlui_network_lobby_show();
        } else {
            /* NATIVE lobby */
            /* Right-side grey overlay boxes (LAN and Internet peer areas) */
            effect_66_init(0x8A, 42, 1, 0, -1, -1, -0x7FF0);
            Order[0x8A] = 3;
            Order_Timer[0x8A] = 1;
            effect_66_init(0x8B, 42, 1, 0, -1, -1, -0x7FF1);
            Order[0x8B] = 3;
            Order_Timer[0x8B] = 1;

            /* Menu items: 6 items, 0x70A7 = compact 8px font, master_player=1 */
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

            /* Message system for description text */
            Message_Data->pos_x = 0;
            Message_Data->pos_y = 0x3E;
            Message_Data->pos_z = 0x44;
            Message_Data->request = 35;
            Message_Data->order = 0;
            Message_Data->timer = 1;
            effect_45_init(0, 0, 1);

            Menu_Cursor_Move = 6;
        }

        /* Blue background banner — always init (palette 0x45). */
        effect_57_init(0x4E, MENU_HEADER_MODE_MENU, 0, 0x45, 0);

        /* Enter lobby state */
        SDLNetplayUI_SetNativeLobbyActive(true);
        Netplay_EnterLobby();
        /* fallthrough to case 12 */

    case 12:
        /* Wait for fade-out timer */
        FadeOut(1, 0xFF, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2] += 1;
            task_ptr->r_no[3] = 1;
            FadeInit();
        }
        break;

    case 13:
        /* Fade in */
        if (FadeIn(1, 25, 8)) {
            task_ptr->r_no[2] += 1;
        }
        break;

    case 14: {
        /* --- Lobby input loop --- */

        /* When the casual lobby room screen is visible, it handles its own
         * input via rmlui_casual_lobby_update(). If a match is actively running,
         * we also want to suspend menu input. Skip all menu.c input
         * processing so button presses aren't double-handled. */
        if (rmlui_casual_lobby_is_visible() ||
            (Netplay_GetSessionState() != NETPLAY_SESSION_LOBBY && Netplay_GetSessionState() != NETPLAY_SESSION_IDLE))
            break;

        /* Decelerate slide-in offset */
        if (s_slide_offset > 0) {
            s_slide_offset = (int)(s_slide_offset / 1.18f);
            if (s_slide_offset < 2)
                s_slide_offset = 0;
        }
        const s16 sl = (s16)s_slide_offset;

        /* Custom red banner (brighter than default Akaobi 0xA0D00000) */
        if (task_ptr->free[2] == 0) {
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

        /* Handle cursor movement (12 items: 0..11) */
        {
            s16 prev_cursor = Menu_Cursor_Y[0];
            if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, 11, 0xFF) == 0) {
                MC_Move_Sub(Check_Menu_Lever(1, 0), 0, 11, 0xFF);
            }
            if (popup_active) {
                Menu_Cursor_Y[0] = prev_cursor;
            } else if (prev_cursor != Menu_Cursor_Y[0]) {
                if (task_ptr->free[2] == 0) {
                    Message_Data->order = 1;
                    Message_Data->request = 35 + Menu_Cursor_Y[0];
                    Message_Data->timer = 2;
                    Message_Data->pos_y = 0x3E;
                }
            }
        }

        /* === Left/right toggle handling for toggle items === */
        if (!popup_active) {
            u16 click = (~plsw_01[0] & plsw_00[0]) | (~plsw_01[1] & plsw_00[1]);

            if (click & 12) {
                switch (Menu_Cursor_Y[0]) {
                case 0: { /* LAN AUTO-CONN */
                    bool v = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
                    Config_SetBool(CFG_KEY_NETPLAY_AUTO_CONNECT, !v);
                    Config_Save();
                    SE_dir_cursor_move();
                    break;
                }
                case 1: { /* LAN CONNECT (peer toggling) */
                    NetplayDiscoveredPeer tg_peers[16];
                    int tg_count = Discovery_GetPeers(tg_peers, 16);
                    if (tg_count > 0) {
                        if (click & 4) {
                            g_lobby_peer_idx--;
                            if (g_lobby_peer_idx < 0)
                                g_lobby_peer_idx = tg_count - 1;
                        } else {
                            g_lobby_peer_idx++;
                            if (g_lobby_peer_idx >= tg_count)
                                g_lobby_peer_idx = 0;
                        }
                        SE_dir_cursor_move();
                        if (Discovery_GetChallengeTarget() != 0) {
                            Discovery_SetChallengeTarget(0);
                        }
                    }
                    break;
                }
                case 2: { /* NET AUTO-ACPT */
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
                case 4: { /* REGION LOCK toggle */
                    bool v = Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK);
                    Config_SetBool(CFG_KEY_NETPLAY_REGION_LOCK, !v);
                    Config_Save();
                    SE_dir_cursor_move();
                    break;
                }
                case 5: { /* MAX PING cycle */
                    int cur = Config_GetInt(CFG_KEY_NETPLAY_MAX_PING);
                    /* Cycle through: 0(off) → 50 → 100 → 150 → 200 → 0 */
                    if (click & 4) { /* left */
                        if (cur <= 0)
                            cur = 200;
                        else if (cur <= 50)
                            cur = 0;
                        else
                            cur -= 50;
                    } else { /* right */
                        if (cur >= 200)
                            cur = 0;
                        else if (cur <= 0)
                            cur = 50;
                        else
                            cur += 50;
                    }
                    Config_SetInt(CFG_KEY_NETPLAY_MAX_PING, cur);
                    Config_Save();
                    SE_dir_cursor_move();
                    break;
                }
                case 6: { /* BLOCK WIFI toggle */
                    bool v = Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI);
                    Config_SetBool(CFG_KEY_NETPLAY_BLOCK_WIFI, !v);
                    Config_Save();
                    SE_dir_cursor_move();
                    break;
                }
                case 7: { /* MATCH FT cycle */
                    static const int ft_values[] = { 1, 2, 3, 5, 10 };
                    static const int ft_count = 5;
                    int cur_ft = Config_GetInt(CFG_KEY_NETPLAY_FT);
                    int idx = 1; /* default to FT2 index */
                    for (int fi = 0; fi < ft_count; fi++) {
                        if (ft_values[fi] == cur_ft) {
                            idx = fi;
                            break;
                        }
                    }
                    if (click & 4) { /* left */
                        idx = (idx - 1 + ft_count) % ft_count;
                    } else { /* right */
                        idx = (idx + 1) % ft_count;
                    }
                    Config_SetInt(CFG_KEY_NETPLAY_FT, ft_values[idx]);
                    Config_Save();
                    SE_dir_cursor_move();
                    break;
                }
                case 8: { /* NET CONNECT */
                    if (SDLNetplayUI_IsSearching()) {
                        int p_count = SDLNetplayUI_GetOnlinePlayerCount();
                        if (p_count > 0) {
                            if (click & 4) {
                                g_net_peer_idx--;
                                if (g_net_peer_idx < 0)
                                    g_net_peer_idx = p_count - 1;
                            } else {
                                g_net_peer_idx++;
                                if (g_net_peer_idx >= p_count)
                                    g_net_peer_idx = 0;
                            }
                            SE_dir_cursor_move();
                        }
                    }
                    break;
                }
                case 10: { /* JOIN ROOM (room list scroll) */
                    if (task_ptr->free[2] == 1) {
                        rmlui_network_lobby_room_scroll((click & 4) ? -1 : 1);
                        SE_dir_cursor_move();
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }

        /* === Background text — hide when popup covers the screen === */
        if (task_ptr->free[2] == 0 && !popup_active) {
            /* === Display toggle values (right of labels) === */
            {
                bool lan_ac = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
                SSPutStr_Bigger(136 + sl, 63, 5, lan_ac ? (s8*)"ON" : (s8*)"OFF", 1.0f, lan_ac ? 9 : 1, 1.0f);

                bool net_ac = Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT);
                SSPutStr_Bigger(136 + sl, 115, 5, net_ac ? (s8*)"ON" : (s8*)"OFF", 1.0f, net_ac ? 9 : 1, 1.0f);

                bool auto_s = Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH);
                SSPutStr_Bigger(136 + sl, 129, 5, auto_s ? (s8*)"ON" : (s8*)"OFF", 1.0f, auto_s ? 9 : 1, 1.0f);
            }

            /* === LAN / NET Headers === */
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

            /* === Peer / Online Info (Right Side) === */
            {
                s16 peer_x = 200;
                s16 lan_peer_y = 63;
                s16 net_peer_y = 115;

                NetplayDiscoveredPeer d_peers[16];
                int d_count = Discovery_GetPeers(d_peers, 16);
                if (d_count > 0) {
                    if (g_lobby_peer_idx >= d_count)
                        g_lobby_peer_idx = d_count - 1;
                    if (g_lobby_peer_idx < 0)
                        g_lobby_peer_idx = 0;
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "%d FOUND", d_count);
                    SSPutStr_Bigger(peer_x + sl, lan_peer_y, 5, (s8*)buf, 1.0f, 9, 1.0f);
                    SDL_snprintf(buf, sizeof(buf), "> %s", d_peers[g_lobby_peer_idx].name);
                    SSPutStr_Bigger(peer_x + sl, (u16)(lan_peer_y + 15), 5, (s8*)buf, 1.0f, 0, 1.0f);
                } else {
                    g_lobby_peer_idx = 0;
                    SSPutStr_Bigger(peer_x + sl, lan_peer_y, 5, (s8*)"NONE", 1.0f, 1, 1.0f);
                }

                if (SDLNetplayUI_IsSearching()) {
                    int online_count = SDLNetplayUI_GetOnlinePlayerCount();
                    char s_buf[64];
                    if (online_count > 0) {
                        if (g_net_peer_idx >= online_count)
                            g_net_peer_idx = online_count - 1;
                        if (g_net_peer_idx < 0)
                            g_net_peer_idx = 0;
                        SDL_snprintf(s_buf, sizeof(s_buf), "%d ONLINE", online_count);
                        SSPutStr_Bigger(peer_x + sl, net_peer_y, 5, (s8*)s_buf, 1.0f, 9, 1.0f);
                        SDL_snprintf(s_buf, sizeof(s_buf), "> %s", SDLNetplayUI_GetOnlinePlayerName(g_net_peer_idx));
                        SSPutStr_Bigger(peer_x + sl, (u16)(net_peer_y + 15), 5, (s8*)s_buf, 1.0f, 0, 1.0f);
                    } else {
                        g_net_peer_idx = 0;
                        SSPutStr_Bigger(peer_x + sl, net_peer_y, 5, (s8*)"SEARCHING", 1.0f, 9, 1.0f);
                    }
                } else {
                    SSPutStr_Bigger(peer_x + sl, net_peer_y, 5, (s8*)"IDLE", 1.0f, 1, 1.0f);
                }
            }

            /* === Grey description banner === */
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
            }

            /* === Status line === */
            {
                const char* status = SDLNetplayUI_GetStatusMsg();
                if (status[0]) {
                    SSPutStr_Bigger(40 + sl, 215, 5, (s8*)status, 1.0f, 9, 1.0f);
                } else {
                    NetplayDiscoveredPeer c_peers[16];
                    int c_count = Discovery_GetPeers(c_peers, 16);
                    uint32_t current_target = Discovery_GetChallengeTarget();
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
                            if (c_peers[i].instance_id == current_target) {
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
        } /* end native background text */

        /* === Incoming Challenge Popup (Internet) === */
        if (SDLNetplayUI_HasPendingInvite()) {
            if (task_ptr->free[2] == 0)
                NetLobby_DrawIncomingPopup(SDLNetplayUI_GetPendingInviteName(),
                                           SDLNetplayUI_GetPendingInviteRegion(),
                                           SDLNetplayUI_GetPendingInvitePing());

            switch (IO_Result) {
            case 0x100:
                Netplay_SetNegotiatedFT(SDLNetplayUI_GetPendingInviteFT());
                SDLNetplayUI_AcceptPendingInvite();
                SE_selected();
                break;
            case 0x200:
                SDLNetplayUI_DeclinePendingInvite();
                SE_selected();
                break;
            default:
                break;
            }
        } else if (SDLNetplayUI_HasOutgoingChallenge()) {
            if (task_ptr->free[2] == 0)
                NetLobby_DrawOutgoingPopup(SDLNetplayUI_GetOutgoingChallengeName(),
                                           SDLNetplayUI_GetOutgoingChallengePing());

            if (IO_Result == 0x100 || IO_Result == 0x200) {
                SDLNetplayUI_CancelOutgoingChallenge();
                Discovery_SetChallengeTarget(0);
                SE_selected();
            }
        } else if (Discovery_GetChallengeTarget() != 0) {
            {
                NetplayDiscoveredPeer op[16];
                int op_n = Discovery_GetPeers(op, 16);
                uint32_t tgt = Discovery_GetChallengeTarget();
                const char* tgt_name = "...";
                int tgt_ping = -1;
                for (int i = 0; i < op_n; i++) {
                    if (op[i].instance_id == tgt) {
                        tgt_name = op[i].display_name[0] ? op[i].display_name : op[i].name;
                        tgt_ping = op[i].player_id[0] ? PingProbe_GetRTT(op[i].player_id) : -1;
                        break;
                    }
                }
                if (task_ptr->free[2] == 0)
                    NetLobby_DrawOutgoingPopup(tgt_name, tgt_ping);
            }

            if (IO_Result == 0x100 || IO_Result == 0x200) {
                Discovery_SetChallengeTarget(0);
                SE_selected();
            }
        } else {
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
                if (task_ptr->free[2] == 0)
                    NetLobby_DrawIncomingPopup(
                        ip_peers[lan_challenger].display_name[0] ? ip_peers[lan_challenger].display_name
                                                                 : ip_peers[lan_challenger].name,
                        "",
                        ip_peers[lan_challenger].player_id[0] ? PingProbe_GetRTT(ip_peers[lan_challenger].player_id)
                                                              : -1);

                switch (IO_Result) {
                case 0x100:
                    Netplay_SetNegotiatedFT(ip_peers[lan_challenger].ft_value);
                    Discovery_SetChallengeTarget(ip_peers[lan_challenger].instance_id);
                    SE_selected();
                    break;
                case 0x200:
                    Discovery_DismissChallenger(ip_peers[lan_challenger].instance_id);
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
                        if (cp_count > 0 && g_lobby_peer_idx >= 0 && g_lobby_peer_idx < cp_count) {
                            NetplayDiscoveredPeer* p = &cp_peers[g_lobby_peer_idx];

                            Discovery_SetChallengeTarget(p->instance_id);
                            SE_selected();
                        } else {
                            SE_selected();
                        }
                        break;
                    }
                    case 2: { /* NET AUTO-ACPT toggle */
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
                    case 4: { /* REGION LOCK toggle */
                        bool v = Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK);
                        Config_SetBool(CFG_KEY_NETPLAY_REGION_LOCK, !v);
                        Config_Save();
                        SE_selected();
                        break;
                    }
                    case 5: { /* MAX PING cycle */
                        int cur = Config_GetInt(CFG_KEY_NETPLAY_MAX_PING);
                        if (cur >= 200)
                            cur = 0;
                        else if (cur <= 0)
                            cur = 50;
                        else
                            cur += 50;
                        Config_SetInt(CFG_KEY_NETPLAY_MAX_PING, cur);
                        Config_Save();
                        SE_selected();
                        break;
                    }
                    case 6: { /* BLOCK WIFI toggle */
                        bool v = Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI);
                        Config_SetBool(CFG_KEY_NETPLAY_BLOCK_WIFI, !v);
                        Config_Save();
                        SE_selected();
                        break;
                    }
                    case 7: { /* MATCH FT cycle (confirm = advance) */
                        static const int ft_values[] = { 1, 2, 3, 5, 10 };
                        static const int ft_count = 5;
                        int cur_ft = Config_GetInt(CFG_KEY_NETPLAY_FT);
                        int idx = 1;
                        for (int fi = 0; fi < ft_count; fi++) {
                            if (ft_values[fi] == cur_ft) {
                                idx = fi;
                                break;
                            }
                        }
                        idx = (idx + 1) % ft_count;
                        Config_SetInt(CFG_KEY_NETPLAY_FT, ft_values[idx]);
                        Config_Save();
                        SE_selected();
                        break;
                    }
                    case 8: /* NET CONNECT */
                        if (SDLNetplayUI_IsSearching()) {
                            int p_count = SDLNetplayUI_GetOnlinePlayerCount();
                            if (p_count > 0 && g_net_peer_idx >= 0 && g_net_peer_idx < p_count) {
                                Netplay_SetNegotiatedFT(Config_GetInt(CFG_KEY_NETPLAY_FT));
                                SDLNetplayUI_ConnectToPlayer(g_net_peer_idx);
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

                    case 9: /* CREATE ROOM (RmlUI only) */
                        if (task_ptr->free[2] == 1) {
                            rmlui_network_lobby_create_room();
                        }
                        SE_selected();
                        break;
                    case 10: /* JOIN ROOM (RmlUI only) */
                        if (task_ptr->free[2] == 1) {
                            rmlui_network_lobby_join_room();
                        }
                        SE_selected();
                        break;
                    case 11:
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
                    if (task_ptr->free[2] == 1)
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

    /* ================================================================
     * LAN-ONLY LOBBY PHASE (cases 20–24)
     * Stripped-down clone of cases 10–14 with only LAN discovery.
     * ================================================================ */
    case 20:
        /* Phase 20: Start fade, set suicide, request blue BG mode */
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->r_no[3] = 0;
        task_ptr->timer = 5;
        Menu_Suicide[0] = 1;        /* kill gateway items (master_player=0) */
        Menu_Suicide[1] = 0;        /* enable lobby items (master_player=1) */
        Message_Data->kind_req = 4; /* blue-BG background mode */
        break;

    case 21:
        /* Phase 21: Destroy old effects, rebuild LAN-only lobby */
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

        /* Right-side grey overlay box (LAN peer area) */
        effect_66_init(0x8A, 42, 1, 0, -1, -1, -0x7FF0);
        Order[0x8A] = 3;
        Order_Timer[0x8A] = 1;

        /* Menu items: 3 items (AUTO-CONN, CONNECT, EXIT), 0x70A7 = compact 8px font, master_player=1 */
        {
            static const s16 lan_lobby_strings[] = { 79, 80, 81 };
            for (ix = 0; ix < 3; ix++) {
                effect_61_init(0, ix + 0x50, 0, 1, lan_lobby_strings[ix], ix, 0x70A7);
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

        /* Message system for description text */
        Message_Data->pos_x = 0;
        Message_Data->pos_y = 0x3E;
        Message_Data->pos_z = 0x44;
        Message_Data->request = 35;
        Message_Data->order = 0;
        Message_Data->timer = 1;
        effect_45_init(0, 0, 1);

        Menu_Cursor_Move = 3;

        /* Blue background banner — always init (palette 0x45). */
        effect_57_init(0x4E, MENU_HEADER_MODE_MENU, 0, 0x45, 0);

        /* Enter lobby state (LAN-only — no server registration) */
        SDLNetplayUI_SetNativeLobbyActive(true);
        Netplay_EnterLobby();
        /* fallthrough to case 22 */

    case 22:
        /* Wait for fade-out timer */
        FadeOut(1, 0xFF, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2] += 1;
            task_ptr->r_no[3] = 1;
            FadeInit();
        }
        break;

    case 23:
        /* Fade in */
        if (FadeIn(1, 25, 8)) {
            task_ptr->r_no[2] += 1;
        }
        break;

    case 24: {
        /* --- LAN-only lobby input loop --- */

        /* If a match is actively running, suspend menu input. Skip all menu.c input
         * processing so button presses aren't double-handled. */
        if (Netplay_GetSessionState() != NETPLAY_SESSION_LOBBY && Netplay_GetSessionState() != NETPLAY_SESSION_IDLE)
            break;

        /* Decelerate slide-in offset */
        if (s_slide_offset > 0) {
            s_slide_offset = (int)(s_slide_offset / 1.18f);
            if (s_slide_offset < 2)
                s_slide_offset = 0;
        }
        const s16 sl = (s16)s_slide_offset;

        /* Custom red banner */
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

            /* White top border */
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

            /* White bottom border */
            ap[0].y = 41;
            ap[1].y = 41;
            ap[2].y = 42;
            ap[3].y = 42;
            Renderer_Queue2DPrimitive((f32*)ap, PrioBase[67], (uintptr_t)acol[0].color, 0);
        }

        /* Compute popup_active — LAN-only: only LAN challenges */
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
        bool popup_active = lan_incoming || lan_outgoing;

        /* Handle cursor movement (3 items: 0..2) */
        {
            s16 prev_cursor = Menu_Cursor_Y[0];
            if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, 2, 0xFF) == 0) {
                MC_Move_Sub(Check_Menu_Lever(1, 0), 0, 2, 0xFF);
            }
            if (popup_active) {
                Menu_Cursor_Y[0] = prev_cursor;
            } else if (prev_cursor != Menu_Cursor_Y[0]) {
                Message_Data->order = 1;
                Message_Data->request = 35 + Menu_Cursor_Y[0];
                Message_Data->timer = 2;
                Message_Data->pos_y = 0x3E;
            }
        }

        /* === Left/right toggle handling === */
        if (!popup_active) {
            u16 click = (~plsw_01[0] & plsw_00[0]) | (~plsw_01[1] & plsw_00[1]);

            if (click & 12) {
                switch (Menu_Cursor_Y[0]) {
                case 0: { /* AUTO-CONN */
                    bool v = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
                    Config_SetBool(CFG_KEY_NETPLAY_AUTO_CONNECT, !v);
                    Config_Save();
                    SE_dir_cursor_move();
                    break;
                }
                case 1: { /* CONNECT (peer toggling) */
                    NetplayDiscoveredPeer tg_peers[16];
                    int tg_count = Discovery_GetPeers(tg_peers, 16);
                    if (tg_count > 0) {
                        if (click & 4) {
                            g_lobby_peer_idx--;
                            if (g_lobby_peer_idx < 0)
                                g_lobby_peer_idx = tg_count - 1;
                        } else {
                            g_lobby_peer_idx++;
                            if (g_lobby_peer_idx >= tg_count)
                                g_lobby_peer_idx = 0;
                        }
                        SE_dir_cursor_move();
                        if (Discovery_GetChallengeTarget() != 0) {
                            Discovery_SetChallengeTarget(0);
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
        if (!popup_active) {
            /* Display toggle values */
            {
                bool lan_ac = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
                SSPutStr_Bigger(136 + sl, 63, 5, lan_ac ? (s8*)"ON" : (s8*)"OFF", 1.0f, lan_ac ? 9 : 1, 1.0f);
            }

            /* LAN Header */
            {
                const char* lan_hdr = "----- LAN -----";
                int lan_hdr_px = (int)SDL_strlen(lan_hdr) * 8;
                s16 lan_hdr_x = (s16)((384 - lan_hdr_px) / 2);
                SSPutStr_Bigger(lan_hdr_x + sl, 50, 5, (s8*)lan_hdr, 1.0f, 0, 1.0f);
            }

            /* Peer Info (Right Side) */
            {
                s16 peer_x = 200;
                s16 lan_peer_y = 77;

                NetplayDiscoveredPeer d_peers[16];
                int d_count = Discovery_GetPeers(d_peers, 16);
                if (d_count > 0) {
                    if (g_lobby_peer_idx >= d_count)
                        g_lobby_peer_idx = d_count - 1;
                    if (g_lobby_peer_idx < 0)
                        g_lobby_peer_idx = 0;
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "%d FOUND", d_count);
                    SSPutStr_Bigger(peer_x + sl, lan_peer_y, 5, (s8*)buf, 1.0f, 9, 1.0f);
                    SDL_snprintf(buf, sizeof(buf), "> %s", d_peers[g_lobby_peer_idx].name);
                    SSPutStr_Bigger(peer_x + sl, (u16)(lan_peer_y + 15), 5, (s8*)buf, 1.0f, 0, 1.0f);
                } else {
                    g_lobby_peer_idx = 0;
                    SSPutStr_Bigger(peer_x + sl, lan_peer_y, 5, (s8*)"NONE", 1.0f, 1, 1.0f);
                }
            }

            /* Grey description banner */
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
            }

            /* Status line — LAN only */
            {
                NetplayDiscoveredPeer c_peers[16];
                int c_count = Discovery_GetPeers(c_peers, 16);
                uint32_t current_target = Discovery_GetChallengeTarget();
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
                        if (c_peers[i].instance_id == current_target) {
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
        } /* end LAN-only background text */

        /* === LAN Incoming/Outgoing Challenge Popups === */
        if (Discovery_GetChallengeTarget() != 0) {
            {
                NetplayDiscoveredPeer op[16];
                int op_n = Discovery_GetPeers(op, 16);
                uint32_t tgt = Discovery_GetChallengeTarget();
                const char* tgt_name = "...";
                int tgt_ping = -1;
                for (int i = 0; i < op_n; i++) {
                    if (op[i].instance_id == tgt) {
                        tgt_name = op[i].display_name[0] ? op[i].display_name : op[i].name;
                        tgt_ping = op[i].player_id[0] ? PingProbe_GetRTT(op[i].player_id) : -1;
                        break;
                    }
                }
                NetLobby_DrawOutgoingPopup(tgt_name, tgt_ping);
            }

            if (IO_Result == 0x100 || IO_Result == 0x200) {
                Discovery_SetChallengeTarget(0);
                SE_selected();
            }
        } else {
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
                NetLobby_DrawIncomingPopup(
                    ip_peers[lan_challenger].display_name[0] ? ip_peers[lan_challenger].display_name
                                                             : ip_peers[lan_challenger].name,
                    "",
                    ip_peers[lan_challenger].player_id[0] ? PingProbe_GetRTT(ip_peers[lan_challenger].player_id) : -1);

                switch (IO_Result) {
                case 0x100:
                    Netplay_SetNegotiatedFT(ip_peers[lan_challenger].ft_value);
                    Discovery_SetChallengeTarget(ip_peers[lan_challenger].instance_id);
                    SE_selected();
                    break;
                case 0x200:
                    Discovery_DismissChallenger(ip_peers[lan_challenger].instance_id);
                    SE_selected();
                    break;
                default:
                    break;
                }
            } else {
                /* === Handle confirm/cancel (normal LAN-only lobby input) === */
                switch (IO_Result) {
                case 0x100: /* Confirm */
                    switch (Menu_Cursor_Y[0]) {
                    case 0: { /* AUTO-CONN toggle */
                        bool v = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
                        Config_SetBool(CFG_KEY_NETPLAY_AUTO_CONNECT, !v);
                        Config_Save();
                        SE_selected();
                        break;
                    }
                    case 1: { /* CONNECT */
                        NetplayDiscoveredPeer cp_peers[16];
                        int cp_count = Discovery_GetPeers(cp_peers, 16);
                        if (cp_count > 0 && g_lobby_peer_idx >= 0 && g_lobby_peer_idx < cp_count) {
                            NetplayDiscoveredPeer* p = &cp_peers[g_lobby_peer_idx];
                            Discovery_SetChallengeTarget(p->instance_id);
                            SE_selected();
                        } else {
                            SE_selected();
                        }
                        break;
                    }
                    case 2:
                        /* EXIT */
                        goto lan_lobby_exit;
                    }
                    break;

                case 0x200: /* Cancel */
                    if (Discovery_GetChallengeTarget() != 0) {
                        Discovery_SetChallengeTarget(0);
                        SE_selected();
                        break;
                    }
                lan_lobby_exit:
                    SE_selected();
                    SDLNetplayUI_SetNativeLobbyActive(false);
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
        } /* end LAN-only incoming / normal input */
        break;
    }
    }
}

/**
 * @brief Re-activate TASK_MENU at the Network_Lobby input loop (RmlUI mode).
 *
 * After a casual room match ends, Soft_Reset_Sub() kills TASK_MENU. When
 * the user then leaves the room and returns to the network lobby, the menu
 * task must be restored so input processing works (case 14 = RmlUI lobby loop).
 */
void Menu_ReenterNetworkLobby(void) {
    s16 ix;
    InitTask_ClearAllRNo();
    for (ix = 0; ix < 4; ix++) {
        G_No[ix] = 0;
        E_No[ix] = 0;
        D_No[ix] = 0;
    }

    G_No[0] = 2;
    G_No[1] = 12; // Menu Idle State
    E_No[0] = 1;
    E_No[1] = 2;
    E_No[2] = 2;
    Break_Into = 0;

    Demo_Flag = 1;
    Game_pause = 0;
    judge_flag = 0;
    Pause_Down = 0;
    Disp_Attack_Data = 0;
    seraph_flag = 0;
    End_Training = 0;
    Forbid_Reset = 0;
    Exec_Wipe = 0;
    Present_Mode = MODE_NETWORK;
    Mode_Type = MODE_NETWORK;
    Insert_Y = 23;

    // Re-create MTS slots purged by Soft_Reset_Sub() → Purge_mmtm_area(6).
    // mto_list[6] is all-zeros so nothing is recreated automatically.
    // Menu effects need:  slot 12 (MS) for effect_45 (message display),
    //                     slot 13 (SL) for effect_57/61/66/04 (banners/text).
    make_texcash_work(12);
    make_texcash_work(13);

    // Replicate Menu_Init setup.  Menu_Init (r_no[1]=0) normally runs once
    // before Mode_Select to configure background layers, cursor state, and
    // the saver task.  Since we jump straight to Network_Lobby (r_no[1]=21),
    // Menu_Init is never called.
    for (ix = 0; ix < 4; ix++) {
        Menu_Suicide[ix] = 0;
        Unsubstantial_BG[ix] = 0;
        Cursor_Y_Pos[0][ix] = 0;
    }
    Menu_Cursor_Y[0] = 0;
    Menu_Cursor_Y[1] = 0;
    All_Clear_Suicide();
    pulpul_stop();
    bg_etc_write_ex(2);
    Setup_Virtual_BG(0, 0x200, 0);
    Setup_BG(1, 0x200, 0);
    Setup_BG(2, 0x200, 0);
    base_y_pos = 0;
    cpReadyTask(TASK_SAVER, Saver_Task);

    // TASK_INIT must be DEACTIVATED here.  Its r_no[0] is zeroed above,
    // and Init_Task dispatches r_no[0]==0 → Init_Task_1st() which performs
    // a full cold-boot init (clears G_No[], resets textures, creates
    // TASK_RESET).  Leaving condition=1 causes the entire game state to be
    // clobbered on the next frame, freezing the lobby.
    InitTask_Deactivate();
    task[TASK_GAME].condition = 1;
    task[TASK_MENU].condition = 1;

    cpReadyTask(TASK_MENU, Menu_Task);
    MenuTask_SetPhase(MTP_AFTER_TITLE);

    /* Signal the migrated network_lobby on_enter to skip gateway and
     * jump straight to lobby phase 10 (RmlUI mode). */
    extern bool g_lobby_reenter_from_match;
    g_lobby_reenter_from_match = true;
    MenuScreen_Goto(MENU_SCREEN_NETWORK_LOBBY);
}

/* toSelectGame() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/** @brief Draw the two game-select button images. */

/* Training_Mode() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Option_Select() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* System_Direction() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Direction_Menu() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Load_Replay() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

const u8 Setup_Index_64[10] = { 1, 2, 3, 3, 4, 5, 6, 7, 8, 8 };

/* Game_Option() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Button_Config() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Sound_Test() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Memory_Card() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

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
    /* Phase 5b: Screens migrated to the registry dispatch here */
    if (MenuScreen_IsInGameActive()) {
        MenuScreen_InGameTick(task_ptr);
        return;
    }

    /* Intercept r_no[1] values that map to migrated screens */
    MenuScreenId ig_mapped = MenuScreen_FromInGameIndex(task_ptr->r_no[1]);
    if (ig_mapped != MENU_SCREEN_NONE) {
        MenuScreen_Goto(ig_mapped);
        MenuScreen_InGameTick(task_ptr);
        return;
    }

    /* Legacy dispatch (un-migrated In-Game screens only).
     * Indices 1–3 are intercepted by MenuScreen_FromInGameIndex() above.
     * Only index 0 (Menu_Init) and 4 (Pad_Come_Out) are still dispatched here. */
    void (*In_Game_Jmp_Tbl[IN_GAME_JMP_COUNT])() = {
        Menu_Init,   /* [0] Menu_Init — bootstrap (un-migrated) */
        Menu_Init,   /* [1] DEAD: migrated to MENU_SCREEN_PAUSE_MENU */
        Menu_Init,   /* [2] DEAD: migrated to MENU_SCREEN_BUTTON_CONFIG_IG */
        Menu_Init,   /* [3] DEAD: migrated to MENU_SCREEN_CHAR_CHANGE_IG */
        Pad_Come_Out /* [4] Pad_Come_Out — no-op stub (un-migrated) */
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

/* VS_Result() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/* Save_Replay() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

/** @brief Save Replay step 2 â€” execute memory-card write. */
void Setup_Save_Replay_2nd(struct _TASK* task_ptr, s16 arg1) {
    if (FadeIn(1, 25, 8)) {
        task_ptr->r_no[2]++;
        task_ptr->free[3] = 0;
        Menu_Cursor_X[0] = Setup_Final_Cursor_Pos(Menu_Cursor_X[0], 8);
    }
}

/** @brief Set up replay parameters (type, character, master player). */
void Setup_Replay_Sub(s16 type, MenuHeader char_type, s16 master_player) {
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
            void (*Training_Jmp_Tbl[TRAINING_JMP_COUNT])() = {
                Training_Init, /* [0] Training_Init — bootstrap (un-migrated) */
                Training_Init, /* [1] DEAD: migrated to MENU_SCREEN_NORMAL_TRAINING */
                Training_Init, /* [2] DEAD: migrated to MENU_SCREEN_BLOCKING_TRAINING */
                Training_Init, /* [3] DEAD: migrated to MENU_SCREEN_DUMMY_SETTING */
                Training_Init, /* [4] DEAD: migrated to MENU_SCREEN_TRAINING_OPTION */
                Training_Init, /* [5] DEAD: migrated to MENU_SCREEN_BUTTON_CONFIG_TR */
                Training_Init, /* [6] DEAD: migrated to MENU_SCREEN_CHAR_CHANGE_TR */
                Training_Init, /* [7] DEAD: migrated to MENU_SCREEN_BLOCKING_TR_OPTION */
            };
            Training_Jmp_Tbl[task_ptr->r_no[1]](task_ptr);
        }
    }

    /* Post-dispatch rendering — runs after BOTH registry and legacy paths */
    Akaobi();
    ToneDown(0xAA, 2);

    if ((!use_rmlui || !rmlui_menu_training) && Training_Index < TRAINING_LETTER_COUNT) {
        SSPutStr_Bigger(
            training_letter_data[Training_Index].pos_x, 0x18, 9, training_letter_data[Training_Index].menu, 1, 2, 1);
    }
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
void Normal_Training(struct _TASK* task_ptr) {
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
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Menu_Suicide[0] = 1;
        Training_Index = 2;

        for (ix = 0, s6 = y = 80; ix < 7; ix++, s5 = y += 16) {
            effect_A3_init(0, 1, ix, ix, 1, 48, y, 0);
        }

        for (ix = 0, y = 80, s4 = group = 2; ix < 5; ix++, group++, s3 = y += 16) {
            effect_A3_init(0, group, ix, ix, 1, 0xE6, y, 0);
        }

        break;

    case 1:
        Dummy_Move_Sub(task_ptr, Champion, 0, 0, 6);

        if (Menu_Cursor_Y[0] == 5 && IO_Result & 0x100) {
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
        Menu_Suicide[0] = 0;
        Menu_Suicide[1] = 1;
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        Training_Disp_Sub(task_ptr);
        break;
    }
}

/** @brief Training Option sub-menu â€” configure training parameters. */
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
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Menu_Suicide[0] = 1;
        Training_Index = 3;

        for (ix = 0, s6 = y = 72; ix < 6; ix++, s5 = y += 16) {
            effect_A3_init(0, 9, ix, ix, 1, 48, y, 1);
        }

        for (ix = 0, y = 72, s4 = group = 10; ix < 4; ix++, group++, s3 = y += 16) {
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
void Blocking_Training(struct _TASK* task_ptr) {
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

            effect_A3_init(1, 14, ix, ix, 0, x, y, 0);
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
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Menu_Suicide[0] = 1;
        Training_Index = 3;
        effect_A3_init(1, 24, 99, 0, 1, 51, 56, 1);
        effect_A3_init(1, 24, 99, 1, 1, 51, 106, 1);

        for (ix = 0, s6 = y = 72; ix < 6; ix++, s5 = y += 16) {
            if (ix == 2) {
                y += 20;
            }

            if (ix == 4) {
                y += 8;
            }

            effect_A3_init(1, 19, ix, ix, 1, 64, y, 0);
        }

        for (ix = 0, y = 72, s4 = group = 18; ix < 4; ix++, group++, s3 = y += 16) {
            if (ix == 2) {
                y += 20;
            }

            effect_A3_init(1, group + 2, ix, ix, 1, 264, y, 0);
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
void Character_Change(struct _TASK* task_ptr) {
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
            for (ix3 = 0; ix3 < 8; ix3++) {
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
            if (!(use_rmlui && rmlui_menu_replay)) {
                effect_57_init(110, MENU_HEADER_REPLAY, 0, 63, 999);
                Order[110] = 3;
                Order_Dir[110] = 8;
                Order_Timer[110] = 1;
            }
            Setup_File_Property(1, 0xFF);
            rmlui_replay_picker_open(1); /* always use RmlUI — ImGui removed */
            if (!(use_rmlui && rmlui_menu_replay)) {
                effect_66_init(138, 41, 0, 0, -1, -1, -0x7FF3);
                Order[138] = 3;
                Order_Timer[138] = 1;
            }
            break;

        case 1:
            Menu_Sub_case1(task_ptr);
            break;

        case 2:
            Setup_Save_Replay_2nd(task_ptr, 1);
            break;

        case 3: {
            int pick_result = rmlui_replay_picker_poll();
            if (pick_result == 0) {
                int slot = rmlui_replay_picker_get_slot();
                NativeSave_SaveReplay(slot);
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

/* Extra_Option() — REMOVED: migrated to MenuScreen registry (ms_*.c) */

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
        ans = ~(plsw_01[Pause_ID]) & plsw_00[Pause_ID];

        switch (ans) {
        case SWK_UP:
            Menu_Cursor_Y[0]--;
            if (Menu_Cursor_Y[0] < 0) {
                Menu_Cursor_Y[0] = 0;
            } else {
                SE_dir_cursor_move();
            }
            break;

        case SWK_DOWN:
            Menu_Cursor_Y[0]++;
            if (Menu_Cursor_Y[0] > 1) {
                Menu_Cursor_Y[0] = 1;
            } else {
                SE_dir_cursor_move();
            }
            break;

        case 0x100: /* Confirm */
        case 0x200: /* Cancel */
            if (Menu_Cursor_Y[0] || ans == 0x200) {
                /* User selected NO (cursor 1) or cancelled */
                task_ptr->r_no[1] = 1;
                Menu_Suicide[3] = 1;
            } else {
                /* User selected YES (cursor 0) - gracefully exit to menu */
                ToneDown(192, 32);
                Replay_Status[0] = 0;
                Replay_Status[1] = 0;
                Back_to_Mode_Select(task_ptr);
            }
            break;
        }
        break;
    }
}
