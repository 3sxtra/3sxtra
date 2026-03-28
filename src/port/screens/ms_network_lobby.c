/**
 * @file ms_network_lobby.c
 * @brief Migrated Network Lobby screen — Task 17.
 *
 * Replaces the dispatch entry for Network_Lobby() (AT_Jmp_Tbl index 21)
 * with a MenuScreen registry thin-wrapper.  The legacy Network_Lobby()
 * function body (~1230 lines) is called directly from on_tick — this
 * preserves pixel-perfect behavioral parity while properly integrating
 * with the MenuScreen lifecycle.
 *
 * The screen has 4 major sub-sections driven by r_no[2]:
 *   - Gateway (cases 0-3):   4-item menu (Lobby/LAN/Leaderboard/Exit)
 *   - Leaderboard (4-8):     Full-screen RmlUI leaderboard
 *   - Full Lobby (10-14):    12-item lobby with toggles, popups, challenges
 *   - LAN-Only Lobby (20-24): 3-item stripped-down LAN lobby
 *
 * Menu_ReenterNetworkLobby() (in menu.c) is rewritten to use
 * MenuScreen_Goto(MENU_SCREEN_NETWORK_LOBBY) with the g_lobby_reenter_from_match
 * flag so the on_enter callback can skip the gateway and jump to phase 10.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 4).
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/engine/workuser.h"    /* Menu_Cursor_Y, Order, etc. */
#include "sf33rd/Source/Game/menu/menu.h"          /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* Network_Lobby, Menu_Sub_case1 */
#include "sf33rd/Source/Game/ui/sc_sub.h"          /* FadeOut, FadeIn, FadeInit */
#include "structs.h"

/* Blue background */
#include "sf33rd/Source/Game/effect/eff45.h"
#include "sf33rd/Source/Game/menu/menu_network_constants.h"

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_leaderboard.h"
#include "port/sdl/rmlui/rmlui_network_lobby.h"
#include "port/sdl/rmlui/rmlui_network_replay_picker.h"
#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include "port/sdl/rmlui/rmlui_tournament_lobby.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  Re-entry flag — set by Menu_ReenterNetworkLobby (menu.c)
 *
 *  When true, on_enter skips the gateway (case 0) and jumps straight
 *  to lobby phase 10 with free[2]=1 (RmlUI mode).
 * ═══════════════════════════════════════════════════════════════════════════ */

