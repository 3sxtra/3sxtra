/**
 * @file ms_leaderboard.c
 * @brief Migrated Leaderboard screen — Task 14.
 *
 * Refactored from Network_Lobby phases 4–8 in menu.c.  The leaderboard is a
 * full-screen RmlUI overlay that shows top players from the lobby server.
 *
 * The screen has 5 internal phases (mapped from legacy r_no[2] cases):
 *   Phase 0 (LB_PHASE_INIT):    Fade out, kill gateway items, request blue BG
 *   Phase 1 (LB_PHASE_REBUILD): Destroy old effects, show blue BG + RmlUI
 *   Phase 2 (LB_PHASE_TIMER):   Wait for timer (overlaps with rebuild)
 *   Phase 3 (LB_PHASE_FADE_IN): FadeIn transition
 *   Phase 4 (LB_PHASE_ACTIVE):  Input loop — B/Cancel exits back to gateway
 *
 * Exit path:
 *   - Cancel (B button) → hides RmlUI leaderboard, kills blue BG items,
 *     returns to Network_Lobby gateway (r_no[2]=0) via ExitToLegacy.
 *
 * The leaderboard is not a top-level AT_Jmp_Tbl entry — it's a sub-phase of
 * Network_Lobby.  It will be activated when Network_Lobby is fully migrated
 * (Task 17).  Until then, MENU_USE_NEW_LEADERBOARD remains 0 and the legacy
 * Network_Lobby cases 4–8 handle the leaderboard.
 *
 * Legacy location: menu.c lines 738–804 (Network_Lobby cases 4–8).
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 3).
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/effect/eff45.h"           /* effect_45_init, Message_Data */
#include "sf33rd/Source/Game/effect/eff57.h"           /* effect_57_init, MenuHeader */
#include "sf33rd/Source/Game/effect/effect.h"          /* effect_work_init */
#include "sf33rd/Source/Game/engine/workuser.h"        /* Menu_Cursor_Y, Menu_Suicide, Order, Order_Dir, Order_Timer, plsw_00, plsw_01 */
#include "sf33rd/Source/Game/menu/menu.h"              /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h"     /* AT_JMP_COUNT */
#include "sf33rd/Source/Game/sound/sound3rd.h"         /* SE_selected */
#include "sf33rd/Source/Game/ui/sc_sub.h"              /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                                   /* struct _TASK */

/* RmlUi leaderboard */
#include "port/sdl/rmlui/rmlui_leaderboard.h"         /* rmlui_leaderboard_show/hide */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 *
 *  We track the sub-phase with an enum rather than reusing r_no[2] directly
 *  because the dispatcher owns the outer lifecycle (ENTER/WAIT/FADE_IN/ACTIVE).
 *  The on_enter callback handles the init/rebuild phase, and on_tick drives
 *  a custom internal state machine for the input loop.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    LB_PHASE_INIT,        /* case 4: Fade out, kill gateway items, blue BG */
    LB_PHASE_REBUILD,     /* case 5: effect_work_init, Menu_Common_Init, banner */
    LB_PHASE_TIMER,       /* case 6: wait for timer */
    LB_PHASE_FADE_IN,     /* case 7: FadeIn */
    LB_PHASE_ACTIVE,      /* case 8: input loop — B/Cancel exits */
} LeaderboardPhase;

static LeaderboardPhase s_phase = LB_PHASE_INIT;

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from Network_Lobby cases 4–6
 *
 *  Performs the full init sequence:
 *    Phase 4 (INIT): FadeOut, r_no[3]=0, timer=5, kill gateway items,
 *                    request blue BG via Message_Data->kind_req=4.
 *    Phase 5 (REBUILD): effect_work_init, Menu_Common_Init, clear cursors,
 *                       setup Order[0x4E] blue BG, effect_57 header banner.
 *    RmlUI leaderboard is shown during rebuild (auto-fetches page 0).
 * ═══════════════════════════════════════════════════════════════════════════ */

