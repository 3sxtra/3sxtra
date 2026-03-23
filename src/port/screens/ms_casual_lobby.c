/**
 * @file ms_casual_lobby.c
 * @brief Migrated Casual Lobby (Room) screen.
 *
 * Promotes the casual lobby from an RmlUi overlay layered on top of
 * Network_Lobby case 14 to a proper MenuScreen with lifecycle callbacks.
 * This ensures clean separation of native rendering and input handling
 * between the room view and the network lobby.
 *
 * The screen is purely a lifecycle wrapper — all SSE polling, server sync,
 * input navigation, chat, and match proposal handling remain in
 * rmlui_casual_lobby_update() (called from sdl_app.c every frame).
 * The on_tick callback only checks for a leave-room signal to trigger
 * the appropriate MenuScreen transition.
 *
 * Transitions:
 *   IN:  From ms_network_lobby.c when a room create/join completes.
 *   IN:  From netplay.c EXITING handler when re-entering a room after match.
 *   OUT: MenuScreen_Goto(MENU_SCREEN_NETWORK_LOBBY) when user clicks "Leave Room".
 */

#include "port/menu_screen.h"
#include "port/menu_task.h"

#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include "structs.h"

#include <SDL3/SDL_log.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — show the casual lobby RmlUi document
 *
 *  Sets timer=0 to skip the dispatcher's WAIT/FADE_IN phases (the room
 *  screen has no native effects or fade transitions — it's pure RmlUi).
 * ═══════════════════════════════════════════════════════════════════════════ */

static void casual_lobby_enter(struct _TASK* task_ptr) {
    task_ptr->timer = 0; /* bypass WAIT/FADE_IN — go straight to ACTIVE */

    /* Show the casual lobby RmlUi overlay.  This starts the SSE
     * connection and fetches the initial room state from the server. */
    rmlui_casual_lobby_show();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — check for leave-room signal
 *
 *  The bulk of room logic (SSE events, input, chat, match proposals) is
 *  driven by rmlui_casual_lobby_update() from sdl_app.c.  This callback
 *  only detects the "Leave Room" request and transitions back to the
 *  network lobby via the MenuScreen dispatcher.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void casual_lobby_tick(struct _TASK* task_ptr) {
    (void)task_ptr;

    bool wl = rmlui_casual_lobby_wants_leave();
    if (wl) {
        rmlui_casual_lobby_consume_leave();
        extern bool g_lobby_reenter_from_match;
        g_lobby_reenter_from_match = true;
        MenuScreen_Goto(MENU_SCREEN_NETWORK_LOBBY);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — hide the casual lobby
 * ═══════════════════════════════════════════════════════════════════════════ */

static void casual_lobby_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    /* rmlui_hide callback below handles the actual hide */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void casual_lobby_rmlui_show(void) {
    /* Shown explicitly from on_enter with proper room setup — no-op here */
}

static void casual_lobby_rmlui_hide(void) {
    rmlui_casual_lobby_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_CASUAL_LOBBY]
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_casual_lobby_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_casual_lobby_reg_ptr)(void) = ms_casual_lobby_register;
static void ms_casual_lobby_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_casual_lobby_register(void) {
#else
void ms_casual_lobby_register(void) {
#endif
    g_screens[MENU_SCREEN_CASUAL_LOBBY] = (MenuScreen) {
        .name = "casual_lobby",
        .id = MENU_SCREEN_CASUAL_LOBBY,
        .parent = MENU_SCREEN_NETWORK_LOBBY,
        .on_enter = casual_lobby_enter,
        .on_tick = casual_lobby_tick,
        .on_exit = casual_lobby_exit,
        .cursor_max = 0,   /* input handled internally by rmlui_casual_lobby_update */
        .cancel_item = -1,
        .rmlui_show = casual_lobby_rmlui_show,
        .rmlui_hide = casual_lobby_rmlui_hide,
        .header_type = MENU_HEADER_MODE_MENU,
        .effect_slot = 0,
    };
    SDL_Log("[ms_casual_lobby] registered screen id=%d on_tick=%p",
            MENU_SCREEN_CASUAL_LOBBY, (void*)(uintptr_t)casual_lobby_tick);
}
