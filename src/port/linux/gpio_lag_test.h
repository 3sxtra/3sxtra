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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_GPIO_LAG_TEST

void GpioLagTest_Init(void);
void GpioLagTest_Shutdown(void);
void GpioLagTest_OnInputPoll(void);
void GpioLagTest_Toggle(void);
bool GpioLagTest_IsEnabled(void);

#else /* !ENABLE_GPIO_LAG_TEST — inline no-ops */

static inline void GpioLagTest_Init(void) {}
static inline void GpioLagTest_Shutdown(void) {}
static inline void GpioLagTest_OnInputPoll(void) {}
static inline void GpioLagTest_Toggle(void) {}
static inline bool GpioLagTest_IsEnabled(void) {
    return false;
}

#endif /* ENABLE_GPIO_LAG_TEST */

#ifdef __cplusplus
}
#endif

#endif /* GPIO_LAG_TEST_H */
