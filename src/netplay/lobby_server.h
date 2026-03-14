#ifndef NETPLAY_LOBBY_SERVER_H
#define NETPLAY_LOBBY_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char player_id[64];
    char display_name[32];
    char region[8];
    char country[4]; // ISO 3166-1 alpha-2 (e.g. "US", "JP"), server-derived
    char room_code[16];
    char connect_to[16];
    char status[16];         // "searching" or "idle"
    char connection_type[8]; // "wifi", "wired", or "unknown"
    int rtt_ms;              // Server RTT in ms (-1 = unknown)
    int ft;                  // First-To match mode (1=unranked, 2=FT2, etc.)
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
                                const char* room_code, const char* connect_to, int rtt_ms, const char* connection_type,
                                int ft);

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
    int player_char; // Character index (0-19)
    int opponent_char;
    int rounds; // Total rounds played (e.g. 3 for a 2-1 win)
    char source[16]; // "casual" or "ranked"
    int ft;          // First-to value for this session (e.g. 2 for FT2)
} MatchResult;

/// Submit a match result to the lobby server for cross-validation.
/// Both players must submit; server only records if they agree on the winner.
/// If out_match_id is provided, it receives the server-assigned match ID (on successful recording).
bool LobbyServer_ReportMatch(const MatchResult* result, int* out_match_id);

/// Upload a replay file for a successfully recorded match.
/// The match_id must be the one returned by LobbyServer_ReportMatch.
bool LobbyServer_UploadReplay(int match_id, const void* replay_data, size_t replay_size);

/// Report a mid-match disconnect (ragequit). The remaining player calls this
/// when GekkoPlayerDisconnected fires before a natural match conclusion.
/// Server-side: if only one player submits a match result within 30s, the
/// server may auto-record the result and increment the absent player's
/// disconnect counter.
bool LobbyServer_ReportDisconnect(const char* player_id, const char* opponent_id);

typedef struct {
    int wins;
    int losses;
    int disconnects;
    float rating;
    float rd;      // Rating Deviation (Glicko-2)
    char tier[16]; // e.g. "bronze", "silver", "gold"
} PlayerStats;

/// Get stats for a player from the server. Returns true on success.
bool LobbyServer_GetPlayerStats(const char* player_id, PlayerStats* out);

// === Phase 3: Leaderboards ===

typedef struct {
    int rank;
    char player_id[64];
    char display_name[32];
    int wins;
    int losses;
    int disconnects;
    float rating;
    char tier[16];
} LeaderboardEntry;

/// Fetch a page of the leaderboard. Returns entry count (up to max_entries).
/// page is 0-indexed. *out_total receives total player count (may be NULL).
/// Returns -1 on error.
int LobbyServer_GetLeaderboard(LeaderboardEntry* out, int max_entries, int page, int* out_total);

// === Room Discovery ===

typedef struct {
    char code[8];     // 4-char room code + null
    char name[32];    // Room display name
    int player_count; // Current player count (1-8)
    int ft;           // First-To match mode (1=unranked, default for region rooms)
} RoomListItem;

/// Fetch list of active rooms from the lobby server.
/// Returns number of rooms written to out_rooms (up to max_rooms).
int LobbyServer_ListRooms(RoomListItem* out_rooms, int max_rooms);

// === Phase 5: Casual Lobbies (8-Player Rooms) ===

#define MAX_ROOM_PLAYERS 8
#define MAX_CHAT_MESSAGES 50

typedef struct {
    uint64_t id;
    char sender_id[64];
    char sender_name[32];
    char text[128];
} ChatMessage;

typedef struct {
    char player_id[64];
    char display_name[32];
    char region[16];
    char country[4]; // ISO 3166-1 alpha-2 (e.g. "US", "JP"), server-derived
} RoomPlayer;

