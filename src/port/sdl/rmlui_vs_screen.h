#pragma once
/**
 * @file rmlui_vs_screen.h
 * @brief RmlUi VS Screen overlay — text elements (P1/P2 char names,
 *        stage name, "VS" label) overlaid on CPS3 sprite animations.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_vs_screen_init(void);
void rmlui_vs_screen_update(void);
void rmlui_vs_screen_show(void);
void rmlui_vs_screen_hide(void);
void rmlui_vs_screen_shutdown(void);

#ifdef __cplusplus
}
#endif
