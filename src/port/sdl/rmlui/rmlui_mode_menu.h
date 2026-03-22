#pragma once
/**
 * @file rmlui_mode_menu.h
 * @brief RmlUi Mode Select screen — replaces CPS3 effect_61/effect_04 items.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the mode menu data model and document. */
#ifdef ENABLE_RMLUI

void rmlui_mode_menu_init(void);

/** Per-frame dirty-check sync. */
void rmlui_mode_menu_update(void);

/** Show the mode menu document. */
void rmlui_mode_menu_show(void);

/** Hide the mode menu document. */
void rmlui_mode_menu_hide(void);

/** Destroy the data model. */
void rmlui_mode_menu_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_mode_menu_init(void) {}
static inline void rmlui_mode_menu_update(void) {}
static inline void rmlui_mode_menu_show(void) {}
static inline void rmlui_mode_menu_hide(void) {}
static inline void rmlui_mode_menu_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