typedef struct {
    char id[8];
    char name[32];
    char host[64];

    RoomPlayer players[MAX_ROOM_PLAYERS];
    int player_count;

    char queue[MAX_ROOM_PLAYERS][64]; // Array of player_ids
    int queue_count;

    char match_p1[64];
    char match_p2[64];
    int match_active;

    ChatMessage chat[MAX_CHAT_MESSAGES];
    int chat_count;
    int ft; // First-To match mode for this room
} RoomState;

// Sync room logic
bool LobbyServer_CreateRoom(const char* name, int ft, RoomState* out_room);
bool LobbyServer_JoinRoom(const char* room_code, RoomState* out_room);
bool LobbyServer_LeaveRoom(const char* room_code);
bool LobbyServer_JoinQueue(const char* room_code);
bool LobbyServer_LeaveQueue(const char* room_code);
bool LobbyServer_SendChat(const char* room_code, const char* text);

/// Fetch room state without side effects (read-only GET).
bool LobbyServer_GetRoomState(const char* room_code, RoomState* out);

// === SSE Streaming Client ===

typedef enum {
    SSE_EVENT_NONE = 0,
    SSE_EVENT_SYNC,          // Full room state sync (on connect)
    SSE_EVENT_JOIN,          // Player joined
    SSE_EVENT_LEAVE,         // Player left
    SSE_EVENT_CHAT,          // New chat message
    SSE_EVENT_QUEUE_UPDATE,  // Queue changed
    SSE_EVENT_HOST_MIGRATED, // Host changed
    SSE_EVENT_MATCH_PROPOSE, // Phase 6: Match proposed (await accept/decline)
    SSE_EVENT_MATCH_START,   // Match started (both accepted)
    SSE_EVENT_MATCH_DECLINE, // Phase 6: Match proposal declined or timed out
    SSE_EVENT_MATCH_END      // Match ended (winner stays on, loser to back)
} SSEEventType;

typedef struct {
    SSEEventType type;
    RoomState room;       // Populated on SYNC events
    ChatMessage chat_msg; // Populated on CHAT events
    char player_id[64];   // Populated on JOIN/LEAVE events
    char display_name[32];
    char match_winner_id[64]; // Populated on MATCH_END events
    char match_loser_id[64];  // Populated on MATCH_END events
    // Phase 6: Match proposal data (populated on MATCH_PROPOSE/DECLINE)
    char propose_p1_id[64];
    char propose_p1_name[32];
    char propose_p1_conn_type[16];
    int propose_p1_rtt_ms;
    char propose_p1_room_code[32];
    char propose_p1_region[8];
    char propose_p2_id[64];
    char propose_p2_name[32];
    char propose_p2_conn_type[16];
    int propose_p2_rtt_ms;
    char propose_p2_room_code[32];
    char propose_p2_region[8];
    char propose_decliner_id[64]; // Populated on MATCH_DECLINE
    char propose_reason[16];      // "declined" or "timeout" (MATCH_DECLINE)
    int propose_ft;               // FT value for proposed match (from room)
} SSEEvent;

/// Start SSE connection to a room (spawns background thread).
/// Only one SSE connection at a time — call SSEDisconnect first if switching rooms.
bool LobbyServer_SSEConnect(const char* room_code);

/// Stop the SSE connection and join the background thread.
void LobbyServer_SSEDisconnect(void);

/// Poll for the next SSE event. Returns the event type, or SSE_EVENT_NONE if idle.
/// Copies event data into out_event if non-NULL.
SSEEventType LobbyServer_SSEPoll(SSEEvent* out_event);

/// Returns true if the SSE connection is currently active.
bool LobbyServer_SSEIsConnected(void);

/// Report match end to the server for "Winner Stays On" rotation.
/// Called when the game engine detects a match conclusion.
bool LobbyServer_ReportMatchEnd(const char* room_code, const char* winner_id);

// === Phase 6: Match Accept/Decline ===

/// Accept a proposed match. Returns true if the server acknowledged.
bool LobbyServer_AcceptMatch(const char* room_code);

/// Decline a proposed match. Returns true if the server acknowledged.
bool LobbyServer_DeclineMatch(const char* room_code);

#ifdef __cplusplus
}
#endif

#endif
