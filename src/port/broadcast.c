/**
 * @file broadcast.c
 * @brief Platform-agnostic video broadcast dispatcher.
 *
 * Routes broadcast operations (init, shutdown, send texture) to the
 * appropriate platform backend: Spout2 (Windows), Syphon (macOS),
 * or PipeWire (Linux). On unsupported platforms (e.g. RPi4), all
 * operations are no-ops.
 */
#include "port/broadcast.h"
#include <stdbool.h>
#include <stddef.h>

extern BroadcastConfig broadcast_config;

// Platform backends
#if defined(PLATFORM_RPI4)
// RPi4/Batocera: no broadcast backend available
static BroadcastPort* s_backend = NULL;
#elif defined(_WIN32)
extern BroadcastPort g_broadcast_port_win32;
static BroadcastPort* s_backend = &g_broadcast_port_win32;
#elif defined(__APPLE__) && defined(HAVE_SYPHON)
extern BroadcastPort g_broadcast_port_macos;
static BroadcastPort* s_backend = &g_broadcast_port_macos;
#elif defined(__linux__)
extern BroadcastPort g_broadcast_port_linux;
static BroadcastPort* s_backend = &g_broadcast_port_linux;
#else
static BroadcastPort* s_backend = NULL;
#endif

static bool s_initialized = false;
static bool s_was_enabled = false;

/** @brief Initialize the broadcast backend if enabled at startup. */
void Broadcast_Initialize(void) {
    if (!s_backend)
        return;

    // We defer actual initialization until the first Send or if enabled at startup
    if (broadcast_config.enabled) {
        if (s_backend->Init("3SX Game Output")) {
            s_initialized = true;
        }
    }
    s_was_enabled = broadcast_config.enabled;
}

/** @brief Shut down the broadcast backend and release resources. */
void Broadcast_Shutdown(void) {
    if (s_backend && s_initialized) {
        s_backend->Shutdown();
        s_initialized = false;
    }
}

/** @brief Handle enable/disable toggling and config updates each frame. */
void Broadcast_Update(void) {
    if (!s_backend)
        return;

    // Handle Enable/Disable toggling
    if (broadcast_config.enabled && !s_was_enabled) {
        if (!s_initialized) {
            if (s_backend->Init("3SX Game Output")) {
                s_initialized = true;
            }
        }
    } else if (!broadcast_config.enabled && s_was_enabled) {
        if (s_initialized) {
            s_backend->Shutdown();
            s_initialized = false;
        }
    }

    s_was_enabled = broadcast_config.enabled;

    if (s_initialized && s_backend->UpdateConfig) {
        s_backend->UpdateConfig(&broadcast_config);
    }
}

/** @brief Send an OpenGL texture to the broadcast output. Lazy-initializes if needed. */
bool Broadcast_Send(uint32_t texture_id, uint32_t width, uint32_t height, bool is_flipped) {
    if (!s_backend || !broadcast_config.enabled)
        return false;

    if (!s_initialized) {
        if (s_backend->Init("3SX Game Output")) {
            s_initialized = true;
        } else {
            return false;
        }
    }

    return s_backend->SendTexture(texture_id, width, height, is_flipped);
}
