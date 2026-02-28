#pragma once
/**
 * @file rmlui_button_config.h
 * @brief RmlUi Button Config — replaces CPS3 effect_23/effect_66
 *        objects in Button_Config() with an HTML/CSS overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_button_config_init(void);
void rmlui_button_config_update(void);
void rmlui_button_config_show(void);
void rmlui_button_config_hide(void);
void rmlui_button_config_shutdown(void);

#ifdef __cplusplus
}
#endif
