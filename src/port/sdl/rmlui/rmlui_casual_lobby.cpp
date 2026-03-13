/**
 * @file rmlui_casual_lobby.cpp
 * @brief RmlUi Casual Lobby data model.
 *
 * Binds the C RoomState structure to the RmlUi frontend to display
 * the player list, active match, and chat room.
 */

#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include "port/sdl/rmlui/rmlui_network_lobby.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include "structs.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "netplay/identity.h"
#include "netplay/lobby_server.h"
#include "netplay/netplay.h"
#include "port/sdl/netplay/sdl_netplay_ui.h"
} // extern "C"

#include <RmlUi/Core/Input.h>

static Rml::ElementDocument* s_chat_doc = nullptr;
static bool s_chat_open = false;

class ChatSubmitListener : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        if (event.GetId() == Rml::EventId::Keydown) {
            auto key = (Rml::Input::KeyIdentifier)event.GetParameter<int>("key_identifier", 0);
            if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
                Rml::Element* input = event.GetTargetElement();
                Rml::String text = input->GetAttribute<Rml::String>("value", "");
                if (!text.empty()) {
                    extern Rml::String s_room_code;
                    LobbyServer_SendChat(s_room_code.c_str(), text.c_str());
                    input->SetAttribute("value", "");
                }
                if (s_chat_doc) {
                    s_chat_doc->Hide();
                    s_chat_doc->GetOwnerDocument()->GetContext()->GetRootElement()->Focus(); // Remove focus
                }
                s_chat_open = false;
            } else if (key == Rml::Input::KI_ESCAPE) {
                if (s_chat_doc) {
                    s_chat_doc->Hide();
                    s_chat_doc->GetOwnerDocument()->GetContext()->GetRootElement()->Focus();
                }
                s_chat_open = false;
            }
        }
    }
};
static ChatSubmitListener s_chat_listener;

// ─── Data Structs for RmlUi ──────────────────────────────────────

struct RmlQueuePlayer {
    int index;
    Rml::String name;
    bool is_self;
};

struct RmlChatMessage {
    Rml::String sender;
    Rml::String text;
};

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;
static bool s_is_visible = false;

// The backend C state sync
static RoomState s_room_state;
Rml::String s_room_code;
static Rml::String s_room_name;
static Rml::String s_my_id;

static int s_player_count = 0;
static int s_match_active = 0;
static int s_match_winner = 0;
static Rml::String s_match_p1_name;
static Rml::String s_match_p2_name;
static bool s_is_playing = false;
static bool s_is_spectating = false;
static bool s_in_queue = false;

static std::vector<RmlQueuePlayer> s_queue;
static std::vector<RmlChatMessage> s_chat;

static Rml::String s_chat_input;
static bool s_is_typing = false;
static Rml::String s_status_text;

static int s_cursor_x = 0; // 0 = left panel buttons, 1 = right panel chat
static int s_cursor_y = 0;

static Uint64 s_last_poll_time = 0;

// Phase 6: Match proposal state
static int s_proposal_active = 0;
static Rml::String s_proposal_opponent_name;
static int s_proposal_opponent_ping = -1;
static Rml::String s_proposal_opponent_conn_type;
static int s_proposal_countdown = 10;
static int s_proposal_countdown_pct = 100; // 0-100 for countdown bar width
static int s_proposal_cursor = 0; // 0 = accept, 1 = decline
static Uint64 s_proposal_start_time = 0;
static Rml::ElementDocument* s_match_accept_doc = nullptr;
static char s_proposal_opponent_room_code[32] = {0};
static char s_proposal_opponent_region[8] = {0};
static bool s_proposal_we_are_p1 = false;

