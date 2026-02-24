#ifndef NETPLAY_LOBBY_SERVER_H
#define NETPLAY_LOBBY_SERVER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char player_id[64];
    char display_name[32];
    char region[8];
    char room_code[16];
    char connect_to[16];
} LobbyPlayer;

/// Initialize lobby server client — reads URL and key from config.ini.
/// Must be called after Config_Init().
void LobbyServer_Init(void);

/// Returns true if the lobby server is configured (URL and key both set).
bool LobbyServer_IsConfigured(void);

/// Register or update player presence on the lobby server.
/// connect_to may be NULL or "" (no connection intent) or a target room code.
bool LobbyServer_UpdatePresence(const char* player_id, const char* display_name, const char* region,
                                const char* room_code, const char* connect_to);

/// Mark player as searching for a match.
bool LobbyServer_StartSearching(const char* player_id);

/// Mark player as no longer searching.
bool LobbyServer_StopSearching(const char* player_id);

/// Get list of currently searching players (optionally filtered by region).
/// Returns number of players written to out_players (up to max_players).
/// region_filter may be NULL or "" to get all regions.
int LobbyServer_GetSearching(LobbyPlayer* out_players, int max_players, const char* region_filter);

/// Remove this player from the lobby server entirely.
bool LobbyServer_Leave(const char* player_id);

#ifdef __cplusplus
}
#endif

#endif
