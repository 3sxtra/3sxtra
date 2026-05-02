/**
 * @file gpio_lag_test.c
 * @brief GPIO 17 button reader for input lag measurement (Pi4 / Batocera).
 *
 * Uses the Linux GPIO character device (/dev/gpiochip0) with v2 ioctl ABI
 * to read a momentary push-button on GPIO 17 (active low, hardware pull-up).
 * When the button is pressed, Medium Kick (0x200) is OR'd into Player 1's
 * io_w.sw[0] word.
 *
 * The button's LED is hardware-wired (lights on press without software
 * control), so a high-speed camera can see the LED flash and correlate
 * it with the on-screen frame counter in the debug HUD.
 *
 * Frame tracking: records the frame the button was first pressed (receive_frame)
 * and the frame when the game character reacted (active_frame, via routine_no
 * change). Results persist on-screen for ~3 seconds after button release.
 *
 * Uses raw ioctl instead of libgpiod for zero external dependencies on
 * Batocera (which has no package manager).
 */
#ifdef ENABLE_GPIO_LAG_TEST

#include "port/linux/gpio_lag_test.h"
#include "game_state.h"
#include "sf33rd/Source/Game/io/ioconv.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/engine/plcnt.h"

#include <SDL3/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/gpio.h>

/* ── Configuration ─────────────────────────────────────────────────── */

#define GPIO_CHIP "/dev/gpiochip0"
#define GPIO_LINE 17
#define MK_FLAG 0x200 /* Medium Kick game flag */
#define CONSUMER_LABEL "3sx-lag-test"
#define DISPLAY_PERSIST_FRAMES 180 /* ~3 seconds at 60fps */

/* ── State ─────────────────────────────────────────────────────────── */

static bool s_enabled = false;
static bool s_gpio_ready = false;
static int s_chip_fd = -1; /* /dev/gpiochip0 fd */
static int s_line_fd = -1; /* Line request fd (for get_values) */
static int s_debug_counter = 0;

/* ── Frame tracking state ──────────────────────────────────────────── */

static bool s_button_held = false;   /* Current GPIO button state */
static bool s_button_prev = false;   /* Previous frame button state */
static bool s_tracking = false;      /* Currently tracking a press→react cycle */
static bool s_result_ready = false;  /* active_frame has been detected */
static uint32_t s_receive_frame = 0; /* Frame when button first pressed */
static uint32_t s_active_frame = 0;  /* Frame when routine_no changed */
static int s_initial_routine_0 = 0;  /* routine_no[0] at press time */
static int s_initial_routine_1 = 0;  /* routine_no[1] at press time */
static int s_display_timer = 0;      /* Countdown for OSD persistence */
static uint64_t s_receive_ticks = 0; /* SDL perf counter at button press */
static uint64_t s_active_ticks = 0;  /* SDL perf counter at game reaction */

/* ── Public API ────────────────────────────────────────────────────── */

void GpioLagTest_Init(void) {
    SDL_Log("[GpioLagTest] Initializing GPIO %d via %s...", GPIO_LINE, GPIO_CHIP);

    /* 1. Open the GPIO chip */
    s_chip_fd = open(GPIO_CHIP, O_RDONLY | O_CLOEXEC);
    if (s_chip_fd < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "[GpioLagTest] Failed to open %s (errno=%d: %s)",
                     GPIO_CHIP,
                     errno,
                     strerror(errno));
        return;
    }
    SDL_Log("[GpioLagTest] Opened %s (fd=%d)", GPIO_CHIP, s_chip_fd);

    /* 2. Request GPIO line as input with pull-up bias */
    struct gpio_v2_line_request req;
    memset(&req, 0, sizeof(req));

    req.offsets[0] = GPIO_LINE;
    req.num_lines = 1;
    strncpy(req.consumer, CONSUMER_LABEL, sizeof(req.consumer) - 1);

    req.config.flags = GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
    /* Single line, no per-line attribute overrides needed */
    req.config.num_attrs = 0;

    int ret = ioctl(s_chip_fd, GPIO_V2_GET_LINE_IOCTL, &req);
    if (ret < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "[GpioLagTest] GPIO_V2_GET_LINE_IOCTL failed (errno=%d: %s)",
                     errno,
                     strerror(errno));
        close(s_chip_fd);
        s_chip_fd = -1;
        return;
    }

    s_line_fd = req.fd;
    SDL_Log("[GpioLagTest] Line %d requested OK (line_fd=%d)", GPIO_LINE, s_line_fd);

    /* 3. Do a test read */
    struct gpio_v2_line_values vals;
    memset(&vals, 0, sizeof(vals));
    vals.mask = 1; /* bit 0 = our single line */

    ret = ioctl(s_line_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &vals);
    if (ret < 0) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION, "[GpioLagTest] Test read failed (errno=%d: %s)", errno, strerror(errno));
    } else {
        SDL_Log("[GpioLagTest] Test read: value=%llu (1=HIGH/released, 0=LOW/pressed)", (unsigned long long)vals.bits);
    }

    s_gpio_ready = true;
    s_enabled = true;
    SDL_Log("[GpioLagTest] GPIO %d initialized OK — MK injection ACTIVE", GPIO_LINE);
}

