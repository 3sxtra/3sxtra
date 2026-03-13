#ifndef RMLUI_CASUAL_LOBBY_H
#define RMLUI_CASUAL_LOBBY_H

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

#ifdef __cplusplus
}
#endif

#endif
