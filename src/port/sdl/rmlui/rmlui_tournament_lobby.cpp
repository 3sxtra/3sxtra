/**
 * @file rmlui_tournament_lobby.cpp
 * @brief RmlUi Tournament Lobby data model.
 *
 * Variant of the casual lobby for tournament-type rooms. Adds bracket
 * display, multi-match selector, TO controls, and tournament status.
 * Reuses the same SSE polling + async HTTP patterns as rmlui_casual_lobby.cpp.
 */

#include "port/sdl/rmlui/rmlui_tournament_lobby.h"
#include "game_state.h"
#include "port/sdl/rmlui/rmlui_game_hud.h"
#include "port/sdl/rmlui/rmlui_ingame_chat.h"
#include "port/sdl/rmlui/rmlui_network_lobby.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include "netplay/identity.h"
#include "netplay/lobby_server.h"
#include "netplay/netplay.h"
#include "netplay/ping_probe.h"
#include "port/menu_screen.h"
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/menu/menu.h"
#include "structs.h"
} // extern "C"

#include <RmlUi/Core/Input.h>

static bool s_chat_open = false;
static bool s_wants_leave = false;

// ─── Data Structs for RmlUi ──────────────────────────────────────

struct RmlBracketEntry {
    int index;
    int round;
    int position;
    Rml::String p1_name;
    Rml::String p2_name;
    Rml::String winner_name;
    Rml::String bracket_side; // "W", "L", "GF" (double elim)
    bool completed;
};

struct RmlActiveMatch {
    int index;
    int match_index;
    Rml::String p1_name;
    Rml::String p2_name;
    Rml::String bracket_side; // "W", "L", "GF" (double elim)
    bool active;
    bool is_ours; // true if we are p1 or p2
};

struct RmlRoomPlayer {
    int index;
    Rml::String name;
    Rml::String country;
    bool is_self;
    bool is_playing;
    bool is_queued;
};

struct RmlChatMessage {
    int index;
    Rml::String sender;
    Rml::String text;
};

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;
static bool s_is_visible = false;

// Backend C state
static RoomState s_room_state;
static TournamentState s_tournament_state;
static Rml::String s_room_code;
static Rml::String s_room_name;
static Rml::String s_room_password;
static int s_room_visibility = 0;
static Rml::String s_qr_image_path;
static Rml::String s_my_id;

static int s_player_count = 0;
static int s_max_players = 16;
static Rml::String s_status_text;

// Tournament-specific
static int s_tournament_started = 0;
static int s_tournament_paused = 0;
static int s_tournament_round = 0;
static int s_tournament_total_rounds = 0;
static Rml::String s_tournament_format_label;
static bool s_is_host = false;

// Bracket display
static std::vector<RmlBracketEntry> s_bracket;
static int s_bracket_display_count = 0;

// Active matches
static std::vector<RmlActiveMatch> s_active_matches;
static int s_active_match_count = 0;
static int s_match_selector_idx = 0;

// Player list
static std::vector<RmlRoomPlayer> s_players;

// Chat
static std::vector<RmlChatMessage> s_chat;
static int s_chat_display_count = 0;
static Rml::String s_chat_input;
static bool s_is_typing = false;

// Playing/spectating
static bool s_is_playing = false;

// Navigation
static int s_cursor_x = 0; // 0=left (bracket/actions), 1=right (chat)
static int s_cursor_y = 0; // context-dependent

// DQ player selector (Fix 3)
static int s_dq_target_idx = 0;
static Rml::String s_dq_target_name;

// Override result selector (Fix 4)
static int s_override_winner = 0; // 0=p1, 1=p2
static Rml::String s_override_label;

static Uint64 s_last_poll_time = 0;
static bool s_match_ended_pending_reshow = false;

// Match proposal (reuse same pattern as casual lobby)
static int s_proposal_active = 0;
static Rml::String s_proposal_opponent_name;
static int s_proposal_opponent_ping = -1;
static Rml::String s_proposal_opponent_conn_type;
#define PROPOSAL_TIMEOUT_SEC 30
static int s_proposal_countdown = PROPOSAL_TIMEOUT_SEC;
static int s_proposal_countdown_pct = 100;
static int s_proposal_cursor = 0;
static Uint64 s_proposal_start_time = 0;
static char s_proposal_opponent_room_code[64] = { 0 };
static char s_proposal_opponent_player_id[64] = { 0 };
static int s_proposal_ft = 1;
static bool s_proposal_we_are_p1 = false;
static int s_proposal_match_index = 0;

// Async accept/decline
static SDL_AtomicInt s_async_match_active = { 0 };

struct AsyncMatchData {
    char room_code[16];
    int action; // 1 = accept, 2 = decline
};

static int SDLCALL async_match_action_fn(void* data) {
    AsyncMatchData* d = (AsyncMatchData*)data;
    if (d->action == 1)
        LobbyServer_AcceptMatch(d->room_code);
    else if (d->action == 2)
        LobbyServer_DeclineMatch(d->room_code);
    free(d);
    SDL_SetAtomicInt(&s_async_match_active, 0);
    return 0;
}

static void AsyncMatchAction(const char* room_code, int action) {
    if (SDL_GetAtomicInt(&s_async_match_active) != 0)
        return;
    SDL_SetAtomicInt(&s_async_match_active, 1);
    AsyncMatchData* d = (AsyncMatchData*)malloc(sizeof(AsyncMatchData));
    if (!d) {
        SDL_SetAtomicInt(&s_async_match_active, 0);
        return;
    }
    snprintf(d->room_code, sizeof(d->room_code), "%s", room_code);
    d->action = action;
    SDL_Thread* t = SDL_CreateThread(async_match_action_fn, "AsyncTMatch", d);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(d);
        SDL_SetAtomicInt(&s_async_match_active, 0);
    }
}

// Async TO actions
static SDL_AtomicInt s_async_to_active = { 0 };

struct AsyncTOData {
    char room_code[16];
    int action; // 1=start, 2=pause, 3=resume, 4=dq, 5=override
    char target_player[64];
    int match_index;    // for override
    char winner_id[64]; // for override
};