void GpioLagTest_Shutdown(void) {
    if (s_line_fd >= 0) {
        close(s_line_fd);
        s_line_fd = -1;
    }
    if (s_chip_fd >= 0) {
        close(s_chip_fd);
        s_chip_fd = -1;
    }
    s_gpio_ready = false;
    s_enabled = false;
    SDL_Log("[GpioLagTest] GPIO %d released", GPIO_LINE);
}

void GpioLagTest_OnInputPoll(void) {
    if (!s_enabled || !s_gpio_ready || s_line_fd < 0)
        return;

    struct gpio_v2_line_values vals;
    memset(&vals, 0, sizeof(vals));
    vals.mask = 1;

    int ret = ioctl(s_line_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &vals);
    if (ret < 0) {
        if (s_debug_counter++ % 300 == 0) {
            SDL_LogWarn(
                SDL_LOG_CATEGORY_APPLICATION, "[GpioLagTest] read failed (errno=%d: %s)", errno, strerror(errno));
        }
        return;
    }

    /* Log periodically (~every 5 seconds at 60fps) */
    if (s_debug_counter++ % 300 == 0) {
        SDL_Log("[GpioLagTest] GPIO %d = %llu", GPIO_LINE, (unsigned long long)vals.bits);
    }

    /* Track button state for rising-edge detection */
    s_button_prev = s_button_held;

    /* Active low: bit=0 means button is pressed */
    s_button_held = ((vals.bits & 1) == 0);

    if (s_button_held) {
        io_w.sw[0] |= MK_FLAG;
    }

    /* Rising edge: button just pressed */
    if (s_button_held && !s_button_prev) {
        /* Debounce: ignore any rising edges within 30 frames (0.5s) of the last one
           to eliminate physical switch bounce corrupting the receive_frame. */
        static uint32_t s_last_press_frame = 0;
        if (g_state.system_timer - s_last_press_frame < 30) {
            return;
        }
        s_last_press_frame = g_state.system_timer;

        s_receive_frame = g_state.system_timer;
        s_receive_ticks = SDL_GetPerformanceCounter();
        s_active_frame = 0;
        s_active_ticks = 0;
        s_result_ready = false;
        s_tracking = true;
        s_display_timer = DISPLAY_PERSIST_FRAMES;

        /* Capture initial routine_no for change detection */
        s_initial_routine_0 = g_state.plw[0].wu.routine_no[0];
        s_initial_routine_1 = g_state.plw[0].wu.routine_no[1];

        SDL_Log("[GpioLagTest] Button PRESSED — receive_frame=%u, routine_no=[%d,%d]",
                s_receive_frame,
                s_initial_routine_0,
                s_initial_routine_1);
    }
}

void GpioLagTest_UpdateFrameTracking(void) {
    if (!s_enabled)
        return;

    /* Check if game state reacted to the input (routine_no changed) */
    if (s_tracking && !s_result_ready) {
        if (g_state.plw[0].wu.routine_no[0] != s_initial_routine_0 || g_state.plw[0].wu.routine_no[1] != s_initial_routine_1) {

            s_active_frame = g_state.system_timer;
            s_active_ticks = SDL_GetPerformanceCounter();
            s_result_ready = true;

            double lag_ms = (double)(s_active_ticks - s_receive_ticks) * 1000.0 / (double)SDL_GetPerformanceFrequency();

            SDL_Log("[GpioLagTest] State CHANGED — active_frame=%u, lag=%d frames (%.2fms), "
                    "routine_no: [%d,%d] -> [%d,%d]",
                    s_active_frame,
                    (int)(s_active_frame - s_receive_frame),
                    lag_ms,
                    s_initial_routine_0,
                    s_initial_routine_1,
                    g_state.plw[0].wu.routine_no[0],
                    g_state.plw[0].wu.routine_no[1]);
        }
    }

    /* Persistence timer: keep OSD visible for ~3 seconds after press */
    if (s_display_timer > 0) {
        s_display_timer--;
        if (s_display_timer == 0) {
            s_tracking = false;
            s_result_ready = false;
        }
    }
}

void GpioLagTest_Toggle(void) {
    if (!s_gpio_ready) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[GpioLagTest] Cannot toggle — GPIO not initialised");
        return;
    }
    s_enabled = !s_enabled;
    SDL_Log("[GpioLagTest] %s", s_enabled ? "ENABLED" : "DISABLED");
}

bool GpioLagTest_IsEnabled(void) {
    return s_enabled;
}

GpioLagTestState GpioLagTest_GetState(void) {
    GpioLagTestState state;
    state.enabled = s_enabled;
    state.input_held = s_button_held;
    state.tracking = s_tracking;
    state.result_ready = s_result_ready;
    state.current_frame = g_state.system_timer;
    state.receive_frame = s_receive_frame;
    state.active_frame = s_active_frame;
    state.lag_frames = s_result_ready ? (int32_t)(s_active_frame - s_receive_frame) : 0;
    state.display_timer = s_display_timer;
    state.receive_ticks = s_receive_ticks;
    state.active_ticks = s_active_ticks;
    if (s_result_ready && s_active_ticks > s_receive_ticks) {
        state.lag_ms = (double)(s_active_ticks - s_receive_ticks) * 1000.0 / (double)SDL_GetPerformanceFrequency();
    } else {
        state.lag_ms = 0.0;
    }
    return state;
}

#endif /* ENABLE_GPIO_LAG_TEST */
