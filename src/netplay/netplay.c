#include "netplay.h"
#include "discovery.h"

#include "game_state.h"
#define Game GekkoGame // workaround: upstream GekkoSessionType::Game collides with void Game()
#include "gekkonet.h"
#undef Game
#include "sdl_net_adapter.h"
#include "main.h"
#include "port/char_data.h"
#include "port/config/config.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "stun.h"
// dc_ghost.h does not exist in our repo; njdp2d_draw was renamed to Renderer_Flush2DPrimitives.
#include "port/rendering/renderer.h"
extern void njUserMain();
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "port/sdl/renderer/sdl_game_renderer.h"
#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include "port/sdl/rmlui/rmlui_network_lobby.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/utils/djb2_hash.h"
#include "types.h"

#include <stdbool.h>
#include <string.h>

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define INPUT_HISTORY_MAX 120
#define FRAME_SKIP_TIMER_MAX 60 // Allow skipping a frame roughly every second
#define STATS_UPDATE_TIMER_MAX 60
#define DELAY_FRAMES_DEFAULT 1
#define DELAY_FRAMES_MAX 4
#define PING_SAMPLE_INTERVAL 30
#define PLAYER_COUNT 2

// Uncomment to enable packet drops
// #define LOSSY_ADAPTER

// 3SX-private: forward declaration for event queue (defined at end of file)
static void push_event(NetplayEventType type);

static GekkoSession* session = NULL;
static unsigned short local_port = 0;
static unsigned short remote_port = 0;
static char remote_ip_string[64] = { 0 };
static const char* remote_ip = NULL;
static int player_number = 0;
static int player_handle = 0;
static NET_DatagramSocket* stun_socket = NULL; // Pre-punched STUN socket for internet play
static int s_negotiated_ft = 0;  // FT value agreed upon for the upcoming match (0 = use config default)
static uint32_t handshake_ready_since = 0; // Ticks when both peers signaled ready (LAN handshake hold)
static NetplaySessionState session_state = NETPLAY_SESSION_IDLE;
static u16 input_history[2][INPUT_HISTORY_MAX] = { 0 };
static float frames_behind = 0;
static int frame_skip_timer = 0;
static int transition_ready_frames = 0;

static int stats_update_timer = 0;
static int frame_max_rollback = 0;
static NetworkStats network_stats = { 0 };

// --- Dynamic delay from ping ---
static int dynamic_delay = DELAY_FRAMES_DEFAULT;
static bool dynamic_delay_applied = false;
static float ping_sum = 0;
static float jitter_sum = 0;
static int ping_sample_count = 0;
static int ping_sample_timer = 0;

#if defined(LOSSY_ADAPTER)
static GekkoNetAdapter* base_adapter = NULL;
static GekkoNetAdapter lossy_adapter = { 0 };

static float random_float() {
    return (float)rand() / RAND_MAX;
}

static void LossyAdapter_SendData(GekkoNetAddress* addr, const char* data, int length) {
    const float number = random_float();

    // Adjust this number to change drop probability
    if (number <= 0.25) {
        return;
    }

    base_adapter->send_data(addr, data, length);
}
#endif

static void clean_input_buffers() {
    p1sw_0 = 0;
    p2sw_0 = 0;
    p1sw_1 = 0;
    p2sw_1 = 0;
    p1sw_buff = 0;
    p2sw_buff = 0;
    SDL_zeroa(PLsw);
    SDL_zeroa(plsw_00);
    SDL_zeroa(plsw_01);
}

/**
 * @brief Canonicalize all game state before the first synced frame.
 *
 * @netplay_sync — THIS IS THE MOST IMPORTANT FUNCTION FOR INITIAL SYNC.
 *
 * Both peers enter netplay from slightly different local states (different
 * menus, timers, button configs, character select progress). This function
 * forces every divergent global to a known identical value so that the
 * first rollback frame starts from the same state on both sides.
 *
 * Categories of things canonicalized:
 *  - Task/state machine routing numbers (G_No, C_No, SC_No, E_No)
 *  - Mode and play type (MODE_NETWORK, Play_Mode)
 *  - Game settings (Time_Limit, Battle_Number, Damage_Level, etc.)
 *  - Timers (Game_timer, Control_Time, E_Timer, G_Timer, etc.)
 *  - RNG indices (Random_ix16, Random_ix32)
 *  - Button config (Pad_Infor forced to identity, Check_Buff/Convert_Buff zeroed)
 *  - Background state (bg_pos, fm_pos, bg_prm, Screen_Switch)
 *  - Per-player globals (Champion, Connect_Status, Operator_Status, etc.)
 *  - Input buffers (clean_input_buffers)
 */
