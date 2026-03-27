/**
 * @file ms_ranked_matchmaking.c
 * @brief MenuScreen wrapper for Ranked Matchmaking.
 */

#include "port/menu_screen.h"
#include "port/menu_task.h"
#include "port/config/config.h"

#include "port/sdl/rmlui/rmlui_ranked_matchmaking.h"
#include "structs.h"
#include "sf33rd/Source/Game/effect/eff45.h"
#include "sf33rd/Source/Game/menu/menu_network_constants.h"

#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/effect/eff57.h"
#include "sf33rd/Source/Game/ui/sc_sub.h"

#include <SDL3/SDL_log.h>

#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "netplay/netplay.h"
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "sf33rd/Source/Game/menu/menu.h"

extern s16 g_net_peer_idx;

static void ranked_matchmaking_enter(struct _TASK* task_ptr) {
    /* ── Standard Rebuild/Transition Sequence ── */
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[3] = 0;
    task_ptr->timer = 5; /* standard wait for fade transition */

    Netplay_EnterLobby();

    Menu_Suicide[0] = 1; /* kill gateway items (master_player=0) */
    Menu_Suicide[1] = 0; /* enable our items (master_player=1) */
    Message_Data->kind_req = 4; /* NET_BG_MODE_BLUE */

    effect_work_init();
    Menu_Common_Init();
    pulpul_stop();

    /* Reset cursor to top */
    Menu_Cursor_Y[0] = 0;
    Menu_Cursor_Y[1] = 0;

    /* Blue background banner (standard for full-screen RmlUi sub-menus) */
    Order[0x4E] = 5;
    Order_Timer[0x4E] = 1;
    Order_Dir[0x4E] = 1;
    effect_57_init(0x4E, MENU_HEADER_MODE_MENU, 0, 0x45, 0);

    rmlui_ranked_matchmaking_show();
}

