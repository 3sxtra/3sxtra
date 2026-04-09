/**
 * @file gpio_lag_test.h
 * @brief GPIO-based input lag measurement (Pi4 only).
 *
 * Reads a physical button on GPIO 17 (active low) and injects Medium Kick
 * into Player 1's input state for camera-based input lag measurement.
 * The button's hardware-wired LED lights up on press, so a high-speed
 * camera can correlate the LED flash with the on-screen reaction.
 *
 * Compile with -DENABLE_GPIO_LAG_TEST to activate.
 */
#ifndef GPIO_LAG_TEST_H
#define GPIO_LAG_TEST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Snapshot of input lag test state for the OSD renderer. */
typedef struct {
    bool enabled;           /**< Whether GPIO lag test mode is on */
    bool input_held;        /**< Whether the GPIO button is currently pressed */
    bool tracking;          /**< Whether we're tracking a press (receive→active) */
    bool result_ready;      /**< Whether active_frame was detected for this press */
    uint32_t current_frame; /**< Current system_timer value */
    uint32_t receive_frame; /**< Frame when GPIO button press was first detected */
    uint32_t active_frame;  /**< Frame when game state reacted (routine_no changed) */
    int32_t lag_frames;     /**< active_frame - receive_frame (0 if not yet detected) */
    int display_timer;      /**< Countdown frames remaining to keep OSD result visible */
    uint64_t receive_ticks; /**< SDL perf counter when button was pressed */
    uint64_t active_ticks;  /**< SDL perf counter when game state reacted */
    double lag_ms;          /**< Actual measured lag in milliseconds (active - receive) */
} GpioLagTestState;

#ifdef ENABLE_GPIO_LAG_TEST

void GpioLagTest_Init(void);
void GpioLagTest_Shutdown(void);
void GpioLagTest_OnInputPoll(void);
void GpioLagTest_Toggle(void);
bool GpioLagTest_IsEnabled(void);
void GpioLagTest_UpdateFrameTracking(void);
GpioLagTestState GpioLagTest_GetState(void);

#else /* !ENABLE_GPIO_LAG_TEST — inline no-ops */

static inline void GpioLagTest_Init(void) {}
static inline void GpioLagTest_Shutdown(void) {}
static inline void GpioLagTest_OnInputPoll(void) {}
static inline void GpioLagTest_Toggle(void) {}
static inline bool GpioLagTest_IsEnabled(void) {
    return false;
}
static inline void GpioLagTest_UpdateFrameTracking(void) {}
static inline GpioLagTestState GpioLagTest_GetState(void) {
    GpioLagTestState s = { 0 };
    return s;
}

#endif /* ENABLE_GPIO_LAG_TEST */

#ifdef __cplusplus
}
#endif

#endif /* GPIO_LAG_TEST_H */
