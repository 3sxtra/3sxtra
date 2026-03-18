/**
 * @file ms_demo.c
 * @brief MenuScreen registry integration for the attract-mode demo.
 *
 * Implements the on_enter, on_tick, and on_exit callbacks for
 * MENU_SCREEN_DEMO.
 *
 * Two demo flows exist, selected by D_No[0] at the time Play_Demo()
 * is called:
 *   D_No[0]==0  →  Demo00 path (quick start: gameplay runs until timeout)
 *   D_No[0]==1  →  Demo01 path (full attract: char select then gameplay)
 *
 * The callbacks delegate to the existing Demo00() / Demo01() functions
 * which manage their own phase state via D_No[]. The MenuScreen layer
 * only handles the lifecycle gate (enter→active→exit).
 *
 * The public helpers (Setup_Demo_PL, Setup_Demo_Arts, Setup_Demo_Stage)
 * remain in demo02.c — they are called externally by ranking.c.
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/engine/workuser.h"

/* Forward declarations for the legacy demo functions (defined in demo02.c) */
extern void Demo00(void);
extern void Demo01(void);

static void ms_demo_enter(struct _TASK* tp) {
    tp->timer = 0; /* Skip the WAIT/FADE_IN phases — demos manage their own screen transitions */
}

static void ms_demo_tick(struct _TASK* tp) {
    /*
     * Delegate to the legacy demo functions. They manage their own D_No[]
     * state and set Next_Demo = 1 when complete.
     */
    if (D_No[0] == 1) {
        Demo01();
    } else {
        Demo00();
    }

    /* When the legacy code signals completion, request exit */
    if (Next_Demo == 1) {
        MenuScreen_RequestFadeOut();
    }
}

static void ms_demo_exit(struct _TASK* tp) {
    /* Next_Demo was already set by the legacy demo functions */
}

__attribute__((constructor)) static void register_ms_demo(void) {
    extern MenuScreen g_screens[];

    g_screens[MENU_SCREEN_DEMO] = (MenuScreen){
        .name = "demo",
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
        .effect_slot = 0
    };
}
