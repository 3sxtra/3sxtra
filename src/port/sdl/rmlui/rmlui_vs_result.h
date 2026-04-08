#pragma once
/**
 * @file rmlui_vs_result.h
 * @brief RmlUi VS Result Screen — replaces CPS3 effect_A0/effect_91/effect_66
 *        objects in VS_Result() with an HTML/CSS overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_vs_result_init(void);
void rmlui_vs_result_update(void);
void rmlui_vs_result_show(int p1_wins, int p2_wins, int p1_pct, int p2_pct);
void rmlui_vs_result_hide(void);
void rmlui_vs_result_shutdown(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_vs_result_init(void) {}
static inline void rmlui_vs_result_update(void) {}
static inline void rmlui_vs_result_show(int p1_wins, int p2_wins, int p1_pct, int p2_pct) {
    (void)p1_wins;
    (void)p2_wins;
    (void)p1_pct;
    (void)p2_pct;
}
static inline void rmlui_vs_result_hide(void) {}
static inline void rmlui_vs_result_shutdown(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
