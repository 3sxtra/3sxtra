#pragma once
/**
 * @file rmlui_game_option.h
 * @brief RmlUi Game Option screen — replaces CPS3 effect_61/effect_64 items.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the game option data model and document. */
#ifdef ENABLE_RMLUI

void rmlui_game_option_init(void);

/** Per-frame dirty-check sync. */
void rmlui_game_option_update(void);

/** Show the game option document. */
void rmlui_game_option_show(void);

/** Hide the game option document. */
void rmlui_game_option_hide(void);

/** Destroy the data model. */
void rmlui_game_option_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_game_option_init(void) {}
static inline void rmlui_game_option_update(void) {}
static inline void rmlui_game_option_show(void) {}
static inline void rmlui_game_option_hide(void) {}
static inline void rmlui_game_option_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
