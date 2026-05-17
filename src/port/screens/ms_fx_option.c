/**
 * @file ms_fx_option.c
 * @brief FX Option sub-screen.
 *
 * Exposes the RmlUi FX Option menu as a proper MenuScreen state, ensuring
 * that the background option menu sprites are cleared and that the screen
 * transitions cleanly using CPS3 fade effects.
 */

#include "port/menu_screen.h"
#include "game_state.h"

#include "port/input.h"                             /* SWK_UP, etc. */
#include "sf33rd/Source/Game/effect/effect_57_header_for_menus.h" /* effect_57_init */
#include "sf33rd/Source/Game/engine/state_user.h"                 /* g_state.Menu_Cursor_Y */
#include "sf33rd/Source/Game/menu/menu.h"                         /* Menu_Common_Init */
#include "sf33rd/Source/Game/sound/sound3rd.h"                    /* SE_selected */
#include "sf33rd/Source/Game/system/reset.h"                      /* g_state.Menu_Suicide */
#include "sf33rd/Source/Game/ui/hud_subroutines.h"                /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                                              /* struct _TASK */

#include "port/sdl/input/sdl_pad.h" /* SDLPad_GetButtonState */
#include "port/sdl/rmlui/rmlui_fx_option.h"

static bool s_shoulder_l_prev = false;
static bool s_shoulder_r_prev = false;

static void fx_option_enter(struct _TASK* task_ptr) {
    /* Bypass registry WAIT/FADE_IN phases — go straight to ACTIVE */
    task_ptr->timer = 0;

    /* Kill the native CPS3 sprites from the Option Menu */
    g_state.Menu_Suicide[1] = 1;
    g_state.Menu_Suicide[2] = 0;

    /* Show the RmlUi document immediately */
    rmlui_fx_option_show();

    /* Spawn the native "OPTION MENU" sprite header natively since fx_option
     * relies on the legacy sprite rather than an HTML title. */
    g_state.Order[0x4F] = 1;
    g_state.Order_Dir[0x4F] = 8;
    g_state.Order_Timer[0x4F] = 1;
    effect_57_init(0x4F, MENU_HEADER_OPTION_MENU, 0, 0x3F, 2);
}

static void fx_option_tick(struct _TASK* task_ptr) {
    /* ── FX Option Input Loop ── */
    u16 sw = Check_Menu_Lever(0, 0);
    if (sw == 0)
        sw = Check_Menu_Lever(1, 0);

    switch (sw) {
    case SWK_UP:
        rmlui_fx_option_cursor_up();
        SE_dir_cursor_move();
        break;
    case SWK_DOWN:
        rmlui_fx_option_cursor_down();
        SE_dir_cursor_move();
        break;
    case SWK_LEFT:
        rmlui_fx_option_value_left();
        SE_dir_cursor_move();
        break;
    case SWK_RIGHT:
        rmlui_fx_option_value_right();
        SE_dir_cursor_move();
        break;
    default:
        break;
    }

    /* L/R shoulder for page change — edge detection */
    SDLPad_ButtonState pad;
    SDLPad_GetButtonState(0, &pad);
    if (pad.left_shoulder && !s_shoulder_l_prev) {
        rmlui_fx_option_page_left();
        SE_dir_cursor_move();
    }
    if (pad.right_shoulder && !s_shoulder_r_prev) {
        rmlui_fx_option_page_right();
        SE_dir_cursor_move();
    }
    s_shoulder_l_prev = pad.left_shoulder;
    s_shoulder_r_prev = pad.right_shoulder;

    /* Cancel → return to option menu */
    if (sw & SWK_EAST) {
        SE_selected();
        rmlui_fx_option_hide();

        /* Go back to Option Select */
        MenuScreen_Goto(MENU_SCREEN_OPTION_SELECT);
    }
}

static void fx_option_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_shoulder_l_prev = false;
    s_shoulder_r_prev = false;
    rmlui_fx_option_hide();

    /* Kill the native header when leaving FX Options */
    g_state.Order[0x4F] = 4;
    g_state.Order_Timer[0x4F] = 4;
}

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_fx_option_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_fx_option_reg_ptr)(void) = ms_fx_option_register;
static void ms_fx_option_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_fx_option_register(void) {
#else
void ms_fx_option_register(void) {
#endif
    g_screens[MENU_SCREEN_FX_OPTION] = (MenuScreen) {
        .name = "fx_option",
        .id = MENU_SCREEN_FX_OPTION,
        .parent = MENU_SCREEN_OPTION_SELECT,
        .on_enter = fx_option_enter,
        .on_tick = fx_option_tick,
        .on_exit = fx_option_exit,
        .cursor_max = 0,
        .cancel_item = -1,
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_OPTION_MENU,
        .effect_slot = 0x4F,
    };
}
