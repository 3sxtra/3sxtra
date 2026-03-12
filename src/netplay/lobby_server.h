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
    char country[4];          // ISO 3166-1 alpha-2 (e.g. "US", "JP"), server-derived
    char room_code[16];
    char connect_to[16];
    char status[16];          // "searching" or "idle"
    char connection_type[8];  // "wifi", "wired", or "unknown"
    int rtt_ms;               // Server RTT in ms (-1 = unknown)
} LobbyPlayer;

/// Initialize lobby server client — reads URL and key from config.ini.
/// Must be called after Config_Init().
void LobbyServer_Init(void);

/// Returns true if the lobby server is configured (URL and key both set).
bool LobbyServer_IsConfigured(void);

/// Register or update player presence on the lobby server.
/// connect_to may be NULL or "" (no connection intent) or a target room code.
/// rtt_ms: our measured RTT to the lobby server in ms (-1 if unknown).
/// connection_type: "wifi", "wired", or "unknown" (NULL defaults to "unknown").
bool LobbyServer_UpdatePresence(const char* player_id, const char* display_name, const char* region,
                                const char* room_code, const char* connect_to, int rtt_ms,
                                const char* connection_type);

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

/// Report a declined invite to enable server-side rate limiting.
/// Returns true on success.
bool LobbyServer_DeclineInvite(const char* player_id, const char* declined_player_id);

// === Phase 2: Match Reporting ===

typedef struct {
    char player_id[64];
    char opponent_id[64];
    char winner_id[64];
    int player_char;      // Character index (0-19)
    int opponent_char;
    int rounds;           // Total rounds played (e.g. 3 for a 2-1 win)
} MatchResult;

/// Submit a match result to the lobby server for cross-validation.
/// Both players must submit; server only records if they agree on the winner.
bool LobbyServer_ReportMatch(const MatchResult* result);

typedef struct {
    int wins;
    int losses;
    int disconnects;
    float rating;
    float rd;             // Rating Deviation (Glicko-2)
} PlayerStats;

/// Get stats for a player from the server. Returns true on success.
bool LobbyServer_GetPlayerStats(const char* player_id, PlayerStats* out);

#ifdef __cplusplus
}
#endif

#endif
