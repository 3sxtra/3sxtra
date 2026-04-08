/**
 * @file ms_tournament_lobby.c
 * @brief Migrated Tournament Lobby (Room) screen.
 *
 * Lifecycle wrapper for the tournament lobby — identical pattern to
 * ms_casual_lobby.c. All SSE polling, bracket display, match selector,
 * chat, and TO controls are handled by rmlui_tournament_lobby_update()
 * (called from sdl_app.c every frame).
 *
 * Transitions:
 *   IN:  From ms_network_lobby.c when joining a tournament-type room.
 *   OUT: MenuScreen_Goto(MENU_SCREEN_NETWORK_LOBBY) when user clicks "Leave".
 */

#include "port/menu_screen.h"
#include "port/menu_task.h"

#include "port/sdl/rmlui/rmlui_tournament_lobby.h"
#include "structs.h"

#include <SDL3/SDL_log.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — show the tournament lobby RmlUi document
 * ═══════════════════════════════════════════════════════════════════════════ */

static void tournament_lobby_enter(struct _TASK* task_ptr) {
    task_ptr->timer = 0; /* bypass WAIT/FADE_IN — go straight to ACTIVE */
    rmlui_tournament_lobby_show();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — check for leave-room signal
 * ═══════════════════════════════════════════════════════════════════════════ */

static void tournament_lobby_tick(struct _TASK* task_ptr) {
    (void)task_ptr;

    bool wl = rmlui_tournament_lobby_wants_leave();
    if (wl) {
        rmlui_tournament_lobby_consume_leave();
        extern bool g_lobby_reenter_from_match;
        g_lobby_reenter_from_match = true;
        MenuScreen_Goto(MENU_SCREEN_NETWORK_LOBBY);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — hide the tournament lobby
 * ═══════════════════════════════════════════════════════════════════════════ */

static void tournament_lobby_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void tournament_lobby_rmlui_show(void) {
    /* Shown explicitly from on_enter — no-op here */
}

static void tournament_lobby_rmlui_hide(void) {
    rmlui_tournament_lobby_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_TOURNAMENT_LOBBY]
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_tournament_lobby_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_tournament_lobby_reg_ptr)(void) = ms_tournament_lobby_register;
static void ms_tournament_lobby_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_tournament_lobby_register(void) {
#else
void ms_tournament_lobby_register(void) {
#endif
    g_screens[MENU_SCREEN_TOURNAMENT_LOBBY] = (MenuScreen) {
        .name = "tournament_lobby",
        .id = MENU_SCREEN_TOURNAMENT_LOBBY,
        .parent = MENU_SCREEN_NETWORK_LOBBY,
        .on_enter = tournament_lobby_enter,
        .on_tick = tournament_lobby_tick,
        .on_exit = tournament_lobby_exit,
        .cursor_max = 0, /* input handled internally by rmlui_tournament_lobby_update */
        .cancel_item = -1,
        .rmlui_show = tournament_lobby_rmlui_show,
        .rmlui_hide = tournament_lobby_rmlui_hide,
        .header_type = MENU_HEADER_MODE_MENU,
        .effect_slot = 0,
    };
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                 "[ms_tournament_lobby] registered screen id=%d on_tick=%p",
                 MENU_SCREEN_TOURNAMENT_LOBBY,
                 (void*)(uintptr_t)tournament_lobby_tick);
}
