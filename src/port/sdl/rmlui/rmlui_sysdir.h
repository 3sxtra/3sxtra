#pragma once
/**
 * @file rmlui_sysdir.h
 * @brief RmlUi System Direction (Dipswitch) Menu — replaces CPS3 effect_61/64/04
 *        objects in System_Direction() and effect_18/51/40/45/66 in Direction_Menu()
 *        with an HTML/CSS overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_sysdir_init(void);
void rmlui_sysdir_update(void);
void rmlui_sysdir_show(void);
void rmlui_sysdir_hide(void);
void rmlui_sysdir_enter_subpage(void);
void rmlui_sysdir_exit_subpage(void);
void rmlui_sysdir_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_sysdir_init(void) {}
static inline void rmlui_sysdir_update(void) {}
static inline void rmlui_sysdir_show(void) {}
static inline void rmlui_sysdir_hide(void) {}
static inline void rmlui_sysdir_enter_subpage(void) {}
static inline void rmlui_sysdir_exit_subpage(void) {}
static inline void rmlui_sysdir_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
