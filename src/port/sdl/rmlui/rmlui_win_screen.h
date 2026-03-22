#pragma once
/**
 * @file rmlui_win_screen.h
 * @brief RmlUi Winner/Loser Screen — replaces CPS3 effect_76 text objects
 *        in Win_2nd()/Lose_2nd() with an HTML/CSS overlay banner.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_win_screen_init(void);
void rmlui_win_screen_update(void);
void rmlui_win_screen_show(void);
void rmlui_win_screen_hide(void);
void rmlui_win_screen_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_win_screen_init(void) {}
static inline void rmlui_win_screen_update(void) {}
static inline void rmlui_win_screen_show(void) {}
static inline void rmlui_win_screen_hide(void) {}
static inline void rmlui_win_screen_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
