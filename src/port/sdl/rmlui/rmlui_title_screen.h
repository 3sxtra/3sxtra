#pragma once
/**
 * @file rmlui_title_screen.h
 * @brief RmlUi Title Screen — replaces CPS3 SSPutStr "PRESS START BUTTON".
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_title_screen_init(void);
void rmlui_title_screen_update(void);
void rmlui_title_screen_show(void);
void rmlui_title_screen_hide(void);
void rmlui_title_screen_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_title_screen_init(void) {}
static inline void rmlui_title_screen_update(void) {}
static inline void rmlui_title_screen_show(void) {}
static inline void rmlui_title_screen_hide(void) {}
static inline void rmlui_title_screen_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
