#pragma once
/**
 * @file rmlui_vs_screen.h
 * @brief RmlUi VS Screen overlay — text elements (P1/P2 char names,
 *        stage name, "VS" label) overlaid on CPS3 sprite animations.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_vs_screen_init(void);
void rmlui_vs_screen_update(void);
void rmlui_vs_screen_show(void);
void rmlui_vs_screen_hide(void);
void rmlui_vs_screen_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_vs_screen_init(void) {}
static inline void rmlui_vs_screen_update(void) {}
static inline void rmlui_vs_screen_show(void) {}
static inline void rmlui_vs_screen_hide(void) {}
static inline void rmlui_vs_screen_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
