#pragma once
/**
 * @file rmlui_crt_calibration.h
 * @brief RmlUi CRT Calibration screen.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_crt_calibration_init(void);
void rmlui_crt_calibration_update(void);
void rmlui_crt_calibration_show(void);
void rmlui_crt_calibration_hide(void);
void rmlui_crt_calibration_shutdown(void);
void rmlui_crt_calibration_next_pattern(void);
void rmlui_crt_calibration_prev_pattern(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_crt_calibration_init(void) {}
static inline void rmlui_crt_calibration_update(void) {}
static inline void rmlui_crt_calibration_show(void) {}
static inline void rmlui_crt_calibration_hide(void) {}
static inline void rmlui_crt_calibration_shutdown(void) {}
static inline void rmlui_crt_calibration_next_pattern(void) {}
static inline void rmlui_crt_calibration_prev_pattern(void) {}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