static void setup_vs_mode() {
    // ====================================================================
    // PHASE 0: Zero per-player and combat subsystem state.
    //
    // When connecting from the network lobby, the game engine may have been
    // running under the RmlUI overlay (attract mode, demo, etc.), leaving
    // stale data in PLW[] and related player subsystems.
    // The native LAN lobby doesn't hit this because the menu system goes
    // through a proper fade-destroy-reinit cycle.
    //
    // We only zero player/combat state — NOT engine globals (G_No, Country,
    // task routing, etc.) because step_game() runs during TRANSITIONING
    // to advance the game state machine (G_No[1]: 12→1).
    // ====================================================================
    SDL_zeroa(plw);
    SDL_zeroa(zanzou_table);
    SDL_zeroa(super_arts);

    // Task timers and scratch data evolve independently per peer during menus.
    // Zero them for deterministic start. DO NOT zero r_no or condition —
    // those are game state machine routing fields set by the engine.
    for (int i = 0; i < 11; i++) {
        task[i].timer = 0;
        SDL_zeroa(task[i].free);
    }

    // This is pretty much a copy of logic from menu.c
    task[TASK_MENU].r_no[0] = 5; // go to idle routine (doing nothing)
    cpExitTask(TASK_SAVER);
    cpExitTask(TASK_PAUSE);

    // Zero pause flags — if one peer was paused before entering netplay,
    // these would differ on the first synced frame.
    Pause = 0;
    Game_pause = 0;

    // Re-set after zeroing plw — both players must be active for 2P mode
    plw[0].wu.pl_operator = 1;
    plw[1].wu.pl_operator = 1;
    Operator_Status[0] = 1;
    Operator_Status[1] = 1;
    Clear_Personal_Data(0);
    Clear_Personal_Data(1);
    grade_check_work_1st_init(0, 0);
    grade_check_work_1st_init(0, 1);
    grade_check_work_1st_init(1, 0);
    grade_check_work_1st_init(1, 1);
    Setup_Training_Difficulty();

    // Tear down stale backgrounds, reinitialize the effect pool, and stop
    // the select timer. Without this, lingering effects from attract/demo
    // mode start diverged between peers — and since we snapshot EffectState
    // for rollback, every restore to an early frame would replay garbage.
    System_all_clear_Level_B();

    G_No[0] = 2;
    E_No[0] = 1;
    Demo_Flag = 1;

    G_No[1] = 12;
    G_No[2] = 1;
    Mode_Type = MODE_NETWORK;
    Present_Mode = MODE_NETWORK;
    Play_Mode = 0;
    Replay_Status[0] = 0;
    Replay_Status[1] = 0;
    cpExitTask(TASK_MENU);

    // Force standard game settings so both peers use identical values
    // regardless of each player's local DIP switch configuration.
    // Without this, save_w[MODE_NETWORK] retains per-player settings
    // that cause gameplay desyncs (different HP, timer, round count).
    save_w[MODE_NETWORK].Time_Limit = 99;
    save_w[MODE_NETWORK].Battle_Number[0] = 2; // Best of 3 (1P vs CPU)
    save_w[MODE_NETWORK].Battle_Number[1] = 2; // Best of 3 (1P vs 2P)
    save_w[MODE_NETWORK].Damage_Level = 0;     // Normal damage
    save_w[MODE_NETWORK].Handicap = 0;
    save_w[MODE_NETWORK].GuardCheck = 0;

    E_Timer = 0; // E_Timer can have different values depending on when the session was initiated

    Deley_Shot_No[0] = 0;
    Deley_Shot_No[1] = 0;
    Deley_Shot_Timer[0] = 15;
    Deley_Shot_Timer[1] = 15;
    Random_ix16 = 0;
    Round_num = 0;
    Game_timer = 0;
    Random_ix32 = 0;
    Clear_Flash_Init(4);

    // Ensure both peers start with identical timer state regardless of local DIP switch settings.
    // Without this, save_w[Present_Mode].Time_Limit can differ per player's config.
    Counter_hi = 99;
    Counter_low = 60;

    // Flash_Complete runs during the character select screen at slightly different
    // speeds per peer depending on when they connected. Zero it to sync.
    Flash_Complete[0] = 0;
    Flash_Complete[1] = 0;

    // BG scroll positions and parameters evolve independently during the transition
    // phase before synced gameplay. Zero them so both peers start identical.
    SDL_zeroa(bg_pos);
    SDL_zeroa(fm_pos);
    SDL_zeroa(bg_prm);
    Screen_Switch = 0;
    Screen_Switch_Buffer = 0;
    system_timer = 0;
    Interrupt_Timer = 0;

    // Order[] tracks rendering layer visibility for character select UI elements.
    // Weak_PL picks the weaker CPU during demo/attract mode via random_16().
    // Both diverge per peer before battle; zero them for a clean start.
    SDL_zeroa(Order);
    Weak_PL = 0;

    // Force identity button config for MODE_NETWORK so Convert_User_Setting()
    // is a no-op during simulation. Each player's actual config was already
    // baked into their input by get_inputs() via Remap_Buttons().
    {
        const u8 identity[8] = { 0, 1, 2, 11, 3, 4, 5, 11 };
        for (int p = 0; p < 2; p++) {
            for (int s = 0; s < 8; s++)
                save_w[MODE_NETWORK].Pad_Infor[p].Shot[s] = identity[s];
            save_w[MODE_NETWORK].Pad_Infor[p].Vibration = 0;
        }
    }

    // Apply first-to-X wins: FT controls how many GAME wins are needed for a session
    // (tracked server-side). Each individual game is always best-of-3 rounds
    // (Battle_Number = 1, meaning need 2 round wins per game).
    {
        int ft = s_negotiated_ft > 0 ? s_negotiated_ft : Config_GetInt(CFG_KEY_NETPLAY_FT);
        if (ft < 1) ft = 2;
        if (ft > 10) ft = 10;
        // Store FT for match reporting (server-side session tracking)
        s_negotiated_ft = ft; // Keep for match reporting, will be consumed there
        // Rounds per game: always best-of-3 (need 2 round wins)
        save_w[MODE_NETWORK].Battle_Number[0] = 1; // 1 + 1 = 2 round wins needed
        save_w[MODE_NETWORK].Battle_Number[1] = 1; // 1 + 1 = 2 round wins needed
    }

    // Check_Buff and Convert_Buff hold per-player button remapping tables.
    // Each peer loads them from their local config, so they differ between
    // players. Zero them so the simulation uses identity mappings.
    SDL_zeroa(Check_Buff);
    SDL_zeroa(Convert_Buff);

    // Timers that evolved independently during menus/transition.
    // Without this, Game_timer and Control_Time diverge immediately.
    Game_timer = 0;
    Control_Time = 0;
    players_timer = 0;
    G_Timer = 0;

    // Per-player globals that can hold stale values from the previous
    // game session or differ based on who connected first.
    Champion = 0;
    Forbid_Break = 0;
    Connect_Status = 0;
    Stop_SG = 0;
    Exec_Wipe = 0;
    Gap_Timer = 0;
    SDL_zeroa(E_No);

    // State machine routing numbers evolve per-player during character select.
    // Each peer advances C_No/SC_No from its own perspective, causing them to
    // diverge before battle. Zero them so both peers start identical.
    SDL_zeroa(C_No);
    SDL_zeroa(SC_No);

    clean_input_buffers();
}

