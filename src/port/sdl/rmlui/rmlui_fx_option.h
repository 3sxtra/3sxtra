#pragma once
/**
 * @file rmlui_fx_option.h
 * @brief RmlUi FX Option screen — in-game controller-friendly display/shader
 *        settings menu. Uses the game context data-model cursor pattern.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_fx_option_init(void);
void rmlui_fx_option_update(void);
void rmlui_fx_option_show(void);
void rmlui_fx_option_hide(void);
void rmlui_fx_option_shutdown(void);

/* Input forwarding — called from ms_option_select.c */
void rmlui_fx_option_cursor_up(void);
void rmlui_fx_option_cursor_down(void);
void rmlui_fx_option_value_left(void);
void rmlui_fx_option_value_right(void);
void rmlui_fx_option_page_left(void);
void rmlui_fx_option_page_right(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_fx_option_init(void) {}
static inline void rmlui_fx_option_update(void) {}
static inline void rmlui_fx_option_show(void) {}
static inline void rmlui_fx_option_hide(void) {}
static inline void rmlui_fx_option_shutdown(void) {}
static inline void rmlui_fx_option_cursor_up(void) {}
static inline void rmlui_fx_option_cursor_down(void) {}
static inline void rmlui_fx_option_value_left(void) {}
static inline void rmlui_fx_option_value_right(void) {}
static inline void rmlui_fx_option_page_left(void) {}
static inline void rmlui_fx_option_page_right(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
