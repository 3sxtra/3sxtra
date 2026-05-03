/**
 * @file ms_demo.c
 * @brief MenuScreen registry integration for the attract-mode demo.
 *
 * Implements the on_enter, on_tick, and on_exit callbacks for
 * MENU_SCREEN_DEMO.
 *
 * Two demo flows exist, selected by g_state.demo_phase[0] at the time Play_Demo()
 * is called:
 *   g_state.demo_phase[0]==0  →  Demo00 path (quick start: gameplay runs until timeout)
 *   g_state.demo_phase[0]==1  →  Demo01 path (full attract: char select then gameplay)
 *
 * The callbacks delegate to the existing Demo00() / Demo01() functions
 * which manage their own phase state via g_state.demo_phase[]. The MenuScreen layer
 * only handles the lifecycle gate (enter→active→exit).
 *
 * The public helpers (Setup_Demo_PL, Setup_Demo_Arts, Setup_Demo_Stage)
 * remain in demo02.c — they are called externally by ranking.c.
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/engine/state_user.h"

/* Forward declarations for the legacy demo functions (defined in demo02.c) */
extern void Demo_QuickStart(void);
extern void Demo_FullAttract(void);

static void ms_demo_enter(struct _TASK* tp) {
    tp->timer = 0; /* Skip the WAIT/FADE_IN phases — demos manage their own screen transitions */
}

static void ms_demo_tick(struct _TASK* tp) {
    /*
     * Delegate to the legacy demo functions. They manage their own g_state.demo_phase[]
     * state and set g_state.Next_Demo = 1 when complete.
     */
    if (g_state.demo_phase[0] == 1) {
        Demo_FullAttract();
    } else {
        Demo_QuickStart();
    }

    /* When the legacy code signals completion, request exit */
    if (g_state.Next_Demo == 1) {
        MenuScreen_RequestFadeOut();
        /* Clear the flag so the Play_Demo wrapper doesn't return 1 instantly
         * before the fadeout completes. The wrapper will reset it to 1
         * in MENU_PHASE_EXIT. */
        g_state.Next_Demo = 0;
    }
}

static void ms_demo_exit(struct _TASK* tp) {
    /* g_state.Next_Demo was already set by the legacy demo functions */
}

__attribute__((constructor)) static void register_ms_demo(void) {
    extern MenuScreen g_screens[];

    g_screens[MENU_SCREEN_DEMO] = (MenuScreen) { .name = "demo",
                                                 .id = MENU_SCREEN_DEMO,
                                                 .parent = MENU_SCREEN_NONE,
                                                 .on_enter = ms_demo_enter,
                                                 .on_tick = ms_demo_tick,
                                                 .on_exit = ms_demo_exit,
                                                 .cursor_max = 0,
                                                 .cancel_item = -1,
                                                 .rmlui_show = NULL,
                                                 .rmlui_hide = NULL,
                                                 .header_type = (MenuHeader)-1,
                                                 .effect_slot = 0 };
}
