/**
 * @file tate00.c
 * Main Background and Stage Animation Controller
 */

#include "sf33rd/Source/Game/stage/tate00.h"
#include "common.h"
#include "port/modded_stage.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg000.h"
#include "sf33rd/Source/Game/stage/bg010.h"
#include "sf33rd/Source/Game/stage/bg020.h"
#include "sf33rd/Source/Game/stage/bg030.h"
#include "sf33rd/Source/Game/stage/bg040.h"
#include "sf33rd/Source/Game/stage/bg050.h"
#include "sf33rd/Source/Game/stage/bg060.h"
#include "sf33rd/Source/Game/stage/bg070.h"
#include "sf33rd/Source/Game/stage/bg080.h"
#include "sf33rd/Source/Game/stage/bg090.h"
#include "sf33rd/Source/Game/stage/bg100.h"
#include "sf33rd/Source/Game/stage/bg120.h"
#include "sf33rd/Source/Game/stage/bg130.h"
#include "sf33rd/Source/Game/stage/bg140.h"
#include "sf33rd/Source/Game/stage/bg150.h"
#include "sf33rd/Source/Game/stage/bg160.h"
#include "sf33rd/Source/Game/stage/bg180.h"
#include "sf33rd/Source/Game/stage/bg190.h"
#include "sf33rd/Source/Game/stage/bg_sub.h"
#include "sf33rd/Source/Game/stage/bns_bg2.h"
#include "sf33rd/Source/Game/stage/bonus_bg.h"

// ⚡ Bolt: const — place dispatch table in .rodata (read-only memory)
static void (*const ta_move_tbl[22])() = { BG000, BG010, BG020, BG030, BG040,    BG050,    BG060, BG070,
                                           BG080, BG090, BG100, BG010, BG120,    BG130,    BG140, BG150,
                                           BG160, BG180, BG180, BG190, Bonus_bg, Bonus_bg2 };

static void ta0_init00();
static void ta0_init01();
static void ta0_init02();
static void ta0_move();

/** @brief Main entry point for stage background animation. */
void TATE00() {
    // ⚡ Bolt: static const — avoid rebuilding this table on the stack every frame
    static void (*const jump_tbl[4])() = { ta0_init00, ta0_init01, ta0_init02, ta0_move };

    if (Game_pause & 0x80) {
        return;
    }

    jump_tbl[bg_w.bg_routine]();
    Scrn_Renew();
    Irl_Family();
    Irl_Scrn();
}

/** @brief Stage init phase 0 — initialize background layers. */
static void ta0_init00() {
    bg_w.bg_routine++;
    bg_initialize();
}

/** @brief Stage init phase 1 — initialize Akebono and run stage handler. */
static void ta0_init01() {
    bg_w.bg_routine++;
    akebono_initialize();
    /* Skip stage-specific handler when animations are disabled —
     * this prevents stage effects from ever being spawned. */
    if (!ModdedStage_IsAnimationsDisabled() && !ModdedStage_IsRenderingDisabled()) {
        ta_move_tbl[bg_w.bg_index]();
    }
}

/** @brief Stage init phase 2 — run the stage-specific handler. */
static void ta0_init02() {
    bg_w.bg_routine++;
    if (!ModdedStage_IsAnimationsDisabled() && !ModdedStage_IsRenderingDisabled()) {
        ta_move_tbl[bg_w.bg_index]();
    }
}

/** @brief Main per-frame stage animation tick. */
static void ta0_move() {
    /* Skip stage-specific animation handlers when animations are explicitly
     * disabled or when all stage rendering is disabled.  This prevents
     * animated background objects (crowd, fire, birds, etc.) from spawning.
     * Scroll state is kept alive via Scrn_Renew/Irl_*. */
    if (!ModdedStage_IsAnimationsDisabled() && !ModdedStage_IsRenderingDisabled()) {
        ta_move_tbl[bg_w.bg_index]();
    }

    if (bg_w.quake_x_index > 0) {
        bg_w.quake_x_index--;
    }

    if (bg_w.quake_y_index > 0) {
        bg_w.quake_y_index--;
    }
}
