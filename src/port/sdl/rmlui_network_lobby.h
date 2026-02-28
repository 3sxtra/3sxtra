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

void rmlui_network_lobby_init(void);
void rmlui_network_lobby_update(void);
void rmlui_network_lobby_show(void);
void rmlui_network_lobby_hide(void);
void rmlui_network_lobby_shutdown(void);

#ifdef __cplusplus
}
#endif