// Async accept/decline thread functions (avoid blocking UI thread with HTTP calls)
static SDL_AtomicInt s_async_match_active = {0};

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
    if (SDL_GetAtomicInt(&s_async_match_active) != 0) return;
    SDL_SetAtomicInt(&s_async_match_active, 1);
    AsyncMatchData* d = (AsyncMatchData*)malloc(sizeof(AsyncMatchData));
    snprintf(d->room_code, sizeof(d->room_code), "%s", room_code);
    d->action = action;
    SDL_Thread* t = SDL_CreateThread(async_match_action_fn, "AsyncMatch", d);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(d);
        SDL_SetAtomicInt(&s_async_match_active, 0);
    }
}

// ─── Forward Declarations ────────────────────────────────────────
static void refresh_room_state_from_server(void);
static void apply_room_state_to_model(void);

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_casual_lobby_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx) return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("casual_lobby");
    if (!ctor) return;

    // Register Structs
    if (auto h = ctor.RegisterStruct<RmlQueuePlayer>()) {
        h.RegisterMember("index", &RmlQueuePlayer::index);
        h.RegisterMember("name", &RmlQueuePlayer::name);
        h.RegisterMember("is_self", &RmlQueuePlayer::is_self);
    }
    ctor.RegisterArray<std::vector<RmlQueuePlayer>>();
    ctor.Bind("queue_players", &s_queue);

    if (auto h = ctor.RegisterStruct<RmlChatMessage>()) {
        h.RegisterMember("sender", &RmlChatMessage::sender);
        h.RegisterMember("text", &RmlChatMessage::text);
    }
    ctor.RegisterArray<std::vector<RmlChatMessage>>();
    ctor.Bind("chat_messages", &s_chat);

    // Bind Scalars
    ctor.Bind("room_code", &s_room_code);
    ctor.Bind("room_name", &s_room_name);
    ctor.Bind("player_count", &s_player_count);
    
    ctor.Bind("match_active", &s_match_active);
    ctor.Bind("match_winner", &s_match_winner);
    ctor.Bind("match_p1_name", &s_match_p1_name);
    ctor.Bind("match_p2_name", &s_match_p2_name);
    ctor.Bind("is_playing", &s_is_playing);
    ctor.Bind("is_spectating", &s_is_spectating);
    ctor.Bind("in_queue", &s_in_queue);

    ctor.Bind("chat_input", &s_chat_input);
    ctor.Bind("is_typing", &s_is_typing);
    ctor.Bind("status_text", &s_status_text);

    ctor.Bind("cursor_x", &s_cursor_x);
    ctor.Bind("cursor_y", &s_cursor_y);

    // Phase 6: Match proposal bindings
    ctor.Bind("proposal_active", &s_proposal_active);
    ctor.Bind("proposal_opponent_name", &s_proposal_opponent_name);
    ctor.Bind("proposal_opponent_ping", &s_proposal_opponent_ping);
    ctor.Bind("proposal_opponent_conn_type", &s_proposal_opponent_conn_type);
    ctor.Bind("proposal_countdown", &s_proposal_countdown);
    ctor.Bind("proposal_countdown_pct", &s_proposal_countdown_pct);
    ctor.Bind("proposal_cursor", &s_proposal_cursor);

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    memset(&s_room_state, 0, sizeof(s_room_state));
    s_my_id = Identity_GetPlayerId();
}

