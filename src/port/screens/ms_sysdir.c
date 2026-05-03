/**
 * @file ms_sysdir.c
 * @brief Migrated System Direction + Direction Menu screens — Task 15.
 *
 * Replaces System_Direction() and Direction_Menu() functions in menu.c
 * with MenuScreen registry callbacks.
 *
 * System Direction (AT indices 5 and 11) has dual entry points:
 *   - From Mode_Select (r_no[1]==5): uses original master_player 1 path,
 *     returns to Mode_Select on exit.
 *   - From Option_Select (r_no[1]==11): uses master_player 2 path,
 *     returns via Return_Option_Mode_Sub.
 *
 * Direction_Menu (AT index 18) is the per-character dipswitch sub-page
 * system that uses g_state.Menu_Page/g_state.Page_Max/Setup_Next_Page() for multi-page
 * navigation.  It handles L/R page cycling, per-item value toggles via
 * Dir_Move_Sub, and exits back *   System_Direction: menu.c lines 2339–2466 (AT_Jmp_Tbl index 5/11)
 *   Direction_Menu:   menu.c lines 2469–2641 (AT_Jmp_Tbl index 18)
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 4).
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "port/ui/native_imgui.h"                      /* NativeUI */
#include "sf33rd/Source/Game/effect/effect_04_projectile_object.h"           /* effect_04_init */
#include "sf33rd/Source/Game/effect/effect_45_debug_game_state.h"           /* Message_Data */
#include "sf33rd/Source/Game/effect/effect_57_header_for_menus.h"           /* effect_57_init, MenuHeader */
#include "sf33rd/Source/Game/effect/effect_61_menu_options.h"           /* effect_61_init */
#include "sf33rd/Source/Game/effect/effect_64_quake.h"           /* effect_64_init */
#include "sf33rd/Source/Game/effect/effect_66_quake_half_object_flash.h"           /* effect_66_init */
#include "sf33rd/Source/Game/engine/state_user.h"        /* g_state.Menu_Cursor_Y, g_state.Convert_Buff, etc. */
#include "sf33rd/Source/Game/menu/director_data.h"          /* Page_Data */
#include "sf33rd/Source/Game/menu/menu.h"              /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h"     /* System_Dir_Move_Sub, Dir_Move_Sub, etc. */
#include "sf33rd/Source/Game/message/en/msgtable_en.h" /* msgSysDirTbl */
#include "sf33rd/Source/Game/sound/sound3rd.h"         /* SE_selected, SE_dir_selected, SE_cursor_move */
#include "sf33rd/Source/Game/system/reset.h"           /* g_state.Suicide */
#include "sf33rd/Source/Game/system/system_subroutines.h"         /* Check_SysDir_Page */
#include "sf33rd/Source/Game/system/system_director.h"          /* system_dir, g_state.Direction_Working */
#include "sf33rd/Source/Game/ui/hud_subroutines.h"              /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                                   /* struct _TASK */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_sysdir.h"         /* rmlui_sysdir_show/hide/enter_subpage/exit_subpage */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_menu_sysdir */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Extern data
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  SYSTEM DIRECTION — Internal state
 *
 *  s_from_option tracks whether we entered from Option_Select (AT 11) vs
 *  Mode_Select (AT 5).  This determines the exit path and some CPS3 init.
 *  s_wait_done is the one-time post-wait-phase setup flag.
 *  s_exiting tracks the multi-frame fade-out exit to Direction_Menu sub-pages.
 *  s_exit_target is the AT index for the exit destination.
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool s_sysdir_from_option = false;
static bool s_sysdir_wait_done = false;
static bool s_sysdir_exiting = false;
static s16 s_sysdir_exit_target = 0;

/* ═══════════════════════════════════════════════════════════════════════════
 *  System Direction — on_enter
 *
 *  Extracted from System_Direction case 0.  Context-aware:
 *  - From Option_Select (AT 11): kill option items, master_player=2 path
 *  - From Mode_Select (AT 5): inherit BG from Mode_Select
 * ═══════════════════════════════════════════════════════════════════════════ */