#if defined(LOSSY_ADAPTER)
static void configure_lossy_adapter() {
    base_adapter = gekko_default_adapter(local_port);
    lossy_adapter.send_data = LossyAdapter_SendData;
    lossy_adapter.receive_data = base_adapter->receive_data;
    lossy_adapter.free_data = base_adapter->free_data;
}
#endif

static int compute_delay_from_ping(float avg_ping, float jitter) {
    float effective_rtt = avg_ping + jitter;
    if (effective_rtt < 30.0f)
        return 0;
    if (effective_rtt < 70.0f)
        return 1;
    if (effective_rtt < 130.0f)
        return 2;
    if (effective_rtt < 200.0f)
        return 3;
    return DELAY_FRAMES_MAX;
}

static void configure_gekko() {
    GekkoConfig config;
    SDL_zero(config);

    config.num_players = PLAYER_COUNT;
    config.input_size = sizeof(u16);
    config.state_size = sizeof(State);
    config.max_spectators = 4;
    config.input_prediction_window = 12;

    config.desync_detection = true;

    if (gekko_create(&session, GekkoGameSession)) {
        gekko_start(session, &config);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[netplay] Session is already running! probably incorrect.");
    }

    if (stun_socket != NULL) {
        // Internet play: reuse the hole-punched STUN socket
        gekko_net_adapter_set(session, SDLNetAdapter_Create(stun_socket));
        SDL_Log("Using STUN socket for GekkoNet adapter");
    } else {
#if defined(LOSSY_ADAPTER)
        configure_lossy_adapter();
        gekko_net_adapter_set(session, &lossy_adapter);
#else
        gekko_net_adapter_set(session, gekko_default_adapter(local_port));
#endif
    }

    SDL_Log("[netplay] starting a session for player %d at port %hu", player_number, local_port);



    char remote_address_str[100];
    if (remote_ip) {
        SDL_snprintf(remote_address_str, sizeof(remote_address_str), "%s:%hu", remote_ip, remote_port);
    } else {
        SDL_snprintf(remote_address_str, sizeof(remote_address_str), "127.0.0.1:%hu", remote_port);
    }
    GekkoNetAddress remote_address = { .data = remote_address_str, .size = strlen(remote_address_str) };

    for (int i = 0; i < PLAYER_COUNT; i++) {
        const bool is_local_player = (i == player_number);

        if (is_local_player) {
            player_handle = gekko_add_actor(session, GekkoLocalPlayer, NULL);
            gekko_set_local_delay(session, player_handle, DELAY_FRAMES_DEFAULT);
        } else {
            gekko_add_actor(session, GekkoRemotePlayer, &remote_address);
        }
    }
}

