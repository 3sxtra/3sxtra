#pragma once
/**
 * @file rmlui_continue.h
 * @brief RmlUi Continue Screen — replaces CPS3 effect_76 countdown text
 *        in Setup_Continue_OBJ() with an HTML/CSS overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_continue_init(void);
void rmlui_continue_update(void);
void rmlui_continue_show(void);
void rmlui_continue_hide(void);
void rmlui_continue_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_continue_init(void) {}
static inline void rmlui_continue_update(void) {}
static inline void rmlui_continue_show(void) {}
static inline void rmlui_continue_hide(void) {}
static inline void rmlui_continue_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