static void apply_room_state_to_model(void) {
    if (!s_model_handle) return;

    s_room_code = s_room_state.id;
    s_room_name = s_room_state.name;
    s_player_count = s_room_state.player_count;

    // P1 / P2 names
    s_match_active = s_room_state.match_active;
    s_is_playing = false;
    s_match_p1_name = "Player 1";
    s_match_p2_name = "Player 2";
    
    for (int i = 0; i < s_room_state.player_count; i++) {
        if (strcmp(s_room_state.players[i].player_id, s_room_state.match_p1) == 0) {
            s_match_p1_name = s_room_state.players[i].display_name;
            if (s_room_state.players[i].player_id == s_my_id) s_is_playing = true;
        }
        if (strcmp(s_room_state.players[i].player_id, s_room_state.match_p2) == 0) {
            s_match_p2_name = s_room_state.players[i].display_name;
            if (s_room_state.players[i].player_id == s_my_id) s_is_playing = true;
        }
    }

    // Queue
    s_queue.clear();
    s_in_queue = false;
    for (int i = 0; i < s_room_state.queue_count; i++) {
        RmlQueuePlayer qp;
        qp.index = i;
        qp.is_self = (s_my_id == s_room_state.queue[i]);
        if (qp.is_self) s_in_queue = true;

        qp.name = s_room_state.queue[i]; // Fallback
        for (int p = 0; p < s_room_state.player_count; p++) {
            if (strcmp(s_room_state.players[p].player_id, s_room_state.queue[i]) == 0) {
                qp.name = s_room_state.players[p].display_name;
                break;
            }
        }
        s_queue.push_back(qp);
    }

    // Chat
    s_chat.clear();
    for (int i = 0; i < s_room_state.chat_count; i++) {
        RmlChatMessage msg;
        msg.sender = s_room_state.chat[i].sender_name;
        msg.text = s_room_state.chat[i].text;
        s_chat.push_back(msg);
    }

    s_model_handle.DirtyVariable("room_code");
    s_model_handle.DirtyVariable("room_name");
    s_model_handle.DirtyVariable("player_count");
    s_model_handle.DirtyVariable("match_active");
    s_model_handle.DirtyVariable("match_p1_name");
    s_model_handle.DirtyVariable("match_p2_name");
    s_model_handle.DirtyVariable("is_playing");
    s_model_handle.DirtyVariable("in_queue");
    s_model_handle.DirtyVariable("queue_players");
    s_model_handle.DirtyVariable("chat_messages");
}

static void refresh_room_state_from_server(void) {
    if (s_room_code.empty()) return;
    
    // Use the read-only GET /room/state endpoint (no re-join side effect)
    if (LobbyServer_GetRoomState(s_room_code.c_str(), &s_room_state)) {
        apply_room_state_to_model();
    }
}