static u16 get_inputs() {
    // The game doesn't differentiate between controllers and players.
    // That's why we OR the inputs of both local controllers together to get
    // local inputs.
    u16 inputs = p1sw_buff | p2sw_buff;

    // Pre-apply local player's button config so their preferred layout
    // is baked into the input before sending over the network.
    // Each player configures their buttons locally as P1 (Pad_Infor[0]).
    // During simulation, Convert_User_Setting uses identity Pad_Infor
    // (set in setup_vs_mode), so the remapping only happens here.
    inputs = Remap_Buttons(inputs, &save_w[1].Pad_Infor[0]);

    return inputs;
}

static void note_input(u16 input, int player, int frame) {
    if (frame < 0) {
        return;
    }

    input_history[player][frame % INPUT_HISTORY_MAX] = input;
}

static u16 recall_input(int player, int frame) {
    if (frame < 0) {
        return 0;
    }

    return input_history[player][frame % INPUT_HISTORY_MAX];
}

static bool game_ready_to_run_character_select() {
    return G_No[1] == 1;
}

static bool need_to_catch_up() {
    return frames_behind >= 1;
}

/**
 * @brief Execute one game simulation tick.
 *
 * @netplay_sync
 * This is the atomic unit of deterministic simulation. GekkoNet calls this
 * once per frame during normal play and multiple times during rollback replay.
 *
 * The sequence is:
 *  1. SDLGameRenderer_ResetBatchState() — prevent texture stack overflow during
 *     rapid rollback replays (each frame pushes to the stack via SetTexture).
 *  2. No_Trans = !render — skip rendering during rollback replay frames.
 *  3. njUserMain() — the game's main tick function.
 *  4. seqsBeforeProcess() / seqsAfterProcess() — pre/post frame hooks.
 *  5. Renderer_Flush2DPrimitives() — flush 2D draw calls between hooks.
 */
static void step_game(bool render) {
    // Reset renderer texture stack between sub-frames.
    // During rollback, GekkoNet replays many game frames within a single
    // outer frame. Each frame pushes to the texture stack via SetTexture().
    // Without this reset, the stack overflows past FL_PALETTE_MAX.
    // NOTE: Must be at the START — if at the end, it clears the final
    // frame's render tasks before RenderFrame can draw them.
    SDLGameRenderer_ResetBatchState();

    No_Trans = !render;

    njUserMain();
    seqsBeforeProcess();
    Renderer_Flush2DPrimitives();
    seqsAfterProcess();
}

/**
 * @brief Advance one game frame with the given inputs — GekkoNet callback.
 *
 * @netplay_sync
 * Called by GekkoNet for each AdvanceEvent (both during normal play and
 * rollback replay). Injects the confirmed inputs for both players into the
 * game's input globals:
 *
 *  - PLsw[p][0] (current frame) ← inputs[p] from GekkoNet
 *  - PLsw[p][1] (previous frame) ← recall_input(p, frame - 1)
 *  - p1sw_0/p2sw_0 ← mirrored copies for legacy code paths
 *
 * Input history is recorded via note_input() for future previous-frame lookups.
 * Then step_game() runs the actual simulation tick.
 */
static void advance_game(const GekkoGameEvent* event, bool render) {
    const u16* inputs = (u16*)event->data.adv.inputs;
    const int frame = event->data.adv.frame;

    p1sw_0 = PLsw[0][0] = inputs[0];
    p2sw_0 = PLsw[1][0] = inputs[1];
    p1sw_1 = PLsw[0][1] = recall_input(0, frame - 1);
    p2sw_1 = PLsw[1][1] = recall_input(1, frame - 1);

    note_input(inputs[0], 0, frame);
    note_input(inputs[1], 1, frame);

    step_game(render);
}