static int SDLCALL async_to_action_fn(void* data) {
    AsyncTOData* d = (AsyncTOData*)data;
    switch (d->action) {
    case 1:
        LobbyServer_StartBracket(d->room_code);
        break;
    case 2:
        LobbyServer_BracketPause(d->room_code, true);
        break;
    case 3:
        LobbyServer_BracketPause(d->room_code, false);
        break;
    case 4:
        LobbyServer_BracketDQ(d->room_code, d->target_player);
        break;
    case 5:
        LobbyServer_BracketOverride(d->room_code, d->match_index, d->winner_id);
        break;
    case 6:
        LobbyServer_BracketRestartMatch(d->room_code, d->match_index);
        break;
    }
    free(d);
    SDL_SetAtomicInt(&s_async_to_active, 0);
    return 0;
}

static void AsyncTOAction(const char* room_code, int action, const char* target, int match_index,
                          const char* winner_id) {
    if (SDL_GetAtomicInt(&s_async_to_active) != 0)
        return;
    SDL_SetAtomicInt(&s_async_to_active, 1);
    AsyncTOData* d = (AsyncTOData*)calloc(1, sizeof(AsyncTOData));
    if (!d) {
        SDL_SetAtomicInt(&s_async_to_active, 0);
        return;
    }
    snprintf(d->room_code, sizeof(d->room_code), "%s", room_code);
    d->action = action;
    if (target)
        snprintf(d->target_player, sizeof(d->target_player), "%s", target);
    d->match_index = match_index;
    if (winner_id)
        snprintf(d->winner_id, sizeof(d->winner_id), "%s", winner_id);
    SDL_Thread* t = SDL_CreateThread(async_to_action_fn, "AsyncTO", d);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(d);
        SDL_SetAtomicInt(&s_async_to_active, 0);
    }
}

// ─── Forward Declarations ────────────────────────────────────────
static void refresh_room_state_from_server(void);
static void apply_room_state_to_model(void);
static void apply_tournament_state_to_model(void);

static const char* format_label(int fmt) {
    switch (fmt) {
    case TOURNAMENT_SINGLE_ELIM:
        return "SINGLE ELIM";
    case TOURNAMENT_DOUBLE_ELIM:
        return "DOUBLE ELIM";
    case TOURNAMENT_ROUND_ROBIN:
        return "ROUND ROBIN";
    case TOURNAMENT_SWISS:
        return "SWISS";
    default:
        return "TOURNAMENT";
    }
}

// ─── Lazy Init ───────────────────────────────────────────────────
static void do_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("tournament_lobby");
    if (!ctor)
        return;

    // Register structs
    if (auto h = ctor.RegisterStruct<RmlBracketEntry>()) {
        h.RegisterMember("index", &RmlBracketEntry::index);
        h.RegisterMember("round", &RmlBracketEntry::round);
        h.RegisterMember("position", &RmlBracketEntry::position);
        h.RegisterMember("p1_name", &RmlBracketEntry::p1_name);
        h.RegisterMember("p2_name", &RmlBracketEntry::p2_name);
        h.RegisterMember("winner_name", &RmlBracketEntry::winner_name);
        h.RegisterMember("bracket_side", &RmlBracketEntry::bracket_side);
        h.RegisterMember("completed", &RmlBracketEntry::completed);
    }
    ctor.RegisterArray<std::vector<RmlBracketEntry>>();
    ctor.Bind("bracket_entries", &s_bracket);

    if (auto h = ctor.RegisterStruct<RmlActiveMatch>()) {
        h.RegisterMember("index", &RmlActiveMatch::index);
        h.RegisterMember("match_index", &RmlActiveMatch::match_index);
        h.RegisterMember("p1_name", &RmlActiveMatch::p1_name);
        h.RegisterMember("p2_name", &RmlActiveMatch::p2_name);
        h.RegisterMember("bracket_side", &RmlActiveMatch::bracket_side);
        h.RegisterMember("active", &RmlActiveMatch::active);
        h.RegisterMember("is_ours", &RmlActiveMatch::is_ours);
    }
    ctor.RegisterArray<std::vector<RmlActiveMatch>>();
    ctor.Bind("active_matches", &s_active_matches);

    if (auto h = ctor.RegisterStruct<RmlRoomPlayer>()) {
        h.RegisterMember("index", &RmlRoomPlayer::index);
        h.RegisterMember("name", &RmlRoomPlayer::name);
        h.RegisterMember("country", &RmlRoomPlayer::country);
        h.RegisterMember("is_self", &RmlRoomPlayer::is_self);
        h.RegisterMember("is_playing", &RmlRoomPlayer::is_playing);
        h.RegisterMember("is_queued", &RmlRoomPlayer::is_queued);
    }
    ctor.RegisterArray<std::vector<RmlRoomPlayer>>();
    ctor.Bind("room_players", &s_players);

    if (auto h = ctor.RegisterStruct<RmlChatMessage>()) {
        h.RegisterMember("index", &RmlChatMessage::index);
        h.RegisterMember("sender", &RmlChatMessage::sender);
        h.RegisterMember("text", &RmlChatMessage::text);
    }
    ctor.RegisterArray<std::vector<RmlChatMessage>>();
    ctor.Bind("chat_messages", &s_chat);
    ctor.BindFunc("chat_count", [](Rml::Variant& v) { v = s_chat_display_count; });

    // Scalars
    ctor.Bind("room_code", &s_room_code);
    ctor.Bind("room_name", &s_room_name);
    ctor.Bind("room_password", &s_room_password);
    ctor.Bind("room_visibility", &s_room_visibility);
    ctor.Bind("qr_image_path", &s_qr_image_path);
    ctor.Bind("player_count", &s_player_count);
    ctor.Bind("max_players", &s_max_players);
    ctor.Bind("status_text", &s_status_text);

    // Tournament
    ctor.Bind("tournament_started", &s_tournament_started);
    ctor.Bind("tournament_paused", &s_tournament_paused);
    ctor.Bind("tournament_round", &s_tournament_round);
    ctor.Bind("tournament_total_rounds", &s_tournament_total_rounds);
    ctor.Bind("tournament_format_label", &s_tournament_format_label);
    ctor.Bind("is_host", &s_is_host);
    ctor.BindFunc("bracket_count", [](Rml::Variant& v) { v = s_bracket_display_count; });
    ctor.BindFunc("active_match_count", [](Rml::Variant& v) { v = s_active_match_count; });
    ctor.Bind("match_selector_idx", &s_match_selector_idx);

    // Playing/spectating
    ctor.Bind("is_playing", &s_is_playing);

    // Chat
    ctor.Bind("chat_input", &s_chat_input);
    ctor.Bind("is_typing", &s_is_typing);

    // Navigation
    ctor.Bind("cursor_x", &s_cursor_x);
    ctor.Bind("cursor_y", &s_cursor_y);

    // Match proposal
    ctor.Bind("proposal_active", &s_proposal_active);
    ctor.Bind("proposal_opponent_name", &s_proposal_opponent_name);
    ctor.Bind("proposal_opponent_ping", &s_proposal_opponent_ping);
    ctor.Bind("proposal_opponent_conn_type", &s_proposal_opponent_conn_type);
    ctor.Bind("proposal_countdown", &s_proposal_countdown);
    ctor.Bind("proposal_countdown_pct", &s_proposal_countdown_pct);
    ctor.Bind("proposal_cursor", &s_proposal_cursor);

    // DQ player selector
    ctor.Bind("dq_target_name", &s_dq_target_name);
    ctor.Bind("dq_target_idx", &s_dq_target_idx);

    // Override result
    ctor.Bind("override_label", &s_override_label);
    ctor.Bind("override_winner", &s_override_winner);

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    memset(&s_room_state, 0, sizeof(s_room_state));
    memset(&s_tournament_state, 0, sizeof(s_tournament_state));
    s_my_id = Identity_GetPlayerId();
}

