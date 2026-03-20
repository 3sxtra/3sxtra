#pragma once
#include "sf33rd/Source/Game/menu/menu.h"

void NetLobby_DrawIncomingPopup(const char* name, const char* region, int ping);
void NetLobby_DrawOutgoingPopup(const char* name, int ping);
void Network_Lobby(struct _TASK* task_ptr);

extern int g_lobby_peer_idx;
extern int g_net_peer_idx;
