/**
 * @file ms_button_config.c
 * @brief Migrated Button Config screen — Task 12.
 *
 * Replaces the Button_Config() function in menu.c with MenuScreen registry
 * callbacks.  This is the button remapping screen accessed from Option_Select.
 *
 * The screen has 11 items (cursor_max=10):
 *   Items 0–7:  Individual button mappings (LP, MP, HP, LK, MK, HK, ...)
 *   Item 8:     Vibration toggle
 *   Item 9:     "Default" — resets button mappings via Setup_IO_ConvDataDefault
 *   Item 10:    "Exit" — returns to Option_Select
 *
 * Both players can navigate simultaneously. L/R value changes modify
 * g_state.Convert_Buff[1] directly via Button_Config_Sub() / Button_Move_Sub_LR().
 * On exit (confirm or cancel), values are committed via Save_Game_Data().
 * On "Default" (item 9), values are reset via Setup_IO_ConvDataDefault().
 *
 * === Shared Button Config Strategy ===
 * Three screens share button-mapping logic:
 *   - Button_Config       (AT index 10)  — 2-player, from Option_Select
 *   - Button_Config_in_Game (In_Game)    — 1-player, from pause menu (Phase 5b)
 *   - Button_Config_Tr     (Training)   — 1-player, from training menu (Phase 5a)
 *
 * This file implements button_config_common_enter() and button_config_common_tick()
 * as internal shared helpers.  The In_Game and Training variants will be
 * registered as separate MenuScreen entries using thin wrappers that call
 * these common helpers with different player_count parameters.
 *
 * Legacy location: menu.c lines 2784–2875 (AT_Jmp_Tbl index 10).
 * Exit handling: menu_input.c Button_Exit_Check() case 10 (lines 839–857).
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 3).
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "sf33rd/Source/Game/effect/effect_23_quake.h"                   /* effect_23_init */
#include "sf33rd/Source/Game/effect/effect_57_header_for_menus.h"        /* effect_57_init, MenuHeader */
#include "sf33rd/Source/Game/effect/effect_66_quake_half_object_flash.h" /* effect_66_init */
#include "sf33rd/Source/Game/engine/state_user.h"         /* g_state.Menu_Cursor_Y, g_state.Menu_Cursor_Move, etc. */
#include "sf33rd/Source/Game/io/rumble.h"                 /* pp_operator_check_flag */
#include "sf33rd/Source/Game/menu/menu.h"                 /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h"        /* Button_Config_Sub, Button_Exit_Check, etc. */
#include "sf33rd/Source/Game/sound/sound3rd.h"            /* SE_selected */
#include "sf33rd/Source/Game/system/reset.h"              /* g_state.Suicide */
#include "sf33rd/Source/Game/system/system_subroutines.h" /* Save_Game_Data, Copy_Key_Disp_Work */
#include "sf33rd/Source/Game/ui/hud_subroutines.h"        /* FadeOut, FadeIn, FadeInit */
#include "port/sdl/input/controller_image_overlay.h"      /* ControllerImageOverlay_Init/Shutdown */
#include "structs.h"                                      /* struct _TASK */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_button_config.h"  /* rmlui_button_config_show/hide */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_menu_button_config */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 *
 *  s_wait_done: one-time post-wait-phase setup flag.
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool s_wait_done = false;

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from Button_Config case 0
 *
 *  Sets up fade, timer, common init, cursor, pp_operator_check,
 *  ControllerImageOverlay, Copy_Key_Disp_Work, header bar (0x6B,
 *  MENU_HEADER_BUTTON_CONFIG), CPS3 effect_23 button mapping labels
 *  and effect_66 confirmation buttons, and RmlUi button config menu.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void button_config_enter(struct _TASK* task_ptr) {
    s16 ix;
    s16 disp_index;

    s_wait_done = false;

    /* ── Replicate Button_Config case 0 init pattern ── */
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 1; /* advance so Menu_Sub_case1 works in WAIT phase */
    task_ptr->timer = 5;
    Menu_Common_Init();
    pp_operator_check_flag(0);
    ControllerImageOverlay_Init();
    g_state.Menu_Cursor_Y[0] = 0;
    g_state.Menu_Cursor_Y[1] = 0;
    g_state.Menu_Suicide[1] = 1;
    g_state.Menu_Suicide[2] = 0;
    Copy_Key_Disp_Work();

    /* Kill/setup parent effect slots */
    g_state.Order[0x4F] = 4;
    g_state.Order_Timer[0x4F] = 1;
    g_state.Order[0x4E] = 2;
    g_state.Order_Dir[0x4E] = 2;
    g_state.Order_Timer[0x4E] = 1;

    /* Header bar — CPS3 only (skip when RmlUi active) */
    if (!use_rmlui || !rmlui_menu_button_config) {
        effect_57_init(0x6B, MENU_HEADER_BUTTON_CONFIG, 0, 0x3F, 2);
        g_state.Order[0x6B] = 1;
        g_state.Order_Dir[0x6B] = 8;
        g_state.Order_Timer[0x6B] = 1;
    }

    /* Button mapping labels and indicators */
    if (use_rmlui && rmlui_menu_button_config) {
        rmlui_button_config_show();
    } else {
        /* 12 effect_23 button mapping labels — P1 (type=2) and P2 (type=3) */
        for (ix = 0; ix < 12; ix++) {
            effect_23_init(0, ix + 0x50, 0, 2, 2, ix, 0x70A7, ix + 9, 1);
            g_state.Order[ix + 0x50] = 1;
            g_state.Order_Dir[ix + 0x50] = 4;
            g_state.Order_Timer[ix + 0x50] = ix + 0x14;
            effect_23_init(1, ix + 0x5C, 0, 2, 3, ix, 0x70A7, ix + 9, 1);
            g_state.Order[ix + 0x5C] = 1;
            g_state.Order_Dir[ix + 0x5C] = 4;
            g_state.Order_Timer[ix + 0x5C] = ix + 0x14;
        }

        /* 9 effect_23 button labels (rows) — P1 and P2 */
        for (ix = 0; ix < 9; ix++) {
            if (ix == 8) {
                disp_index = 1;
            } else {
                disp_index = 0;
            }

            effect_23_init(0, ix + 0x78, 0, 2, disp_index, ix, 0x70A7, ix, 0);
            g_state.Order[ix + 0x78] = 1;
            g_state.Order_Dir[ix + 0x78] = 4;
            g_state.Order_Timer[ix + 0x78] = ix + 0x14;
            effect_23_init(1, ix + 0x81, 0, 2, disp_index, ix, 0x70A7, ix, 0);
            g_state.Order[ix + 0x81] = 1;
            g_state.Order_Dir[ix + 0x81] = 4;
            g_state.Order_Timer[ix + 0x81] = ix + 0x14;
        }

        g_state.Menu_Cursor_Move = 0x22;

        /* Confirmation button overlays */
        effect_66_init(0x8A, 7, 2, 0, -1, -1, -0x7FFF);
        g_state.Order[0x8A] = 1;
        g_state.Order_Dir[0x8A] = 4;
        g_state.Order_Timer[0x8A] = 0x14;
        effect_66_init(0x8B, 8, 2, 0, -1, -1, -0x7FFF);
        g_state.Order[0x8B] = 1;
        g_state.Order_Dir[0x8B] = 4;
        g_state.Order_Timer[0x8B] = 0x14;
    }

    /* ── Set r_no[1] to 10 for Button_Exit_Check compatibility ──
     * Button_Exit_Check dispatches on task_ptr->r_no[1] == 10.
     * We keep this value so Button_Exit_Check routes correctly while
     * the screen is active. */
    task_ptr->r_no[1] = 10;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — extracted from Button_Config case 3
 *
 *  Input handling: Button_Config_Sub(0/1) for both players handles up/down
 *  cursor movement and L/R value toggles.  Button_Exit_Check(0/1) handles
 *  confirm/cancel/default exit paths.  Save_Game_Data() commits changes.
 *
 *  Button_Exit_Check internally checks r_no[1]==10 for Button_Config and:
 *    - Item 10 or cancel: SE_selected, ControllerImageOverlay_Shutdown,
 *      rmlui_button_config_hide, Return_Option_Mode_Sub, kill slot 0x6B
 *    - Item 9: SE_selected, Setup_IO_ConvDataDefault(PL_id), Save_Game_Data
 *
 *  We detect exit by checking if r_no[1] changed from 10 (meaning
 *  Button_Exit_Check called Return_Option_Mode_Sub which set r_no[1]=7).
 * ═══════════════════════════════════════════════════════════════════════════ */

static void button_config_tick(struct _TASK* task_ptr) {
    /* ── One-time post-wait-phase setup ── */
    if (!s_wait_done) {
        s_wait_done = true;
        g_state.Suicide[3] = 0;
    }

    /* ── Preserve r_no[1]=10 for Button_Exit_Check routing ── */
    task_ptr->r_no[1] = 10;

    /* ── Input handling — same as legacy Button_Config case 3 ── */
    Button_Config_Sub(0);
    Button_Exit_Check(task_ptr, 0);
    Button_Config_Sub(1);
    Button_Exit_Check(task_ptr, 1);
    Save_Game_Data();

    /* ── Check if Button_Exit_Check triggered an exit ──
     * Return_Option_Mode_Sub sets r_no[1]=7.
     * If r_no[1] is no longer 10, we know the exit path fired. */
    if (task_ptr->r_no[1] != 10) {
        /* Button_Exit_Check already set r_no, free[], g_state.Menu_Suicide,
         * hid RmlUi, shut down ControllerImageOverlay, and killed
         * effect 0x6B. Hand off to legacy. */
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void button_config_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_wait_done = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks (nullable in MenuScreen struct)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void button_config_rmlui_show(void) {
    if (use_rmlui && rmlui_menu_button_config)
        rmlui_button_config_show();
}

static void button_config_rmlui_hide(void) {
    if (use_rmlui && rmlui_menu_button_config)
        rmlui_button_config_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_BUTTON_CONFIG]
 *
 *  Uses GCC/MSVC constructor attribute to register at startup.
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
/* MSVC: use CRT initializer section */
#pragma section(".CRT$XCU", read)
static void ms_button_config_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_button_config_reg_ptr)(void) = ms_button_config_register;
static void ms_button_config_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_button_config_register(void) {
#else
/* Fallback: must be called manually from init code */
void ms_button_config_register(void) {
#endif
    g_screens[MENU_SCREEN_BUTTON_CONFIG] = (MenuScreen) {
        .name = "button_config",
        .id = MENU_SCREEN_BUTTON_CONFIG,
        .parent = MENU_SCREEN_OPTION_SELECT,
        .on_enter = button_config_enter,
        .on_tick = button_config_tick,
        .on_exit = button_config_exit,
        .cursor_max = 10,  /* 11 items (0–10) */
        .cancel_item = 10, /* last item = "Exit" */
        .rmlui_show = button_config_rmlui_show,
        .rmlui_hide = button_config_rmlui_hide,
        .header_type = MENU_HEADER_BUTTON_CONFIG,
        .effect_slot = 0x6B,
    };
}
