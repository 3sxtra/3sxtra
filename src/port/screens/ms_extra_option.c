/**
 * @file ms_extra_option.c
 * @brief Migrated Extra Option screen — Task 15.
 *
 * Replaces the Extra_Option() function in menu.c with MenuScreen registry
 * callbacks.  Extra Option is a 4-page settings menu (g_state.Page_Max=3) that uses
 * the same Dir_Move_Sub / Setup_Next_Page infrastructure as Direction_Menu.
 *
 * Key differences from Direction_Menu:
 *   - Uses r_no[1]=14 (0xE) which triggers Ex_Move_Sub_LR in Dir_Move_Sub
 *     (instead of Dir_Move_Sub_LR for SysDir pages)
 *   - Page data comes from Ex_Page_Data[g_state.Menu_Page] instead of Page_Data[]
 *   - Settings stored in CurrentSave()->extra_option.contents[][]
 *   - Exits via Return_Option_Mode_Sub (always returns to Option_Select)
 *   - Has a "Default" action on Page 0, Item 6 that resets all options
 *
 * Legacy location: menu.c lines 4760–4921 (AT_Jmp_Tbl index 14).
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 4).
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/effect/effect_45_debug_game_state.h"           /* Message_Data */
#include "sf33rd/Source/Game/effect/effect_57_header_for_menus.h"           /* effect_57_init, MenuHeader */
#include "sf33rd/Source/Game/engine/state_user.h"        /* g_state.Menu_Cursor_Y, save_w, etc. */
#include "sf33rd/Source/Game/menu/ex_data.h"           /* Ex_Account_Data, Ex_Page_Data */
#include "sf33rd/Source/Game/menu/menu.h"              /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h"     /* Dir_Move_Sub, Setup_Next_Page, etc. */
#include "sf33rd/Source/Game/message/en/msgtable_en.h" /* msgExtraTbl */
#include "sf33rd/Source/Game/sound/sound3rd.h"         /* SE_selected, SE_dir_selected, SE_cursor_move */
#include "sf33rd/Source/Game/system/pause.h"           /* g_state.Pause_ID */
#include "sf33rd/Source/Game/system/work_sys.h"        /* save_w, g_state.Present_Mode */
#include "sf33rd/Source/Game/ui/hud_subroutines.h"              /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                                   /* struct _TASK */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_extra_option.h"   /* rmlui_extra_option_show/hide */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_menu_extra_option */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Extern data
 * ═══════════════════════════════════════════════════════════════════════════ */

/* g_state.Menu_Page, g_state.Page_Max, g_state.Menu_Page_Buff, g_state.Menu_Max — declared in state_user.h */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 *
 *  Extra_Option uses a phase machine similar to Direction_Menu:
 *    EO_PHASE_INIT    — initial Setup_Next_Page (case 1)
 *    EO_PHASE_TIMER   — timer wait (case 2 — FadeOut + timer countdown)
 *    EO_PHASE_FADE_IN — FadeIn (case 3)
 *    EO_PHASE_ACTIVE  — main input loop (case 4)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum ExOptPhase {
    EO_PHASE_INIT,
    EO_PHASE_PAGE, /* page transition after L/R cycle */
    EO_PHASE_TIMER,
    EO_PHASE_FADE_IN,
    EO_PHASE_ACTIVE
} ExOptPhase;