static void process_session() {
    frames_behind = -gekko_frames_ahead(session);

    gekko_network_poll(session);

    u16 local_inputs = get_inputs();
    gekko_add_local_input(session, player_handle, &local_inputs);

    int session_event_count = 0;
    GekkoSessionEvent** session_events = gekko_session_events(session, &session_event_count);

    for (int i = 0; i < session_event_count; i++) {
        const GekkoSessionEvent* event = session_events[i];

        switch (event->type) {
        case GekkoPlayerSyncing:
            SDL_Log("[netplay] player syncing");
            push_event(NETPLAY_EVENT_SYNCHRONIZING);
            break;

        case GekkoPlayerConnected:
            SDL_Log("[netplay] player connected");
            push_event(NETPLAY_EVENT_CONNECTED);
            break;

        case GekkoPlayerDisconnected:
            SDL_Log("[netplay] player disconnected");
            push_event(NETPLAY_EVENT_DISCONNECTED);
            if (session_state != NETPLAY_SESSION_EXITING && session_state != NETPLAY_SESSION_IDLE) {
                clean_input_buffers();
                Soft_Reset_Sub();
                session_state = NETPLAY_SESSION_EXITING;
            }
            break;

        case GekkoSessionStarted:
            SDL_Log("[netplay] session started");
            session_state = NETPLAY_SESSION_RUNNING;
            break;

        case GekkoDesyncDetected: {
            const int frame = event->data.desynced.frame;
            printf("⚠️ desync detected at frame %d (local: 0x%08x, remote: 0x%08x)\n",
                   frame,
                   event->data.desynced.local_checksum,
                   event->data.desynced.remote_checksum);

#if DEBUG
            dump_desync_state(frame, event->data.desynced.local_checksum, event->data.desynced.remote_checksum);
#endif

            // Treat desync like a disconnect: clean up and exit immediately
            // (no blocking message box — that freezes the game loop)
            SDL_Log("[netplay] Desync at frame %d — terminating session", frame);
            push_event(NETPLAY_EVENT_DISCONNECTED);
            clean_input_buffers();
            Soft_Reset_Sub();
            session_state = NETPLAY_SESSION_EXITING;
            break;
        }

        case GekkoEmptySessionEvent:
        case GekkoSpectatorPaused:
        case GekkoSpectatorUnpaused:
            // Do nothing
            break;
        }
    }
}

static void process_events(bool drawing_allowed) {
    int game_event_count = 0;
    GekkoGameEvent** game_events = gekko_update_session(session, &game_event_count);
    int frames_rolled_back = 0;



    for (int i = 0; i < game_event_count; i++) {
        const GekkoGameEvent* event = game_events[i];

        switch (event->type) {
        case GekkoLoadEvent:
            load_state_from_event(event);
            break;

        case GekkoAdvanceEvent: {
            const bool rolling_back = event->data.adv.rolling_back;
            advance_game(event, drawing_allowed && !rolling_back);
            frames_rolled_back += rolling_back ? 1 : 0;
            break;
        }

        case GekkoSaveEvent:
            save_state(event);
            break;

        case GekkoEmptyGameEvent:
            // Do nothing
            break;
        }
    }

    frame_max_rollback = SDL_max(frame_max_rollback, frames_rolled_back);
}

static void step_logic(bool drawing_allowed) {
    process_session();
    process_events(drawing_allowed);
}

static void update_network_stats() {
    // Accumulate ping samples for dynamic delay (before battle starts)
    if (!dynamic_delay_applied) {
        if (ping_sample_timer <= 0) {
            GekkoNetworkStats ns;
            gekko_network_stats(session, player_handle ^ 1, &ns);
            if (ns.avg_ping >= 0) {
                ping_sum += ns.avg_ping;
                jitter_sum += ns.jitter;
                ping_sample_count++;
            }
            ping_sample_timer = PING_SAMPLE_INTERVAL;
        }
        ping_sample_timer--;
    }

    if (stats_update_timer == 0) {
        GekkoNetworkStats net_stats;
        gekko_network_stats(session, player_handle ^ 1, &net_stats);

        network_stats.ping = net_stats.avg_ping;
        network_stats.delay = dynamic_delay;

        if (frame_max_rollback < network_stats.rollback) {
            // Don't decrease the reading by more than a frame to account for
            // the opponent not pressing buttons for 1-2 seconds
            network_stats.rollback -= 1;
        } else {
            network_stats.rollback = frame_max_rollback;
        }

        frame_max_rollback = 0;
        stats_update_timer = STATS_UPDATE_TIMER_MAX;
    }

    stats_update_timer -= 1;
    stats_update_timer = SDL_max(stats_update_timer, 0);
}

static void run_netplay() {
    // Apply dynamic delay once when battle starts
    if (!dynamic_delay_applied && G_No[1] == 2) {
        if (ping_sample_count > 0) {
            float avg = ping_sum / ping_sample_count;
            float jitter_avg = jitter_sum / ping_sample_count;
            dynamic_delay = compute_delay_from_ping(avg, jitter_avg);
        } else {
            dynamic_delay = DELAY_FRAMES_DEFAULT;
        }
        gekko_set_local_delay(session, player_handle, dynamic_delay);
        SDL_Log("[netplay] dynamic delay set to %d (samples=%d, avg_ping=%.1f, jitter=%.1f)",
                dynamic_delay,
                ping_sample_count,
                ping_sample_count > 0 ? ping_sum / ping_sample_count : 0.f,
                ping_sample_count > 0 ? jitter_sum / ping_sample_count : 0.f);
        dynamic_delay_applied = true;
    }

    // Step

    const bool catch_up = need_to_catch_up() && (frame_skip_timer == 0);
    step_logic(!catch_up);

    if (catch_up) {
        step_logic(true);
        frame_skip_timer = FRAME_SKIP_TIMER_MAX;
    }

    frame_skip_timer -= 1;
    frame_skip_timer = SDL_max(frame_skip_timer, 0);

    // Update stats

    update_network_stats();
}

