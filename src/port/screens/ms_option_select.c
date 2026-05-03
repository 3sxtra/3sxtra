/**
 * @file ms_option_select.c
 * @brief Migrated Option Select screen — Task 7.
 *
 * Replaces the Option_Select() function in menu.c with MenuScreen registry
 * callbacks.  This is the top-level options menu: Game Option / Button Config /
 * System Direction / Sound Test / Memory Card / Extra Option / Exit.
 *
 * The item count is dynamic: 6 items normally, 7 when Extra Option is unlocked.
 *
 * Legacy location: menu.c lines 2193–2333 (AT_Jmp_Tbl indices 2, 3, 7, 15).
 *
 * IMPORTANT — Alias Trap (§8 Phase 2):
 *   Option_Select has FOUR legacy indices (2, 3, 7, 15) all mapped to
 *   MENU_SCREEN_OPTION_SELECT in g_legacy_to_screen[].  The call sites in
 *   Return_Option_Mode_Sub() set r_no[1]=7 which the After_Title() hook
 *   intercepts and routes here.  No changes to Return_Option_Mode_Sub()
 *   are needed at this stage because the legacy alias mapping handles it.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 2).
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/AcrSDK/common/pad.h"              /* SWK_UP, SWK_DOWN, etc. */
#include "port/ui/native_imgui.h"                  /* NativeUI */
#include "sf33rd/Source/Game/effect/effect_04_projectile_object.h"       /* effect_04_init */
#include "sf33rd/Source/Game/effect/effect_57_header_for_menus.h"       /* effect_57_init, MenuHeader */
#include "sf33rd/Source/Game/effect/effect_61_menu_options.h"       /* effect_61_init */
#include "sf33rd/Source/Game/engine/state_user.h"    /* g_state.Menu_Cursor_Y, save_w, etc. */
#include "sf33rd/Source/Game/io/rumble.h"          /* pulpul_stop */
#include "sf33rd/Source/Game/menu/menu.h"          /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* MC_Move_Sub, Check_Menu_Lever, Exit_Sub */
#include "sf33rd/Source/Game/rendering/texture_group.h" /* checkSelObjFileLoaded */
#include "sf33rd/Source/Game/sound/sound3rd.h"     /* SE_selected */
#include "sf33rd/Source/Game/system/reset.h"       /* g_state.Suicide */
#include "sf33rd/Source/Game/system/system_subroutines.h"     /* Check_Change_Contents, Copy_Check_w */
#include "sf33rd/Source/Game/system/work_sys.h"    /* g_state.X_Adjust_Buff, g_state.Y_Adjust_Buff, save_w */
#include "sf33rd/Source/Game/ui/hud_subroutines.h"          /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                               /* struct _TASK */

