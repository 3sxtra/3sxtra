/**
 * @file tate00.c
 * Main Background and Stage Animation Controller
 */

#include "sf33rd/Source/Game/stage/stage_animation.h"
#include "game_state.h"
#include "common.h"
#include "port/mods/modded_stage.h"
#include "port/stage_bg_registry.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/stage_subroutines.h"

static void ta0_init00();
static void ta0_init01();
static void ta0_init02();
static void ta0_move();

/** @brief Dispatch the stage-specific background handler via the registry. */
static inline void ta_dispatch(void) {
    const StageBgCallbacks* bg = StageBg_Get((StageBgId)g_state.bg_w.bg_index);
    if (bg)
        bg->on_tick();
}

/** @brief Main entry point for stage background animation. */
void stage_animate() {
    // ⚡ Bolt: static const — avoid rebuilding this table on the stack every frame
    static void (*const jump_tbl[4])() = { ta0_init00, ta0_init01, ta0_init02, ta0_move };

    if (g_state.Game_pause & 0x80) {
        return;
    }

    jump_tbl[g_state.bg_w.bg_routine]();
    Scrn_Renew();
    Irl_Family();
    Irl_Scrn();
}

/** @brief Stage init phase 0 — initialize background layers. */
static void ta0_init00() {
    g_state.bg_w.bg_routine++;
    bg_initialize();
}

/** @brief Stage init phase 1 — initialize Akebono and run stage handler. */
static void ta0_init01() {
    g_state.bg_w.bg_routine++;
    akebono_initialize();
    if (!ModdedStage_IsRenderingDisabled()) {
        ta_dispatch();
    }
}

/** @brief Stage init phase 2 — run the stage-specific handler. */
static void ta0_init02() {
    g_state.bg_w.bg_routine++;
    ta_dispatch();
}

/** @brief Main per-frame stage animation tick. */
static void ta0_move() {
    /* Skip stage-specific animation handlers when all stage rendering is disabled.
     * Scroll state is kept alive via Scrn_Renew/Irl_*. */
    if (!ModdedStage_IsRenderingDisabled()) {
        ta_dispatch();
    }

    if (g_state.bg_w.quake_x_index > 0) {
        g_state.bg_w.quake_x_index--;
    }

    if (g_state.bg_w.quake_y_index > 0) {
        g_state.bg_w.quake_y_index--;
    }
}