static void leaderboard_enter(struct _TASK* task_ptr) {
    s_phase = LB_PHASE_INIT;

    /* ── Case 4: Fade out, kill gateway items, request blue BG ── */
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 5;  /* advance past init */
    task_ptr->r_no[3] = 0;
    task_ptr->timer = 5;
    Menu_Suicide[0] = 1;    /* kill gateway items (master_player=0) */
    Menu_Suicide[1] = 0;
    Message_Data->kind_req = 4;  /* blue-BG background mode */

    /* ── Case 5: Destroy old effects, rebuild with blue BG ── */
    FadeOut(1, 0xFF, 8);

    effect_work_init();
    Menu_Common_Init();
    Menu_Cursor_Y[0] = 0;
    Menu_Cursor_Y[1] = 0;
    Order[0x4E] = 5;
    Order_Timer[0x4E] = 1;
    Order_Dir[0x4E] = 1;

    /* Blue background banner */
    effect_57_init(0x4E, MENU_HEADER_MODE_MENU, 0, 0x45, 0);

    /* Show RmlUI leaderboard (auto-fetches page 0) */
    rmlui_leaderboard_show();

    /* Set r_no[1]=21 for legacy compatibility (Network_Lobby index) */
    task_ptr->r_no[1] = 21;

    /* The dispatcher will handle WAIT (timer) and FADE_IN phases for us */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — extracted from Network_Lobby case 8
 *
 *  Input loop: reads edge-triggered cancel (B button) from both players.
 *  On cancel: hides RmlUI leaderboard, kills blue BG items, exits back to
 *  Network_Lobby gateway by setting r_no[2]=0 and calling ExitToLegacy.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void leaderboard_tick(struct _TASK* task_ptr) {
    /* Read edge-triggered input from both players */
    u16 trigger = 0;
    for (int i = 0; i < 2; i++) {
        trigger |= (~plsw_01[i] & plsw_00[i]);
    }

    if (trigger & 0x0200) {  /* Cancel / B */
        SE_selected();
        rmlui_leaderboard_hide();

        /* Kill blue BG items and prepare for gateway re-entry */
        Menu_Suicide[0] = 0;
        Menu_Suicide[1] = 1;  /* kill blue BG items */

        /* Return to gateway: r_no[2]=0 causes Network_Lobby to re-init */
        task_ptr->r_no[2] = 0;
        task_ptr->r_no[3] = 0;
        task_ptr->free[0] = 0;

        /* Hand off to legacy Network_Lobby dispatch */
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void leaderboard_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_phase = LB_PHASE_INIT;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks
 *
 *  The leaderboard is shown explicitly during on_enter (rebuild phase).
 *  rmlui_hide calls hide for safety in case of early exit.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void leaderboard_rmlui_show(void) {
    /* Leaderboard is opened in on_enter after rebuild — no action here */
}

static void leaderboard_rmlui_hide(void) {
    rmlui_leaderboard_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_LEADERBOARD]
 *
 *  Uses GCC/MSVC constructor attribute to register at startup.
 *
 *  Parent is MENU_SCREEN_NETWORK_LOBBY.  However, since Network_Lobby is
 *  not yet migrated (Task 17), the exit path uses ExitToLegacy with r_no
 *  set to re-enter the gateway.
 *
 *  Toggle remains 0 until Network_Lobby is migrated and can route here.
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
/* MSVC: use CRT initializer section */
#pragma section(".CRT$XCU", read)
static void ms_leaderboard_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_leaderboard_reg_ptr)(void) = ms_leaderboard_register;
static void ms_leaderboard_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void ms_leaderboard_register(void) {
#else
/* Fallback: must be called manually from init code */
void ms_leaderboard_register(void) {
#endif
    g_screens[MENU_SCREEN_LEADERBOARD] = (MenuScreen){
        .name        = "leaderboard",
        .id          = MENU_SCREEN_LEADERBOARD,
        .parent      = MENU_SCREEN_NETWORK_LOBBY,
        .on_enter    = leaderboard_enter,
        .on_tick     = leaderboard_tick,
        .on_exit     = leaderboard_exit,
        .cursor_max  = 1,    /* no cursor items — managed by RmlUI; min 1 for validation */
        .cancel_item = -1,   /* cancel handled by raw pad read */
        .rmlui_show  = leaderboard_rmlui_show,
        .rmlui_hide  = leaderboard_rmlui_hide,
        .header_type = MENU_HEADER_MODE_MENU,  /* blue BG banner */
        .effect_slot = 0x4E,
    };
}
