#pragma once
/**
 * @file rmlui_gameover.h
 * @brief RmlUi Game Over / Results Screen.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_gameover_init(void);
void rmlui_gameover_update(void);
void rmlui_gameover_show_banner(void);  /* Phase 1: red banner transition */
void rmlui_gameover_show_results(void); /* Phase 2: recap/results screen */
void rmlui_gameover_show(void);         /* Alias for show_results */
void rmlui_gameover_hide(void);
void rmlui_gameover_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_gameover_init(void) {}
static inline void rmlui_gameover_update(void) {}
static inline void rmlui_gameover_show_banner(void) {}
static inline void rmlui_gameover_show_results(void) {}
static inline void rmlui_gameover_show(void) {}
static inline void rmlui_gameover_hide(void) {}
static inline void rmlui_gameover_shutdown(void) {}

#endif

#ifdef __cplusplus
}
#endif
