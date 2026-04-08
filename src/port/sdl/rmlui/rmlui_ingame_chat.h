#pragma once
/**
 * @file rmlui_ingame_chat.h
 * @brief In-match text chat overlay (Fightcade-style).
 *
 * Shows ephemeral chat messages on the fight screen and provides a
 * typing bar activated by a hotkey (T). Messages travel P2P via the
 * SDLNetAdapter out-of-band channel, and optionally relay through
 * the lobby server for spectator visibility.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

/// Initialize the in-game chat data model and load the overlay document.
void rmlui_ingame_chat_init(void);

/// Per-frame update — expire old messages, poll P2P inbox.
void rmlui_ingame_chat_update(void);

/// Destroy the data model and unload the document.
void rmlui_ingame_chat_shutdown(void);

/// Returns true if the player is currently typing a chat message.
/// When true, game input should be suppressed for the local player.
bool rmlui_ingame_chat_is_typing(void);

/// Open the chat input bar (call on T key press during match).
void rmlui_ingame_chat_open_input(void);

/// Handle a keyboard event while chat input is open.
/// keycode: SDL_Keycode. text: UTF-8 text from SDL_EVENT_TEXT_INPUT (may be NULL).
void rmlui_ingame_chat_handle_key(int keycode, const char* text);

/// Set the room code for lobby server relay (so spectators see chat).
/// Pass NULL or "" to disable relay (LAN-only mode).
void rmlui_ingame_chat_set_room_code(const char* room_code);

/// Set the opponent's display name (shown for incoming P2P messages).
/// Pass NULL or "" to reset to default "OPP".
void rmlui_ingame_chat_set_opponent_name(const char* name);

#else /* !ENABLE_RMLUI */

static inline void rmlui_ingame_chat_init(void) {}
static inline void rmlui_ingame_chat_update(void) {}
static inline void rmlui_ingame_chat_shutdown(void) {}
static inline bool rmlui_ingame_chat_is_typing(void) {
    return false;
}
static inline void rmlui_ingame_chat_open_input(void) {}
static inline void rmlui_ingame_chat_handle_key(int k, const char* t) {
    (void)k;
    (void)t;
}
static inline void rmlui_ingame_chat_set_room_code(const char* r) {
    (void)r;
}
static inline void rmlui_ingame_chat_set_opponent_name(const char* n) {
    (void)n;
}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
