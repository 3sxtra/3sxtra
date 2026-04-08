#pragma once
/**
 * @file rmlui_network_lobby.h
 * @brief RmlUi Network Lobby — replaces CPS3 effect_61/57/66/45 objects and
 *        SSPutStr_Bigger/Renderer_Queue2DPrimitive rendering in Network_Lobby()
 *        with an HTML/CSS overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

void rmlui_network_lobby_init(void);
void rmlui_network_lobby_update(void);
void rmlui_network_lobby_show(void);
void rmlui_network_lobby_hide(void);
// Returns the password for the active room, if any
const char* rmlui_network_lobby_get_active_password(void);

// Returns the filesystem path to the QR code BMP (empty if none)
const char* rmlui_network_lobby_get_qr_image_path(void);

// For casual/tournament rooms to reset the network lobby states if needed
void rmlui_network_lobby_reset(void);
void rmlui_network_lobby_shutdown(void);
void rmlui_network_lobby_create_room(void);
void rmlui_network_lobby_join_room(void);
void rmlui_network_lobby_room_scroll(int delta);
void rmlui_network_lobby_cycle_room_type(int direction);
void rmlui_network_lobby_cycle_tournament_format(int direction);
void rmlui_network_lobby_cycle_visibility(int direction);
int rmlui_network_lobby_get_create_room_type(void);
int rmlui_network_lobby_get_visibility(void);

void rmlui_network_lobby_password_input(int key);
void rmlui_network_lobby_submit_password(void);
void rmlui_network_lobby_cancel_password(void);
void rmlui_network_lobby_open_create_password(void);
void rmlui_network_lobby_open_join_code(void);
bool rmlui_network_lobby_is_password_popup_visible(void);

/// Returns true if an async Create/Join completed and a room is ready.
bool rmlui_network_lobby_has_pending_room(void);

/// Consume the pending room signal — returns the room code string.
const char* rmlui_network_lobby_consume_pending_room(void);

/// Returns true if the pending room is a tournament (for routing).
bool rmlui_network_lobby_pending_room_is_tournament(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_network_lobby_init(void) {}
static inline void rmlui_network_lobby_update(void) {}
static inline void rmlui_network_lobby_show(void) {}
static inline void rmlui_network_lobby_hide(void) {}
static inline void rmlui_network_lobby_shutdown(void) {}
static inline void rmlui_network_lobby_create_room(void) {}
static inline void rmlui_network_lobby_join_room(void) {}
static inline void rmlui_network_lobby_room_scroll(int delta) {
    (void)delta;
}
static inline void rmlui_network_lobby_cycle_room_type(int direction) {
    (void)direction;
}
static inline void rmlui_network_lobby_cycle_tournament_format(int direction) {
    (void)direction;
}
static inline void rmlui_network_lobby_cycle_visibility(int direction) {
    (void)direction;
}
static inline int rmlui_network_lobby_get_create_room_type(void) {
    return 0;
}
static inline int rmlui_network_lobby_get_visibility(void) {
    return 0;
}
static inline void rmlui_network_lobby_password_input(int key) {
    (void)key;
}
static inline void rmlui_network_lobby_submit_password(void) {}
static inline void rmlui_network_lobby_cancel_password(void) {}
static inline void rmlui_network_lobby_open_create_password(void) {}
static inline void rmlui_network_lobby_open_join_code(void) {}
static inline bool rmlui_network_lobby_is_password_popup_visible(void) {
    return false;
}
static inline bool rmlui_network_lobby_has_pending_room(void) {
    return false;
}
static inline const char* rmlui_network_lobby_consume_pending_room(void) {
    return "";
}
static inline bool rmlui_network_lobby_pending_room_is_tournament(void) {
    return false;
}
static inline const char* rmlui_network_lobby_get_qr_image_path(void) {
    return "";
}

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
