/**
 * @file ms_game_option.c
 * @brief Migrated Game Option screen — Task 11.
 *
 * Replaces the Game_Option() function in menu.c with MenuScreen registry
 * callbacks.  This is the game options settings screen: Difficulty, Time,
 * Rounds, Damage Level, Guard Check, Analog Stick, Handicap, etc.
 *
 * The screen has 12 items (cursor_max=11):
 *   Items 0–9: L/R toggle items (difficulty, time, rounds, etc.)
 *   Item 10:  "Default" — resets all items to Game_Default_Data
 *   Item 11:  "Exit" — returns to Option_Select
 *
 * L/R value changes are live — they modify g_state.Convert_Buff[0] directly via
 * Game_Option_Sub() / GO_Move_Sub_LR().  On exit (confirm or cancel),
 * values are committed via Save_Game_Data().  On "Default", values are
 * reset to Game_Default_Data and synced via Copy_Save_w().
 *
 * Legacy location: menu.c lines 2706–2781 (AT_Jmp_Tbl index 9).
 * Exit handling: menu_input.c Button_Exit_Check() case 9 (lines 810–837).
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 3).
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/effect/eff04.h"       /* effect_04_init */
#include "sf33rd/Source/Game/effect/eff57.h"       /* effect_57_init, MenuHeader */
#include "sf33rd/Source/Game/effect/eff61.h"       /* effect_61_init */
#include "sf33rd/Source/Game/effect/eff64.h"       /* effect_64_init */
#include "sf33rd/Source/Game/engine/workuser.h"    /* g_state.Menu_Cursor_Y, save_w, etc. */
#include "sf33rd/Source/Game/menu/menu.h"          /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* Game_Option_Sub, Exit_Sub, etc. */
#include "sf33rd/Source/Game/sound/sound3rd.h"     /* SE_selected */
#include "sf33rd/Source/Game/system/reset.h"       /* g_state.Suicide */
#include "sf33rd/Source/Game/system/sys_sub.h"     /* Save_Game_Data, Copy_Save_w, Game_Default_Data */
#include "sf33rd/Source/Game/system/work_sys.h"    /* save_w */
#include "sf33rd/Source/Game/ui/sc_sub.h"          /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                               /* struct _TASK */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_game_option.h"    /* rmlui_game_option_show/hide */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_menu_game_option */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Extern data — Setup_Index_64 defined in menu.c
 * ═══════════════════════════════════════════════════════════════════════════ */

