/**
 * @file rmlui_ingame_chat.cpp
 * @brief In-match text chat overlay (Fightcade-style).
 *
 * Renders ephemeral chat messages on the fight screen during netplay.
 * Messages expire after ~5 seconds. A typing bar (activated by T key)
 * sends messages P2P via SDLNetAdapter_SendChat() and optionally relays
 * them to the lobby server for spectator visibility.
 */

#include "port/sdl/rmlui/rmlui_ingame_chat.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstring>
#include <vector>

extern "C" {
#include "netplay/identity.h"
#include "netplay/lobby_server.h"
#include "netplay/sdl_net_adapter.h"
}

// ─── Configuration ───────────────────────────────────────────────
#define CHAT_DISPLAY_SLOTS 4      // Max visible messages
#define CHAT_EXPIRE_FRAMES 300    // ~5 seconds at 60fps
#define CHAT_INPUT_MAX     120    // Max input length (matches adapter limit)

// ─── Data Structs ────────────────────────────────────────────────

struct InGameChatMsg {
    Rml::String sender;
    Rml::String text;
    Uint64 birth_tick;
};

// ─── State ───────────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

static std::vector<InGameChatMsg> s_messages;

static Rml::String s_chat_input;
static bool s_is_typing = false;

// Opponent display name (set by casual lobby, shown for incoming P2P messages)
static char s_opponent_name[32] = "OPP";

// Lobby relay: room code for LobbyServer_SendChat (set by casual lobby)
static char s_relay_room_code[16] = {};

// ─── Async send (avoid blocking game loop with HTTP) ─────────────
struct AsyncChatData {
    char room_code[16];
    char text[CHAT_INPUT_MAX + 1];
};

static int SDLCALL async_chat_send_fn(void* data) {
    AsyncChatData* d = (AsyncChatData*)data;
    LobbyServer_SendChat(d->room_code, d->text);
    free(d);
    return 0;
}

static void relay_to_server(const char* text) {
    if (s_relay_room_code[0] == '\0')
        return;
    if (!LobbyServer_SSEIsConnected())
        return;

    AsyncChatData* d = (AsyncChatData*)malloc(sizeof(AsyncChatData));
    if (!d)
        return;
    SDL_strlcpy(d->room_code, s_relay_room_code, sizeof(d->room_code));
    SDL_strlcpy(d->text, text, sizeof(d->text));
    SDL_Thread* t = SDL_CreateThread(async_chat_send_fn, "ChatRelay", d);
    if (t) {
        SDL_DetachThread(t);
    } else {
        free(d);
    }
}

// ─── Push a message to the display ring ──────────────────────────
static void push_message(const char* sender, const char* text) {
    if (!s_model_handle)
        return;

    if (s_messages.size() >= CHAT_DISPLAY_SLOTS) {
        s_messages.erase(s_messages.begin());
    }

    s_messages.push_back({sender, text, SDL_GetTicks()});
    s_model_handle.DirtyVariable("messages");
}

// ─── Init ────────────────────────────────────────────────────────
extern "C" void rmlui_ingame_chat_init(void) {
    if (s_model_registered)
        return;

    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("ingame_chat");
    if (!ctor)
        return;

    if (auto h = ctor.RegisterStruct<InGameChatMsg>()) {
        h.RegisterMember("sender", &InGameChatMsg::sender);
        h.RegisterMember("text", &InGameChatMsg::text);
    }
    ctor.RegisterArray<std::vector<InGameChatMsg>>();
    ctor.Bind("messages", &s_messages);

    ctor.Bind("chat_input", &s_chat_input);
    ctor.Bind("is_typing", &s_is_typing);

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    // Clear message slots
    s_messages.clear();
    s_chat_input = "";
    s_is_typing = false;

    rmlui_wrapper_show_game_document("ingame_chat");

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[InGameChat] Initialized");
}

