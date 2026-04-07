#include "sf33rd/Source/Game/menu/menu_network.h"
#include "sf33rd/Source/Game/menu/menu_network_constants.h"
#include "sf33rd/Source/Game/menu/menu_input_constants.h"
#include "sf33rd/Source/Game/menu/menu_save.h"
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
#include "port/sdl/rmlui/rmlui_network_replay_picker.h"
#include "port/sdl/rmlui/rmlui_option_menu.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui/rmlui_replay_picker.h"
#include "port/sdl/rmlui/rmlui_sound_menu.h"
#include "port/sdl/rmlui/rmlui_sysdir.h"
#include "port/sdl/rmlui/rmlui_training_menus.h"
#include "port/sdl/rmlui/rmlui_vs_result.h"
#include "port/sdl/rmlui/rmlui_vs_screen.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"


void NetLobby_DrawIncomingPopup(const char* name, const char* region, int ping) {
    /* Dark semi-transparent overlay covering the whole screen */
    {
        PAL_CURSOR_P op[4];
        PAL_CURSOR_COL ocol[4];
        op[0].x = NET_SCREEN_LEFT;
        op[0].y = NET_SCREEN_TOP;
        op[1].x = NET_SCREEN_RIGHT;
        op[1].y = NET_SCREEN_TOP;
        op[2].x = NET_SCREEN_LEFT;
        op[2].y = NET_SCREEN_BOTTOM;
        op[3].x = NET_SCREEN_RIGHT;
        op[3].y = NET_SCREEN_BOTTOM;
        ocol[0].color = ocol[1].color = ocol[2].color = ocol[3].color = NET_COLOR_OVERLAY_DIM;
        Renderer_Queue2DPrimitive((f32*)op, PrioBase[3], (uintptr_t)ocol[0].color, 0);
    }
    /* Centered popup box */
    {
        PAL_CURSOR_P bp[4];
        PAL_CURSOR_COL bcol[4];
        bp[0].x = NET_POPUP_LEFT;
        bp[0].y = NET_POPUP_TOP;
        bp[1].x = NET_POPUP_RIGHT;
        bp[1].y = NET_POPUP_TOP;
        bp[2].x = NET_POPUP_LEFT;
        bp[2].y = NET_POPUP_BOTTOM;
        bp[3].x = NET_POPUP_RIGHT;
        bp[3].y = NET_POPUP_BOTTOM;
        bcol[0].color = bcol[1].color = bcol[2].color = bcol[3].color = NET_COLOR_BOX_BG;
        Renderer_Queue2DPrimitive((f32*)bp, PrioBase[3], (uintptr_t)bcol[0].color, 0);

        /* Red border - top */
        PAL_CURSOR_P bb[4];
        PAL_CURSOR_COL bbcol[4];
        bb[0].x = NET_POPUP_LEFT;
        bb[0].y = NET_BORDER_TOP_Y0;
        bb[1].x = NET_POPUP_RIGHT;
        bb[1].y = NET_BORDER_TOP_Y0;
        bb[2].x = NET_POPUP_LEFT;
        bb[2].y = NET_BORDER_TOP_Y1;
        bb[3].x = NET_POPUP_RIGHT;
        bb[3].y = NET_BORDER_TOP_Y1;
        bbcol[0].color = bbcol[1].color = bbcol[2].color = bbcol[3].color = NET_COLOR_BORDER_RED;
        Renderer_Queue2DPrimitive((f32*)bb, PrioBase[3], (uintptr_t)bbcol[0].color, 0);
        /* Red border - bottom */
        bb[0].y = NET_BORDER_BOT_Y0;
        bb[1].y = NET_BORDER_BOT_Y0;
        bb[2].y = NET_BORDER_BOT_Y1;
        bb[3].y = NET_BORDER_BOT_Y1;
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
    dispButtonImage2(NET_BTN_A_X, NET_BTN_Y, 0, NET_BTN_SPRITE_W, NET_BTN_SPRITE_H, 0, 4); /* A button */
    SSPutStrPro(0, NET_TEXT_ACCEPT_X, NET_TEXT_BTN_Y, 4, NET_COLOR_ACCEPT, (s8*)"ACCEPT");
    dispButtonImage2(NET_BTN_B_X, NET_BTN_Y, 0, NET_BTN_SPRITE_W, NET_BTN_SPRITE_H, 0, 5); /* B button */
    SSPutStrPro(0, NET_TEXT_DECLINE_X, NET_TEXT_BTN_Y, 4, NET_COLOR_DECLINE, (s8*)"DECLINE");
}

void NetLobby_DrawOutgoingPopup(const char* name, int ping) {
    /* Dark semi-transparent overlay covering the whole screen */
    {
        PAL_CURSOR_P op[4];
        PAL_CURSOR_COL ocol[4];
        op[0].x = NET_SCREEN_LEFT;
        op[0].y = NET_SCREEN_TOP;
        op[1].x = NET_SCREEN_RIGHT;
        op[1].y = NET_SCREEN_TOP;
        op[2].x = NET_SCREEN_LEFT;
        op[2].y = NET_SCREEN_BOTTOM;
        op[3].x = NET_SCREEN_RIGHT;
        op[3].y = NET_SCREEN_BOTTOM;
        ocol[0].color = ocol[1].color = ocol[2].color = ocol[3].color = NET_COLOR_OVERLAY_DIM;
        Renderer_Queue2DPrimitive((f32*)op, PrioBase[3], (uintptr_t)ocol[0].color, 0);
    }
    /* Centered popup box */
    {
        PAL_CURSOR_P bp[4];
        PAL_CURSOR_COL bcol[4];
        bp[0].x = NET_POPUP_LEFT;
        bp[0].y = NET_POPUP_TOP;
        bp[1].x = NET_POPUP_RIGHT;
        bp[1].y = NET_POPUP_TOP;
        bp[2].x = NET_POPUP_LEFT;
        bp[2].y = NET_POPUP_BOTTOM;
        bp[3].x = NET_POPUP_RIGHT;
        bp[3].y = NET_POPUP_BOTTOM;
        bcol[0].color = bcol[1].color = bcol[2].color = bcol[3].color = NET_COLOR_BOX_BG;
        Renderer_Queue2DPrimitive((f32*)bp, PrioBase[3], (uintptr_t)bcol[0].color, 0);

        /* Red border - top */
        PAL_CURSOR_P bb[4];
        PAL_CURSOR_COL bbcol[4];
        bb[0].x = NET_POPUP_LEFT;
        bb[0].y = NET_BORDER_TOP_Y0;
        bb[1].x = NET_POPUP_RIGHT;
        bb[1].y = NET_BORDER_TOP_Y0;
        bb[2].x = NET_POPUP_LEFT;
        bb[2].y = NET_BORDER_TOP_Y1;
        bb[3].x = NET_POPUP_RIGHT;
        bb[3].y = NET_BORDER_TOP_Y1;
        bbcol[0].color = bbcol[1].color = bbcol[2].color = bbcol[3].color = NET_COLOR_BORDER_RED;
        Renderer_Queue2DPrimitive((f32*)bb, PrioBase[3], (uintptr_t)bbcol[0].color, 0);
        /* Red border - bottom */
        bb[0].y = NET_BORDER_BOT_Y0;
        bb[1].y = NET_BORDER_BOT_Y0;
        bb[2].y = NET_BORDER_BOT_Y1;
        bb[3].y = NET_BORDER_BOT_Y1;
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
    dispButtonImage2(NET_BTN_CANCEL_X, NET_BTN_Y, 0, NET_BTN_SPRITE_W, NET_BTN_SPRITE_H, 0, 5); /* B button */
    SSPutStrPro(0, NET_TEXT_CANCEL_X, NET_TEXT_BTN_Y, 4, NET_COLOR_DECLINE, (s8*)"CANCEL");
}

void Network_Lobby(struct _TASK* task_ptr) {
    s16 ix;
    static int s_slide_offset = NET_SLIDE_OFFSET_INIT; /* slide-in offset for SSPutStr elements */

    switch (task_ptr->r_no[2]) {
    /* ================================================================
     * GATEWAY PHASE (cases 0–3): LOBBY MODE / LOCAL NETWORK / EXIT
     * ================================================================ */
    case 0:
        /* Phase 0: Fade out, kill Mode_Select items, init gateway submenu */
        Menu_in_Sub(task_ptr);
        Message_Data->kind_req = NET_BG_MODE_BLUE; /* blue-BG background mode */

        /* Blue background banner — same effect used by lobby (case 11)
         * and leaderboard (case 5).  Without this, the gateway shows the
         * fire/lava BG inherited from Mode_Select. */
        Order[EFF_SLOT_HEADER] = 5;
        Order_Timer[EFF_SLOT_HEADER] = 1;
        Order_Dir[EFF_SLOT_HEADER] = 1;
        effect_57_init(EFF_SLOT_HEADER, MENU_HEADER_MODE_MENU, 0, EFF_Z_BLUE_BG, 0);

        /* Red "NETWORK" header */
        effect_57_init(EFF_SLOT_NET_HDR, MENU_HEADER_NETWORK, 0, EFF_Z_NETWORK_HDR, 2);
        Order[EFF_SLOT_NET_HDR] = 1;
        Order_Dir[EFF_SLOT_NET_HDR] = 8;
        Order_Timer[EFF_SLOT_NET_HDR] = 1;
        effect_04_init(1, NET_CURSOR_TYPE_GATEWAY, 0, NET_CURSOR_SLOT_GATEWAY); /* cursor type 7 = 6-item gateway */
        {
            static const s16 gateway_strings[] = { 74, 83, 75, 76, 77, 78, 79 };
            for (ix = 0; ix < 7; ix++) {
                effect_61_init(0, ix + 0x50, 0, 1, gateway_strings[ix], ix, EFF_FONT_CG_LARGE);
                Order[ix + 0x50] = 1;
                Order_Dir[ix + 0x50] = 4;
                Order_Timer[ix + 0x50] = ix + NET_ORDER_TIMER_BASE;
            }
        }
        Menu_Cursor_Move = 7;
        break;

    case 1:
        Menu_Sub_case1(task_ptr);
        break;

    case 2:
        if (FadeIn(1, FADE_SPEED_SLOW, 8) != 0) {
            task_ptr->r_no[2] += 1;
        }
        break;

    case 3:
        /* Gateway input: pick LOBBY MODE / RANKED MATCHMAKING / LOCAL NETWORK / LEADERBOARD / REPLAYS / PROFILE / EXIT */
        if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, 6, FADE_OPAQUE) == 0) {
            MC_Move_Sub(Check_Menu_Lever(1, 0), 0, 6, FADE_OPAQUE);
        }

        if (IO_Result == SWK_SOUTH || IO_Result == SWK_EAST) {
            SE_selected();

            if (Menu_Cursor_Y[0] == 6 || IO_Result == SWK_EAST) {
                /* EXIT — back to Mode_Select */
                Menu_Suicide[0] = 0;
                Menu_Suicide[1] = 1;
                task_ptr->r_no[1] = 1; /* Mode_Select */
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                task_ptr->free[0] = 0;
                Order[EFF_SLOT_NET_HDR] = 4;
                Order_Timer[EFF_SLOT_NET_HDR] = 4;
                break;
            }

            if (Menu_Cursor_Y[0] == 5) {
                /* PROFILE — show RmlUi player profile */
                MenuScreen_Goto(MENU_SCREEN_PLAYER_PROFILE);
                MenuScreen_Tick(task_ptr);
                return;
            } else if (Menu_Cursor_Y[0] == 4) {
                /* REPLAYS — jump to network replays phase */
                task_ptr->r_no[2] = 30;
            } else if (Menu_Cursor_Y[0] == 3) {
                /* LEADERBOARD — show RmlUI leaderboard overlay */
                rmlui_leaderboard_show();
                task_ptr->r_no[2] = 4; /* jump to leaderboard phase */
            } else if (Menu_Cursor_Y[0] == 2) {
                /* LOCAL NETWORK — jump to LAN-only lobby phase */
                task_ptr->free[2] = NET_MODE_LAN;  /* 2=lan-only */
                task_ptr->r_no[2] = 20; /* jump to LAN-only lobby phase */
            } else if (Menu_Cursor_Y[0] == 1) {
                /* RANKED MATCHMAKING */
                MenuScreen_Goto(MENU_SCREEN_RANKED_MATCHMAKING);
                MenuScreen_Tick(task_ptr);
                return;
            } else {
                /* LOBBY MODE (0) — always use RmlUI lobby */
                task_ptr->free[2] = NET_MODE_RMLUI;  /* 1=rmlui */
                task_ptr->r_no[2] = 10; /* jump to lobby phase */
            }
        }
        break;

    /* ================================================================
     * LEADERBOARD PHASE (cases 4–8): full-screen RmlUI leaderboard
     * ================================================================ */
    case 4:
        /* Phase 4: Fade out, kill gateway items, request blue BG */
        FadeOut(1, FADE_OPAQUE, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->r_no[3] = 0;
        task_ptr->timer = 5;
        Menu_Suicide[0] = 1; /* kill gateway items (master_player=0) */
        Menu_Suicide[1] = 0;
        Message_Data->kind_req = NET_BG_MODE_BLUE; /* blue-BG background mode */
        break;

    case 5:
        /* Phase 5: Destroy old effects, show blue BG + RmlUI leaderboard */
        FadeOut(1, FADE_OPAQUE, 8);
        task_ptr->r_no[2] += 1;

        effect_work_init();
        Menu_Common_Init();
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Order[EFF_SLOT_HEADER] = 5;
        Order_Timer[EFF_SLOT_HEADER] = 1;
        Order_Dir[EFF_SLOT_HEADER] = 1;

        /* Blue background banner */
        effect_57_init(EFF_SLOT_HEADER, MENU_HEADER_MODE_MENU, 0, EFF_Z_BLUE_BG, 0);

        /* Show RmlUI leaderboard (auto-fetches page 0) */
        rmlui_leaderboard_show();
        /* fallthrough to case 6 */

    case 6:
        /* Wait for fade-out timer */
        FadeOut(1, FADE_OPAQUE, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2] += 1;
            task_ptr->r_no[3] = 1;
            FadeInit();
        }
        break;

    case 7:
        /* Fade in */
        if (FadeIn(1, FADE_SPEED_SLOW, 8)) {
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
            if (trigger & SWK_LEFT) {
                SE_selected();
                rmlui_leaderboard_prev_page();
            }

            /* Right D-pad: next page */
            if (trigger & SWK_RIGHT) {
                SE_selected();
                rmlui_leaderboard_next_page();
            }

            if (trigger & SWK_EAST) { /* Cancel / B */
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
     * NETWORK REPLAYS PHASE (cases 30–34): online replay browser
     * ================================================================ */
    case 30:
        /* Phase 30: Fade out, kill gateway items, request blue BG */
        FadeOut(1, FADE_OPAQUE, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->r_no[3] = 0;
        task_ptr->timer = 5;
        Menu_Suicide[0] = 1; /* kill gateway items (master_player=0) */
        Menu_Suicide[1] = 0;
        Message_Data->kind_req = NET_BG_MODE_BLUE;
        break;

    case 31:
        /* Phase 31: Destroy old effects, show RmlUI network replay picker */
        FadeOut(1, FADE_OPAQUE, 8);
        task_ptr->r_no[2] += 1;

        effect_work_init();
        Menu_Common_Init();
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Order[EFF_SLOT_HEADER] = 5;
        Order_Timer[EFF_SLOT_HEADER] = 1;
        Order_Dir[EFF_SLOT_HEADER] = 1;

        /* Blue background banner */
        effect_57_init(EFF_SLOT_HEADER, MENU_HEADER_MODE_MENU, 0, EFF_Z_BLUE_BG, 0);

        /* Show RmlUI network replay picker (auto-fetches page 0) */
        rmlui_network_replay_picker_show();
        /* fallthrough to case 32 */

    case 32:
        /* Wait for fade-out timer */
        FadeOut(1, FADE_OPAQUE, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2] += 1;
            task_ptr->r_no[3] = 1;
            FadeInit();
        }
        break;

    case 33:
        /* Fade in */
        if (FadeIn(1, FADE_SPEED_SLOW, 8)) {
            task_ptr->r_no[2] += 1;
        }
        break;

    case 34:
        /* Network replay picker input loop */
        {
            rmlui_network_replay_picker_update();
            int poll = rmlui_network_replay_picker_poll();

            if (poll == -1) {
                /* Cancel — back to gateway */
                SE_selected();
                rmlui_network_replay_picker_hide();
                Menu_Suicide[0] = 0;
                Menu_Suicide[1] = 1;
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                task_ptr->free[0] = 0;
            } else if (poll == 0) {
                /* Replay selected and downloaded — exit to replay playback.
                 * Replay_w has been populated by the async download thread.
                 * We start the game transition natively here. */
                rmlui_network_replay_picker_hide();
                Menu_Suicide[0] = 0;
                Menu_Suicide[1] = 1;
                
                Decide_ID = 0;
                if (Interface_Type[0] == 0) {
                    Decide_ID = 1;
                }
                
                task_ptr->r_no[2] = 35; /* Jump to Load_Replay_Sub transition phase */
                task_ptr->r_no[3] = 0;
                task_ptr->free[0] = 0;
            }
            /* poll == 1: still browsing, keep looping */
        }
        break;

    case 35:
        /* Load_Replay_Sub initiates the multi-phase game exit for replay playback.
         * It will cpExitTask(TASK_MENU) and hand off to the engine when done. */
        Load_Replay_Sub(task_ptr);
        break;

    /* ================================================================
     * LOBBY PHASE (cases 10–13): full lobby with peer lists & popups
     * ================================================================ */
    case 10:
        /* Phase 10: Start fade, set suicide, request blue BG mode */
        FadeOut(1, FADE_OPAQUE, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->r_no[3] = 0;
        task_ptr->timer = 5;
        Menu_Suicide[0] = 1;        /* kill gateway items (master_player=0) */
        Menu_Suicide[1] = 0;        /* enable lobby items (master_player=1) */
        Message_Data->kind_req = NET_BG_MODE_BLUE; /* blue-BG background mode */
        break;

    case 11:
        /* Phase 11: Destroy old effects, rebuild lobby from scratch */
        FadeOut(1, FADE_OPAQUE, 8);
        task_ptr->r_no[2] += 1;

        effect_work_init();
        Menu_Common_Init();
        s_slide_offset = 384;
        Menu_Cursor_Y[0] = 2;
        Menu_Cursor_Y[1] = 0;
        Order[0x4E] = 5;
        Order_Timer[0x4E] = 1;

        /* Red slide-in header bar */
        Order_Dir[0x4E] = 1;

        if (task_ptr->free[2] == NET_MODE_RMLUI) {
            /* RMLUI lobby */
            rmlui_network_lobby_show();
        } else {
            /* NATIVE lobby */
            /* Right-side grey overlay boxes (LAN and Internet peer areas) */
            effect_66_init(EFF_SLOT_CURSOR_BG, EFF_SPRITE_LOBBY_BOX, 1, 0, -1, -1, EFF66_ZORDER_LOBBY_LAN);
            Order[EFF_SLOT_CURSOR_BG] = 3;
            Order_Timer[EFF_SLOT_CURSOR_BG] = 1;
            effect_66_init(EFF_SLOT_LOBBY_BOX2, EFF_SPRITE_LOBBY_BOX, 1, 0, -1, -1, EFF66_ZORDER_LOBBY_NET);
            Order[EFF_SLOT_LOBBY_BOX2] = 3;
            Order_Timer[EFF_SLOT_LOBBY_BOX2] = 1;

            /* Menu items: 6 items, EFF_FONT_COMPACT = compact 8px font, master_player=1 */
            {
                static const s16 lobby_strings[] = { 68, 69, 70, 71, 72, 73 };
                for (ix = 0; ix < 6; ix++) {
                    effect_61_init(0, ix + 0x50, 0, 1, lobby_strings[ix], ix, EFF_FONT_COMPACT);
                    Order[ix + 0x50] = 1;
                    Order_Dir[ix + 0x50] = 4;
                    Order_Timer[ix + 0x50] = ix + NET_ORDER_TIMER_BASE;
                }
            }

            /* Title: "NETWORK LOBBY" in big CG font (EFF_FONT_CG_LARGE), string index NET_STR_LOBBY_TITLE */
            effect_61_init(0, EFF_SLOT_NET_TITLE, 0, 1, NET_STR_LOBBY_TITLE, -1, EFF_FONT_CG_LARGE);
            Order[EFF_SLOT_NET_TITLE] = 1;
            Order_Dir[EFF_SLOT_NET_TITLE] = 4;
            Order_Timer[EFF_SLOT_NET_TITLE] = NET_ORDER_TIMER_TITLE;

            /* Message system for description text */
            Message_Data->pos_x = 0;
            Message_Data->pos_y = NET_MSG_POS_Y;
            Message_Data->pos_z = NET_MSG_POS_Z;
            Message_Data->request = NET_MSG_REQ_BASE;
            Message_Data->order = 0;
            Message_Data->timer = 1;
            effect_45_init(0, 0, 1);

            Menu_Cursor_Move = 6;
        }

        /* Blue background banner — always init (palette 0x45). */
        effect_57_init(EFF_SLOT_HEADER, MENU_HEADER_MODE_MENU, 0, EFF_Z_BLUE_BG, 0);

        /* Enter lobby state */
        SDLNetplayUI_SetNativeLobbyActive(true);
        Netplay_EnterLobby();
        /* fallthrough to case 12 */

    case 12:
        /* Wait for fade-out timer */
        FadeOut(1, FADE_OPAQUE, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2] += 1;
            task_ptr->r_no[3] = 1;
            FadeInit();
        }
        break;

    case 13:
        /* Fade in */
        if (FadeIn(1, FADE_SPEED_SLOW, 8)) {
            task_ptr->r_no[2] += 1;
        }
        break;

    case 14: {
        /* --- Lobby input loop --- */

        /* When a match is actively running, suspend menu input so
         * button presses aren't double-handled. */
        if (Netplay_GetSessionState() != NETPLAY_SESSION_LOBBY && Netplay_GetSessionState() != NETPLAY_SESSION_IDLE)
            break;

        /* === Password popup input intercept === */
        if (rmlui_network_lobby_is_password_popup_visible()) {
            u16 click = (~plsw_01[0] & plsw_00[0]) | (~plsw_01[1] & plsw_00[1]);
            if (click & 1)  /* UP */
                rmlui_network_lobby_password_input(1);
            if (click & 2)  /* DOWN */
                rmlui_network_lobby_password_input(2);
            if (click & 4)  /* LEFT */
                rmlui_network_lobby_password_input(4);
            if (click & 8)  /* RIGHT */
                rmlui_network_lobby_password_input(3);
            if (click & SWK_SOUTH) { /* Confirm */
                rmlui_network_lobby_submit_password();
                SE_selected();
            } else if (click & SWK_EAST) { /* Cancel */
                rmlui_network_lobby_cancel_password();
                SE_selected();
            }
            break; /* skip all normal menu input while popup is active */
        }

        /* Decelerate slide-in offset */
        if (s_slide_offset > 0) {
            s_slide_offset = (int)(s_slide_offset / 1.18f);
            if (s_slide_offset < 2)
                s_slide_offset = 0;
        }
        const s16 sl = (s16)s_slide_offset;

        /* Custom red banner (brighter than default Akaobi 0xA0D00000) */
        if (task_ptr->free[2] == NET_MODE_NATIVE) {
            PAL_CURSOR_P ap[4];
            PAL_CURSOR_COL acol[4];
            u8 ci;
            for (ci = 0; ci < 4; ci++) {
                ap[ci].x = Akaobi_Pos_tbl[ci * 2];
                ap[ci].y = Akaobi_Pos_tbl[(ci * 2) + 1];
                acol[ci].color = NET_COLOR_BORDER_RED; /* fully opaque vibrant red */
            }
            Renderer_Queue2DPrimitive((f32*)ap, PrioBase[69], (uintptr_t)acol[0].color, 0);

            /* White top border (1px, 1px gap above red) */
            ap[0].x = NET_SCREEN_LEFT;
            ap[0].y = NET_HDR_TOP_BORDER_Y0;
            ap[1].x = NET_SCREEN_RIGHT;
            ap[1].y = NET_HDR_TOP_BORDER_Y0;
            ap[2].x = NET_SCREEN_LEFT;
            ap[2].y = NET_HDR_TOP_BORDER_Y1;
            ap[3].x = NET_SCREEN_RIGHT;
            ap[3].y = NET_HDR_TOP_BORDER_Y1;
            acol[0].color = acol[1].color = acol[2].color = acol[3].color = NET_COLOR_WHITE;
            Renderer_Queue2DPrimitive((f32*)ap, PrioBase[67], (uintptr_t)acol[0].color, 0);

            /* White bottom border (1px, 1px gap below red) */
            ap[0].y = NET_HDR_BOT_BORDER_Y0;
            ap[1].y = NET_HDR_BOT_BORDER_Y0;
            ap[2].y = NET_HDR_BOT_BORDER_Y1;
            ap[3].y = NET_HDR_BOT_BORDER_Y1;
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

        /* Handle cursor movement (17 items: 0..16) */
        {
            s16 prev_cursor = Menu_Cursor_Y[0];
            if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, 16, FADE_OPAQUE) == 0) {
                MC_Move_Sub(Check_Menu_Lever(1, 0), 0, 16, FADE_OPAQUE);
            }
            if (Menu_Cursor_Y[0] < 2) {
                if (prev_cursor == 2) Menu_Cursor_Y[0] = 16;
                else Menu_Cursor_Y[0] = 2;
            }
            if (popup_active) {
                Menu_Cursor_Y[0] = prev_cursor;
            } else {
                /* Skip FORMAT row (12) when room type is not tournament */
                if (Menu_Cursor_Y[0] == 12 && rmlui_network_lobby_get_create_room_type() != 2) {
                    if (Menu_Cursor_Y[0] > prev_cursor)
                        Menu_Cursor_Y[0] = 13; /* moving down → skip to CREATE ROOM */
                    else
                        Menu_Cursor_Y[0] = 11; /* moving up → skip to PASSWORD */
                }
                if (prev_cursor != Menu_Cursor_Y[0]) {
                    if (task_ptr->free[2] == NET_MODE_NATIVE) {
                        Message_Data->order = 1;
                        Message_Data->request = NET_MSG_REQ_BASE + Menu_Cursor_Y[0];
                        Message_Data->timer = 2;
                        Message_Data->pos_y = NET_MSG_POS_Y;
                    }
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
                case 9: { /* ROOM TYPE toggle */
                    if (task_ptr->free[2] == NET_MODE_RMLUI) {
                        rmlui_network_lobby_cycle_room_type((click & 4) ? -1 : 1);
                        SE_dir_cursor_move();
                    }
                    break;
                }
                case 10: { /* VISIBILITY toggle */
                    if (task_ptr->free[2] == NET_MODE_RMLUI) {
                        rmlui_network_lobby_cycle_visibility((click & 4) ? -1 : 1);
                        SE_dir_cursor_move();
                    }
                    break;
                }
                case 12: { /* TOURNAMENT FORMAT cycle */
                    if (task_ptr->free[2] == NET_MODE_RMLUI) {
                        rmlui_network_lobby_cycle_tournament_format((click & 4) ? -1 : 1);
                        SE_dir_cursor_move();
                    }
                    break;
                }
                case 14: { /* JOIN ROOM (room list scroll) */
                    if (task_ptr->free[2] == NET_MODE_RMLUI) {
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
        if (task_ptr->free[2] == NET_MODE_NATIVE && !popup_active) {
            /* === Display toggle values (right of labels) === */
            {
                bool lan_ac = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
                SSPutStr_Bigger(NET_TOGGLE_X + sl, NET_LAN_TOGGLE_Y, 5, lan_ac ? (s8*)"ON" : (s8*)"OFF", 1.0f, lan_ac ? 9 : 1, 1.0f);

                bool net_ac = Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT);
                SSPutStr_Bigger(NET_TOGGLE_X + sl, NET_NET_AUTOCONN_Y, 5, net_ac ? (s8*)"ON" : (s8*)"OFF", 1.0f, net_ac ? 9 : 1, 1.0f);

                bool auto_s = Config_GetBool(CFG_KEY_LOBBY_AUTO_SEARCH);
                SSPutStr_Bigger(NET_TOGGLE_X + sl, NET_NET_AUTOSRCH_Y, 5, auto_s ? (s8*)"ON" : (s8*)"OFF", 1.0f, auto_s ? 9 : 1, 1.0f);
            }

            /* === LAN / NET Headers === */
            {
                const char* lan_hdr = "----- LAN -----";
                int lan_hdr_px = (int)SDL_strlen(lan_hdr) * 8;
                s16 lan_hdr_x = (s16)((NET_SCREEN_WIDTH_PX - lan_hdr_px) / 2);
                SSPutStr_Bigger(lan_hdr_x + sl, NET_LAN_HDR_Y, 5, (s8*)lan_hdr, 1.0f, 0, 1.0f);
            }
            {
                const char* net_hdr = "-- INTERNET --";
                int net_hdr_px = (int)SDL_strlen(net_hdr) * 8;
                s16 net_hdr_x = (s16)((NET_SCREEN_WIDTH_PX - net_hdr_px) / 2);
                SSPutStr_Bigger(net_hdr_x + sl, NET_NET_HDR_Y, 5, (s8*)net_hdr, 1.0f, 0, 1.0f);
            }

            /* === Peer / Online Info (Right Side) === */
            {
                s16 peer_x = NET_PEER_INFO_X;
                s16 lan_peer_y = NET_LAN_PEER_Y_FULL;
                s16 net_peer_y = NET_NET_PEER_Y;

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
                    SSPutStr_Bigger(peer_x + sl, (u16)(lan_peer_y + NET_PEER_SUBROW), 5, (s8*)buf, 1.0f, 0, 1.0f);
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
                        SSPutStr_Bigger(peer_x + sl, (u16)(net_peer_y + NET_PEER_SUBROW), 5, (s8*)s_buf, 1.0f, 0, 1.0f);
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
                dp[0].x = NET_SCREEN_LEFT;
                dp[0].y = NET_DESC_TOP;
                dp[1].x = NET_SCREEN_RIGHT;
                dp[1].y = NET_DESC_TOP;
                dp[2].x = NET_SCREEN_LEFT;
                dp[2].y = NET_DESC_BOTTOM;
                dp[3].x = NET_SCREEN_RIGHT;
                dp[3].y = NET_DESC_BOTTOM;
                dcol[0].color = dcol[1].color = dcol[2].color = dcol[3].color = NET_COLOR_DESC_BG;
                Renderer_Queue2DPrimitive((f32*)dp, PrioBase[70], (uintptr_t)dcol[0].color, 0);
            }

            /* === Status line === */
            {
                const char* status = SDLNetplayUI_GetStatusMsg();
                if (status[0]) {
                    SSPutStr_Bigger(NET_STATUS_X + sl, NET_STATUS_Y, 5, (s8*)status, 1.0f, 9, 1.0f);
                } else {
                    NetplayDiscoveredPeer c_peers[16];
                    int c_count = Discovery_GetPeers(c_peers, 16);
                    uint32_t current_target = Discovery_GetChallengeTarget();
                    bool showing_status = false;

                    for (int i = 0; i < c_count; i++) {
                        if (c_peers[i].is_challenging_me) {
                            char c_buf[64];
                            SDL_snprintf(c_buf, sizeof(c_buf), "CHALLENGED BY %s!", c_peers[i].name);
                            SSPutStr_Bigger(NET_STATUS_X + sl, NET_STATUS_Y, 5, (s8*)c_buf, 1.0f, 9, 1.0f);
                            showing_status = true;
                            break;
                        }
                    }

                    if (!showing_status && current_target != 0) {
                        for (int i = 0; i < c_count; i++) {
                            if (c_peers[i].instance_id == current_target) {
                                char c_buf[64];
                                SDL_snprintf(c_buf, sizeof(c_buf), "CHALLENGING %s...", c_peers[i].name);
                                SSPutStr_Bigger(NET_STATUS_X + sl, NET_STATUS_Y, 5, (s8*)c_buf, 1.0f, 9, 1.0f);
                                showing_status = true;
                                break;
                            }
                        }
                    }

                    if (!showing_status && SDLNetplayUI_IsDiscovering()) {
                        SSPutStr_Bigger(NET_STATUS_X + sl, NET_STATUS_Y, 5, (s8*)"DISCOVERING...", 1.0f, 9, 1.0f);
                    }
                }
            }
        } /* end native background text */

        /* === Incoming Challenge Popup (Internet) === */
        if (SDLNetplayUI_HasPendingInvite()) {
            if (task_ptr->free[2] == NET_MODE_NATIVE)
                NetLobby_DrawIncomingPopup(SDLNetplayUI_GetPendingInviteName(),
                                           SDLNetplayUI_GetPendingInviteRegion(),
                                           SDLNetplayUI_GetPendingInvitePing());

            switch (IO_Result) {
            case SWK_SOUTH:
                Netplay_SetNegotiatedFT(SDLNetplayUI_GetPendingInviteFT());
                SDLNetplayUI_AcceptPendingInvite();
                SE_selected();
                break;
            case SWK_EAST:
                SDLNetplayUI_DeclinePendingInvite();
                SE_selected();
                break;
            default:
                break;
            }
        } else if (SDLNetplayUI_HasOutgoingChallenge()) {
            if (task_ptr->free[2] == NET_MODE_NATIVE)
                NetLobby_DrawOutgoingPopup(SDLNetplayUI_GetOutgoingChallengeName(),
                                           SDLNetplayUI_GetOutgoingChallengePing());

            if (IO_Result == SWK_SOUTH || IO_Result == SWK_EAST) {
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
                if (task_ptr->free[2] == NET_MODE_NATIVE)
                    NetLobby_DrawOutgoingPopup(tgt_name, tgt_ping);
            }

            if (IO_Result == SWK_SOUTH || IO_Result == SWK_EAST) {
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
                if (task_ptr->free[2] == NET_MODE_NATIVE)
                    NetLobby_DrawIncomingPopup(
                        ip_peers[lan_challenger].display_name[0] ? ip_peers[lan_challenger].display_name
                                                                 : ip_peers[lan_challenger].name,
                        "",
                        ip_peers[lan_challenger].player_id[0] ? PingProbe_GetRTT(ip_peers[lan_challenger].player_id)
                                                              : -1);

                switch (IO_Result) {
                case SWK_SOUTH:
                    Netplay_SetNegotiatedFT(ip_peers[lan_challenger].ft_value);
                    Discovery_SetChallengeTarget(ip_peers[lan_challenger].instance_id);
                    SE_selected();
                    break;
                case SWK_EAST:
                    Discovery_DismissChallenger(ip_peers[lan_challenger].instance_id);
                    SE_selected();
                    break;
                default:
                    break;
                }
            } else {
                /* === Handle confirm/cancel (normal lobby input) === */
                switch (IO_Result) {
                case SWK_SOUTH: /* Confirm */
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

                    case 9: /* ROOM TYPE toggle (confirm = toggle) */
                        if (task_ptr->free[2] == NET_MODE_RMLUI) {
                            rmlui_network_lobby_cycle_room_type(1);
                        }
                        SE_selected();
                        break;
                    case 10: /* VISIBILITY toggle (confirm = toggle) */
                        if (task_ptr->free[2] == NET_MODE_RMLUI) {
                            rmlui_network_lobby_cycle_visibility(1);
                        }
                        SE_selected();
                        break;
                    case 11: /* PASSWORD (open password entry popup) */
                        if (task_ptr->free[2] == NET_MODE_RMLUI) {
                            rmlui_network_lobby_open_create_password();
                        }
                        SE_selected();
                        break;
                    case 12: /* TOURNAMENT FORMAT cycle (confirm = advance) */
                        if (task_ptr->free[2] == NET_MODE_RMLUI) {
                            rmlui_network_lobby_cycle_tournament_format(1);
                        }
                        SE_selected();
                        break;
                    case 13: /* CREATE ROOM (RmlUI only) */
                        if (task_ptr->free[2] == NET_MODE_RMLUI) {
                            rmlui_network_lobby_create_room();
                        }
                        SE_selected();
                        break;
                    case 14: /* JOIN ROOM (RmlUI only) */
                        if (task_ptr->free[2] == NET_MODE_RMLUI) {
                            rmlui_network_lobby_join_room();
                        }
                        SE_selected();
                        break;
                    case 15: /* JOIN BY CODE (RmlUI only) */
                        if (task_ptr->free[2] == NET_MODE_RMLUI) {
                            rmlui_network_lobby_open_join_code();
                        }
                        SE_selected();
                        break;
                    case 16:
                        /* EXIT */
                        goto lobby_exit;
                    }
                    break;

                case SWK_EAST: /* Cancel */
                    if (Discovery_GetChallengeTarget() != 0) {
                        Discovery_SetChallengeTarget(0);
                        SE_selected();
                        break;
                    }
                lobby_exit:
                    SE_selected();
                    SDLNetplayUI_SetNativeLobbyActive(false);
                    if (task_ptr->free[2] == NET_MODE_RMLUI)
                        rmlui_network_lobby_hide();
                    Netplay_HandleMenuExit();
                    Menu_Suicide[0] = 0;
                    Menu_Suicide[1] = 1;   /* kill our items + blue BG */
                    
                    /* Return to gateway: r_no[2]=0 causes Network_Lobby to re-init */
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
        FadeOut(1, FADE_OPAQUE, 8);
        task_ptr->r_no[2] += 1;
        task_ptr->r_no[3] = 0;
        task_ptr->timer = 5;
        Menu_Suicide[0] = 1;        /* kill gateway items (master_player=0) */
        Menu_Suicide[1] = 0;        /* enable lobby items (master_player=1) */
        Message_Data->kind_req = NET_BG_MODE_BLUE; /* blue-BG background mode */
        break;

    case 21:
        /* Phase 21: Destroy old effects, rebuild LAN-only lobby */
        FadeOut(1, FADE_OPAQUE, 8);
        task_ptr->r_no[2] += 1;

        effect_work_init();
        Menu_Common_Init();
        s_slide_offset = 384;
        Menu_Cursor_Y[0] = 0;
        Menu_Cursor_Y[1] = 0;
        Order[EFF_SLOT_HEADER] = 5;
        Order_Timer[EFF_SLOT_HEADER] = 1;

        /* Red slide-in header bar */
        Order_Dir[EFF_SLOT_HEADER] = 1;

        /* Right-side grey overlay box (LAN peer area) */
        effect_66_init(EFF_SLOT_CURSOR_BG, EFF_SPRITE_LOBBY_BOX, 1, 0, -1, -1, EFF66_ZORDER_LOBBY_LAN);
        Order[EFF_SLOT_CURSOR_BG] = 3;
        Order_Timer[EFF_SLOT_CURSOR_BG] = 1;

        /* Menu items: 3 items (AUTO-CONN, CONNECT, EXIT), EFF_FONT_COMPACT = compact 8px font, master_player=1 */
        {
            static const s16 lan_lobby_strings[] = { 80, 81, 82 };
            for (ix = 0; ix < 3; ix++) {
                effect_61_init(0, ix + 0x50, 0, 1, lan_lobby_strings[ix], ix, EFF_FONT_COMPACT);
                Order[ix + 0x50] = 1;
                Order_Dir[ix + 0x50] = 4;
                Order_Timer[ix + 0x50] = ix + NET_ORDER_TIMER_BASE;
            }
        }

        /* Red "NETWORK" header */
        effect_57_init(EFF_SLOT_NET_HDR, MENU_HEADER_NETWORK, 0, EFF_Z_NETWORK_HDR, 2);
        Order[EFF_SLOT_NET_HDR] = 1;
        Order_Dir[EFF_SLOT_NET_HDR] = 8;
        Order_Timer[EFF_SLOT_NET_HDR] = 1;

        /* Message system for description text */
        Message_Data->pos_x = 0;
        Message_Data->pos_y = NET_MSG_POS_Y;
        Message_Data->pos_z = NET_MSG_POS_Z;
        Message_Data->request = NET_MSG_REQ_BASE;
        Message_Data->order = 0;
        Message_Data->timer = 1;
        effect_45_init(0, 0, 1);

        Menu_Cursor_Move = 3;

        /* Blue background banner — always init (palette 0x45). */
        effect_57_init(EFF_SLOT_HEADER, MENU_HEADER_MODE_MENU, 0, EFF_Z_BLUE_BG, 0);

        /* Enter lobby state (LAN-only — no server registration) */
        SDLNetplayUI_SetNativeLobbyActive(true);
        Netplay_EnterLobby();
        /* fallthrough to case 22 */

    case 22:
        /* Wait for fade-out timer */
        FadeOut(1, FADE_OPAQUE, 8);

        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2] += 1;
            task_ptr->r_no[3] = 1;
            FadeInit();
        }
        break;

    case 23:
        /* Fade in */
        if (FadeIn(1, FADE_SPEED_SLOW, 8)) {
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

        /* (Native banner is handled by effect_57_init) */

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
            if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, 2, FADE_OPAQUE) == 0) {
                MC_Move_Sub(Check_Menu_Lever(1, 0), 0, 2, FADE_OPAQUE);
            }
            if (popup_active) {
                Menu_Cursor_Y[0] = prev_cursor;
            } else if (prev_cursor != Menu_Cursor_Y[0]) {
                Message_Data->order = 1;
                if (Menu_Cursor_Y[0] == 2) {
                    Message_Data->request = NET_MSG_REQ_BASE + 5; /* Use EXIT message */
                } else {
                    Message_Data->request = NET_MSG_REQ_BASE + Menu_Cursor_Y[0];
                }
                Message_Data->timer = 2;
                Message_Data->pos_y = NET_MSG_POS_Y;
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
                SSPutStr_Bigger(NET_TOGGLE_X_LANONLY + sl, NET_LAN_TOGGLE_Y_LANONLY, 5, lan_ac ? (s8*)"ON" : (s8*)"OFF", 1.0f, lan_ac ? 9 : 1, 1.0f);
            }

            /* LAN Header */
            {
                const char* lan_hdr = "----- LAN -----";
                int lan_hdr_px = (int)SDL_strlen(lan_hdr) * 8;
                s16 lan_hdr_x = (s16)((NET_SCREEN_WIDTH_PX - lan_hdr_px) / 2);
                SSPutStr_Bigger(lan_hdr_x + sl, NET_LAN_HDR_Y_LANONLY, 5, (s8*)lan_hdr, 1.0f, 0, 1.0f);
            }

            /* Peer Info (Right Side) */
            {
                s16 peer_x = NET_PEER_INFO_X_LANONLY;
                s16 lan_peer_y = NET_LAN_PEER_Y_LANONLY;

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
                    SSPutStr_Bigger(peer_x + sl, (u16)(lan_peer_y + NET_PEER_SUBROW), 5, (s8*)buf, 1.0f, 0, 1.0f);
                } else {
                    g_lobby_peer_idx = 0;
                    SSPutStr_Bigger(peer_x + sl, lan_peer_y, 5, (s8*)"NONE", 1.0f, 1, 1.0f);
                }
            }

            /* Grey description banner */
            {
                PAL_CURSOR_P dp[4];
                PAL_CURSOR_COL dcol[4];
                dp[0].x = NET_SCREEN_LEFT;
                dp[0].y = NET_DESC_TOP;
                dp[1].x = NET_SCREEN_RIGHT;
                dp[1].y = NET_DESC_TOP;
                dp[2].x = NET_SCREEN_LEFT;
                dp[2].y = NET_DESC_BOTTOM;
                dp[3].x = NET_SCREEN_RIGHT;
                dp[3].y = NET_DESC_BOTTOM;
                dcol[0].color = dcol[1].color = dcol[2].color = dcol[3].color = NET_COLOR_DESC_BG;
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
                        SSPutStr_Bigger(NET_STATUS_X + sl, NET_STATUS_Y, 5, (s8*)c_buf, 1.0f, 9, 1.0f);
                        showing_status = true;
                        break;
                    }
                }

                if (!showing_status && current_target != 0) {
                    for (int i = 0; i < c_count; i++) {
                        if (c_peers[i].instance_id == current_target) {
                            char c_buf[64];
                            SDL_snprintf(c_buf, sizeof(c_buf), "CHALLENGING %s...", c_peers[i].name);
                            SSPutStr_Bigger(NET_STATUS_X + sl, NET_STATUS_Y, 5, (s8*)c_buf, 1.0f, 9, 1.0f);
                            showing_status = true;
                            break;
                        }
                    }
                }

                if (!showing_status && SDLNetplayUI_IsDiscovering()) {
                    SSPutStr_Bigger(NET_STATUS_X + sl, NET_STATUS_Y, 5, (s8*)"DISCOVERING...", 1.0f, 9, 1.0f);
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

            if (IO_Result == SWK_SOUTH || IO_Result == SWK_EAST) {
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
                case SWK_SOUTH:
                    Netplay_SetNegotiatedFT(ip_peers[lan_challenger].ft_value);
                    Discovery_SetChallengeTarget(ip_peers[lan_challenger].instance_id);
                    SE_selected();
                    break;
                case SWK_EAST:
                    Discovery_DismissChallenger(ip_peers[lan_challenger].instance_id);
                    SE_selected();
                    break;
                default:
                    break;
                }
            } else {
                /* === Handle confirm/cancel (normal LAN-only lobby input) === */
                switch (IO_Result) {
                case SWK_SOUTH: /* Confirm */
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

                case SWK_EAST: /* Cancel */
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
                    
                    /* Return to gateway: r_no[2]=0 causes Network_Lobby to re-init */
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

void Menu_ReenterNetworkLobby(void) {
    s16 ix;
    InitTask_ClearAllRNo();
    for (ix = 0; ix < 4; ix++) {
        G_No[ix] = 0;
        E_No[ix] = 0;
        D_No[ix] = 0;
    }

    G_No[0] = GAME_STATE_MENU;
    G_No[1] = GAME_MODE_MENU_IDLE; // Menu Idle State
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
    Task_Activate(TASK_GAME);
    MenuTask_Activate();

    cpReadyTask(TASK_MENU, Menu_Task);
    MenuTask_SetPhase(MTP_AFTER_TITLE);

    /* Signal the migrated network_lobby on_enter to skip gateway and
     * jump straight to lobby phase 10 (RmlUI mode). */
    extern bool g_lobby_reenter_from_match;
    g_lobby_reenter_from_match = true;
    MenuScreen_Goto(MENU_SCREEN_NETWORK_LOBBY);
}