void Netplay_SetPlayerNumber(int player_num) {
    SDL_assert(player_num == 0 || player_num == 1);
    player_number = player_num;
}

int Netplay_GetPlayerNumber(void) {
    return player_number;
}

void Netplay_SetRemoteIP(const char* ip) {
    if (ip) {
        SDL_strlcpy(remote_ip_string, ip, sizeof(remote_ip_string));
        remote_ip = remote_ip_string;
    } else {
        remote_ip = NULL;
    }
}

void Netplay_SetLocalPort(unsigned short port) {
    local_port = port;
}

void Netplay_SetRemotePort(unsigned short port) {
    remote_port = port;
}

void Netplay_SetStunSocket(NET_DatagramSocket* socket) {
    // If we already hold a STUN socket, close it first
    if (stun_socket != NULL && stun_socket != socket) {
        NET_DestroyDatagramSocket(stun_socket);
    }
    stun_socket = socket;
}

void Netplay_SetNegotiatedFT(int ft) {
    s_negotiated_ft = ft;
}

int Netplay_GetNegotiatedFT(void) {
    return s_negotiated_ft;
}

void Netplay_Begin() {
    /* Hide the RmlUI lobby overlay on connection (safe no-op if not shown) */
    rmlui_network_lobby_hide();

    setup_vs_mode();
    Discovery_Shutdown();

    SDL_zeroa(input_history);
    frames_behind = 0;
    frame_skip_timer = 0;
    transition_ready_frames = 0;

    // Reset dynamic delay sampling for this session
    dynamic_delay = DELAY_FRAMES_DEFAULT;
    dynamic_delay_applied = false;
    ping_sum = 0;
    jitter_sum = 0;
    ping_sample_count = 0;
    ping_sample_timer = 0;

#if DEBUG
    // Removed because battle_start_frame is now effectively private in game_state.c
    // and correctly managed by save_state() etc.
#ifdef _WIN32
    _mkdir("states");
#else
    mkdir("states", 0777);
#endif
#endif

    session_state = NETPLAY_SESSION_TRANSITIONING;

    SDL_Log("[netplay] *** BEGIN: local player = P%d (slot %d), remote = %s:%hu, local port = %hu ***",
            player_number + 1, player_number,
            remote_ip ? remote_ip : "(null)", remote_port, local_port);
}

void Netplay_EnterLobby() {
    session_state = NETPLAY_SESSION_LOBBY;
    handshake_ready_since = 0;
    Discovery_Init(Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT));
}