// ─── Update loop ─────────────────────────────────────────────────
extern "C" void rmlui_casual_lobby_update(void) {
    if (!s_model_registered || !s_is_visible) return;

    // Drain all queued SSE events from the ring buffer (up to 16 per frame)
    SSEEvent sse_evt;
    for (int sse_i = 0; sse_i < 16; sse_i++) {
        SSEEventType sse_type = LobbyServer_SSEPoll(&sse_evt);
        if (sse_type == SSE_EVENT_NONE) break;

        if (sse_type == SSE_EVENT_SYNC) {
            // Full room state sync — replace everything
            memcpy(&s_room_state, &sse_evt.room, sizeof(RoomState));
            apply_room_state_to_model();
        } else if (sse_type == SSE_EVENT_CHAT) {
            // Append chat message to the UI model directly
            RmlChatMessage msg;
            msg.sender = sse_evt.chat_msg.sender_name;
            msg.text = sse_evt.chat_msg.text;
            s_chat.push_back(msg);
            if (s_chat.size() > MAX_CHAT_MESSAGES) s_chat.erase(s_chat.begin());
            s_model_handle.DirtyVariable("chat_messages");
        } else if (sse_type == SSE_EVENT_JOIN || sse_type == SSE_EVENT_LEAVE ||
                   sse_type == SSE_EVENT_QUEUE_UPDATE || sse_type == SSE_EVENT_HOST_MIGRATED) {
            // For structural changes, re-fetch full state as fallback
            refresh_room_state_from_server();
        } else if (sse_type == SSE_EVENT_MATCH_PROPOSE) {
            // Phase 6: Match proposed — check if we are a participant
            bool we_are_p1 = (strcmp(sse_evt.propose_p1_id, s_my_id.c_str()) == 0);
            bool we_are_p2 = (strcmp(sse_evt.propose_p2_id, s_my_id.c_str()) == 0);

            if (we_are_p1 || we_are_p2) {
                // Determine opponent info
                const char* opp_name = we_are_p1 ? sse_evt.propose_p2_name : sse_evt.propose_p1_name;
                const char* opp_conn = we_are_p1 ? sse_evt.propose_p2_conn_type : sse_evt.propose_p1_conn_type;
                int opp_rtt = we_are_p1 ? sse_evt.propose_p2_rtt_ms : sse_evt.propose_p1_rtt_ms;
                const char* opp_room = we_are_p1 ? sse_evt.propose_p2_room_code : sse_evt.propose_p1_room_code;
                const char* opp_region = we_are_p1 ? sse_evt.propose_p2_region : sse_evt.propose_p1_region;

                // Connection filter auto-decline: skip popup if opponent fails filters
                if (!SDLNetplayUI_PlayerPassesFilters(opp_conn, opp_rtt, opp_region)) {
                    SDL_Log("Casual Lobby: Auto-declining match (opponent fails connection filters)");
                    AsyncMatchAction(s_room_code.c_str(), 2); // async decline
                    s_status_text = "Auto-declined (connection filter).";
                    s_model_handle.DirtyVariable("status_text");
                    refresh_room_state_from_server();
                    continue; // skip showing popup
                }

                s_proposal_active = 1;
                s_proposal_opponent_name = opp_name;
                s_proposal_opponent_conn_type = opp_conn;
                s_proposal_opponent_ping = opp_rtt;
                s_proposal_countdown = 10;
                s_proposal_countdown_pct = 100;
                s_proposal_cursor = 0; // Default to Accept
                s_proposal_start_time = SDL_GetTicks();
                s_proposal_we_are_p1 = we_are_p1;
                snprintf(s_proposal_opponent_room_code, sizeof(s_proposal_opponent_room_code), "%s", opp_room);
                snprintf(s_proposal_opponent_region, sizeof(s_proposal_opponent_region), "%s", opp_region);

                // Show popup document
                Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
                if (ctx && !s_match_accept_doc) {
                    s_match_accept_doc = ctx->LoadDocument("assets/ui/match_accept.rml");
                }
                if (s_match_accept_doc) {
                    s_match_accept_doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
                }

                s_status_text = Rml::String("Match proposed: ") + sse_evt.propose_p1_name + " vs " + sse_evt.propose_p2_name;
                SDL_Log("Casual Lobby: Match proposed! %s vs %s", sse_evt.propose_p1_name, sse_evt.propose_p2_name);

                s_model_handle.DirtyVariable("proposal_active");
                s_model_handle.DirtyVariable("proposal_opponent_name");
                s_model_handle.DirtyVariable("proposal_opponent_ping");
                s_model_handle.DirtyVariable("proposal_opponent_conn_type");
                s_model_handle.DirtyVariable("proposal_countdown");
                s_model_handle.DirtyVariable("proposal_cursor");
            } else {
                s_status_text = Rml::String("Match proposed: ") + sse_evt.propose_p1_name + " vs " + sse_evt.propose_p2_name;
            }
            s_model_handle.DirtyVariable("status_text");
            refresh_room_state_from_server();
        } else if (sse_type == SSE_EVENT_MATCH_DECLINE) {
            // Phase 6: Proposal declined or timed out
            if (s_proposal_active) {
                s_proposal_active = 0;
                if (s_match_accept_doc) s_match_accept_doc->Hide();
                s_model_handle.DirtyVariable("proposal_active");

                bool we_declined = (strcmp(sse_evt.propose_decliner_id, s_my_id.c_str()) == 0);
                if (we_declined) {
                    s_status_text = "You declined. Back to queue.";
                } else if (sse_evt.propose_decliner_id[0]) {
                    s_status_text = "Opponent declined. Waiting...";
                } else {
                    s_status_text = "Match timed out.";
                }
            } else {
                s_status_text = "Match proposal cancelled.";
            }
            s_model_handle.DirtyVariable("status_text");
            refresh_room_state_from_server();
        } else if (sse_type == SSE_EVENT_MATCH_START) {
            // Match started (both accepted) — re-fetch state to get p1/p2
            // Hide proposal popup if still showing
            if (s_proposal_active) {
                s_proposal_active = 0;
                if (s_match_accept_doc) s_match_accept_doc->Hide();
                s_model_handle.DirtyVariable("proposal_active");
            }

            refresh_room_state_from_server();
            
            // Check if we are one of the players
            if (strcmp(s_room_state.match_p1, s_my_id.c_str()) == 0 ||
                strcmp(s_room_state.match_p2, s_my_id.c_str()) == 0) {
                s_is_playing = true;
                s_status_text = "YOUR MATCH — GET READY!";
                SDL_Log("Casual Lobby: You're up! Match starting.");

                // Hide the lobby overlay so gameplay is visible
                rmlui_wrapper_hide_game_document("casual_lobby");

                // P2P connection trigger: use stored opponent room code from proposal phase
                if (s_proposal_opponent_room_code[0]) {
                    SDLNetplayUI_StartCasualMatchPunch(
                        s_proposal_opponent_room_code,
                        s_proposal_opponent_name.c_str(),
                        s_proposal_we_are_p1);
                    s_proposal_opponent_room_code[0] = '\0'; // consumed
                }
            } else {
                s_status_text = Rml::String("Match: ") + s_match_p1_name.c_str() + " vs " + s_match_p2_name.c_str();
            }
            s_model_handle.DirtyVariable("is_playing");
            s_model_handle.DirtyVariable("status_text");
        } else if (sse_type == SSE_EVENT_MATCH_END) {
            // Match ended — update status and re-fetch
            Rml::String winner_name = "";
            for (int i = 0; i < s_room_state.player_count; i++) {
                if (strcmp(s_room_state.players[i].player_id, sse_evt.match_winner_id) == 0) {
                    winner_name = s_room_state.players[i].display_name;
                    break;
                }
            }
            if (winner_name.empty()) winner_name = sse_evt.match_winner_id;
            s_status_text = winner_name + " WINS! Winner stays on.";
            s_model_handle.DirtyVariable("status_text");
            
            // If we were spectating, stop and re-show lobby
            if (s_is_spectating) {
                Netplay_StopSpectate();
                s_is_spectating = false;
                s_model_handle.DirtyVariable("is_spectating");
                rmlui_wrapper_show_game_document("casual_lobby");
            }
            
            // If we were playing, re-show lobby
            if (s_is_playing) {
                rmlui_wrapper_show_game_document("casual_lobby");
            }
            s_is_playing = false;
            s_model_handle.DirtyVariable("is_playing");
            
            refresh_room_state_from_server();
        }
    }

    // Fallback poll every 3 seconds in case SSE drops or isn't connected
    Uint64 now = SDL_GetTicks();
    if (now - s_last_poll_time > 3000) {
        s_last_poll_time = now;
        if (!LobbyServer_SSEIsConnected()) {
            refresh_room_state_from_server();
        }
    }

    if (s_chat_open) {
        return; // Suspend lobby navigation while chat is open
    }

    // Phase 6: Match proposal countdown timer
    if (s_proposal_active) {
        Uint64 elapsed = SDL_GetTicks() - s_proposal_start_time;
        int remaining = 10 - (int)(elapsed / 1000);
        if (remaining < 0) remaining = 0;

        // Smooth percentage (0-100) for countdown bar width
        int elapsed_ms = (int)(elapsed);
        int pct = 100 - (elapsed_ms * 100 / 10000);
        if (pct < 0) pct = 0;
        if (pct != s_proposal_countdown_pct) {
            s_proposal_countdown_pct = pct;
            s_model_handle.DirtyVariable("proposal_countdown_pct");
        }

        if (remaining != s_proposal_countdown) {
            s_proposal_countdown = remaining;
            s_model_handle.DirtyVariable("proposal_countdown");
            if (remaining <= 0) {
                // Auto-decline on timeout
                s_proposal_active = 0;
                if (s_match_accept_doc) s_match_accept_doc->Hide();
                s_model_handle.DirtyVariable("proposal_active");
                AsyncMatchAction(s_room_code.c_str(), 2);
                s_status_text = "Timed out — auto-declined.";
                s_model_handle.DirtyVariable("status_text");
                SDL_Log("Casual Lobby: Match proposal auto-declined (timeout)");
            }
        }

        // Proposal input handling (overrides normal lobby navigation)
        u16 trigger = 0;
        for (int i = 0; i < 2; i++) {
            trigger |= (~PLsw[i][1] & PLsw[i][0]);
        }

        // Left/Right to switch between Accept and Decline
        if (trigger & 0x04) { // Left
            if (s_proposal_cursor != 0) {
                s_proposal_cursor = 0;
                s_model_handle.DirtyVariable("proposal_cursor");
            }
        }
        if (trigger & 0x08) { // Right
            if (s_proposal_cursor != 1) {
                s_proposal_cursor = 1;
                s_model_handle.DirtyVariable("proposal_cursor");
            }
        }

        // Confirm (LP=0x0100, Start=0x0800)
        if (trigger & (0x0100 | 0x0800)) {
            if (s_proposal_cursor == 0) {
                // Accept
                s_proposal_active = 0;
                if (s_match_accept_doc) s_match_accept_doc->Hide();
                s_model_handle.DirtyVariable("proposal_active");
                AsyncMatchAction(s_room_code.c_str(), 1);
                s_status_text = "Accepted! Waiting for opponent...";
                s_model_handle.DirtyVariable("status_text");
                SDL_Log("Casual Lobby: Match accepted");
            } else {
                // Decline
                s_proposal_active = 0;
                if (s_match_accept_doc) s_match_accept_doc->Hide();
                s_model_handle.DirtyVariable("proposal_active");
                AsyncMatchAction(s_room_code.c_str(), 2);
                s_status_text = "Declined match.";
                s_model_handle.DirtyVariable("status_text");
                SDL_Log("Casual Lobby: Match declined");
            }
        }

        // Cancel button (back/select = 0x0200) always declines
        if (trigger & 0x0200) {
            s_proposal_active = 0;
            if (s_match_accept_doc) s_match_accept_doc->Hide();
            s_model_handle.DirtyVariable("proposal_active");
            AsyncMatchAction(s_room_code.c_str(), 2);
            s_status_text = "Declined match.";
            s_model_handle.DirtyVariable("status_text");
            SDL_Log("Casual Lobby: Match declined via cancel button");
        }

        return; // Suspend normal lobby navigation while proposal is active
    }

    // --- Input Navigation ---
    u16 trigger = 0;
    for (int i = 0; i < 2; i++) {
        trigger |= (~PLsw[i][1] & PLsw[i][0]);
    }

    int prev_x = s_cursor_x;
    int prev_y = s_cursor_y;

    if (trigger & 0x01) { // Up
        if (s_cursor_x == 0) {
            if (s_cursor_y == 10) s_cursor_y = 9;
            else if (s_cursor_y == 9 && s_match_active && !s_is_playing) s_cursor_y = 0;
            else if (s_cursor_y == 9) s_cursor_y = 0; // jump to top anyway
        }
    }
    if (trigger & 0x02) { // Down
        if (s_cursor_x == 0) {
            if (s_cursor_y == 0) s_cursor_y = 9;
            else if (s_cursor_y == 9) s_cursor_y = 10;
        }
    }
    if (trigger & 0x04) { // Left
        s_cursor_x = 0;
    }
    if (trigger & 0x08) { // Right
        s_cursor_x = 1;
    }

    // Confirm Buttons (LP=0x0100, Start=0x0800)
    if (trigger & (0x0100 | 0x0800)) {
        if (s_cursor_x == 1) { // Chat
            Rml::Context* win_ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
            if (win_ctx) {
                if (!s_chat_doc) {
                    s_chat_doc = win_ctx->LoadDocument("assets/ui/casual_lobby_chat.rml");
                    if (s_chat_doc) {
                        if (Rml::Element* input = s_chat_doc->GetElementById("chat-input-field")) {
                            input->AddEventListener(Rml::EventId::Keydown, &s_chat_listener);
                        }
                    }
                }
                if (s_chat_doc) {
                    s_chat_open = true;
                    s_chat_doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::Document);
                    if (Rml::Element* input = s_chat_doc->GetElementById("chat-input-field")) {
                        input->SetAttribute("value", ""); // clear input on open
                        input->Focus();
                    }
                }
            }
        } else {
            if (s_cursor_y == 0 && !s_is_playing && s_match_active) {
                // Spectate — hide lobby overlay so game view is visible
                SDL_Log("Casual Lobby: Spectate clicked");
                s_is_spectating = true;
                s_status_text = "Spectating...";
                s_model_handle.DirtyVariable("is_spectating");
                s_model_handle.DirtyVariable("status_text");
                rmlui_wrapper_hide_game_document("casual_lobby");
            } else if (s_cursor_y == 9) {
                // Join/Leave Queue
                if (s_in_queue) {
                    LobbyServer_LeaveQueue(s_room_code.c_str());
                } else {
                    LobbyServer_JoinQueue(s_room_code.c_str());
                }
                refresh_room_state_from_server();
            } else if (s_cursor_y == 10) {
                // Leave Room — go back to network lobby
                LobbyServer_LeaveRoom(s_room_code.c_str());
                rmlui_casual_lobby_hide();
                rmlui_network_lobby_show();
            }
        }
    }

    if (prev_x != s_cursor_x || prev_y != s_cursor_y) {
        s_model_handle.DirtyVariable("cursor_x");
        s_model_handle.DirtyVariable("cursor_y");
    }
}