static void sysdir_enter(struct _TASK* task_ptr) {
    s16 ix;
    s16 char_index;

    s_sysdir_wait_done = false;
    s_sysdir_exiting = false;
    s_sysdir_exit_target = 0;

    /* Determine entry context from r_no[1].
     * AT index 11 = from Option_Select, AT index 5 = from Mode_Select.
     * After the integration hook intercepts, r_no[1] may already be
     * overwritten. We check: if parent screen is currently Option_Select
     * or if r_no[1] was 11 when we entered. The registry hooks stock
     * r_no[1] before Goto, so we can check the raw value. */
    s_sysdir_from_option = (task_ptr->r_no[1] == 11);

    if (s_sysdir_from_option) {
        /* Option_Select context: kill option items, enable sub-menu items */
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] = 1; /* advance for Menu_Sub_case1 in WAIT phase */
        task_ptr->timer = 5;
        Menu_Common_Init();
        g_state.Menu_Cursor_Y[0] = 0;
        g_state.Menu_Suicide[1] = 1; /* kill Option items (master_player=1) */
        g_state.Menu_Suicide[2] = 0; /* enable our items (master_player=2) */
        g_state.Order[0x4F] = 4;
        g_state.Order_Timer[0x4F] = 1;
        g_state.Order[0x4E] = 2;
        g_state.Order_Dir[0x4E] = 2;
        g_state.Order_Timer[0x4E] = 1;
    } else {
        /* Mode_Select context: inherit BG from Mode_Select.
         * Menu_in_Sub does: FadeOut, r_no[2]+=1, timer=5, Menu_Common_Init,
         * cursor restore from g_state.Cursor_Y_Pos, g_state.Menu_Suicide setup. */
        Menu_in_Sub(task_ptr);
        g_state.Order[0x4E] = 2;
        g_state.Order_Dir[0x4E] = 3;
        g_state.Order_Timer[0x4E] = 1;
    }

    g_state.Convert_Buff[3][0][0] = g_state.Direction_Working[1];

    /* Orange/red header — gated when RmlUI active */
    if (!use_rmlui || !rmlui_menu_sysdir) {
        effect_57_init(0x6D, MENU_HEADER_SYSTEM_DIRECTION, 0, 0x3F, 2);
        g_state.Order[0x6D] = 1;
        g_state.Order_Dir[0x6D] = 8;
        g_state.Order_Timer[0x6D] = 1;
    }

    if (use_rmlui && rmlui_menu_sysdir) {
        rmlui_sysdir_show();
    } else {
        effect_04_init(1, 3, 0, 0x48);
        effect_64_init(0x61U, 0, s_sysdir_from_option ? 2 : 1, 0xA, 0, 0x7047, 0xB, 3, 0);
        g_state.Order[0x61] = 1;
        g_state.Order_Dir[0x61] = 4;
        g_state.Order_Timer[0x61] = 0x14;

        g_state.Menu_Cursor_Move = 4;
    }

    g_state.Page_Max = Check_SysDir_Page();

    /* Set r_no[1] for legacy helper compatibility (System_Dir_Move_Sub,
     * Exit_Sub, etc. check r_no[1] to differentiate SysDir vs Extra_Option) */
    task_ptr->r_no[1] = s_sysdir_from_option ? 11 : 5;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  System Direction — on_tick
 *
 *  Extracted from System_Direction case 3 + default (Exit_Sub path).
 *  Handles up/down cursor with LR toggles via System_Dir_Move_Sub,
 *  exit/confirm/cancel logic.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void sysdir_tick(struct _TASK* task_ptr) {
    /* ── One-time post-wait-phase setup ── */
    if (!s_sysdir_wait_done) {
        s_sysdir_wait_done = true;
        g_state.Suicide[3] = 0;
    }

    /* ── Handle multi-frame Exit_Sub exit to Direction_Menu sub-pages ── */
    if (s_sysdir_exiting) {
        /* Exit_Sub handles the multi-frame fade-out → sets r_no[1] to
         * the Direction_Menu AT index (s_exit_target = g_state.Menu_Cursor_Y[0] + 0x11).
         * Let it run until it changes r_no[1]. */
        Exit_Sub(task_ptr, 1, s_sysdir_exit_target);
        if (task_ptr->r_no[1] != (s_sysdir_from_option ? 11 : 5)) {
            /* Exit_Sub finished and changed r_no[1] → hand off to legacy */
            MenuScreen_ExitToLegacy(task_ptr);
        }
        return;
    }

    /* ── Preserve r_no[1] for System_Dir_Move_Sub and legacy helper compat ── */
    task_ptr->r_no[1] = s_sysdir_from_option ? 11 : 5;

    /* ── Input handling — same as legacy System_Direction case 3 ── */
    System_Dir_Move_Sub(0);

    if (g_state.IO_Result == 0) {
        System_Dir_Move_Sub(1);
    }

    /* ── Render NativeUI declaratively to match legacy focus ── */
    if (!use_rmlui || !rmlui_menu_sysdir) {
        const int SYSDIR_GRAPHIC_START_OFFSET = 42; // Base offset to reach string 43 (0x2B "PAGE 1")

        NativeUI_SetFocusIndex(g_state.Menu_Cursor_Y[0]);
        NativeUI_Begin(0, 0, UI_DIR_VERTICAL);
        NativeUI_SetNextIndex(1); // Item 0 is handled natively by effect_64 combo box!
        NativeUI_SetGraphicOffset(SYSDIR_GRAPHIC_START_OFFSET);
        NativeUI_SetMasterPlayer(s_sysdir_from_option ? 2 : 1);

        NativeUI_Button("PAGE 1");
        NativeUI_Button("PAGE 2");
        NativeUI_Button("PAGE 3");
        NativeUI_Button("EXIT");
        NativeUI_End();
    }

    switch (g_state.IO_Result) {
    case 0x100:
        if (g_state.Menu_Cursor_Y[0] == 0) {
            break;
        }
        /* fallthrough — confirm on non-first item behaves like cancel */

    case 0x200:
        SE_selected();
        g_state.Order[0x6D] = 4;
        g_state.Order_Timer[0x6D] = 4;

        if (g_state.Menu_Cursor_Y[0] == 4 || g_state.IO_Result == 0x200) {
            NativeUI_Clear();
            if (s_sysdir_from_option) {
                /* Return to Option_Select */
                if (use_rmlui && rmlui_menu_sysdir)
                    rmlui_sysdir_hide();
                Return_Option_Mode_Sub(task_ptr);
                /* Return_Option_Mode_Sub sets r_no[1]=7 → integration hook
                 * will intercept and route to MENU_SCREEN_OPTION_SELECT */
                MenuScreen_ExitToLegacy(task_ptr);
            } else {
                /* Return to Mode_Select */
                if (use_rmlui && rmlui_menu_sysdir)
                    rmlui_sysdir_hide();
                g_state.Menu_Suicide[0] = 0;
                g_state.Menu_Suicide[1] = 1;
                task_ptr->r_no[1] = 1;
                task_ptr->r_no[2] = 0;
                task_ptr->r_no[3] = 0;
                task_ptr->free[0] = 0;
                MenuScreen_ExitToLegacy(task_ptr);
            }
            break;
        }

        /* Navigate to Direction_Menu sub-page via Exit_Sub */
        s_sysdir_exiting = true;
        s_sysdir_exit_target = g_state.Menu_Cursor_Y[0] + 0x11;
        NativeUI_Clear();
        task_ptr->r_no[2] = 4; /* advance past case 3 for Exit_Sub */
        task_ptr->free[0] = 0;
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  System Direction — on_exit
 * ═══════════════════════════════════════════════════════════════════════════ */

static void sysdir_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_sysdir_wait_done = false;
    s_sysdir_exiting = false;
    s_sysdir_from_option = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  System Direction — RmlUi callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void sysdir_rmlui_show(void) {
    if (use_rmlui && rmlui_menu_sysdir)
        rmlui_sysdir_show();
}

static void sysdir_rmlui_hide(void) {
    if (use_rmlui && rmlui_menu_sysdir)
        rmlui_sysdir_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  DIRECTION MENU — Internal state
 *
 *  Direction_Menu is the per-character dipswitch sub-page view.
 *  It uses g_state.Menu_Page/g_state.Page_Max for multi-page navigation and
 *  Setup_Next_Page() for page transitions.
 *
 *  Internal phases:
 *    DM_PHASE_INIT    — initial setup (case 0)
 *    DM_PHASE_PAGE    — page transition (case 1: Setup_Next_Page)
 *    DM_PHASE_TIMER   — timer countdown after page setup (case 2)
 *    DM_PHASE_FADE_IN — FadeIn (case 3)
 *    DM_PHASE_ACTIVE  — main input loop (case 4)
 *    DM_PHASE_EXIT    — Exit_Sub back to SysDir (default)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum DirMenuPhase {
    DM_PHASE_INIT,
    DM_PHASE_PAGE,
    DM_PHASE_TIMER,
    DM_PHASE_FADE_IN,
    DM_PHASE_ACTIVE,
    DM_PHASE_EXIT
} DirMenuPhase;

static DirMenuPhase s_dm_phase = DM_PHASE_INIT;
static bool s_dm_page_setup_done = false;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Direction Menu — on_enter
 *
 *  Extracted from Direction_Menu case 0.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void dirm_enter(struct _TASK* task_ptr) {
    s_dm_phase = DM_PHASE_INIT;
    s_dm_page_setup_done = false;

    g_state.Menu_Cursor_Y[1] = g_state.Menu_Cursor_Y[0];

    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 1; /* advance for internal page phase */
    task_ptr->timer = 0;   /* 0 to bypass registry WAIT/FADE_IN */
    g_state.Menu_Suicide[1] = 1;
    g_state.Menu_Suicide[2] = 0;
    g_state.Menu_Page = 0;
    g_state.Menu_Page_Buff = g_state.Menu_Page;
    Message_Data->kind_req = 3;

    if (use_rmlui && rmlui_menu_sysdir) {
        rmlui_sysdir_enter_subpage();
        /* Replace the Mode_Select/SysDir backgrounds with the green subpage BG.
           In native, Setup_Next_Page calls effect_work_init() which destroys
           everything, then recreates 0x4E with palette 0x45. Since we skip
           effect_work_init() in RmlUI mode, do the swap explicitly. */
        g_state.Order[0x6D] = 4; /* kill the orange SysDir overlay */
        g_state.Order_Timer[0x6D] = 1;
        g_state.Order[0x4E] = 5;
        g_state.Order_Timer[0x4E] = 1;
        g_state.Order_Dir[0x4E] = 3;
        effect_57_init(0x4E, 0, 0, 0x45, 0); /* green subpage BG */
    }

    /* Set r_no[1] for legacy helper compatibility.
     * Direction_Menu uses AT index 18. Setup_Next_Page checks r_no[1]==0xE
     * (14) for Extra_Option vs SysDir mode. SysDir mode = anything != 0xE. */
    task_ptr->r_no[1] = 18;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Direction Menu — on_tick
 *
 *  The tick callback manages its own internal phase machine because Dir_Menu
 *  has a complex multi-case structure: page transitions, timer waits,
 *  fade-in, active input, and exit.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void dirm_tick(struct _TASK* task_ptr) {
    g_state.Menu_Cursor_Y[1] = g_state.Menu_Cursor_Y[0];

    /* ── Page setup on first tick (or after page change) ── */
    if (!s_dm_page_setup_done) {
        s_dm_page_setup_done = true;

        /* Case 1 logic: setup page data */
        FadeOut(1, 0xFF, 8);
        task_ptr->timer = 5;
        if (!use_rmlui || !rmlui_menu_sysdir) {
            Setup_Next_Page(task_ptr, 0);
        } else {
            /* RmlUi mode: data setup only — no native effects to destroy */
            g_state.Menu_Page_Buff = g_state.Menu_Page;
            Menu_Common_Init();
            g_state.Menu_Cursor_Y[0] = 0;
            g_state.Menu_Max = Page_Data[g_state.Menu_Page];
            system_dir[1].contents[g_state.Menu_Page][g_state.Menu_Max] = 1;
        }

        s_dm_phase = DM_PHASE_TIMER;
        return;
    }

    switch (s_dm_phase) {
    case DM_PHASE_INIT:
        /* Already handled above on first tick */
        s_dm_phase = DM_PHASE_TIMER;
        break;

    case DM_PHASE_PAGE:
        /* Re-entering page setup after L/R page change */
        FadeOut(1, 0xFF, 8);
        if (!use_rmlui || !rmlui_menu_sysdir) {
            Setup_Next_Page(task_ptr, 0);
        } else {
            g_state.Menu_Page_Buff = g_state.Menu_Page;
            Menu_Common_Init();
            g_state.Menu_Cursor_Y[0] = 0;
            g_state.Menu_Max = Page_Data[g_state.Menu_Page];
            system_dir[1].contents[g_state.Menu_Page][g_state.Menu_Max] = 1;
        }
        s_dm_phase = DM_PHASE_TIMER;
        break;

    case DM_PHASE_TIMER:
        /* Case 2: FadeOut + timer countdown */
        FadeOut(1, 0xFF, 8);
        if (--task_ptr->timer == 0) {
            s_dm_phase = DM_PHASE_FADE_IN;
            FadeInit();
        }
        break;

    case DM_PHASE_FADE_IN:
        /* Case 3: FadeIn transition */
        if (FadeIn(1, 0x19, 8) != 0) {
            s_dm_phase = DM_PHASE_ACTIVE;
        }
        break;

    case DM_PHASE_ACTIVE: {
        /* Case 4: main input loop */
        g_state.Pause_ID = 0;
        Dir_Move_Sub(task_ptr, 0);
        if (g_state.IO_Result == 0) {
            g_state.Pause_ID = 1;
            Dir_Move_Sub(task_ptr, 1);
        }

        if (g_state.Menu_Cursor_Y[1] != g_state.Menu_Cursor_Y[0]) {
            SE_cursor_move();
            system_dir[1].contents[g_state.Menu_Page][g_state.Menu_Max] = 1;

            if (g_state.Menu_Cursor_Y[0] < g_state.Menu_Max) {
                Message_Data->order = 1;
                Message_Data->request = g_state.Menu_Page * 0xC + g_state.Menu_Cursor_Y[0] * 2 + 1;
                Message_Data->timer = 2;

                if (msgSysDirTbl[0]->msgNum[g_state.Menu_Page * 0xC + g_state.Menu_Cursor_Y[0] * 2 + 1] == 1) {
                    Message_Data->pos_y = 0x36;
                } else {
                    Message_Data->pos_y = 0x3E;
                }
            } else {
                Message_Data->order = 1;
                Message_Data->request = system_dir[1].contents[g_state.Menu_Page][g_state.Menu_Max] + 0x74;
                Message_Data->timer = 2;
                Message_Data->pos_y = 0x36;
            }
        }

        switch (g_state.IO_Result) {
        case 0x200:
            /* Cancel: exit back to SysDir */
            s_dm_phase = DM_PHASE_EXIT;
            g_state.Menu_Suicide[0] = 0;
            g_state.Menu_Suicide[1] = 0;
            g_state.Menu_Suicide[2] = 1;
            SE_dir_selected();
            if (use_rmlui && rmlui_menu_sysdir)
                rmlui_sysdir_exit_subpage();
            /* Set r_no[2] for Exit_Sub compatibility */
            task_ptr->r_no[2] = 5;
            task_ptr->free[0] = 0;
            break;

        case 0x80:
        case 0x800:
            /* L button: previous page */
            task_ptr->timer = 5;
            if (--g_state.Menu_Page < 0) {
                g_state.Menu_Page = (s8)g_state.Page_Max;
            }
            SE_dir_selected();
            s_dm_phase = DM_PHASE_PAGE;
            s_dm_page_setup_done = false;
            break;

        case 0x40:
        case 0x400:
            /* R button: next page */
            task_ptr->timer = 5;
            if (++g_state.Menu_Page > g_state.Page_Max) {
                g_state.Menu_Page = 0;
            }
            SE_dir_selected();
            s_dm_phase = DM_PHASE_PAGE;
            s_dm_page_setup_done = false;
            break;

        case 0x100:
            /* Confirm */
            if (g_state.Menu_Cursor_Y[0] == g_state.Menu_Max) {
                /* On the page navigation item */
                switch (system_dir[1].contents[g_state.Menu_Page][g_state.Menu_Max]) {
                case 0:
                    /* Prev page */
                    task_ptr->timer = 5;
                    if (--g_state.Menu_Page < 0) {
                        g_state.Menu_Page = (s8)g_state.Page_Max;
                    }
                    s_dm_phase = DM_PHASE_PAGE;
                    s_dm_page_setup_done = false;
                    break;

                case 2:
                    /* Next page */
                    task_ptr->timer = 5;
                    if (++g_state.Menu_Page > g_state.Page_Max) {
                        g_state.Menu_Page = 0;
                    }
                    s_dm_phase = DM_PHASE_PAGE;
                    s_dm_page_setup_done = false;
                    break;

                default:
                    /* Exit (page nav item = "Exit") */
                    s_dm_phase = DM_PHASE_EXIT;
                    g_state.Menu_Suicide[0] = 0;
                    g_state.Menu_Suicide[1] = 0;
                    g_state.Menu_Suicide[2] = 1;
                    task_ptr->r_no[2] = 5;
                    task_ptr->free[0] = 0;
                    break;
                }
                SE_selected();
            }
            break;
        }
        break;
    }

    case DM_PHASE_EXIT:
        /* Default / Exit_Sub: fade out and return to SysDir */
        Exit_Sub(task_ptr, 2, 5);
        /* Exit_Sub will change r_no[1] when complete → hand off to legacy
         * which routes back to System_Direction via the integration hook */
        if (task_ptr->r_no[1] != 18) {
            MenuScreen_ExitToLegacy(task_ptr);
        }
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Direction Menu — on_exit
 * ═══════════════════════════════════════════════════════════════════════════ */

static void dirm_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_dm_phase = DM_PHASE_INIT;
    s_dm_page_setup_done = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Direction Menu — RmlUi callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void dirm_rmlui_show(void) {
    /* Direction_Menu opens subpage view in on_enter directly */
}

static void dirm_rmlui_hide(void) {
    if (use_rmlui && rmlui_menu_sysdir)
        rmlui_sysdir_exit_subpage();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_sysdir_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_sysdir_reg_ptr)(void) = ms_sysdir_register;
static void ms_sysdir_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_sysdir_register(void) {
#else
void ms_sysdir_register(void) {
#endif
    /* System Direction screen */
    g_screens[MENU_SCREEN_SYSTEM_DIRECTION] = (MenuScreen) {
        .name = "system_direction",
        .id = MENU_SCREEN_SYSTEM_DIRECTION,
        .parent = MENU_SCREEN_MODE_SELECT, /* default; from_option uses Option_Select exit */
        .on_enter = sysdir_enter,
        .on_tick = sysdir_tick,
        .on_exit = sysdir_exit,
        .cursor_max = 4,  /* 5 items (0–4): dipswitch toggle + 3 sub-pages + Exit */
        .cancel_item = 4, /* last item = "Exit" or cancel */
        .rmlui_show = sysdir_rmlui_show,
        .rmlui_hide = sysdir_rmlui_hide,
        .header_type = MENU_HEADER_SYSTEM_DIRECTION,
        .effect_slot = 0x6D,
    };

    /* Direction Menu (sub-page navigation) */
    g_screens[MENU_SCREEN_DIRECTION_MENU] = (MenuScreen) {
        .name = "direction_menu",
        .id = MENU_SCREEN_DIRECTION_MENU,
        .parent = MENU_SCREEN_SYSTEM_DIRECTION,
        .on_enter = dirm_enter,
        .on_tick = dirm_tick,
        .on_exit = dirm_exit,
        .cursor_max = 6,   /* varies per page via g_state.Menu_Max; 6 = safe default */
        .cancel_item = -1, /* cancel handled via g_state.IO_Result 0x200 */
        .rmlui_show = dirm_rmlui_show,
        .rmlui_hide = dirm_rmlui_hide,
        .header_type = MENU_HEADER_MODE_MENU, /* subpage uses green BG */
        .effect_slot = 0x4E,
    };
}
