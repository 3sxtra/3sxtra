/**
 * @file broadcast_spout.cpp
 * @brief Spout2 video broadcast backend (Windows).
 *
 * Implements the Broadcast API for Windows using Spout2, enabling
 * real-time frame sharing of the game's OpenGL framebuffer with
 * external applications (OBS, resolume, etc.).
 */
#include "SpoutSender.h"
#include "port/broadcast.h"
#include <memory>
#include <string>

// Global SpoutSender instance
static std::unique_ptr<SpoutSender> g_spout_sender;
static std::string g_current_sender_name;

extern "C" {

static bool Spout_Init(const char* sender_name) {
    if (!g_spout_sender) {
        g_spout_sender = std::make_unique<SpoutSender>();
    }

    g_current_sender_name = sender_name ? sender_name : "3SX Broadcast";
    g_spout_sender->SetSenderName(g_current_sender_name.c_str());

    return true;
}

static void Spout_Shutdown() {
    if (g_spout_sender) {
        g_spout_sender->ReleaseSender();
        g_spout_sender.reset();
    }
}

static bool Spout_SendTexture(uint32_t texture_id, uint32_t width, uint32_t height, bool is_flipped) {
    if (!g_spout_sender)
        return false;

    // Spout handles initialization on the first SendTexture call if not already done.
    // texture_target is GL_TEXTURE_2D (0x0DE1)
    return g_spout_sender->SendTexture(texture_id, 0x0DE1, width, height, is_flipped);
}

static void Spout_UpdateConfig(const BroadcastConfig* config) {
    // Config updates could handle name changes, etc.
    // For now, if disabled, we might want to release.
    if (config && !config->enabled && g_spout_sender) {
        g_spout_sender->ReleaseSender();
    }
}

BroadcastPort g_broadcast_port_win32 = { Spout_Init, Spout_Shutdown, Spout_SendTexture, Spout_UpdateConfig };

} // extern "C"
