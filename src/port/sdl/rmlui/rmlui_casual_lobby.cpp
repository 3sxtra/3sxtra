/**
 * @file rmlui_casual_lobby.cpp
 * @brief RmlUi Casual Lobby data model.
 *
 * Binds the C RoomState structure to the RmlUi frontend to display
 * the player list, active match, and chat room.
 */

#include "port/sdl/rmlui/rmlui_casual_lobby.h"
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
static bool s_in_queue = false;

static std::vector<RmlQueuePlayer> s_queue;
static std::vector<RmlChatMessage> s_chat;

static Rml::String s_chat_input;
static bool s_is_typing = false;
static Rml::String s_status_text;

static int s_cursor_x = 0; // 0 = left panel buttons, 1 = right panel chat
static int s_cursor_y = 0;

static Uint64 s_last_poll_time = 0;

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
    ctor.Bind("in_queue", &s_in_queue);

    ctor.Bind("chat_input", &s_chat_input);
    ctor.Bind("is_typing", &s_is_typing);
    ctor.Bind("status_text", &s_status_text);

    ctor.Bind("cursor_x", &s_cursor_x);
    ctor.Bind("cursor_y", &s_cursor_y);

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
                // Spectate
                SDL_Log("Casual Lobby: Spectate clicked");
            } else if (s_cursor_y == 9) {
                // Join/Leave Queue
                if (s_in_queue) {
                    LobbyServer_LeaveQueue(s_room_code.c_str());
                } else {
                    LobbyServer_JoinQueue(s_room_code.c_str());
                }
                refresh_room_state_from_server();
            } else if (s_cursor_y == 10) {
                // Leave Room
                LobbyServer_LeaveRoom(s_room_code.c_str());
                rmlui_casual_lobby_hide();
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
}

extern "C" bool rmlui_casual_lobby_is_visible(void) {
    return s_is_visible;
}