static ExOptPhase s_eo_phase = EO_PHASE_INIT;
static bool s_eo_page_setup_done = false;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Extra Option — on_enter
 *
 *  Extracted from Extra_Option case 0.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void extra_option_enter(struct _TASK* task_ptr) {
    s_eo_phase = EO_PHASE_INIT;
    s_eo_page_setup_done = false;

    g_state.Menu_Cursor_Y[1] = g_state.Menu_Cursor_Y[0];

    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 1; /* advance for Setup_Next_Page in first tick */
    task_ptr->r_no[3] = 0;
    task_ptr->timer = 0; /* 0 to bypass registry WAIT/FADE_IN */
    g_state.Menu_Suicide[1] = 1;
    g_state.Menu_Suicide[2] = 0;
    g_state.Menu_Page = 0;
    g_state.Page_Max = 3;
    g_state.Menu_Page_Buff = g_state.Menu_Page;
    Message_Data->kind_req = 4;

    if (use_rmlui && rmlui_menu_extra_option)
        rmlui_extra_option_show();

    /* Set r_no[1]=14 (0xE) so Dir_Move_Sub dispatches to Ex_Move_Sub_LR
     * instead of Dir_Move_Sub_LR, and Setup_Next_Page uses Extra_Option
     * branch (mode_type=1). */
    task_ptr->r_no[1] = 14;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Extra Option — on_tick
 *
 *  Internal phase machine managing page transitions and input.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void extra_option_tick(struct _TASK* task_ptr) {
    g_state.Menu_Cursor_Y[1] = g_state.Menu_Cursor_Y[0];

    /* Ensure r_no[1] stays at 0xE for Dir_Move_Sub / Setup_Next_Page compat */
    task_ptr->r_no[1] = 14;

    /* ── Initial page setup on first tick ── */
    if (!s_eo_page_setup_done) {
        s_eo_page_setup_done = true;

        /* Case 1: Setup_Next_Page */
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] = 2; /* set for timer phase compat */
        task_ptr->timer = 5;
        Setup_Next_Page(task_ptr, task_ptr->r_no[3]);

        s_eo_phase = EO_PHASE_TIMER;
        return;
    }

    switch (s_eo_phase) {
    case EO_PHASE_INIT:
        s_eo_phase = EO_PHASE_TIMER;
        break;

    case EO_PHASE_PAGE:
        /* Re-entering page setup after L/R page change */
        FadeOut(1, 0xFF, 8);
        task_ptr->r_no[2] = 2;
        Setup_Next_Page(task_ptr, task_ptr->r_no[3]);
        s_eo_phase = EO_PHASE_TIMER;
        break;

    case EO_PHASE_TIMER:
        /* Case 2: FadeOut + timer countdown */
        FadeOut(1, 0xFF, 8);
        if (--task_ptr->timer == 0) {
            task_ptr->r_no[2] = 3;
            task_ptr->r_no[3] = 1;
            FadeInit();
            s_eo_phase = EO_PHASE_FADE_IN;
        }
        break;

    case EO_PHASE_FADE_IN:
        /* Case 3: FadeIn */
        if (FadeIn(1, 25, 8)) {
            task_ptr->r_no[2] = 4;
            s_eo_phase = EO_PHASE_ACTIVE;
        }
        break;

    case EO_PHASE_ACTIVE: {
        /* Case 4: main input loop */
        g_state.Pause_ID = 0;
        Dir_Move_Sub(task_ptr, 0);

        if (g_state.IO_Result == 0) {
            g_state.Pause_ID = 1;
            Dir_Move_Sub(task_ptr, 1);
        }

        if (g_state.Menu_Cursor_Y[1] != g_state.Menu_Cursor_Y[0]) {
            SE_cursor_move();
            CurrentSave()->extra_option.contents[g_state.Menu_Page][g_state.Menu_Max] = 1;

            if (g_state.Menu_Cursor_Y[0] < g_state.Menu_Max) {
                Message_Data->order = 1;
                Message_Data->request = Ex_Account_Data[g_state.Menu_Page] + g_state.Menu_Cursor_Y[0];
                Message_Data->timer = 2;

                if (msgExtraTbl[0]->msgNum[g_state.Menu_Cursor_Y[0] + (g_state.Menu_Page * 8)] == 1) {
                    Message_Data->pos_y = 54;
                } else {
                    Message_Data->pos_y = 62;
                }
            } else {
                Message_Data->order = 1;
                Message_Data->request = CurrentSave()->extra_option.contents[g_state.Menu_Page][g_state.Menu_Max] + 32;
                Message_Data->timer = 2;
                Message_Data->pos_y = 54;
            }
        }

        switch (g_state.IO_Result) {
        case 0x200:
            /* Cancel: exit to Option_Select */
            if (use_rmlui && rmlui_menu_extra_option)
                rmlui_extra_option_hide();
            Return_Option_Mode_Sub(task_ptr);
            g_state.Order[115] = 4;
            g_state.Order_Timer[115] = 4;
            save_w[4].extra_option = save_w[1].extra_option;
            save_w[5].extra_option = save_w[1].extra_option;
            SE_dir_selected();
            /* Return_Option_Mode_Sub sets r_no[1]=7 → integration hook
             * will intercept and route to MENU_SCREEN_OPTION_SELECT */
            MenuScreen_ExitToLegacy(task_ptr);
            break;

        case 0x80:
        case 0x800:
            /* L button: previous page */
            task_ptr->timer = 5;
            if (--g_state.Menu_Page < 0) {
                g_state.Menu_Page = g_state.Page_Max;
            }
            SE_dir_selected();
            s_eo_phase = EO_PHASE_PAGE;
            s_eo_page_setup_done = false;
            break;

        case 0x40:
        case 0x400:
            /* R button: next page */
            task_ptr->timer = 5;
            if (++g_state.Menu_Page > g_state.Page_Max) {
                g_state.Menu_Page = 0;
            }
            SE_dir_selected();
            s_eo_phase = EO_PHASE_PAGE;
            s_eo_page_setup_done = false;
            break;

        case 0x100:
            /* Confirm */
            /* Special case: Page 0, Item 6 = "Default" — reset all */
            if (g_state.Menu_Page == 0 && g_state.Menu_Cursor_Y[0] == 6) {
                CurrentSave()->extra_option = save_w[0].extra_option;
                SE_selected();
                break;
            }

            if (g_state.Menu_Cursor_Y[0] != g_state.Menu_Max) {
                break;
            }

            /* On the page navigation item */
            switch (CurrentSave()->extra_option.contents[g_state.Menu_Page][g_state.Menu_Max]) {
            case 0:
                /* Prev page */
                task_ptr->timer = 5;
                if (--g_state.Menu_Page < 0) {
                    g_state.Menu_Page = g_state.Page_Max;
                }
                s_eo_phase = EO_PHASE_PAGE;
                s_eo_page_setup_done = false;
                break;

            case 2:
                /* Next page */
                task_ptr->timer = 5;
                if (++g_state.Menu_Page > g_state.Page_Max) {
                    g_state.Menu_Page = 0;
                }
                s_eo_phase = EO_PHASE_PAGE;
                s_eo_page_setup_done = false;
                break;

            default:
                /* Exit */
                if (use_rmlui && rmlui_menu_extra_option)
                    rmlui_extra_option_hide();
                Return_Option_Mode_Sub(task_ptr);
                save_w[4].extra_option = save_w[1].extra_option;
                save_w[5].extra_option = save_w[1].extra_option;
                g_state.Order[115] = 4;
                g_state.Order_Timer[115] = 4;
                MenuScreen_ExitToLegacy(task_ptr);
                break;
            }

            SE_selected();
            break;
        }
        break;
    }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Extra Option — on_exit
 * ═══════════════════════════════════════════════════════════════════════════ */

static void extra_option_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_eo_phase = EO_PHASE_INIT;
    s_eo_page_setup_done = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Extra Option — RmlUi callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void extra_option_rmlui_show(void) {
    if (use_rmlui && rmlui_menu_extra_option)
        rmlui_extra_option_show();
}

static void extra_option_rmlui_hide(void) {
    if (use_rmlui && rmlui_menu_extra_option)
        rmlui_extra_option_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_extra_option_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_extra_option_reg_ptr)(void) = ms_extra_option_register;
static void ms_extra_option_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_extra_option_register(void) {
#else
void ms_extra_option_register(void) {
#endif
    g_screens[MENU_SCREEN_EXTRA_OPTION] = (MenuScreen) {
        .name = "extra_option",
        .id = MENU_SCREEN_EXTRA_OPTION,
        .parent = MENU_SCREEN_OPTION_SELECT,
        .on_enter = extra_option_enter,
        .on_tick = extra_option_tick,
        .on_exit = extra_option_exit,
        .cursor_max = 7,   /* varies per page via g_state.Menu_Max; 7 = safe default (page 0/1 max) */
        .cancel_item = -1, /* cancel handled via g_state.IO_Result 0x200 */
        .rmlui_show = extra_option_rmlui_show,
        .rmlui_hide = extra_option_rmlui_hide,
        .header_type = MENU_HEADER_EXTRA_OPTION,
        .effect_slot = 0x73,
    };
}