extern "C" void rmlui_casual_lobby_set_room(const char* room_code) {
    s_room_code = room_code;
    refresh_room_state_from_server();
}

extern "C" void rmlui_casual_lobby_show(void) {
    s_is_visible = true;
    rmlui_wrapper_show_game_document("casual_lobby");
    // Start SSE connection for real-time updates
    if (!s_room_code.empty()) {
        LobbyServer_SSEConnect(s_room_code.c_str());
    }
    refresh_room_state_from_server();
}

extern "C" void rmlui_casual_lobby_hide(void) {
    s_is_visible = false;
    rmlui_wrapper_hide_game_document("casual_lobby");
    LobbyServer_SSEDisconnect();
    if (s_chat_doc && s_chat_open) {
        s_chat_doc->Hide();
        s_chat_open = false;
    }
    if (s_match_accept_doc && s_proposal_active) {
        s_match_accept_doc->Hide();
        s_proposal_active = 0;
    }
}

extern "C" void rmlui_casual_lobby_shutdown(void) {
    LobbyServer_SSEDisconnect();
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("casual_lobby");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx) ctx->RemoveDataModel("casual_lobby");
        s_model_registered = false;
    }
    if (s_chat_doc) {
        s_chat_doc->Close();
        s_chat_doc = nullptr;
    }
    if (s_match_accept_doc) {
        s_match_accept_doc->Close();
        s_match_accept_doc = nullptr;
    }
    s_proposal_active = 0;
}

extern "C" bool rmlui_casual_lobby_is_visible(void) {
    return s_is_visible;
}

extern "C" const char* rmlui_casual_lobby_get_room_code(void) {
    return s_room_code.c_str();
}