extern "C" void rmlui_tournament_lobby_init(void) {
    do_init();
}

// ─── Apply state to model ────────────────────────────────────────

static void apply_room_state_to_model(void) {
    if (!s_model_handle)
        return;

    s_room_code = s_room_state.id;
    s_room_name = s_room_state.name;
    s_room_password = rmlui_network_lobby_get_active_password();
    s_room_visibility = s_room_state.visibility;
    const char* qr_path = rmlui_network_lobby_get_qr_image_path();
    s_qr_image_path = (qr_path && qr_path[0]) ? Rml::String(qr_path) : Rml::String();
    s_player_count = s_room_state.player_count;

    s_is_host = (strcmp(s_room_state.host, s_my_id.c_str()) == 0);

    // Player list — grow-only
    int new_player_count = s_room_state.player_count;
    if ((size_t)new_player_count > s_players.size())
        s_players.resize(new_player_count);
    for (int i = 0; i < new_player_count; i++) {
        s_players[i].index = i;
        s_players[i].name = s_room_state.players[i].display_name;
        s_players[i].country = s_room_state.players[i].country;
        for (auto& ch : s_players[i].country)
            ch = (char)tolower((unsigned char)ch);
        s_players[i].is_self = (s_my_id == s_room_state.players[i].player_id);
        s_players[i].is_playing = false; // Updated from tournament matches
        s_players[i].is_queued = false;
    }
    for (size_t i = new_player_count; i < s_players.size(); i++) {
        s_players[i].index = (int)i;
        s_players[i].name = "";
        s_players[i].country = "";
        s_players[i].is_self = false;
        s_players[i].is_playing = false;
        s_players[i].is_queued = false;
    }

    // Chat — grow-only
    int new_chat_count = s_room_state.chat_count;
    if ((size_t)new_chat_count > s_chat.size())
        s_chat.resize(new_chat_count);
    for (int i = 0; i < new_chat_count; i++) {
        s_chat[i].index = i;
        s_chat[i].sender = s_room_state.chat[i].sender_name;
        s_chat[i].text = s_room_state.chat[i].text;
    }
    for (size_t i = new_chat_count; i < s_chat.size(); i++) {
        s_chat[i].index = (int)i;
        s_chat[i].sender = "";
        s_chat[i].text = "";
    }
    s_chat_display_count = new_chat_count;

    s_model_handle.DirtyVariable("room_code");
    s_model_handle.DirtyVariable("room_name");
    s_model_handle.DirtyVariable("room_password");
    s_model_handle.DirtyVariable("room_visibility");
    s_model_handle.DirtyVariable("qr_image_path");
    s_model_handle.DirtyVariable("player_count");
    s_model_handle.DirtyVariable("is_host");
    s_model_handle.DirtyVariable("room_players");
    s_model_handle.DirtyVariable("chat_messages");
    s_model_handle.DirtyVariable("chat_count");
}

