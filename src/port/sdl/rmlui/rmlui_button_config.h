#pragma once
/**
 * @file rmlui_button_config.h
 * @brief RmlUi Button Config — replaces CPS3 effect_23/effect_66
 *        objects in Button_Config() with an HTML/CSS overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_button_config_init(void);
void rmlui_button_config_update(void);
void rmlui_button_config_show(void);
void rmlui_button_config_hide(void);
void rmlui_button_config_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_button_config_init(void) {}
static inline void rmlui_button_config_update(void) {}
static inline void rmlui_button_config_show(void) {}
static inline void rmlui_button_config_hide(void) {}
static inline void rmlui_button_config_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