#include "port/sdl/input/sdl_pad.h" /* SDLPad_GetButtonState — shoulder buttons */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_fx_option.h"      /* rmlui_fx_option_show/hide/cursor */
#include "port/sdl/rmlui/rmlui_option_menu.h"    /* rmlui_option_menu_show/hide */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_menu_option */
#include "port/sdl/rmlui/rmlui_wrapper.h"        /* rmlui_wrapper_hide_all_game_documents */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 *
 *  s_exiting: true when the user selected a sub-screen item and we're in
 *             the multi-frame Exit_Sub fade-out.
 *  s_exit_target: the AT_Jmp_Tbl index to transition to after fade-out.
 *  s_cancel_exit: true when the user canceled (back to Mode_Select).
 *  s_wait_done: one-time post-wait-phase setup flag.
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool s_exiting = false;
static s16 s_exit_target = 0;
static bool s_cancel_exit = false;
static bool s_wait_done = false;

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from Option_Select case 0
 *
 *  Calls Menu_in_Sub pattern, sets up header bar, menu items (effect_61),
 *  cursor bar (effect_04), and RmlUi option menu.
 *
 *  Dynamic cursor_max: 7 items (0–6) normally, 8 items (0–7) with Extra
 *  Option unlocked.  The FX OPTION item is always present.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void option_select_enter(struct _TASK* task_ptr) {
    s16 ix;
    s16 char_index;

    s_exiting = false;
    s_exit_target = 0;
    s_cancel_exit = false;
    s_wait_done = false;

    /* ── Replicate Menu_in_Sub pattern ── */
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 1; /* advance so Menu_Sub_case1 works in WAIT phase */
    task_ptr->timer = 5;
    Menu_Common_Init();

    /* Hide all Phase 3 game documents when entering a new sub-menu. */
    if (use_rmlui)
        rmlui_wrapper_hide_all_game_documents();

    g_state.Menu_Cursor_Y[0] = g_state.Cursor_Y_Pos[0][1];
    g_state.Menu_Suicide[0] = 1;
    g_state.Menu_Suicide[1] = 0;
    g_state.Order[0x64] = 4;
    g_state.Order_Timer[0x64] = 1;

    /* ── Option_Select case 0 specific setup ── */
    if (!use_rmlui || !rmlui_menu_option) {
        g_state.Order[0x4E] = 2;
        g_state.Order_Dir[0x4E] = 0;
        g_state.Order_Timer[0x4E] = 1;
    }

    if (!use_rmlui || !rmlui_menu_option) {
        effect_57_init(0x4F, MENU_HEADER_OPTION_MENU, 0, 0x3F, 2);
        g_state.Order[0x4F] = 1;
        g_state.Order_Dir[0x4F] = 8;
        g_state.Order_Timer[0x4F] = 1;
    }

    /* ── Dynamic item count based on Extra_Option unlock ── */
    /* FX OPTION is always present (+1 to both paths). */
    if (CurrentSave()->Extra_Option == 0 && CurrentSave()->Unlock_All == 0) {
        /* 7 items — no Extra Option, but FX Option present */
        if (use_rmlui && rmlui_menu_option) {
            rmlui_option_menu_show();
        } else {
            effect_04_init(1, 4, 0, 0x48);
            g_state.Menu_Cursor_Move = 7;
        }
        g_screens[MENU_SCREEN_OPTION_SELECT].cursor_max = 6;
    } else {
        /* 8 items — Extra Option unlocked + FX Option */
        if (use_rmlui && rmlui_menu_option) {
            rmlui_option_menu_show();
        } else {
            effect_04_init(1, 1, 0, 0x48);
            g_state.Menu_Cursor_Move = 8;
        }
        g_screens[MENU_SCREEN_OPTION_SELECT].cursor_max = 7;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — extracted from Option_Select case 3 + default
 *
 *  Input handling:
 *    - Confirm (0x100): on last item or cancel (0x200), go back to Mode_Select
 *      with auto-save check.  On other items, fade-out to sub-screen via
 *      Exit_Sub (cursor_ix=1).
 *    - The "exiting" sub-state uses Exit_Sub for the multi-frame fade, then
 *      hands off to legacy dispatch via MenuScreen_ExitToLegacy().
 *    - The "cancel_exit" sub-state is the back-to-Mode_Select path.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void option_select_tick(struct _TASK* task_ptr) {
    s16 ix;

    /* ── Handle "exiting to sub-screen" sub-state ── */
    if (s_exiting) {
        if (use_rmlui && rmlui_menu_option)
            rmlui_option_menu_hide();
        if (Exit_Sub(task_ptr, 1, s_exit_target) != 0) {
            /* Exit_Sub set r_no[1] = s_exit_target, r_no[2]=0, r_no[3]=0.
             * Exit to legacy so the integration hook picks up the new r_no[1]. */
            MenuScreen_ExitToLegacy(task_ptr);
        }
        return;
    }

    /* ── Handle "cancel / back to Mode_Select" sub-state ── */
    if (s_cancel_exit) {
        /* This path was: set r_no[1]=1, r_no[2]=0, r_no[3]=0, free[0]=0
         * and kill effect slot 0x4F. The integration hook will route
         * r_no[1]=1 to MENU_SCREEN_MODE_SELECT. */
        MenuScreen_ExitToLegacy(task_ptr);
        return;
    }

    /* ── One-time post-wait-phase setup (replaces Option_Select case 1) ── */
    if (!s_wait_done) {
        s_wait_done = true;
        checkSelObjFileLoaded();
        g_state.Suicide[3] = 0;
    }

    /* ── Determine dynamic cursor_max ── */
    /* +1 for FX Option item (always present) */
    if (CurrentSave()->Extra_Option || CurrentSave()->Unlock_All) {
        ix = 1; /* Extra Option unlocked */
    } else {
        ix = 0;
    }

    /* ── Cursor movement ── */
    if (MC_Move_Sub(Check_Menu_Lever(0, 0), 0, ix + 6, 0xFF) == 0) {
        MC_Move_Sub(Check_Menu_Lever(1, 0), 0, ix + 6, 0xFF);
    }

    /* ── Render NativeUI declaratively to match legacy focus ── */
    if (!use_rmlui || !rmlui_menu_option) {
        const int OPTION_UNLOCKED_GRAPHIC_START = 91;
        const int OPTION_LOCKED_GRAPHIC_START = 84;
        NativeUI_SetFocusIndex(g_state.Menu_Cursor_Y[0]);
        NativeUI_Begin(0, 0, UI_DIR_VERTICAL);
        NativeUI_SetGraphicOffset(ix ? OPTION_UNLOCKED_GRAPHIC_START : OPTION_LOCKED_GRAPHIC_START);
        NativeUI_SetMasterPlayer(
            1); // Bind to g_state.Menu_Suicide[1] (which is ALIVE=0) since g_state.Menu_Suicide[0]=1 (DEAD)
        NativeUI_SetLetterType(0x70A7); // Use narrower font for option select

        NativeUI_Button("GAME OPTION");
        NativeUI_Button("BUTTON CONFIG.");
        NativeUI_Button("SYSTEM DIRECTION");
        NativeUI_Button("SOUND");
        NativeUI_Button("SAVE / LOAD");

        if (ix) {
            NativeUI_Button("EXTRA OPTION");
        }

        NativeUI_Button("FX OPTION");
        NativeUI_Button("EXIT");

        NativeUI_End();
    }

    /* ── Input dispatch ── */
    if (g_state.IO_Result == 0x100 || g_state.IO_Result == 0x200) {
        SE_selected();

        /* ── Cancel / last item (EXIT) → back to Mode Select ── */
        if (g_state.Menu_Cursor_Y[0] == ix + 6 || g_state.IO_Result == 0x200) {
            NativeUI_Clear();
            g_state.Menu_Suicide[0] = 0;
            g_state.Menu_Suicide[1] = 1;
            task_ptr->r_no[1] = 1; /* Mode_Select AT index */
            task_ptr->r_no[2] = 0;
            task_ptr->r_no[3] = 0;
            task_ptr->free[0] = 0;
            g_state.Order[0x4F] = 4;
            g_state.Order_Timer[0x4F] = 4;

            if (use_rmlui && rmlui_menu_option)
                rmlui_option_menu_hide();

            /* Auto-save check (replicated from legacy) */
            if (Check_Change_Contents()) {
                if (CurrentSave()->Auto_Save) {
                    task_ptr->r_no[0] = 4; /* Disp_Auto_Save */
                    task_ptr->r_no[1] = 0;
                    g_state.Forbid_Reset = 1;
                    Copy_Check_w();
                    /* Exit to legacy — Disp_Auto_Save will handle it */
                    s_cancel_exit = true;
                    return;
                }
            }

            /* Normal exit to Mode_Select (or already handled auto-save above) */
            s_cancel_exit = true;
            return;
        }

        /* ── FX OPTION item: ix + 5 (the item just before EXIT) ── */
        if (g_state.Menu_Cursor_Y[0] == ix + 5) {
            if (use_rmlui && rmlui_menu_option)
                rmlui_option_menu_hide();

            /* Save the cursor position so we return exactly here */
            g_state.Cursor_Y_Pos[0][1] = g_state.Menu_Cursor_Y[0];
            g_state.Cursor_Y_Pos[1][1] = g_state.Menu_Cursor_Y[1];
            NativeUI_Clear();

            MenuScreen_Goto(MENU_SCREEN_FX_OPTION);
            return;
        }

        /* ── Confirm on a CPS3 sub-screen item → fade-out to sub-screen ── */
        task_ptr->free[0] = 0;

        /* Set up X/Y adjust buffers (replicated from legacy Option_Select case 3) */
        g_state.X_Adjust_Buff[0] = g_state.X_Adjust;
        g_state.X_Adjust_Buff[1] = g_state.X_Adjust;
        g_state.X_Adjust_Buff[2] = g_state.X_Adjust;
        g_state.Y_Adjust_Buff[0] = g_state.Y_Adjust;
        g_state.Y_Adjust_Buff[1] = g_state.Y_Adjust;
        g_state.Y_Adjust_Buff[2] = g_state.Y_Adjust;

        /* Exit_Sub target: AT index = g_state.Menu_Cursor_Y[0] + 9
         * Item 0 → AT 9 (Game_Option)
         * Item 1 → AT 10 (Button_Config)
         * Item 2 → AT 11 (System_Direction from Option)
         * Item 3 → AT 12 (Sound_Test)
         * Item 4 → AT 13 (Memory_Card)
         * Item 5 → AT 14 (Extra_Option) — only when unlocked */
        s_exiting = true;
        s_exit_target = g_state.Menu_Cursor_Y[0] + 9;
        NativeUI_Clear();
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup / hide RmlUi
 * ═══════════════════════════════════════════════════════════════════════════ */

static void option_select_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_exiting = false;
    s_exit_target = 0;
    s_cancel_exit = false;
    s_wait_done = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks (nullable in MenuScreen struct)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void option_select_rmlui_show(void) {
    if (use_rmlui && rmlui_menu_option)
        rmlui_option_menu_show();
}

static void option_select_rmlui_hide(void) {
    if (use_rmlui && rmlui_menu_option)
        rmlui_option_menu_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_OPTION_SELECT]
 *
 *  Uses GCC/MSVC constructor attribute to register at startup.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* g_screens declared above — used by both on_enter and registration */

#if defined(_MSC_VER)
/* MSVC: use CRT initializer section */
#pragma section(".CRT$XCU", read)
static void ms_option_select_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_option_select_reg_ptr)(void) = ms_option_select_register;
static void ms_option_select_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_option_select_register(void) {
#else
/* Fallback: must be called manually from init code */
void ms_option_select_register(void) {
#endif
    g_screens[MENU_SCREEN_OPTION_SELECT] = (MenuScreen) {
        .name = "option_select",
        .id = MENU_SCREEN_OPTION_SELECT,
        .parent = MENU_SCREEN_MODE_SELECT,
        .on_enter = option_select_enter,
        .on_tick = option_select_tick,
        .on_exit = option_select_exit,
        .cursor_max = 7,   /* default: 8 items (0–7) — on_enter may override to 6 */
        .cancel_item = -1, /* dynamic — handled in on_tick */
        .rmlui_show = option_select_rmlui_show,
        .rmlui_hide = option_select_rmlui_hide,
        .header_type = MENU_HEADER_OPTION_MENU,
        .effect_slot = 0x4F,
    };
}