static void apply_tournament_state_to_model(void) {
    if (!s_model_handle)
        return;

    s_tournament_started = s_tournament_state.tournament_started;
    s_tournament_paused = s_tournament_state.tournament_paused;
    s_tournament_round = s_tournament_state.tournament_round;
    s_tournament_total_rounds = s_tournament_state.tournament_total_rounds;
    s_tournament_format_label = format_label(s_tournament_state.tournament_format);

    // Bracket entries — grow-only
    int new_bracket_count = s_tournament_state.bracket_size;
    if ((size_t)new_bracket_count > s_bracket.size())
        s_bracket.resize(new_bracket_count);
    for (int i = 0; i < new_bracket_count; i++) {
        const BracketEntry& be = s_tournament_state.bracket[i];
        s_bracket[i].index = i;
        s_bracket[i].round = be.round;
        s_bracket[i].position = be.position;
        s_bracket[i].p1_name = be.player1_name[0] ? be.player1_name : "TBD";
        s_bracket[i].p2_name = be.player2_name[0] ? be.player2_name : "TBD";
        s_bracket[i].winner_name =
            be.winner_id[0] ? (strcmp(be.winner_id, be.player1_id) == 0 ? be.player1_name : be.player2_name) : "";
        s_bracket[i].bracket_side = be.bracket_side[0] ? be.bracket_side : "";
        s_bracket[i].completed = be.completed != 0;
    }
    for (size_t i = new_bracket_count; i < s_bracket.size(); i++) {
        s_bracket[i].index = (int)i;
        s_bracket[i].p1_name = "";
        s_bracket[i].p2_name = "";
        s_bracket[i].winner_name = "";
        s_bracket[i].bracket_side = "";
        s_bracket[i].completed = false;
    }
    s_bracket_display_count = new_bracket_count;

    // Active matches — grow-only
    int new_match_count = s_tournament_state.match_count;
    if ((size_t)new_match_count > s_active_matches.size())
        s_active_matches.resize(new_match_count);
    for (int i = 0; i < new_match_count; i++) {
        const RoomMatch& rm = s_tournament_state.matches[i];
        s_active_matches[i].index = i;
        s_active_matches[i].match_index = rm.match_index;
        s_active_matches[i].bracket_side = rm.bracket_side[0] ? rm.bracket_side : "";
        s_active_matches[i].active = rm.active != 0;
        s_active_matches[i].is_ours = (strcmp(rm.p1, s_my_id.c_str()) == 0 || strcmp(rm.p2, s_my_id.c_str()) == 0);

        // Resolve display names from player list
        s_active_matches[i].p1_name = rm.p1;
        s_active_matches[i].p2_name = rm.p2;
        for (int p = 0; p < s_room_state.player_count; p++) {
            if (strcmp(s_room_state.players[p].player_id, rm.p1) == 0)
                s_active_matches[i].p1_name = s_room_state.players[p].display_name;
            if (strcmp(s_room_state.players[p].player_id, rm.p2) == 0)
                s_active_matches[i].p2_name = s_room_state.players[p].display_name;
        }

        // Mark these players as playing in the player list
        for (size_t pl = 0; pl < s_players.size(); pl++) {
            if (s_players[pl].name == s_active_matches[i].p1_name ||
                s_players[pl].name == s_active_matches[i].p2_name) {
                s_players[pl].is_playing = true;
            }
        }
    }
    for (size_t i = new_match_count; i < s_active_matches.size(); i++) {
        s_active_matches[i].index = (int)i;
        s_active_matches[i].p1_name = "";
        s_active_matches[i].p2_name = "";
        s_active_matches[i].active = false;
        s_active_matches[i].is_ours = false;
    }
    s_active_match_count = new_match_count;

    if (s_match_selector_idx >= s_active_match_count && s_active_match_count > 0)
        s_match_selector_idx = s_active_match_count - 1;
    if (s_active_match_count == 0)
        s_match_selector_idx = 0;

    s_model_handle.DirtyVariable("tournament_started");
    s_model_handle.DirtyVariable("tournament_paused");
    s_model_handle.DirtyVariable("tournament_round");
    s_model_handle.DirtyVariable("tournament_total_rounds");
    s_model_handle.DirtyVariable("tournament_format_label");
    s_model_handle.DirtyVariable("bracket_entries");
    s_model_handle.DirtyVariable("bracket_count");
    s_model_handle.DirtyVariable("active_matches");
    s_model_handle.DirtyVariable("active_match_count");
    s_model_handle.DirtyVariable("match_selector_idx");
    s_model_handle.DirtyVariable("room_players"); // re-dirty for is_playing

    // ── Update HUD Match Banner Data ──
    bool match_found = false;
    RoomMatch target_match = {};

    // Find our active match, or the one we selected to view
    for (int i = 0; i < s_tournament_state.match_count; i++) {
        if (s_tournament_state.matches[i].active) {
            bool is_ours = (strcmp(s_tournament_state.matches[i].p1, s_my_id.c_str()) == 0 ||
                            strcmp(s_tournament_state.matches[i].p2, s_my_id.c_str()) == 0);
            if (is_ours) {
                target_match = s_tournament_state.matches[i];
                match_found = true;
                break;
            }
        }
    }

    if (match_found && s_room_state.ft > 0) {
        g_match_banner_visible = true;
        g_match_ft = s_room_state.ft;

        // Resolve names and country from players list
        for (int p = 0; p < s_room_state.player_count; p++) {
            if (strcmp(s_room_state.players[p].player_id, target_match.p1) == 0) {
                snprintf(g_match_p1_name, sizeof(g_match_p1_name), "%s", s_room_state.players[p].display_name);
                snprintf(g_match_p1_country, sizeof(g_match_p1_country), "%s", s_room_state.players[p].country);
            }
            if (strcmp(s_room_state.players[p].player_id, target_match.p2) == 0) {
                snprintf(g_match_p2_name, sizeof(g_match_p2_name), "%s", s_room_state.players[p].display_name);
                snprintf(g_match_p2_country, sizeof(g_match_p2_country), "%s", s_room_state.players[p].country);
            }
        }
    } else {
        g_match_banner_visible = false;
    }
}

static SDL_AtomicInt s_async_refresh_active = { 0 };
static bool s_async_refresh_completed = false;
static bool s_async_refresh_success = false;
static bool s_async_refresh_pending_retrigger = false;
static RoomState s_async_room_state_buffer;
static TournamentState s_async_tournament_state_buffer;

struct AsyncRefreshData {
    char room_code[16];
};

static int SDLCALL async_refresh_fn(void* data) {
    AsyncRefreshData* d = (AsyncRefreshData*)data;
    bool success = LobbyServer_GetRoomState(d->room_code, &s_async_room_state_buffer);
    s_async_refresh_success = success;
    if (success) {
        memset(&s_async_tournament_state_buffer, 0, sizeof(TournamentState));
        LobbyServer_GetBracket(d->room_code, &s_async_tournament_state_buffer);
    }
    s_async_refresh_completed = true;
    free(d);
    return 0;
}

static void refresh_room_state_from_server(void) {
    if (s_room_code.empty())
        return;

    if (SDL_GetAtomicInt(&s_async_refresh_active) != 0) {
        s_async_refresh_pending_retrigger = true;
        return;
    }

    SDL_SetAtomicInt(&s_async_refresh_active, 1);
    s_async_refresh_completed = false;

    AsyncRefreshData* d = (AsyncRefreshData*)malloc(sizeof(AsyncRefreshData));
    if (!d) {
        SDL_SetAtomicInt(&s_async_refresh_active, 0);
        return;
    }
    snprintf(d->room_code, sizeof(d->room_code), "%s", s_room_code.c_str());

    SDL_Thread* t = SDL_CreateThread(async_refresh_fn, "AsyncRefreshT", d);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(d);
        SDL_SetAtomicInt(&s_async_refresh_active, 0);
    }
}

