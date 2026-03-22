#pragma once
/**
 * @file rmlui_option_menu.h
 * @brief RmlUi Option Menu screen — replaces CPS3 effect_61/effect_04 items.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the option menu data model and document. */
#ifdef ENABLE_RMLUI

void rmlui_option_menu_init(void);

/** Per-frame dirty-check sync. */
void rmlui_option_menu_update(void);

/** Show the option menu document. */
void rmlui_option_menu_show(void);

/** Hide the option menu document. */
void rmlui_option_menu_hide(void);

/** Destroy the data model. */
void rmlui_option_menu_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_option_menu_init(void) {}
static inline void rmlui_option_menu_update(void) {}
static inline void rmlui_option_menu_show(void) {}
static inline void rmlui_option_menu_hide(void) {}
static inline void rmlui_option_menu_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
