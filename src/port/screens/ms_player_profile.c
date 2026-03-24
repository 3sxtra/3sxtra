/**
 * @file ms_player_profile.c
 * @brief Player Profile screen.
 *
 * Full-screen RmlUI overlay that displays the user's profile statistics,
 * rating, rank, and recent match history.
 * Easily accessible from the Network Lobby Gateway.
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/effect/effect.h" /* effect_work_init */
#include "sf33rd/Source/Game/engine/workuser.h" /* plsw_00, plsw_01, Order */
#include "sf33rd/Source/Game/menu/menu.h"       /* Menu_Common_Init */
#include "sf33rd/Source/Game/sound/sound3rd.h"  /* SE_selected */
#include "sf33rd/Source/Game/ui/sc_sub.h"       /* FadeOut */
#include "structs.h"                            /* struct _TASK */

/* RmlUi player profile bindings */
#include "port/sdl/rmlui/rmlui_player_profile.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter
 * ═══════════════════════════════════════════════════════════════════════════ */

static void profile_enter(struct _TASK* task_ptr) {
    /* Setup blue background header similar to leaderboard */
    effect_work_init();
    Menu_Common_Init();
    
    Order[0x4E] = 5;
    
    /* Blue background banner */
    effect_57_init(0x4E, MENU_HEADER_MODE_MENU, 0, 0x45, 0);

    /* Show RmlUI player profile sheet (auto-fetches profile) */
    rmlui_player_profile_show();

    /* Set r_no[1]=21 for legacy compatibility (Network_Lobby index) */
    task_ptr->r_no[1] = 21;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick
 * ═══════════════════════════════════════════════════════════════════════════ */

static void profile_tick(struct _TASK* task_ptr) {
    /* Read edge-triggered input from both players */
    u16 trigger = 0;
    for (int i = 0; i < 2; i++) {
        trigger |= (~plsw_01[i] & plsw_00[i]);
    }

    if (trigger & 0x0200) { /* Cancel / B */
        SE_selected();
        rmlui_player_profile_hide();

        /* Return to gateway: r_no[2]=0 causes Network_Lobby to re-init */
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        task_ptr->free[0] = 0;

        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void profile_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void profile_rmlui_show(void) {
    /* Explicitly opened in on_enter */
}

static void profile_rmlui_hide(void) {
    rmlui_player_profile_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_player_profile_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_profile_reg_ptr)(void) = ms_player_profile_register;
static void ms_player_profile_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_player_profile_register(void) {
#else
void ms_player_profile_register(void) {
#endif
    g_screens[MENU_SCREEN_PLAYER_PROFILE] = (MenuScreen) {
        .name = "player_profile",
        .id = MENU_SCREEN_PLAYER_PROFILE,
        .parent = MENU_SCREEN_NETWORK_LOBBY,
        .on_enter = profile_enter,
        .on_tick = profile_tick,
        .on_exit = profile_exit,
        .cursor_max = 1,
        .cancel_item = -1,
        .rmlui_show = profile_rmlui_show,
        .rmlui_hide = profile_rmlui_hide,
        .header_type = MENU_HEADER_MODE_MENU, /* blue BG banner */
        .effect_slot = 0x4E,
    };
}
