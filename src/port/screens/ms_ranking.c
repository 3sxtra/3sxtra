/**
 * @file ms_ranking.c
 * @brief MenuScreen registry integration for the Ranking display.
 *
 * Implements the on_enter, on_tick, and on_exit callbacks for
 * MENU_SCREEN_RANKING.
 *
 * Two ranking flows exist, selected by g_state.demo_phase[0] at the time Ranking() is
 * called:
 *   g_state.demo_phase[0]==0  →  Ranking_Display path (attract-mode ranking)
 *   g_state.demo_phase[0]==1  →  Ranking_ScoreEntry path (post-game ranking)
 *
 * The callbacks delegate to the existing Ranking_ScoreEntry() / Ranking_Display()
 * dispatchers which manage their own phase state via g_state.demo_phase[]. The
 * MenuScreen layer only handles the lifecycle gate (enter→active→exit).
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/screen/ranking.h"
#include "sf33rd/Source/Game/system/work_sys.h"

static void ms_ranking_enter(struct _TASK* tp) {
    tp->timer = 0; /* Skip the WAIT/FADE_IN phases — rankings manage their own fades */
}

static void ms_ranking_tick(struct _TASK* tp) {
    /*
     * Delegate to the legacy dispatchers. They manage their own g_state.demo_phase[]
     * state and set g_state.Ranking_X = 1 when complete.
     */
    if (g_state.demo_phase[0] == 1) {
        Ranking_ScoreEntry();
    } else {
        Ranking_Display();
    }

    /* When the legacy code signals completion, request exit */
    if (g_state.Ranking_X == 1) {
        MenuScreen_RequestFadeOut();
        /* Clear the flag so the Ranking wrapper doesn't return 1 instantly
         * before the fadeout completes. The wrapper will reset it to 1
         * in MENU_PHASE_EXIT. */
        g_state.Ranking_X = 0;
    }
}

static void ms_ranking_exit(struct _TASK* tp) {
    /* g_state.Ranking_X acts as both the internal completion flag (checked in tick)
     * and the external return value (set in the generic wrapper exit or here). */
}

__attribute__((constructor)) static void register_ms_ranking(void) {
    extern MenuScreen g_screens[];

    g_screens[MENU_SCREEN_RANKING] = (MenuScreen) { .name = "ranking",
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
                                                    .effect_slot = 0 };
}