// ─── Update loop ─────────────────────────────────────────────────
extern "C" void rmlui_tournament_lobby_update(void) {
    if (!s_model_registered) {
        do_init();
        if (!s_model_registered)
            return;
    }
    if (!s_is_visible)
        return;

    if (s_async_refresh_completed) {
        s_async_refresh_completed = false;

        if (s_async_refresh_success) {
            memcpy(&s_room_state, &s_async_room_state_buffer, sizeof(RoomState));
            apply_room_state_to_model();
            memcpy(&s_tournament_state, &s_async_tournament_state_buffer, sizeof(TournamentState));
            apply_tournament_state_to_model();
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "[TournamentLobby] Room %s no longer exists, returning to lobby",
                        s_room_code.c_str());
            s_status_text = "Room closed.";
            if (s_model_handle)
                s_model_handle.DirtyVariable("status_text");
            rmlui_tournament_lobby_hide();
            rmlui_network_lobby_show();
        }

        SDL_SetAtomicInt(&s_async_refresh_active, 0);

        if (s_async_refresh_pending_retrigger) {
            s_async_refresh_pending_retrigger = false;
            refresh_room_state_from_server();
        }
    }

    // Deferred overlay re-show
    if (s_match_ended_pending_reshow) {
        NetplaySessionState ns = Netplay_GetSessionState();
        if (ns == NETPLAY_SESSION_IDLE || ns == NETPLAY_SESSION_LOBBY) {
            s_match_ended_pending_reshow = false;
            s_is_playing = false;
            s_model_handle.DirtyVariable("is_playing");
            rmlui_wrapper_show_game_document("tournament_lobby");
        } else {
            return;
        }
    }

    // Drain SSE events
    SSEEvent sse_evt;
    for (int sse_i = 0; sse_i < 16; sse_i++) {
        SSEEventType sse_type = LobbyServer_SSEPoll(&sse_evt);
        if (sse_type == SSE_EVENT_NONE)
            break;

        if (sse_type == SSE_EVENT_SYNC) {
            memcpy(&s_room_state, &sse_evt.room, sizeof(RoomState));
            apply_room_state_to_model();
            // Fetch bracket on sync
            if (LobbyServer_GetBracket(s_room_code.c_str(), &s_tournament_state))
                apply_tournament_state_to_model();
        } else if (sse_type == SSE_EVENT_CHAT) {
            int new_idx = s_chat_display_count;
            if (new_idx >= (int)s_chat.size()) {
                s_chat.push_back({ new_idx, sse_evt.chat_msg.sender_name, sse_evt.chat_msg.text });
            } else {
                s_chat[new_idx].index = new_idx;
                s_chat[new_idx].sender = sse_evt.chat_msg.sender_name;
                s_chat[new_idx].text = sse_evt.chat_msg.text;
            }
            s_chat_display_count++;
            if (s_chat_display_count > MAX_CHAT_MESSAGES) {
                for (int i = 1; i < s_chat_display_count; i++) {
                    s_chat[i - 1].sender = s_chat[i].sender;
                    s_chat[i - 1].text = s_chat[i].text;
                }
                s_chat_display_count--;
                s_chat[s_chat_display_count].sender = "";
                s_chat[s_chat_display_count].text = "";
            }
            s_model_handle.DirtyVariable("chat_messages");
            s_model_handle.DirtyVariable("chat_count");
        } else if (sse_type == SSE_EVENT_JOIN || sse_type == SSE_EVENT_LEAVE || sse_type == SSE_EVENT_QUEUE_UPDATE ||
                   sse_type == SSE_EVENT_HOST_MIGRATED) {
            refresh_room_state_from_server();
        } else if (sse_type == SSE_EVENT_BRACKET_UPDATE) {
            memcpy(&s_tournament_state, &sse_evt.tournament, sizeof(TournamentState));
            apply_tournament_state_to_model();
            s_status_text = "Bracket updated.";
            s_model_handle.DirtyVariable("status_text");
        } else if (sse_type == SSE_EVENT_ROUND_ADVANCE) {
            s_status_text =
                Rml::String("Round ") + Rml::String(std::to_string(sse_evt.round_number + 1).c_str()) + " starting!";
            s_model_handle.DirtyVariable("status_text");
            refresh_room_state_from_server();
        } else if (sse_type == SSE_EVENT_MATCH_PROPOSE) {
            bool we_are_p1 = (strcmp(sse_evt.propose_p1_id, s_my_id.c_str()) == 0);
            bool we_are_p2 = (strcmp(sse_evt.propose_p2_id, s_my_id.c_str()) == 0);

            if (we_are_p1 || we_are_p2) {
                const char* opp_name = we_are_p1 ? sse_evt.propose_p2_name : sse_evt.propose_p1_name;
                const char* opp_conn = we_are_p1 ? sse_evt.propose_p2_conn_type : sse_evt.propose_p1_conn_type;
                int opp_rtt = we_are_p1 ? sse_evt.propose_p2_rtt_ms : sse_evt.propose_p1_rtt_ms;
                const char* opp_room = we_are_p1 ? sse_evt.propose_p2_room_code : sse_evt.propose_p1_room_code;
                const char* opp_id = we_are_p1 ? sse_evt.propose_p2_id : sse_evt.propose_p1_id;

                s_proposal_active = 1;
                s_proposal_opponent_name = opp_name;
                s_proposal_opponent_conn_type = opp_conn;
                int p2p_rtt = PingProbe_GetRTT(opp_id);
                s_proposal_opponent_ping = (p2p_rtt >= 0) ? p2p_rtt : opp_rtt;
                s_proposal_countdown = PROPOSAL_TIMEOUT_SEC;
                s_proposal_countdown_pct = 100;
                s_proposal_cursor = 0;
                s_proposal_start_time = SDL_GetTicks();
                s_proposal_we_are_p1 = we_are_p1;
                s_proposal_match_index = sse_evt.match_index;
                snprintf(s_proposal_opponent_room_code, sizeof(s_proposal_opponent_room_code), "%s", opp_room);
                snprintf(s_proposal_opponent_player_id, sizeof(s_proposal_opponent_player_id), "%s", opp_id);
                s_proposal_ft = sse_evt.propose_ft > 0 ? sse_evt.propose_ft : 1;

                s_status_text =
                    Rml::String("Match proposed: ") + sse_evt.propose_p1_name + " vs " + sse_evt.propose_p2_name;

                s_model_handle.DirtyVariable("proposal_active");
                s_model_handle.DirtyVariable("proposal_opponent_name");
                s_model_handle.DirtyVariable("proposal_opponent_ping");
                s_model_handle.DirtyVariable("proposal_opponent_conn_type");
                s_model_handle.DirtyVariable("proposal_countdown");
                s_model_handle.DirtyVariable("proposal_countdown_pct");
                s_model_handle.DirtyVariable("proposal_cursor");
            } else {
                s_status_text = Rml::String("Match: ") + sse_evt.propose_p1_name + " vs " + sse_evt.propose_p2_name;
            }
            s_model_handle.DirtyVariable("status_text");
        } else if (sse_type == SSE_EVENT_MATCH_DECLINE) {
            if (s_proposal_active) {
                s_proposal_active = 0;
                s_model_handle.DirtyVariable("proposal_active");
                s_status_text = "Match proposal cancelled.";
            }
            s_model_handle.DirtyVariable("status_text");
            refresh_room_state_from_server();
        } else if (sse_type == SSE_EVENT_MATCH_START) {
            if (s_proposal_active) {
                s_proposal_active = 0;
                s_model_handle.DirtyVariable("proposal_active");
            }
            refresh_room_state_from_server();

            if (strcmp(s_room_state.match_p1, s_my_id.c_str()) == 0 ||
                strcmp(s_room_state.match_p2, s_my_id.c_str()) == 0) {
                s_is_playing = true;
                s_status_text = "YOUR MATCH — GET READY!";
                rmlui_wrapper_hide_game_document("tournament_lobby");

                if (s_proposal_opponent_room_code[0] || s_proposal_opponent_player_id[0]) {
                    rmlui_ingame_chat_set_opponent_name(s_proposal_opponent_name.c_str());
                    SDLNetplayUI_StartCasualMatchPunch(s_proposal_opponent_room_code,
                                                       s_proposal_opponent_name.c_str(),
                                                       s_proposal_opponent_player_id,
                                                       s_proposal_we_are_p1);
                    s_proposal_opponent_room_code[0] = '\0';
                }
            } else {
                s_status_text = "Match in progress.";
            }
            s_model_handle.DirtyVariable("is_playing");
            s_model_handle.DirtyVariable("status_text");
        } else if (sse_type == SSE_EVENT_MATCH_END) {
            Rml::String winner_name = sse_evt.match_winner_id;
            for (int i = 0; i < s_room_state.player_count; i++) {
                if (strcmp(s_room_state.players[i].player_id, sse_evt.match_winner_id) == 0) {
                    winner_name = s_room_state.players[i].display_name;
                    break;
                }
            }
            s_status_text = winner_name + " advances!";
            s_model_handle.DirtyVariable("status_text");

            if (s_is_playing) {
                NetplaySessionState ns = Netplay_GetSessionState();
                if (ns == NETPLAY_SESSION_IDLE || ns == NETPLAY_SESSION_LOBBY) {
                    rmlui_wrapper_show_game_document("tournament_lobby");
                    s_is_playing = false;
                    s_model_handle.DirtyVariable("is_playing");
                } else {
                    s_match_ended_pending_reshow = true;
                }
            }

            refresh_room_state_from_server();
        }
    }

    // Fallback poll
    Uint64 now = SDL_GetTicks();
    if (now - s_last_poll_time > 3000 && !s_is_playing) {
        s_last_poll_time = now;
        if (!LobbyServer_SSEIsConnected()) {
            refresh_room_state_from_server();
        }
    }

    // Chat input handling
    if (s_chat_open) {
        u16 chat_trigger = 0;
        for (int i = 0; i < 2; i++)
            chat_trigger |= (~g_state.PLsw[i][1] & g_state.PLsw[i][0]);
        if (chat_trigger & 0x0200) {
            s_chat_open = false;
            s_is_typing = false;
            s_chat_input = "";
            s_model_handle.DirtyVariable("is_typing");
            s_model_handle.DirtyVariable("chat_input");
            SDL_StopTextInput(SDL_GetKeyboardFocus());
        }
        return;
    }

    // Proposal countdown
    if (s_proposal_active) {
        Uint64 elapsed = SDL_GetTicks() - s_proposal_start_time;
        int remaining = PROPOSAL_TIMEOUT_SEC - (int)(elapsed / 1000);
        if (remaining < 0)
            remaining = 0;

        int elapsed_ms = (int)(elapsed);
        int pct = 100 - (elapsed_ms * 100 / (PROPOSAL_TIMEOUT_SEC * 1000));
        if (pct < 0)
            pct = 0;
        if (pct != s_proposal_countdown_pct) {
            s_proposal_countdown_pct = pct;
            s_model_handle.DirtyVariable("proposal_countdown_pct");
        }
        if (remaining != s_proposal_countdown) {
            s_proposal_countdown = remaining;
            s_model_handle.DirtyVariable("proposal_countdown");
            if (remaining <= 0) {
                s_proposal_active = 0;
                s_model_handle.DirtyVariable("proposal_active");
                AsyncMatchAction(s_room_code.c_str(), 2);
                s_status_text = "Timed out — auto-declined.";
                s_model_handle.DirtyVariable("status_text");
            }
        }

        u16 trigger = 0;
        for (int i = 0; i < 2; i++)
            trigger |= (~g_state.PLsw[i][1] & g_state.PLsw[i][0]);
        if (trigger & 0x04) {
            if (s_proposal_cursor != 0) {
                s_proposal_cursor = 0;
                s_model_handle.DirtyVariable("proposal_cursor");
            }
        }
        if (trigger & 0x08) {
            if (s_proposal_cursor != 1) {
                s_proposal_cursor = 1;
                s_model_handle.DirtyVariable("proposal_cursor");
            }
        }
        if (trigger & (0x0100 | 0x0800)) {
            s_proposal_active = 0;
            s_model_handle.DirtyVariable("proposal_active");
            if (s_proposal_cursor == 0) {
                Netplay_SetNegotiatedFT(s_proposal_ft);
                AsyncMatchAction(s_room_code.c_str(), 1);
                s_status_text = "Accepted! Waiting for opponent...";
            } else {
                AsyncMatchAction(s_room_code.c_str(), 2);
                s_status_text = "Declined match.";
            }
            s_model_handle.DirtyVariable("status_text");
        }
        if (trigger & 0x0200) {
            s_proposal_active = 0;
            s_model_handle.DirtyVariable("proposal_active");
            AsyncMatchAction(s_room_code.c_str(), 2);
            s_status_text = "Declined match.";
            s_model_handle.DirtyVariable("status_text");
        }
        return;
    }

    // Skip navigation during match
    if (s_is_playing || s_match_ended_pending_reshow)
        return;

    // --- Input Navigation ---
    u16 trigger = 0;
    for (int i = 0; i < 2; i++)
        trigger |= (~g_state.PLsw[i][1] & g_state.PLsw[i][0]);

    if (trigger & 0x01) { // Up
        if (s_cursor_x == 0) {
            if (s_cursor_y > 0)
                s_cursor_y--;
        }
    }
    if (trigger & 0x02) { // Down
        if (s_cursor_x == 0) {
            // Dynamic max_y based on state
            int max_y = 1; // view(0) + leave(last)
            if (s_is_host && s_tournament_started) {
                max_y = 5; // view(0), pause(1), dq(2), override(3), restart(4), leave(5)
            } else if (s_is_host && !s_tournament_started) {
                max_y = 2; // view(0), start(1), leave(2)
            }
            if (s_cursor_y < max_y)
                s_cursor_y++;
        }
    }
    // Fix 1: Left/Right scroll match selector
    if (trigger & 0x04) { // Left
        if (s_cursor_x == 0 && s_cursor_y == 0 && s_active_match_count > 1) {
            s_match_selector_idx = (s_match_selector_idx - 1 + s_active_match_count) % s_active_match_count;
            s_model_handle.DirtyVariable("match_selector_idx");
        } else {
            s_cursor_x = 0;
        }
    }
    if (trigger & 0x08) { // Right
        if (s_cursor_x == 0 && s_cursor_y == 0 && s_active_match_count > 1) {
            s_match_selector_idx = (s_match_selector_idx + 1) % s_active_match_count;
            s_model_handle.DirtyVariable("match_selector_idx");
        } else {
            s_cursor_x = 1;
        }
    }

    // Left/Right for DQ target cycling (cursor_y == 2, host, started)
    if (s_cursor_y == 2 && s_is_host && s_tournament_started && s_player_count > 0) {
        if (trigger & 0x04) {
            s_dq_target_idx = (s_dq_target_idx - 1 + s_player_count) % s_player_count;
        }
        if (trigger & 0x08) {
            s_dq_target_idx = (s_dq_target_idx + 1) % s_player_count;
        }
        if (s_dq_target_idx < s_player_count)
            s_dq_target_name = s_players[s_dq_target_idx].name;
        s_model_handle.DirtyVariable("dq_target_name");
        s_model_handle.DirtyVariable("dq_target_idx");
    }

    // Left/Right for Override winner cycling (cursor_y == 3, host, started)
    if (s_cursor_y == 3 && s_is_host && s_tournament_started) {
        if ((trigger & 0x04) || (trigger & 0x08)) {
            s_override_winner = 1 - s_override_winner;
            if (s_match_selector_idx < s_active_match_count) {
                s_override_label = s_override_winner == 0 ? s_active_matches[s_match_selector_idx].p1_name
                                                          : s_active_matches[s_match_selector_idx].p2_name;
            }
            s_model_handle.DirtyVariable("override_label");
            s_model_handle.DirtyVariable("override_winner");
        }
    }

    if (trigger & (0x0100 | 0x0800)) { // Confirm
        if (s_cursor_x == 1) {
            // Chat
            s_chat_open = true;
            s_is_typing = true;
            s_chat_input = "";
            s_model_handle.DirtyVariable("is_typing");
            s_model_handle.DirtyVariable("chat_input");
            SDL_StartTextInput(SDL_GetKeyboardFocus());
        } else {
            // Dynamic leave position
            int leave_y = 1; // non-host default
            if (s_is_host && s_tournament_started)
                leave_y = 5;
            else if (s_is_host && !s_tournament_started)
                leave_y = 2;

            if (s_cursor_y == 1 && s_is_host && !s_tournament_started) {
                // TO: Start bracket
                AsyncTOAction(s_room_code.c_str(), 1, NULL, 0, NULL);
                s_status_text = "Starting bracket...";
                s_model_handle.DirtyVariable("status_text");
            } else if (s_cursor_y == 1 && s_is_host && s_tournament_started && !s_tournament_paused) {
                // TO: g_state.Pause
                AsyncTOAction(s_room_code.c_str(), 2, NULL, 0, NULL);
                s_status_text = "Pausing tournament...";
                s_model_handle.DirtyVariable("status_text");
            } else if (s_cursor_y == 1 && s_is_host && s_tournament_started && s_tournament_paused) {
                // TO: Resume
                AsyncTOAction(s_room_code.c_str(), 3, NULL, 0, NULL);
                s_status_text = "Resuming tournament...";
                s_model_handle.DirtyVariable("status_text");
            } else if (s_cursor_y == 2 && s_is_host && s_tournament_started && s_player_count > 0) {
                // TO: DQ selected player
                if (s_dq_target_idx < (int)s_players.size() && !s_players[s_dq_target_idx].name.empty()) {
                    // Resolve player_id from RoomState
                    const char* pid = "";
                    for (int p = 0; p < s_room_state.player_count; p++) {
                        if (strcmp(s_room_state.players[p].display_name, s_players[s_dq_target_idx].name.c_str()) ==
                            0) {
                            pid = s_room_state.players[p].player_id;
                            break;
                        }
                    }
                    AsyncTOAction(s_room_code.c_str(), 4, pid, 0, NULL);
                    s_status_text = Rml::String("DQ: ") + s_players[s_dq_target_idx].name;
                    s_model_handle.DirtyVariable("status_text");
                }
            } else if (s_cursor_y == 3 && s_is_host && s_tournament_started) {
                // TO: Override result for selected match
                if (s_match_selector_idx < s_active_match_count) {
                    const RmlActiveMatch& m = s_active_matches[s_match_selector_idx];
                    // Resolve winner player_id
                    const char* winner_pid = "";
                    const Rml::String& winner_name = s_override_winner == 0 ? m.p1_name : m.p2_name;
                    for (int p = 0; p < s_room_state.player_count; p++) {
                        if (strcmp(s_room_state.players[p].display_name, winner_name.c_str()) == 0) {
                            winner_pid = s_room_state.players[p].player_id;
                            break;
                        }
                    }
                    AsyncTOAction(s_room_code.c_str(), 5, NULL, m.match_index, winner_pid);
                    s_status_text = Rml::String("Override: ") + winner_name + " wins";
                    s_model_handle.DirtyVariable("status_text");
                }
            } else if (s_cursor_y == 4 && s_is_host && s_tournament_started) {
                // TO: Restart selected match
                if (s_match_selector_idx < s_active_match_count) {
                    const RmlActiveMatch& m = s_active_matches[s_match_selector_idx];
                    AsyncTOAction(s_room_code.c_str(), 6, NULL, m.match_index, NULL);
                    s_status_text = Rml::String("Restarting match...");
                    s_model_handle.DirtyVariable("status_text");
                }
            } else if (s_cursor_y == leave_y) {
                // Leave room
                s_wants_leave = true;
            }
        }
    }

    // Cancel = leave room
    if (trigger & 0x0200) {
        s_wants_leave = true;
    }

    // Always dirty cursor for navigation feedback
    s_model_handle.DirtyVariable("cursor_x");
    s_model_handle.DirtyVariable("cursor_y");
}