// ─── Per-frame update ────────────────────────────────────────────
extern "C" void rmlui_ingame_chat_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    Uint64 now = SDL_GetTicks();
    bool dirty = false;

    // Expire old messages
    while (!s_messages.empty()) {
        if ((now - s_messages.front().birth_tick) > (CHAT_EXPIRE_FRAMES * 1000 / 60)) {
            s_messages.erase(s_messages.begin());
            dirty = true;
        } else {
            break; // Oldest is at the front; if it's not expired, others aren't either
        }
    }

    // Poll P2P chat inbox
    char incoming[CHAT_INPUT_MAX + 1];
    while (SDLNetAdapter_PollChat(incoming, sizeof(incoming))) {
        // P2P messages come from the opponent — use "Opponent" as sender
        // (we don't have their display name at this layer; the lobby overlay
        // can set a nicer name if available)
        push_message(s_opponent_name, incoming);
        dirty = true;
    }

    if (dirty) {
        s_model_handle.DirtyVariable("messages");
    }
}

// ─── Input handling ──────────────────────────────────────────────
extern "C" void rmlui_ingame_chat_open_input(void) {
    if (!s_model_registered)
        return;
    if (s_is_typing)
        return; // Already open

    s_is_typing = true;
    s_chat_input = "";
    s_model_handle.DirtyVariable("is_typing");
    s_model_handle.DirtyVariable("chat_input");
    SDL_Window* focus = SDL_GetKeyboardFocus();
    if (focus)
        SDL_StartTextInput(focus);
}

static void close_input(void) {
    s_is_typing = false;
    s_chat_input = "";
    if (s_model_handle) {
        s_model_handle.DirtyVariable("is_typing");
        s_model_handle.DirtyVariable("chat_input");
    }
    SDL_Window* focus = SDL_GetKeyboardFocus();
    if (focus)
        SDL_StopTextInput(focus);
}

static void send_message(void) {
    if (s_chat_input.empty())
        return;

    const char* my_name = Identity_GetDisplayName();
    if (!my_name || !my_name[0])
        my_name = "You";

    // Push to local display
    push_message(my_name, s_chat_input.c_str());

    // Send P2P
    SDLNetAdapter_SendChat(s_chat_input.c_str());

    // Relay to lobby server (async, for spectators)
    relay_to_server(s_chat_input.c_str());

    close_input();
}

extern "C" void rmlui_ingame_chat_handle_key(int keycode, const char* text) {
    if (!s_is_typing)
        return;

    if (keycode == SDLK_RETURN || keycode == SDLK_KP_ENTER) {
        send_message();
        return;
    }

    if (keycode == SDLK_ESCAPE) {
        close_input();
        return;
    }

    if (keycode == SDLK_BACKSPACE) {
        if (!s_chat_input.empty()) {
            // Remove last UTF-8 character (may be multi-byte)
            size_t len = s_chat_input.size();
            while (len > 0 && ((unsigned char)s_chat_input[len - 1] & 0xC0) == 0x80)
                len--;
            if (len > 0)
                len--;
            s_chat_input.resize(len);
            s_model_handle.DirtyVariable("chat_input");
        }
        return;
    }

    // Append text input (from SDL_EVENT_TEXT_INPUT)
    if (text && text[0]) {
        if (s_chat_input.size() + SDL_strlen(text) <= CHAT_INPUT_MAX) {
            s_chat_input += text;
            s_model_handle.DirtyVariable("chat_input");
        }
    }
}

extern "C" bool rmlui_ingame_chat_is_typing(void) {
    return s_is_typing;
}

// ─── Lobby relay configuration ───────────────────────────────────
extern "C" void rmlui_ingame_chat_set_room_code(const char* room_code) {
    if (room_code && room_code[0]) {
        SDL_strlcpy(s_relay_room_code, room_code, sizeof(s_relay_room_code));
    } else {
        s_relay_room_code[0] = '\0';
    }
}

extern "C" void rmlui_ingame_chat_set_opponent_name(const char* name) {
    if (name && name[0]) {
        SDL_strlcpy(s_opponent_name, name, sizeof(s_opponent_name));
    } else {
        SDL_strlcpy(s_opponent_name, "OPP", sizeof(s_opponent_name));
    }
}

// ─── Shutdown ────────────────────────────────────────────────────
extern "C" void rmlui_ingame_chat_shutdown(void) {
    if (s_model_registered) {
        close_input();
        rmlui_wrapper_hide_game_document("ingame_chat");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("ingame_chat");
        s_model_registered = false;
        s_messages.clear();
        s_relay_room_code[0] = '\0';
    }
    SDL_Log("[InGameChat] Shut down");
}