bool g_lobby_reenter_from_match = false;
bool g_lobby_reenter_lan_match = false;
bool g_lobby_reenter_to_replay = false;

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter
 *
 *  Sets r_no[1]=21 and r_no[2]=0 so the first on_tick call runs
 *  Network_Lobby case 0 (gateway init).  For re-entry from match,
 *  sets r_no[2]=10 and free[2]=1 to jump to lobby phase 10.
 *
 *  The dispatcher's standard WAIT/FADE_IN phases are bypassed: on_enter
 *  sets timer=0 so Menu_Sub_case1 returns immediately, and FadeIn returns
 *  immediately when no FadeOut was active.  This gets us to ACTIVE/on_tick
 *  within ~2 frames where the legacy code drives its own setup.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void network_lobby_enter(struct _TASK* task_ptr) {
    task_ptr->r_no[1] = 21; /* for legacy compat */

    if (g_lobby_reenter_to_replay) {
        /* Re-entry from profile with a downloaded replay ready to play.
         * Skips gateway and jumps straight to Replay transition. */
        g_lobby_reenter_to_replay = false;
        task_ptr->r_no[2] = 35;
        task_ptr->r_no[3] = 0;
        task_ptr->free[0] = 0;
    } else if (g_lobby_reenter_lan_match) {
        /* Re-entry from Native LAN match — jump to lobby phase 20 (LAN mode). */
        g_lobby_reenter_lan_match = false;
        task_ptr->r_no[2] = 20;
        task_ptr->r_no[3] = 0;
        task_ptr->free[2] = 0; /* 0 = Native mode */
    } else if (g_lobby_reenter_from_match) {
        /* Re-entry from match — jump to lobby phase 10 (RmlUI mode).
         * The legacy Network_Lobby() case 10 will do its own FadeOut,
         * timer, effect setup, and blue BG rebuild. */
        g_lobby_reenter_from_match = false;
        task_ptr->r_no[2] = 10;
        task_ptr->r_no[3] = 0;
        task_ptr->free[2] = 1; /* 1 = RmlUI mode */
    } else {
        /* Fresh entry from Mode_Select — start at Gateway (case 0).
         * The legacy Network_Lobby() case 0 will run Menu_in_Sub and
         * set up gateway items. */
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        task_ptr->free[0] = 0;

        /* Request blue background immediately so it's active from the
         * first frame of the gateway (cases 0-3).  Without this, the
         * fire/lava BG inherited from Mode_Select persists until the
         * user enters the lobby phase (case 10) or leaderboard (case 4). */
        Message_Data->kind_req = NET_BG_MODE_BLUE;
    }

    /* Set timer=0 so the dispatcher's WAIT phase completes immediately.
     * The legacy code sets its own timer in case 0. */
    task_ptr->timer = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — delegates to legacy Network_Lobby() body
 *
 *  Called every frame in ACTIVE phase.  The legacy function drives its
 *  own internal phase machine via r_no[2].  We detect exit (r_no[1]
 *  changed from 21) and hand off via MenuScreen_ExitToLegacy.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void network_lobby_tick(struct _TASK* task_ptr) {
    /* Call the legacy function — it operates on the current r_no[2] */
    Network_Lobby(task_ptr);

    /* Room create/join completed — transition to the appropriate lobby screen.
     * The signal is set by rmlui_network_lobby_update() when the async
     * LobbyServer_CreateRoom/JoinRoom thread completes successfully.
     * Tournament-type rooms go to the tournament lobby; casual rooms go
     * to the casual lobby. */
    if (rmlui_network_lobby_has_pending_room()) {
        bool is_tournament = rmlui_network_lobby_pending_room_is_tournament();
        const char* code = rmlui_network_lobby_consume_pending_room();
        if (is_tournament) {
            rmlui_tournament_lobby_set_room(code);
            MenuScreen_Goto(MENU_SCREEN_TOURNAMENT_LOBBY);
        } else {
            rmlui_casual_lobby_set_room(code);
            MenuScreen_Goto(MENU_SCREEN_CASUAL_LOBBY);
        }
        return;
    }

    /* Detect exit: the legacy code sets r_no[1]=1 (Mode_Select) when
     * the user exits.  If r_no[1] changed from 21, hand off to legacy
     * dispatch (which the After_Title hook will intercept and route to
     * the migrated Mode_Select). */
    if (task_ptr->r_no[1] != 21) {
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void network_lobby_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    /* Nothing to reset — the legacy code handles its own cleanup */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void network_lobby_rmlui_show(void) {
    /* RmlUI lobby is shown explicitly in the lobby phase (case 11),
     * not on initial gateway entry.  No-op here. */
}

static void network_lobby_rmlui_hide(void) {
    /* Safety hide on screen exit */
    rmlui_network_lobby_hide();
    rmlui_leaderboard_hide();
    rmlui_network_replay_picker_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_NETWORK_LOBBY]
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_network_lobby_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_network_lobby_reg_ptr)(void) = ms_network_lobby_register;
static void ms_network_lobby_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_network_lobby_register(void) {
#else
void ms_network_lobby_register(void) {
#endif
    g_screens[MENU_SCREEN_NETWORK_LOBBY] = (MenuScreen) {
        .name = "network_lobby",
        .id = MENU_SCREEN_NETWORK_LOBBY,
        .parent = MENU_SCREEN_MODE_SELECT,
        .on_enter = network_lobby_enter,
        .on_tick = network_lobby_tick,
        .on_exit = network_lobby_exit,
        .cursor_max = 15,  /* 16 items in full lobby (0..15) */
        .cancel_item = 15, /* EXIT item */
        .rmlui_show = network_lobby_rmlui_show,
        .rmlui_hide = network_lobby_rmlui_hide,
        .header_type = MENU_HEADER_NETWORK,
        .effect_slot = 0x70,
    };
}