// ─── Show / Hide / Lifecycle ─────────────────────────────────────

extern "C" void rmlui_tournament_lobby_show(void) {
    if (!s_model_registered)
        do_init();
    if (!s_model_registered)
        return;

    s_is_visible = true;
    s_wants_leave = false;
    s_is_playing = false;

    s_match_ended_pending_reshow = false;
    s_proposal_active = 0;
    s_chat_open = false;
    s_cursor_x = 0;
    s_cursor_y = 0;

    // Connect SSE
    if (!s_room_code.empty()) {
        LobbyServer_SSEConnect(s_room_code.c_str());
    }

    // Initial fetch
    refresh_room_state_from_server();

    rmlui_wrapper_show_game_document("tournament_lobby");
    SDL_Log("[TournamentLobby] Show: room=%s", s_room_code.c_str());
}

extern "C" void rmlui_tournament_lobby_hide(void) {
    if (s_is_visible) {
        LobbyServer_SSEDisconnect();
    }
    s_is_visible = false;
    rmlui_wrapper_hide_game_document("tournament_lobby");
    SDL_Log("[TournamentLobby] Hide");
}

extern "C" void rmlui_tournament_lobby_shutdown(void) {
    rmlui_tournament_lobby_hide();
}

extern "C" bool rmlui_tournament_lobby_is_visible(void) {
    return s_is_visible;
}

