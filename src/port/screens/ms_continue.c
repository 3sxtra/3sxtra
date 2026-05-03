/**
 * @file ms_continue.c
 * @brief MenuScreen registry integration for the Continue screen.
 *
 * Implements the on_enter, on_tick, and on_exit callbacks for
 * MENU_SCREEN_CONTINUE.
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/effect/effect_49_work_user_character_state.h"
#include "sf33rd/Source/Game/effect/effect_58_sound_se_request.h"
#include "sf33rd/Source/Game/effect/effect_76_quake.h"
#include "sf33rd/Source/Game/effect/effect_95_data_table.h"
#include "sf33rd/Source/Game/effect/effect_a9_suicide_handler.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/screen/continue.h"
#include "sf33rd/Source/Game/sound/sound_effects.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_data.h"

#include "port/sdl/rmlui/rmlui_continue.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"

/* The external global that legacy code sets, which we can optionally update */
extern u8 CONTINUE_X;

/* Local helpers used in the legacy code */
static void Setup_Continue_OBJ(void);
static s16 Check_Exit_Continue(void);

static void ms_continue_enter(struct _TASK* tp) {
    g_state.Target_BG_X[3] = g_state.bg_w.bgw[3].wxy[0].disp.pos + 0x1CA;
    g_state.Target_BG_X[1] = g_state.bg_w.bgw[1].wxy[0].disp.pos + 0x1CA;
    g_state.Offset_BG_X[3] = 0;
    g_state.Offset_BG_X[1] = 0;
    g_state.bg_mvxy.a[0].sp = 0xE0000;
    g_state.bg_mvxy.d[0].sp = 0;
    g_state.Next_Step = 0;

    Setup_Continue_OBJ();
    if (!use_rmlui || !rmlui_screen_continue) {
        effect_A9_init(0x37, 0, 0x13, 0);
    }
    BGM_Request(58);
    if (!use_rmlui || !rmlui_screen_continue) {
        spawn_effect_76(0x38, 3, 1);
    }
    effect_58_init(0xC, 1, 3);
    effect_58_init(0xC, 1, 1);
    g_state.Suicide[2] = 1;
    effect_58_init(0x10, 5, 2);

    tp->free[0] = 0; // our phase
    tp->timer = 0;   // wait timer
}

static void ms_continue_tick(struct _TASK* tp) {
    switch (tp->free[0]) {
    case 0:
        /* Wait for g_state.Next_Step */
        if (g_state.Next_Step) {
            tp->free[0] += 1;
            tp->timer = 0x14;
        }
        break;

    case 1:
        /* Wait for timer or g_state.Scene_Cut */
        if (g_state.Scene_Cut) {
            tp->timer = 1;
        }
        if (--tp->timer <= 0) {
            tp->free[0] += 1;
            g_state.Continue_Count_Down[g_state.LOSER] = 0;
        }
        break;

    case 2:
        /* Wait for continue countdown */
        if (g_state.Continue_Count[g_state.LOSER] < 0) {
            tp->free[0] += 1;
        }
        break;

    case 3:
        /* Wait for exit condition */
        if ((tp->timer = Check_Exit_Continue()) != 0) {
            tp->free[0] += 1;
        }
        break;

    case 4:
        /* Delay before exit */
        if (--tp->timer <= 0) {
            MenuScreen_RequestFadeOut(); /* Trigger FadeOut -> EXIT */
        }
        break;
    }
}

static void ms_continue_exit(struct _TASK* tp) {
    CONTINUE_X = 1; /* signal the caller that we're done */
    if (use_rmlui && rmlui_screen_continue) {
        rmlui_continue_hide();
    }
}

/** @brief Spawn all visual effects/objects for the continue screen. */
static void Setup_Continue_OBJ(void) {
    effect_49_init(4);
    effect_49_init(8);

    effect_95_init(4);
    effect_95_init(8);
    effect_95_init(1);
    effect_95_init(2);

    if (use_rmlui && rmlui_screen_continue) {
        rmlui_continue_show();
    } else {
        spawn_effect_76(0x3B, 3, 1);
        spawn_effect_76(0x3C, 3, 1);
        spawn_effect_76(0x3D, 3, 1);
        spawn_effect_76(0x3E, 3, 1);
        spawn_effect_76(0x3F, 3, 1);
    }
}

/** @brief Check whether both fighters have finished their exit animations. */
static s16 Check_Exit_Continue(void) {
    if (((g_state.E_Number[0][0]) == 2) || ((g_state.E_Number[1][0]) == 2)) {
        return 0;
    }
    if (g_state.E_Number[g_state.LOSER ^ 1][0] == 0) {
        return 0x3C;
    }
    if ((g_state.E_Number[g_state.LOSER ^ 1][0] != 3) && (g_state.E_Number[g_state.LOSER ^ 1][0] != 0)) {
        return 0;
    }
    if ((g_state.E_Number[g_state.LOSER][0] != 3) && (g_state.E_Number[g_state.LOSER][0] != 0)) {
        return 0;
    }
    return 1;
}

__attribute__((constructor)) static void register_ms_continue() {
    extern MenuScreen g_screens[];
    g_screens[MENU_SCREEN_CONTINUE] = (MenuScreen) { .name = "continue",
                                                     .id = MENU_SCREEN_CONTINUE,
                                                     .parent = MENU_SCREEN_NONE,
                                                     .on_enter = ms_continue_enter,
                                                     .on_tick = ms_continue_tick,
                                                     .on_exit = ms_continue_exit,
                                                     .cursor_max = 0,
                                                     .cancel_item = -1,
                                                     .rmlui_show = NULL, /* Managed internally via Setup_Continue_OBJ */
                                                     .rmlui_hide = NULL, /* Managed internally in exit */
                                                     .header_type = (MenuHeader)-1,
                                                     .effect_slot = 0 };
}
