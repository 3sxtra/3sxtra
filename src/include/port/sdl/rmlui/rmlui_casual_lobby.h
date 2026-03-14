#ifndef RMLUI_CASUAL_LOBBY_H
#define RMLUI_CASUAL_LOBBY_H

#include <stdbool.h>

// Forward declaration for SDL_Event
union SDL_Event;

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_casual_lobby_init(void);
void rmlui_casual_lobby_update(void);
void rmlui_casual_lobby_show(void);
void rmlui_casual_lobby_hide(void);
void rmlui_casual_lobby_shutdown(void);

// Returns true if the casual lobby screen is currently visible
bool rmlui_casual_lobby_is_visible(void);

// Force the UI to refresh its state using the given room code
void rmlui_casual_lobby_set_room(const char* room_code);

// Returns the current room code (empty string if not in a room)
const char* rmlui_casual_lobby_get_room_code(void);

/// Handle SDL keyboard/text events for the chat popup.
/// Returns true if the event was consumed (chat is active).
bool rmlui_casual_lobby_handle_key_event(const union SDL_Event* event);

#ifdef __cplusplus
}
#endif

#endif