extern "C" void rmlui_tournament_lobby_set_room(const char* room_code) {
    if (!s_model_registered)
        do_init();
    s_room_code = room_code ? room_code : "";
    memset(&s_room_state, 0, sizeof(s_room_state));
    memset(&s_tournament_state, 0, sizeof(s_tournament_state));
    s_chat_display_count = 0;
    s_bracket_display_count = 0;
    s_active_match_count = 0;
    SDL_Log("[TournamentLobby] Set room: %s", s_room_code.c_str());
}

extern "C" const char* rmlui_tournament_lobby_get_room_code(void) {
    return s_room_code.c_str();
}

extern "C" bool rmlui_tournament_lobby_wants_leave(void) {
    return s_wants_leave;
}

extern "C" void rmlui_tournament_lobby_consume_leave(void) {
    s_wants_leave = false;
    if (!s_room_code.empty()) {
        LobbyServer_LeaveRoom(s_room_code.c_str());
    }
    s_room_code = "";
    memset(&s_room_state, 0, sizeof(s_room_state));
    memset(&s_tournament_state, 0, sizeof(s_tournament_state));
    rmlui_tournament_lobby_hide();
}

extern "C" bool rmlui_tournament_lobby_handle_key_event(const union SDL_Event* event) {
    if (!s_is_visible || !s_chat_open)
        return false;

    // Handle text input for chat (same pattern as casual lobby)
    if (event->type == SDL_EVENT_TEXT_INPUT) {
        s_chat_input += event->text.text;
        s_model_handle.DirtyVariable("chat_input");
        return true;
    }
    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.key == SDLK_BACKSPACE && !s_chat_input.empty()) {
            s_chat_input.resize(s_chat_input.size() - 1);
            s_model_handle.DirtyVariable("chat_input");
            return true;
        }
        if (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER) {
            if (!s_chat_input.empty()) {
                LobbyServer_SendChat(s_room_code.c_str(), s_chat_input.c_str());
            }
            s_chat_open = false;
            s_is_typing = false;
            s_chat_input = "";
            s_model_handle.DirtyVariable("is_typing");
            s_model_handle.DirtyVariable("chat_input");
            SDL_StopTextInput(SDL_GetKeyboardFocus());
            return true;
        }
        if (event->key.key == SDLK_ESCAPE) {
            s_chat_open = false;
            s_is_typing = false;
            s_chat_input = "";
            s_model_handle.DirtyVariable("is_typing");
            s_model_handle.DirtyVariable("chat_input");
            SDL_StopTextInput(SDL_GetKeyboardFocus());
            return true;
        }
    }
    return false;
}