void Netplay_Run() {
    switch (session_state) {
    case NETPLAY_SESSION_LOBBY:
        Discovery_Update();

        {
            bool local_auto = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
            uint32_t local_challenge = Discovery_GetChallengeTarget();
            bool should_be_ready = false;
            NetplayDiscoveredPeer* target_peer = NULL;

            NetplayDiscoveredPeer peers[16];
            int count = Discovery_GetPeers(peers, 16);

            bool we_initiated = false;
            for (int i = 0; i < count; i++) {
                // If we explicitly challenge them AND they explicitly challenge us OR have auto-connect on
                if (local_challenge == peers[i].instance_id) {
                    if (peers[i].is_challenging_me || peers[i].wants_auto_connect) {
                        target_peer = &peers[i];
                        should_be_ready = true;
                        // Mutual challenge (both challenged each other, e.g. accept = challenge back).
                        // Use instance_id tiebreaker so both peers agree on who initiated.
                        if (peers[i].is_challenging_me) {
                            uint32_t local_id = Discovery_GetLocalInstanceID();
                            if (local_id != peers[i].instance_id) {
                                we_initiated = (local_id < peers[i].instance_id);
                            } else {
                                // ID collision (same-machine same-binary): tiebreak by port
                                we_initiated = (configuration.netplay.port < target_peer->port);
                            }
                        } else {
                            we_initiated = true; // Only we challenged, they have auto-connect
                        }
                        break;
                    }
                }

                // If they explicitly challenge us AND we have auto-connect on
                if (peers[i].is_challenging_me && local_auto) {
                    target_peer = &peers[i];
                    should_be_ready = true;
                    we_initiated = false; // They initiated
                    break;
                }

                // If both have auto-connect on
                if (local_auto && peers[i].wants_auto_connect) {
                    target_peer = &peers[i];
                    should_be_ready = true;
                    // Tiebreaker: lower instance ID = P1 (initiator)
                    uint32_t local_id = Discovery_GetLocalInstanceID();
                    if (local_id != peers[i].instance_id) {
                        we_initiated = (local_id < peers[i].instance_id);
                    } else {
                        // ID collision (same-machine same-binary): tiebreak by port
                        we_initiated = (configuration.netplay.port < target_peer->port);
                    }
                    break;
                }
            }

            Discovery_SetReady(should_be_ready);

            if (should_be_ready && target_peer && target_peer->peer_ready) {
                if (handshake_ready_since == 0) {
                    handshake_ready_since = SDL_GetTicks();
                    uint32_t local_id = Discovery_GetLocalInstanceID();
                    SDL_Log("[netplay] LAN handshake: local_id=0x%08X peer_id=0x%08X "
                            "we_initiated=%d → will be P%d",
                            local_id, target_peer->instance_id,
                            we_initiated, we_initiated ? 1 : 2);
                }
                // Hold for 1 second to let peer also process our ready beacon
                if (SDL_GetTicks() - handshake_ready_since >= 1000) {
                    handshake_ready_since = 0;
                    Discovery_SetReady(false);
                    Discovery_SetChallengeTarget(0);
                    // Initiator = P1 (0), Receiver = P2 (1)
                    Netplay_SetPlayerNumber(we_initiated ? 0 : 1);
                    Netplay_SetRemoteIP(target_peer->ip);
                    Netplay_SetRemotePort(target_peer->port);
                    Netplay_SetLocalPort(configuration.netplay.port);
                    SDLNetplayUI_SetNativeLobbyActive(false);
                    Netplay_Begin();
                }
            } else {
                handshake_ready_since = 0;
            }
        }
        break;

    case NETPLAY_SESSION_TRANSITIONING:
        if (game_ready_to_run_character_select()) {
            transition_ready_frames += 1;
            if (transition_ready_frames == 1)
                printf("[netplay] character select reached (G_No[1]=%d)\n", G_No[1]);
        } else {
            transition_ready_frames = 0;
            // Keep both peers in a deterministic pre-session state by
            // ignoring local controller input while transitioning into
            // character select.
            clean_input_buffers();
            step_game(true);
        }

        if (transition_ready_frames >= 2) {
            printf("[netplay] transition done, configuring gekko\n");
            configure_gekko();
            session_state = NETPLAY_SESSION_CONNECTING;
        }

        break;

    case NETPLAY_SESSION_CONNECTING:
    case NETPLAY_SESSION_RUNNING:
        run_netplay();
        break;

    case NETPLAY_SESSION_EXITING:
        if (session != NULL) {
            // cleanup session and then return to idle
            gekko_destroy(&session);

            // Close STUN socket if we used it for this session
            if (stun_socket != NULL) {
                SDLNetAdapter_Destroy();  // Release cached DNS before destroying socket
                NET_DestroyDatagramSocket(stun_socket);
                stun_socket = NULL;
            }

#ifndef LOSSY_ADAPTER
            // also cleanup default socket.
            gekko_default_adapter_destroy();
#endif
        }

        // If we're in a casual room, re-enter LOBBY instead of IDLE so the
        // game stays in menu/lobby mode and doesn't restart its init flow.
        // Soft_Reset_Sub (called during disconnect) hides all RmlUI documents,
        // so we need to re-show the casual lobby overlay.
        {
            const char* room = rmlui_casual_lobby_get_room_code();
            if (room && room[0]) {
                session_state = NETPLAY_SESSION_LOBBY;
                Discovery_Init(false);  // Restart LAN beacons for casual room LAN shortcut
                rmlui_casual_lobby_show();
                // Park the game engine in an idle state so no game logic runs
                // behind the room overlay while waiting for the next match.
                task[TASK_INIT].r_no[0] = 0;
                task[TASK_INIT].r_no[1] = 0;
                task[TASK_INIT].condition = 0;
                task[TASK_GAME].condition = 0;
                SDL_Log("[netplay] Re-entering LOBBY for casual room %s", room);
            } else {
                Discovery_Shutdown();
                session_state = NETPLAY_SESSION_IDLE;
            }
        }
        break;

    case NETPLAY_SESSION_IDLE:
        break;

    case NETPLAY_SESSION_SPECTATING:
        if (session) {
            gekko_network_poll(session);

            // Process session events (connected/disconnected/paused/unpaused)
            int sess_count = 0;
            GekkoSessionEvent** sess_events = gekko_session_events(session, &sess_count);
            for (int i = 0; i < sess_count; i++) {
                const GekkoSessionEvent* event = sess_events[i];
                switch (event->type) {
                case GekkoPlayerConnected:
                    SDL_Log("[spectate] connected to host");
                    push_event(NETPLAY_EVENT_CONNECTED);
                    break;
                case GekkoPlayerDisconnected:
                    SDL_Log("[spectate] host disconnected");
                    push_event(NETPLAY_EVENT_DISCONNECTED);
                    Netplay_StopSpectate();
                    return;
                case GekkoSpectatorPaused:
                    SDL_Log("[spectate] paused (buffering)");
                    break;
                case GekkoSpectatorUnpaused:
                    SDL_Log("[spectate] unpaused");
                    break;
                default:
                    break;
                }
            }

            // Process game events — spectators only receive advance + load, no saves
            int game_count = 0;
            GekkoGameEvent** game_events = gekko_update_session(session, &game_count);
            for (int i = 0; i < game_count; i++) {
                const GekkoGameEvent* event = game_events[i];
                switch (event->type) {
                case GekkoLoadEvent:
                    load_state_from_event(event);
                    break;
                case GekkoAdvanceEvent:
                    advance_game(event, true); // Always render for spectators
                    break;
                case GekkoSaveEvent:
                    save_state(event);
                    break;
                default:
                    break;
                }
            }
        }
        break;
    }
}

