#pragma once
/**
 * @file rmlui_game_option.h
 * @brief RmlUi Game Option screen — replaces CPS3 effect_61/effect_64 items.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the game option data model and document. */
void rmlui_game_option_init(void);

/** Per-frame dirty-check sync. */
void rmlui_game_option_update(void);

/** Show the game option document. */
void rmlui_game_option_show(void);

/** Hide the game option document. */
void rmlui_game_option_hide(void);

/** Destroy the data model. */
void rmlui_game_option_shutdown(void);

#ifdef __cplusplus
}
#endif
