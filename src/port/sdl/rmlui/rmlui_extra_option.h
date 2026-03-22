#pragma once
/**
 * @file rmlui_extra_option.h
 * @brief RmlUi Extra Option screen — replaces CPS3 Dir_Move_Sub/Setup_Next_Page
 *        effect pipeline with a 4-page HTML/CSS toggle table overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_extra_option_init(void);
void rmlui_extra_option_update(void);
void rmlui_extra_option_show(void);
void rmlui_extra_option_hide(void);
void rmlui_extra_option_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_extra_option_init(void) {}
static inline void rmlui_extra_option_update(void) {}
static inline void rmlui_extra_option_show(void) {}
static inline void rmlui_extra_option_hide(void) {}
static inline void rmlui_extra_option_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