NetplaySessionState Netplay_GetSessionState() {
    return session_state;
}

void Netplay_HandleMenuExit() {
    switch (session_state) {
    case NETPLAY_SESSION_IDLE:
    case NETPLAY_SESSION_EXITING:
        // Do nothing
        break;

    case NETPLAY_SESSION_LOBBY:
        Discovery_Shutdown();
        session_state = NETPLAY_SESSION_IDLE;
        break;

    case NETPLAY_SESSION_TRANSITIONING:
    case NETPLAY_SESSION_CONNECTING:
    case NETPLAY_SESSION_RUNNING:
    case NETPLAY_SESSION_SPECTATING:
        session_state = NETPLAY_SESSION_EXITING;
        break;
    }
}

// === 3SX-private extensions ===

#define EVENT_QUEUE_MAX 8
static NetplayEvent event_queue[EVENT_QUEUE_MAX];
static int event_queue_count = 0;

bool Netplay_IsEnabled() {
    return session_state != NETPLAY_SESSION_IDLE && session_state != NETPLAY_SESSION_SPECTATING;
}

void Netplay_GetNetworkStats(NetworkStats* stats) {
    if (stats) {
        SDL_copyp(stats, &network_stats);
    }
}

static void push_event(NetplayEventType type) {
    if (event_queue_count < EVENT_QUEUE_MAX) {
        event_queue[event_queue_count].type = type;
        event_queue_count++;
    }
}

bool Netplay_PollEvent(NetplayEvent* out) {
    if (!out || event_queue_count == 0)
        return false;
    *out = event_queue[0];
    // shift queue
    for (int i = 1; i < event_queue_count; i++) {
        event_queue[i - 1] = event_queue[i];
    }
    event_queue_count--;
    return true;
}

int Netplay_GetPlayerHandle(void) {
    return player_handle;
}
int Netplay_GetBattleStartFrame(void) {
    return -1;
}

void Netplay_BeginSpectate(const char* host_ip, unsigned short host_port) {
    if (session_state != NETPLAY_SESSION_IDLE) {
        SDL_Log("[spectate] cannot start: session state is %d", session_state);
        return;
    }

    GekkoConfig config;
    SDL_zero(config);
    config.num_players = PLAYER_COUNT;
    config.input_size = sizeof(u16);
    config.state_size = sizeof(State);
    config.max_spectators = 1;
    config.spectator_delay = 15; // 15 frames (~250ms at 60fps)
    config.input_prediction_window = 12;
    config.desync_detection = false; // Spectators don't need desync detection

    if (!gekko_create(&session, GekkoSpectateSession)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[spectate] failed to create session");
        return;
    }

    gekko_start(session, &config);
    gekko_net_adapter_set(session, gekko_default_adapter(0)); // OS-assigned port

    // Connect to the match host as a spectator
    char addr_str[100];
    SDL_snprintf(addr_str, sizeof(addr_str), "%s:%hu", host_ip, host_port);
    GekkoNetAddress addr = { .data = addr_str, .size = (unsigned int)strlen(addr_str) };
    gekko_add_actor(session, GekkoSpectator, &addr);

    setup_vs_mode();
    session_state = NETPLAY_SESSION_SPECTATING;
    SDL_Log("[spectate] connecting to %s", addr_str);
}

void Netplay_StopSpectate(void) {
    if (session_state != NETPLAY_SESSION_SPECTATING)
        return;

    if (session) {
        gekko_destroy(&session);
        gekko_default_adapter_destroy();
    }

    clean_input_buffers();
    Soft_Reset_Sub();
    session_state = NETPLAY_SESSION_IDLE;
    SDL_Log("[spectate] stopped");
}
