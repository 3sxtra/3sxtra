#pragma once
/**
 * @file rmlui_sound_menu.h
 * @brief RmlUi Sound Test / Screen Adjust Menu — replaces CPS3 effect_57/61/64/A8
 *        objects in Sound_Test() with an HTML/CSS overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_sound_menu_init(void);
void rmlui_sound_menu_update(void);
void rmlui_sound_menu_show(void);
void rmlui_sound_menu_hide(void);
void rmlui_sound_menu_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_sound_menu_init(void) {}
static inline void rmlui_sound_menu_update(void) {}
static inline void rmlui_sound_menu_show(void) {}
static inline void rmlui_sound_menu_hide(void) {}
static inline void rmlui_sound_menu_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
