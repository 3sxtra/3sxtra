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
void rmlui_network_lobby_shutdown(void);
void rmlui_network_lobby_create_room(void);
void rmlui_network_lobby_join_room(void);
void rmlui_network_lobby_room_scroll(int delta);

/// Returns true if an async Create/Join completed and a room is ready.
bool rmlui_network_lobby_has_pending_room(void);

/// Consume the pending room signal — returns the room code string.
const char* rmlui_network_lobby_consume_pending_room(void);

#else /* !ENABLE_RMLUI */

static inline void rmlui_network_lobby_init(void) {}
static inline void rmlui_network_lobby_update(void) {}
static inline void rmlui_network_lobby_show(void) {}
static inline void rmlui_network_lobby_hide(void) {}
static inline void rmlui_network_lobby_shutdown(void) {}
static inline void rmlui_network_lobby_create_room(void) {}
static inline void rmlui_network_lobby_join_room(void) {}
static inline void rmlui_network_lobby_room_scroll(int delta) { (void)delta; }
static inline bool rmlui_network_lobby_has_pending_room(void) { return false; }
static inline const char* rmlui_network_lobby_consume_pending_room(void) { return ""; }

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
