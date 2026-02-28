#pragma once
/**
 * @file rmlui_option_menu.h
 * @brief RmlUi Option Menu screen — replaces CPS3 effect_61/effect_04 items.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the option menu data model and document. */
void rmlui_option_menu_init(void);

/** Per-frame dirty-check sync. */
void rmlui_option_menu_update(void);

/** Show the option menu document. */
void rmlui_option_menu_show(void);

/** Hide the option menu document. */
void rmlui_option_menu_hide(void);

/** Destroy the data model. */
void rmlui_option_menu_shutdown(void);

#ifdef __cplusplus
}
#endif
