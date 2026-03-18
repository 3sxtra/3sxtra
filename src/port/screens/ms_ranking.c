/**
 * @file ms_ranking.c
 * @brief MenuScreen registry integration for the Ranking display.
 *
 * Implements the on_enter, on_tick, and on_exit callbacks for
 * MENU_SCREEN_RANKING.
 *
 * Two ranking flows exist, selected by D_No[0] at the time Ranking() is
 * called:
 *   D_No[0]==0  →  Ranking_01 path (attract-mode ranking)
 *   D_No[0]==1  →  Ranking_00 path (post-game ranking)
 *
 * The callbacks delegate to the existing Ranking_00() / Ranking_01()
 * dispatchers which manage their own phase state via D_No[]. The
 * MenuScreen layer only handles the lifecycle gate (enter→active→exit).
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/screen/ranking.h"
#include "sf33rd/Source/Game/system/work_sys.h"

static void ms_ranking_enter(struct _TASK* tp) {
    tp->timer = 0; /* Skip the WAIT/FADE_IN phases — rankings manage their own fades */
}

static void ms_ranking_tick(struct _TASK* tp) {
    /*
     * Delegate to the legacy dispatchers. They manage their own D_No[]
     * state and set Ranking_X = 1 when complete.
     */
    if (D_No[0] == 1) {
        Ranking_00();
    } else {
        Ranking_01();
    }

    /* When the legacy code signals completion, request exit */
    if (Ranking_X == 1) {
        MenuScreen_RequestFadeOut();
    }
}

static void ms_ranking_exit(struct _TASK* tp) {
    /* Ranking_X was already set by the legacy dispatchers */
}

__attribute__((constructor)) static void register_ms_ranking(void) {
    extern MenuScreen g_screens[];

    g_screens[MENU_SCREEN_RANKING] = (MenuScreen){
        .name = "ranking",
        .id = MENU_SCREEN_RANKING,
        .parent = MENU_SCREEN_NONE,
        .on_enter = ms_ranking_enter,
        .on_tick = ms_ranking_tick,
        .on_exit = ms_ranking_exit,
        .cursor_max = 0,
        .cancel_item = -1,
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = (MenuHeader)-1,
        .effect_slot = 0
    };
}
