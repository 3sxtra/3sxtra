#ifndef PORT_BROADCAST_H
#define PORT_BROADCAST_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BROADCAST_SOURCE_NATIVE = 0, // Raw engine texture (384x224)
    BROADCAST_SOURCE_FINAL = 1   // Final composited frame (with shaders/bezels)
} BroadcastSource;

typedef struct BroadcastConfig {
    bool enabled;
    BroadcastSource source;
    bool show_ui;
} BroadcastConfig;

/// Interface for platform-specific broadcast backends
typedef struct BroadcastPort {
    /// Initialize the broadcast backend
    bool (*Init)(const char* sender_name);

    /// Shutdown the broadcast backend
    void (*Shutdown)();

    /// Send a texture to the broadcast system
    /// @param texture_id OpenGL texture ID
    /// @param width Width of the texture
    /// @param height Height of the texture
    /// @param is_flipped Whether the texture is vertically flipped (OpenGL default)
    bool (*SendTexture)(uint32_t texture_id, uint32_t width, uint32_t height, bool is_flipped);

    /// Update configuration
    void (*UpdateConfig)(const BroadcastConfig* config);
} BroadcastPort;

/** @brief Global broadcast configuration (defined in game_globals.c). */
extern BroadcastConfig broadcast_config;

// Public API
#ifdef ENABLE_BROADCAST
void Broadcast_Initialize(void);
void Broadcast_Shutdown(void);
bool Broadcast_Send(uint32_t texture_id, uint32_t width, uint32_t height, bool is_flipped);
void Broadcast_Update(void); // Call this to sync config changes
#else
static inline void Broadcast_Initialize(void) {}
static inline void Broadcast_Shutdown(void) {}
static inline bool Broadcast_Send(uint32_t t, uint32_t w, uint32_t h, bool f) {
    (void)t;
    (void)w;
    (void)h;
    (void)f;
    return false;
}
static inline void Broadcast_Update(void) {}
#endif

#endif