static void ranked_matchmaking_tick(struct _TASK* task_ptr) {
    (void)task_ptr;
    
    rmlui_ranked_matchmaking_update();
    
    if (rmlui_ranked_matchmaking_wants_leave()) {
        rmlui_ranked_matchmaking_consume_leave();
        MenuScreen_Goto(MENU_SCREEN_NETWORK_LOBBY);
        return;
    }

    /* ─── Popup Handling ─── */
    if (SDLNetplayUI_HasPendingInvite()) {
        u16 trigger = 0;
        for (int i = 0; i < 2; i++) trigger |= (~plsw_01[i] & plsw_00[i]);

        if (trigger & 0x0100) { /* LP / SOUTH - Accept */
            Netplay_SetNegotiatedFT(SDLNetplayUI_GetPendingInviteFT());
            SDLNetplayUI_AcceptPendingInvite();
            SE_selected();
        } else if (trigger & 0x0200) { /* MK / EAST - Decline */
            SDLNetplayUI_DeclinePendingInvite();
            SE_selected();
        }
        return; /* Block menu navigation when popup is active */
    } else if (SDLNetplayUI_HasOutgoingChallenge()) {
        u16 trigger = 0;
        for (int i = 0; i < 2; i++) trigger |= (~plsw_01[i] & plsw_00[i]);

        if (trigger & (0x0100 | 0x0200)) { /* LP or MK - Cancel */
            SDLNetplayUI_CancelOutgoingChallenge();
            SE_selected();
        }
        return;
    }

    /* ─── Menu Navigation ─── */
    u16 res = MenuScreen_HandleCursor(7, 0xFF);
    u16 sw = MenuScreen_HandleCursorLR();
    int cursor = (int)Menu_Cursor_Y[0];

    if (res & 0x0100) { /* LP / Select */
        switch (cursor) {
            case 0: /* AUTO-ACCEPT */
                Config_SetBool(CFG_KEY_LOBBY_AUTO_CONNECT, !Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT));
                Config_Save();
                SE_dir_cursor_move();
                break;
            case 1: /* AUTO-SEARCH */
            {
                bool searching = !SDLNetplayUI_IsSearching();
                Config_SetBool(CFG_KEY_LOBBY_AUTO_SEARCH, searching);
                Config_Save();
                if (searching) SDLNetplayUI_StartSearch();
                else SDLNetplayUI_StopSearch();
                SE_dir_cursor_move();
                break;
            }
            case 2: /* REGION LOCK */
                Config_SetBool(CFG_KEY_NETPLAY_REGION_LOCK, !Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK));
                Config_Save();
                SE_dir_cursor_move();
                break;
            case 3: /* MAX PING */
            {
                int cur = Config_GetInt(CFG_KEY_NETPLAY_MAX_PING);
                if (cur >= 200) cur = 0;
                else if (cur <= 0) cur = 50;
                else cur += 50;
                Config_SetInt(CFG_KEY_NETPLAY_MAX_PING, cur);
                Config_Save();
                SE_selected();
                break;
            }
            case 4: /* BLOCK WIFI */
                Config_SetBool(CFG_KEY_NETPLAY_BLOCK_WIFI, !Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI));
                Config_Save();
                SE_dir_cursor_move();
                break;
            case 5: /* MATCH FT */
            {
                static const int fts[] = { 1, 2, 3, 5, 10 };
                int val = Config_GetInt(CFG_KEY_NETPLAY_FT);
                int idx = 1;
                for (int i = 0; i < 5; i++) if (fts[i] == val) { idx = i; break; }
                idx = (idx + 1) % 5;
                Config_SetInt(CFG_KEY_NETPLAY_FT, fts[idx]);
                Config_Save();
                SE_selected();
                break;
            }
            case 6: /* CONNECT */
                if (SDLNetplayUI_IsSearching() && SDLNetplayUI_GetOnlinePlayerCount() > 0) {
                    Netplay_SetNegotiatedFT(Config_GetInt(CFG_KEY_NETPLAY_FT));
                    SDLNetplayUI_ConnectToPlayer(g_net_peer_idx);
                    SE_selected();
                }
                break;
            case 7: /* EXIT */
                SE_selected();
                MenuScreen_Goto(MENU_SCREEN_NETWORK_LOBBY);
                break;
        }
    }

    /* ─── Setting Cycling (L/R) ─── */
    if (sw & 0x000C) { /* Left or Right */
        int dir = (sw & 0x0008) ? 1 : -1;
        switch (cursor) {
            case 0: /* AUTO-ACCEPT */
                Config_SetBool(CFG_KEY_LOBBY_AUTO_CONNECT, !Config_GetBool(CFG_KEY_LOBBY_AUTO_CONNECT));
                Config_Save();
                SE_dir_cursor_move();
                break;
            case 1: /* AUTO-SEARCH */
            {
                bool searching = !SDLNetplayUI_IsSearching();
                Config_SetBool(CFG_KEY_LOBBY_AUTO_SEARCH, searching);
                Config_Save();
                if (searching) SDLNetplayUI_StartSearch();
                else SDLNetplayUI_StopSearch();
                SE_dir_cursor_move();
                break;
            }
            case 2: /* REGION LOCK */
                Config_SetBool(CFG_KEY_NETPLAY_REGION_LOCK, !Config_GetBool(CFG_KEY_NETPLAY_REGION_LOCK));
                Config_Save();
                SE_dir_cursor_move();
                break;
            case 3: /* MAX PING */
            {
                int cur = Config_GetInt(CFG_KEY_NETPLAY_MAX_PING);
                if (dir == -1) { /* left */
                    if (cur <= 0) cur = 200;
                    else if (cur <= 50) cur = 0;
                    else cur -= 50;
                } else { /* right */
                    if (cur >= 200) cur = 0;
                    else if (cur <= 0) cur = 50;
                    else cur += 50;
                }
                Config_SetInt(CFG_KEY_NETPLAY_MAX_PING, cur);
                Config_Save();
                SE_dir_cursor_move();
                break;
            }
            case 4: /* BLOCK WIFI */
                Config_SetBool(CFG_KEY_NETPLAY_BLOCK_WIFI, !Config_GetBool(CFG_KEY_NETPLAY_BLOCK_WIFI));
                Config_Save();
                SE_dir_cursor_move();
                break;
            case 5: /* MATCH FT */
            {
                static const int fts[] = { 1, 2, 3, 5, 10 };
                int val = Config_GetInt(CFG_KEY_NETPLAY_FT);
                int idx = 0;
                for (int i=0; i<5; i++) if (fts[i] == val) { idx = i; break; }
                idx = (idx + dir + 5) % 5;
                Config_SetInt(CFG_KEY_NETPLAY_FT, fts[idx]);
                Config_Save();
                SE_dir_cursor_move();
                break;
            }
            case 6: /* CONNECT (cycle peer) */
            {
                int count = SDLNetplayUI_GetOnlinePlayerCount();
                if (count > 0) {
                    g_net_peer_idx = (s16)((g_net_peer_idx + dir + count) % count);
                    SE_dir_cursor_move();
                }
                break;
            }
        }
    }

    if (res & 0x0200) { /* MK / Back */
        SE_selected();
        MenuScreen_Goto(MENU_SCREEN_NETWORK_LOBBY);
    }
}

static void ranked_matchmaking_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

static void ranked_matchmaking_rmlui_show(void) { }
static void ranked_matchmaking_rmlui_hide(void) {
    rmlui_ranked_matchmaking_hide();
}

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_ranked_matchmaking_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_ranked_matchmaking_reg_ptr)(void) = ms_ranked_matchmaking_register;
static void ms_ranked_matchmaking_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_ranked_matchmaking_register(void) {
#else
void ms_ranked_matchmaking_register(void) {
#endif
    g_screens[MENU_SCREEN_RANKED_MATCHMAKING] = (MenuScreen) {
        .name = "ranked_matchmaking",
        .id = MENU_SCREEN_RANKED_MATCHMAKING,
        .parent = MENU_SCREEN_NETWORK_LOBBY,
        .on_enter = ranked_matchmaking_enter,
        .on_tick = ranked_matchmaking_tick,
        .on_exit = ranked_matchmaking_exit,
        .cursor_max = 7,
        .cancel_item = 7,
        .rmlui_show = ranked_matchmaking_rmlui_show,
        .rmlui_hide = ranked_matchmaking_rmlui_hide,
        .header_type = MENU_HEADER_NETWORK,
        .effect_slot = 0,
    };
}

