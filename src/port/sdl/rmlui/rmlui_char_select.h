#pragma once
/**
 * @file rmlui_char_select.h
 * @brief RmlUi Character Select overlay — text elements (timer, char names,
 *        SA labels) overlaid on CPS3 sprite portraits.
 */

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

#ifdef ENABLE_RMLUI

void rmlui_char_select_init(void);
void rmlui_char_select_update(void);
void rmlui_char_select_show(void);
void rmlui_char_select_hide(void);
void rmlui_char_select_shutdown(void);

/** True while the RmlUI char select overlay is visible (set by show/hide). */
extern bool rmlui_char_select_visible;

#else /* !ENABLE_RMLUI */

static inline void rmlui_char_select_init(void) {}
static inline void rmlui_char_select_update(void) {}
static inline void rmlui_char_select_show(void) {}
static inline void rmlui_char_select_hide(void) {}
static inline void rmlui_char_select_shutdown(void) {}
static const bool rmlui_char_select_visible = false;

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
