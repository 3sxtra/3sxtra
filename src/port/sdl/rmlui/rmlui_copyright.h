#pragma once
/**
 * @file rmlui_copyright.h
 * @brief RmlUi copyright text overlay — replaces Disp_Copyright() SSPutStrPro calls.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_copyright_init(void);
void rmlui_copyright_show(void);
void rmlui_copyright_hide(void);
void rmlui_copyright_update(void);
void rmlui_copyright_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_copyright_init(void) {}
static inline void rmlui_copyright_show(void) {}
static inline void rmlui_copyright_hide(void) {}
static inline void rmlui_copyright_update(void) {}
static inline void rmlui_copyright_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
