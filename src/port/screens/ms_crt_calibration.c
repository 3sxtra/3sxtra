/**
 * @file ms_crt_calibration.c
 * @brief CRT Calibration Screen
 */

#include "port/menu_screen.h"
#include "game_state.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "port/sdl/rmlui/rmlui_crt_calibration.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/menu/menu_internal.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/system/reset.h"

static void crt_calibration_enter(struct _TASK* task_ptr) {
    /* Bypass registry WAIT/FADE_IN phases — go straight to ACTIVE */
    task_ptr->timer = 0;

    /* Kill native sprites */
    g_state.Menu_Suicide[1] = 1;
    g_state.Menu_Suicide[2] = 0;

    rmlui_crt_calibration_show();
}

static void crt_calibration_tick(struct _TASK* task_ptr) {
    u16 sw = Check_Menu_Lever(0, 0);
    if (sw == 0)
        sw = Check_Menu_Lever(1, 0);

    /* Cancel (EAST/B button) or Confirm (SOUTH/A button) or Start -> return to option menu */
    if ((sw & SWK_EAST) || (sw & SWK_SOUTH) || (sw & SWK_START)) {
        SE_selected();
        rmlui_crt_calibration_hide();

        /* Go back to Option Select */
        MenuScreen_Goto(MENU_SCREEN_OPTION_SELECT);
    }
}

static void crt_calibration_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    rmlui_crt_calibration_hide();
}

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void ms_crt_calibration_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_crt_calibration_reg_ptr)(void) = ms_crt_calibration_register;
static void ms_crt_calibration_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_crt_calibration_register(void) {
#else
void ms_crt_calibration_register(void) {
#endif
    g_screens[MENU_SCREEN_CRT_CALIBRATION] = (MenuScreen) {
        .name = "crt_calibration",
        .id = MENU_SCREEN_CRT_CALIBRATION,
        .parent = MENU_SCREEN_OPTION_SELECT,
        .on_enter = crt_calibration_enter,
        .on_tick = crt_calibration_tick,
        .on_exit = crt_calibration_exit,
        .cursor_max = 0,
        .cancel_item = -1,
        .rmlui_show = NULL,
        .rmlui_hide = NULL,
        .header_type = MENU_HEADER_OPTION_MENU,
        .effect_slot = 0xFF,
    };
}
