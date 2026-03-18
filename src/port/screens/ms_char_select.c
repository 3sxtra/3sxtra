/**
 * @file ms_char_select.c
 * @brief MenuScreen registry integration for the character select screen.
 *
 * Implements the on_enter, on_tick, and on_exit callbacks for
 * MENU_SCREEN_CHAR_SELECT.
 *
 * The character select screen was originally driven by Select_Player()
 * in sel_pl.c, called synchronously from Game01().  This file provides
 * the MenuScreen lifecycle layer so Select_Player() can delegate to
 * the registry, following the same thin-wrapper pattern used by
 * Continue_Scene() (continue.c) and Play_Demo() (demo02.c).
 *
 * The internal jump tables and state arrays remain untouched inside
 * sel_pl.c — only the top-level dispatcher is wrapped.
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/screen/sel_pl.h"

static void ms_char_select_enter(struct _TASK* tp) {
    tp->timer = 0; /* Skip WAIT/FADE_IN — char select manages its own transitions */
}

static void ms_char_select_tick(struct _TASK* tp) {
    (void)tp;

    /* Delegate to the extracted per-frame body of Select_Player().
     * This runs Sel_PL_Control, Switch_Work, per-player Sel_PL, and
     * the exit/handicap phases.  SEL_PL_X is set to 1 when done.
     *
     * Exit is NOT signalled via MenuScreen_RequestFadeOut() because
     * the char select manages its own fade transition internally
     * (Exit_4th → Exit_7th).  The thin wrapper in Select_Player()
     * detects SEL_PL_X == 1 and calls MenuScreen_ExitToLegacy(). */
    Sel_PL_Control_Frame();
}

static void ms_char_select_exit(struct _TASK* tp) {
    (void)tp;
    /* SEL_PL_X was already set by the legacy Sel_PL_Control_Frame() */
}

__attribute__((constructor)) static void register_ms_char_select(void) {
    extern MenuScreen g_screens[];

    g_screens[MENU_SCREEN_CHAR_SELECT] = (MenuScreen) { .name = "char_select",
                                                        .id = MENU_SCREEN_CHAR_SELECT,
                                                        .parent = MENU_SCREEN_NONE,
                                                        .on_enter = ms_char_select_enter,
                                                        .on_tick = ms_char_select_tick,
                                                        .on_exit = ms_char_select_exit,
                                                        .cursor_max = 0,
                                                        .cancel_item = -1,
                                                        .rmlui_show = NULL, /* Managed internally in sel_pl.c */
                                                        .rmlui_hide = NULL, /* Managed internally in sel_pl.c */
                                                        .header_type = (MenuHeader)-1,
                                                        .effect_slot = 0 };
}