extern const u8 Setup_Index_64[10];

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 *
 *  s_wait_done: one-time post-wait-phase setup flag.
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool s_wait_done = false;

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from Game_Option case 0
 *
 *  Sets up fade, timer, common init, cursor, header bar (0x6A,
 *  MENU_HEADER_GAME_OPTION), effect_61 item labels (12 items),
 *  effect_64 value indicators (10 sliders), and RmlUi game option menu.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void game_option_enter(struct _TASK* task_ptr) {
    s16 char_index;
    s16 ix;
    s16 unused_s3;
    s16 unused_s2;

    s_wait_done = false;

    /* ── Replicate Game_Option case 0 init pattern ── */
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 1; /* advance so Menu_Sub_case1 works in WAIT phase */
    task_ptr->timer = 5;
    Menu_Common_Init();
    g_state.Menu_Cursor_Y[0] = 0;
    g_state.Menu_Suicide[1] = 1;
    g_state.Menu_Suicide[2] = 0;
    g_state.Menu_Cursor_Y[0] = 0;
    g_state.Menu_Cursor_Y[1] = 0;

    /* Kill/setup parent effect slots */
    g_state.Order[0x4F] = 4;
    g_state.Order_Timer[0x4F] = 1;
    g_state.Order[0x4E] = 2;
    g_state.Order_Dir[0x4E] = 2;
    g_state.Order_Timer[0x4E] = 1;

    /* Header bar — CPS3 only (skip when RmlUi active) */
    if (!use_rmlui || !rmlui_menu_game_option) {
        effect_57_init(0x6A, MENU_HEADER_GAME_OPTION, 0, 0x3F, 2);
        g_state.Order[0x6A] = 1;
        g_state.Order_Dir[0x6A] = 8;
        g_state.Order_Timer[0x6A] = 1;
    }

    /* Item labels and value indicators */
    if (use_rmlui && rmlui_menu_game_option) {
        rmlui_game_option_show();
    } else {
        /* 12 effect_61 item labels */
        for (ix = 0, unused_s3 = char_index = 0x19; ix < 0xC; ix++, unused_s2 = char_index++) {
            effect_61_init(0, ix + 0x50, 0, 2, char_index, ix, 0x70A7);
            g_state.Order[ix + 0x50] = 1;
            g_state.Order_Dir[ix + 0x50] = 4;
            g_state.Order_Timer[ix + 0x50] = ix + 0x14;
        }

        /* 10 effect_64 value sliders */
        for (ix = 0; ix < 0xA; ix++) {
            effect_64_init(ix + 0x5D, 0, 2, Setup_Index_64[ix], ix, 0x70A7, ix + 1, 0, 0);
            g_state.Order[ix + 0x5D] = 1;
            g_state.Order_Dir[ix + 0x5D] = 4;
            g_state.Order_Timer[ix + 0x5D] = ix + 0x14;
        }
        g_state.Menu_Cursor_Move = 0xA;
    }

    /* ── Set r_no[1] to 9 for Button_Exit_Check compatibility ──
     * Button_Exit_Check dispatches on task_ptr->r_no[1] == 9.
     * We keep this value so Button_Exit_Check routes correctly while
     * the screen is active. */
    task_ptr->r_no[1] = 9;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — extracted from Game_Option case 3
 *
 *  Input handling: Game_Option_Sub(0/1) for both players handles up/down
 *  cursor movement and L/R value toggles.  Button_Exit_Check(0/1) handles
 *  confirm/cancel/default exit paths.  Save_Game_Data() commits changes.
 *
 *  Button_Exit_Check internally checks r_no[1]==9 for Game_Option and
 *  calls Return_Option_Mode_Sub on exit/cancel, or resets to defaults
 *  on the "Default" item.  After Return_Option_Mode_Sub sets r_no[1]=7,
 *  our next frame intercepts via MenuScreen_ExitToLegacy.
 *
 *  We detect the exit by checking if r_no[1] changed from 9 (meaning
 *  Button_Exit_Check called Return_Option_Mode_Sub which set r_no[1]=7).
 * ═══════════════════════════════════════════════════════════════════════════ */

static void game_option_tick(struct _TASK* task_ptr) {
    /* ── One-time post-wait-phase setup ── */
    if (!s_wait_done) {
        s_wait_done = true;
        g_state.Suicide[3] = 0;
    }

    /* ── Preserve r_no[1]=9 for Button_Exit_Check routing ── */
    task_ptr->r_no[1] = 9;

    /* ── Input handling — same as legacy Game_Option case 3 ── */
    Game_Option_Sub(0);
    Button_Exit_Check(task_ptr, 0);
    Game_Option_Sub(1);
    Button_Exit_Check(task_ptr, 1);
    Save_Game_Data();

    /* ── Check if Button_Exit_Check triggered an exit ──
     * Return_Option_Mode_Sub sets r_no[1]=7.
     * If r_no[1] is no longer 9, we know the exit path fired. */
    if (task_ptr->r_no[1] != 9) {
        /* Button_Exit_Check already set r_no, free[], g_state.Menu_Suicide,
         * hid RmlUi, and killed effect 0x6A. Hand off to legacy. */
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void game_option_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_wait_done = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks (nullable in MenuScreen struct)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void game_option_rmlui_show(void) {
    if (use_rmlui && rmlui_menu_game_option)
        rmlui_game_option_show();
}

static void game_option_rmlui_hide(void) {
    if (use_rmlui && rmlui_menu_game_option)
        rmlui_game_option_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_GAME_OPTION]
 *
 *  Uses GCC/MSVC constructor attribute to register at startup.
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
/* MSVC: use CRT initializer section */
#pragma section(".CRT$XCU", read)
static void ms_game_option_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_game_option_reg_ptr)(void) = ms_game_option_register;
static void ms_game_option_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_game_option_register(void) {
#else
/* Fallback: must be called manually from init code */
void ms_game_option_register(void) {
#endif
    g_screens[MENU_SCREEN_GAME_OPTION] = (MenuScreen) {
        .name = "game_option",
        .id = MENU_SCREEN_GAME_OPTION,
        .parent = MENU_SCREEN_OPTION_SELECT,
        .on_enter = game_option_enter,
        .on_tick = game_option_tick,
        .on_exit = game_option_exit,
        .cursor_max = 11,  /* 12 items (0–11) */
        .cancel_item = 11, /* last item = "Exit" */
        .rmlui_show = game_option_rmlui_show,
        .rmlui_hide = game_option_rmlui_hide,
        .header_type = MENU_HEADER_GAME_OPTION,
        .effect_slot = 0x6A,
    };
}
