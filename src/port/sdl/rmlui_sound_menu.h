#pragma once
/**
 * @file rmlui_sound_menu.h
 * @brief RmlUi Sound Test / Screen Adjust Menu — replaces CPS3 effect_57/61/64/A8
 *        objects in Sound_Test() with an HTML/CSS overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_sound_menu_init(void);
void rmlui_sound_menu_update(void);
void rmlui_sound_menu_show(void);
void rmlui_sound_menu_hide(void);
void rmlui_sound_menu_shutdown(void);

#ifdef __cplusplus
}
#endif
