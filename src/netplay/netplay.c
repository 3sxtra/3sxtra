#include "netplay.h"
#include "discovery.h"

#include "game_state.h"
#define Game GekkoGame // workaround: upstream GekkoSessionType::Game collides with void Game()
#include "gekkonet.h"
#undef Game
#include "sdl_net_adapter.h"
#include "main.h"
#include "arcade/arcade_char_data.h"
#include "port/config/config.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/player_control.h"
#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/game.h"
#include "sf33rd/Source/Game/engine/slow_motion.h"
#include "sf33rd/Source/Game/io/afs_loader.h"
#include "sf33rd/Source/Game/io/rumble.h"
#include "sf33rd/Source/Game/rendering/color_palette.h"
#include "stun.h"
#include "net_tuning.h"
#include "lobby_server.h"
// dc_ghost.h does not exist in our repo; njdp2d_draw was renamed to Renderer_Flush2DPrimitives.
#include "port/rendering/renderer.h"
extern void njUserMain();
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "port/sdl/renderer/sdl_game_renderer.h"
#include "port/sdl/rmlui/rmlui_casual_lobby.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"
#include "port/menu_screen.h"
#include "port/sdl/rmlui/rmlui_ingame_chat.h"
#include "sf33rd/Source/Game/menu/menu.h"
#include "port/menu_task.h"
#include "sf33rd/Source/Game/rendering/rendering_transform.h"
#include "sf33rd/Source/Game/rendering/texture_cache.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
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
// FRAME_SKIP_TIMER_MAX is now dynamic
#define STATS_UPDATE_TIMER_MAX 60
#define DELAY_FRAMES_DEFAULT 1
#define DELAY_FRAMES_MAX 5
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
static NET_DatagramSocket* stun_socket = NULL;     // Pre-punched STUN socket for internet play
static NET_DatagramSocket* fallback_socket = NULL; // Created when STUN fails; same API as stun_socket
static int s_negotiated_ft = 0;            // FT value agreed upon for the upcoming match (0 = use config default)
static uint32_t handshake_ready_since = 0; // Ticks when both peers signaled ready (LAN handshake hold)
static NetplaySessionState session_state = NETPLAY_SESSION_IDLE;
static u16 input_history[2][INPUT_HISTORY_MAX] = { 0 };
static float frames_behind = 0;
static int frame_skip_timer = 0;
static int transition_ready_frames = 0;

static int stats_update_timer = 0;
static int frame_max_rollback = 0;
static NetworkStats network_stats = { 0 };

// --- Dynamic tuning from ping ---
static int dynamic_delay = DELAY_FRAMES_DEFAULT;
static int dynamic_frame_skip_max = 3;
static bool dynamic_delay_applied = false;
static float ping_sum = 0;
static float jitter_sum = 0;
static int ping_sample_count = 0;
static int ping_sample_timer = 0;

static MenuScreenId s_netplay_origin_screen = MENU_SCREEN_NONE;
static bool s_netplay_origin_native_lan = false;

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
    SDL_zeroa(g_state.PLsw);
    SDL_zeroa(g_state.plsw_00);
    SDL_zeroa(g_state.plsw_01);
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
 *  - Task/state machine routing numbers (g_state.G_No, g_state.C_No, g_state.SC_No, g_state.E_No)
 *  - Mode and play type (MODE_NETWORK, g_state.Play_Mode)
 *  - Game settings (Time_Limit, Battle_Number, Damage_Level, etc.)
 *  - Timers (g_state.Game_timer, g_state.Control_Time, g_state.entry_timer, g_state.fsm_timer, etc.)
 *  - RNG indices (g_state.Random_ix16, g_state.Random_ix32)
 *  - Button config (Pad_Infor forced to identity, g_state.Check_Buff/g_state.Convert_Buff zeroed)
 *  - Background state (g_state.bg_pos, g_state.fm_pos, g_state.bg_prm, g_state.Screen_Switch)
 *  - Per-player globals (g_state.Champion, g_state.Connect_Status, g_state.Operator_Status, etc.)
 *  - Input buffers (clean_input_buffers)
 */
static void setup_vs_mode() {
    // ====================================================================
    // PHASE 0: Zero per-player and combat subsystem state.
    //
    // When connecting from the network lobby, the game engine may have been
    // running under the RmlUI overlay (attract mode, demo, etc.), leaving
    // stale data in PlayerEntity[] and related player subsystems.
    // The native LAN lobby doesn't hit this because the menu system goes
    // through a proper fade-destroy-reinit cycle.
    //
    // We only zero player/combat state — NOT engine globals (g_state.G_No, g_state.Country,
    // task routing, etc.) because step_game() runs during TRANSITIONING
    // to advance the game state machine (g_state.fsm[1]: 12→1).
    // ====================================================================
    SDL_zeroa(g_state.plw);
    SDL_zeroa(g_state.afterimage_table);
    SDL_zeroa(g_state.super_arts);

    // Task timers and scratch data evolve independently per peer during menus.
    // Zero them for deterministic start. DO NOT zero r_no or condition —
    // those are game state machine routing fields set by the engine.
    for (int i = 0; i < 11; i++) {
        task[i].timer = 0;
        SDL_zeroa(task[i].free);
    }

    // This is pretty much a copy of logic from menu.c
    MenuTask_SetPhase(MTP_NETPLAY_IDLE); // go to idle routine (doing nothing)
    cpExitTask(TASK_SAVER);
    cpExitTask(TASK_PAUSE);

    // Zero pause flags — if one peer was paused before entering netplay,
    // these would differ on the first synced frame.
    g_state.Pause = 0;
    g_state.Game_pause = 0;

    // Re-set after zeroing g_state.plw — both players must be active for 2P mode
    g_state.plw[0].wu.pl_operator = 1;
    g_state.plw[1].wu.pl_operator = 1;
    g_state.Operator_Status[0] = 1;
    g_state.Operator_Status[1] = 1;
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

    g_state.fsm[0] = 2;
    g_state.entry_phase[0] = 1;
    g_state.Demo_Flag = 1;

    g_state.fsm[1] = 12;
    g_state.fsm[2] = 1;
    g_state.Mode_Type = MODE_NETWORK;
    g_state.Present_Mode = MODE_NETWORK;
    g_state.Play_Mode = 0;
    g_state.Replay_Status[0] = 0;
    g_state.Replay_Status[1] = 0;
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

    g_state.entry_timer =
        0; // g_state.entry_timer can have different values depending on when the session was initiated

    g_state.Deley_Shot_No[0] = 0;
    g_state.Deley_Shot_No[1] = 0;
    g_state.Deley_Shot_Timer[0] = 15;
    g_state.Deley_Shot_Timer[1] = 15;
    g_state.Random_ix16 = 0;
    g_state.Round_num = 0;
    g_state.Game_timer = 0;
    g_state.Random_ix32 = 0;
    Clear_Flash_Init(4);

    // Ensure both peers start with identical timer state regardless of local DIP switch settings.
    // Without this, CurrentSave()->Time_Limit can differ per player's config.
    g_state.Counter_hi = 99;
    g_state.Counter_low = 60;

    // g_state.Flash_Complete runs during the character select screen at slightly different
    // speeds per peer depending on when they connected. Zero it to sync.
    g_state.Flash_Complete[0] = 0;
    g_state.Flash_Complete[1] = 0;

    // BG scroll positions and parameters evolve independently during the transition
    // phase before synced gameplay. Zero them so both peers start identical.
    SDL_zeroa(g_state.bg_pos);
    SDL_zeroa(g_state.fm_pos);
    SDL_zeroa(g_state.bg_prm);
    g_state.Screen_Switch = 0;
    g_state.Screen_Switch_Buffer = 0;
    g_state.system_timer = 0;
    Interrupt_Timer = 0;

    // g_state.Order[] tracks rendering layer visibility for character select UI elements.
    // g_state.Weak_PL picks the weaker CPU during demo/attract mode via random_16().
    // Both diverge per peer before battle; zero them for a clean start.
    SDL_zeroa(g_state.Order);
    g_state.Weak_PL = 0;

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
        if (ft < 1)
            ft = 2;
        if (ft > 10)
            ft = 10;
        // Store FT for match reporting (server-side session tracking)
        s_negotiated_ft = ft; // Keep for match reporting, will be consumed there
        // Rounds per game: always best-of-3 (need 2 round wins)
        save_w[MODE_NETWORK].Battle_Number[0] = 1; // 1 + 1 = 2 round wins needed
        save_w[MODE_NETWORK].Battle_Number[1] = 1; // 1 + 1 = 2 round wins needed
    }

    // g_state.Check_Buff and g_state.Convert_Buff hold per-player button remapping tables.
    // Each peer loads them from their local config, so they differ between
    // players. Zero them so the simulation uses identity mappings.
    SDL_zeroa(g_state.Check_Buff);
    SDL_zeroa(g_state.Convert_Buff);

    // Timers that evolved independently during menus/transition.
    // Without this, g_state.Game_timer and g_state.Control_Time diverge immediately.
    g_state.Game_timer = 0;
    g_state.Control_Time = 0;
    g_state.players_timer = 0;
    g_state.fsm_timer = 0;

    // Per-player globals that can hold stale values from the previous
    // game session or differ based on who connected first.
    g_state.Champion = 0;
    g_state.Forbid_Break = 0;
    g_state.Connect_Status = 0;
    g_state.Stop_SG = 0;
    g_state.Exec_Wipe = 0;
    g_state.Gap_Timer = 0;
    SDL_zeroa(g_state.entry_phase);

    // State machine routing numbers evolve per-player during character select.
    // Each peer advances g_state.C_No/g_state.SC_No from its own perspective, causing them to
    // diverge before battle. Zero them so both peers start identical.
    SDL_zeroa(g_state.manage_phase);
    SDL_zeroa(g_state.next_cpu_phase);

    // ====================================================================
    // PHASE 3: Zero checksummed globals not covered above.
    //
    // save_state() checksums a whitelist of gameplay-critical fields.
    // On LAN, the native menu system's fade-destroy-reinit cycle leaves
    // these at zero already. On INTERNET (casual lobby), the game engine
    // runs under the RmlUI overlay (attract/demo mode), leaving stale
    // values that differ per peer and cause frame-0 desyncs.
    // ====================================================================

    // Extended RNG indices (attract mode advances these independently)
    g_state.Random_ix16_ex = 0;
    g_state.Random_ix32_ex = 0;
    g_state.Random_ix16_com = 0;
    g_state.Random_ix32_com = 0;
    g_state.Random_ix16_ex_com = 0;
    g_state.Random_ix32_ex_com = 0;

    // Round/match state
    g_state.Round_Level = 0;
    g_state.Round_Result = 0;
    SDL_zeroa(g_state.PL_Wins);
    g_state.Conclusion_Type = 0;
    SDL_zeroa(g_state.win_type);

    // Player identity (set later by character select, but must start clean)
    // Clean up stale attract/demo sequences that mutate start-of-match state
    g_state.Combo_Demo_Flag = 0;
    g_state.Select_Demo_Index = 0;
    g_state.Demo_Stage_Index = 0;
    g_state.Demo_PL_Index = 0;

    SDL_zeroa(g_state.My_char);
    SDL_zeroa(g_state.Super_Arts);

    // Combat flags (stale from previous match or attract mode demo fights)
    SDL_zeroa(g_state.Attack_Flag);
    SDL_zeroa(g_state.Counter_Attack);
    SDL_zeroa(g_state.Guard_Flag);
    SDL_zeroa(g_state.Flip_Flag);
    SDL_zeroa(g_state.Lie_Flag);
    SDL_zeroa(g_state.Attack_Counter);
    SDL_zeroa(g_state.Bullet_No);
    SDL_zeroa(g_state.Bullet_Counter);
    SDL_zeroa(g_state.parry_counter);

    // Game flow
    g_state.VS_Stage = 0;

    // Slow motion
    g_state.slowmo_timer = 0;
    g_state.slowmo_flag = 0;
    g_state.execute_flag = 0;

    // Stun gauge / vitality
    SDL_zeroa(g_state.stun_state);
    g_state.Max_vitality = 160; // MAX_VITALITY_DEFAULT — must not be 0 (setup_vitality divides by it)

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

static void compute_tuning_from_ping(float avg_ping, float jitter, int* out_delay, int* out_skip_max) {
    float effective_rtt = avg_ping + jitter;
    if (effective_rtt < 90.0f) {
        *out_delay = 0;
        *out_skip_max = 2;
    } else if (effective_rtt < 150.0f) {
        *out_delay = 1;
        *out_skip_max = 3;
    } else if (effective_rtt < 200.0f) {
        *out_delay = 3;
        *out_skip_max = 4;
    } else if (effective_rtt < 250.0f) {
        *out_delay = 4;
        *out_skip_max = 5;
    } else {
        *out_delay = 5;
        *out_skip_max = 5;
    }
}

static void configure_gekko() {
    GekkoConfig config;
    Discovery_Shutdown();
    SDL_zero(config);

    config.num_players = PLAYER_COUNT;
    config.input_size = sizeof(u16);
    config.state_size = sizeof(RollbackState);
    config.input_prediction_window = 8; // Absolute max 8 per recommendations

    config.desync_detection = true;

    if (gekko_create(&session, GekkoGameSession)) {
        gekko_start(session, &config);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[netplay] Session is already running! probably incorrect.");
    }

    if (stun_socket != NULL) {
        // Internet play: reuse the hole-punched STUN socket
        NetTuning_SetRecvBuf(stun_socket, 256 * 1024);
        gekko_net_adapter_set(session, SDLNetAdapter_Create(stun_socket));
        SDL_Log("Using active STUN socket for GekkoNet adapter (port matches Lobby Presence)");
    } else {
#if defined(LOSSY_ADAPTER)
        configure_lossy_adapter();
        gekko_net_adapter_set(session, &lossy_adapter);
#else
        // No STUN socket — create a new SDL3_Net dual-stack socket on the
        // configured netplay port. Using SDLNetAdapter (not ASIO default)
        // ensures consistent address formatting with the peer, which is
        // critical for GekkoNet's address-based sync handshake matching.
        fallback_socket = NET_CreateDatagramSocket(NULL, local_port);
        if (fallback_socket) {
            NetTuning_SetRecvBuf(fallback_socket, 256 * 1024);
            gekko_net_adapter_set(session, SDLNetAdapter_Create(fallback_socket));
            SDL_Log("Using SDL3_Net fallback socket for GekkoNet adapter (port %hu)", local_port);
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "[netplay] Failed to create fallback socket on port %hu: %s",
                         local_port,
                         SDL_GetError());
            gekko_net_adapter_set(session, gekko_default_adapter(local_port));
        }
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

    // Enable cross-IP normalization: if packets arrive from a different address
    // family (e.g. IPv6 instead of IPv4), the adapter rewrites the source to
    // match this expected address so GekkoNet's string matching still works.
    SDLNetAdapter_SetExpectedRemote(remote_address_str);

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
    // Suppress input while typing a chat message — send neutral (0)
    // so no phantom button presses go through the rollback session.
    if (rmlui_ingame_chat_is_typing())
        return 0;

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
    return g_state.fsm[1] == 1;
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
 *  1. No_Trans = !render — skip rendering during rollback replay frames.
 *  2. njUserMain() — the game's main tick function.
 *  3. seqsBeforeProcess() / seqsAfterProcess() — pre/post frame hooks.
 *  4. Renderer_Flush2DPrimitives() — flush 2D draw calls between hooks.
 */
static void step_game(bool render) {
    No_Trans = !render;

    njUserMain();
    Renderer_Flush2DPrimitives();
}

/**
 * @brief Advance one game frame with the given inputs — GekkoNet callback.
 *
 * @netplay_sync
 * Called by GekkoNet for each AdvanceEvent (both during normal play and
 * rollback replay). Injects the confirmed inputs for both players into the
 * game's input globals:
 *
 *  - g_state.PLsw[p][0] (current frame) ← inputs[p] from GekkoNet
 *  - g_state.PLsw[p][1] (previous frame) ← recall_input(p, frame - 1)
 *  - p1sw_0/p2sw_0 ← mirrored copies for legacy code paths
 *
 * Input history is recorded via note_input() for future previous-frame lookups.
 * Then step_game() runs the actual simulation tick.
 */
static void advance_game(const GekkoGameEvent* event, bool render) {
    const u16* inputs = (u16*)event->data.adv.inputs;
    const int frame = event->data.adv.frame;

    p1sw_0 = g_state.PLsw[0][0] = inputs[0];
    p2sw_0 = g_state.PLsw[1][0] = inputs[1];
    p1sw_1 = g_state.PLsw[0][1] = recall_input(0, frame - 1);
    p2sw_1 = g_state.PLsw[1][1] = recall_input(1, frame - 1);

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

static bool process_events(bool drawing_allowed) {
    int game_event_count = 0;
    GekkoGameEvent** game_events = gekko_update_session(session, &game_event_count);
    int frames_rolled_back = 0;
    bool advanced = false;

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
            advanced = true;
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
    return advanced;
}

static bool step_logic(bool drawing_allowed) {
    process_session();
    return process_events(drawing_allowed);
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
    // Apply dynamic tuning once when battle starts
    if (!dynamic_delay_applied && g_state.fsm[1] == 2) {
        if (ping_sample_count > 0) {
            float avg = ping_sum / ping_sample_count;
            float jitter_avg = jitter_sum / ping_sample_count;
            compute_tuning_from_ping(avg, jitter_avg, &dynamic_delay, &dynamic_frame_skip_max);
        } else {
            dynamic_delay = DELAY_FRAMES_DEFAULT;
            dynamic_frame_skip_max = 3;
        }
        gekko_set_local_delay(session, player_handle, dynamic_delay);
        SDL_Log("[netplay] dynamic tuning set: delay=%d, skip_max=%d (samples=%d, avg_ping=%.1f, jitter=%.1f)",
                dynamic_delay,
                dynamic_frame_skip_max,
                ping_sample_count,
                ping_sample_count > 0 ? ping_sum / ping_sample_count : 0.f,
                ping_sample_count > 0 ? jitter_sum / ping_sample_count : 0.f);
        dynamic_delay_applied = true;
    }

    // Step

    const bool catch_up = need_to_catch_up() && (frame_skip_timer <= 0);
    step_logic(!catch_up);

    if (catch_up) {
        step_logic(true);
        frame_skip_timer = dynamic_frame_skip_max;
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
    MenuScreenId cur = MenuScreen_GetCurrent();
    if (cur == MENU_SCREEN_NETWORK_LOBBY || cur == MENU_SCREEN_CASUAL_LOBBY || cur == MENU_SCREEN_TOURNAMENT_LOBBY ||
        cur == MENU_SCREEN_RANKED_MATCHMAKING) {
        s_netplay_origin_screen = cur;
    } else {
        s_netplay_origin_screen = MENU_SCREEN_NETWORK_LOBBY;
    }
    s_netplay_origin_native_lan = SDLNetplayUI_IsNativeLobbyActive();
    SDLNetplayUI_SetNativeLobbyActive(false);

    /* Hide the RmlUI lobby overlay on connection (safe no-op if not shown) */
    rmlui_wrapper_hide_all_game_documents();

    setup_vs_mode();
    Discovery_Shutdown();

    SDL_zeroa(input_history);
    frames_behind = 0;
    frame_skip_timer = 0;
    transition_ready_frames = 0;

    // Reset dynamic delay sampling for this session
    dynamic_delay = DELAY_FRAMES_DEFAULT;
    dynamic_frame_skip_max = 3;
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

    SDL_Log("[netplay] *** BEGIN: local player = P%d (slot %d), local port = %hu ***",
            player_number + 1,
            player_number,
            local_port);
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
                                // g_state.ID collision (same-machine same-binary): tiebreak by port
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
                    // Tiebreaker: lower instance g_state.ID = P1 (initiator)
                    uint32_t local_id = Discovery_GetLocalInstanceID();
                    if (local_id != peers[i].instance_id) {
                        we_initiated = (local_id < peers[i].instance_id);
                    } else {
                        // g_state.ID collision (same-machine same-binary): tiebreak by port
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
                            local_id,
                            target_peer->instance_id,
                            we_initiated,
                            we_initiated ? 1 : 2);
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
                printf("[netplay] character select reached (g_state.fsm[1]=%d)\n", g_state.fsm[1]);
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

            // Release cached DNS entries before destroying any socket
            SDLNetAdapter_Destroy();

            // Close whichever socket was used for this session
            if (stun_socket != NULL) {
                NET_DestroyDatagramSocket(stun_socket);
                stun_socket = NULL;
            }
            if (fallback_socket != NULL) {
                NET_DestroyDatagramSocket(fallback_socket);
                fallback_socket = NULL;
            }

#ifndef LOSSY_ADAPTER
            // also cleanup default socket (only used as last-resort fallback).
            gekko_default_adapter_destroy();
#endif
        }

        // Re-enter LOBBY instead of IDLE so the game stays in menu/lobby mode
        // and doesn't restart its init flow (which causes flow/state issues).
        // Soft_Reset_Sub (called during disconnect) hides all RmlUI documents
        // and forces the engine to the Title Screen. Menu_ReenterNetworkLobby
        // stops the title screen and forces the engine back to the lobby state.
        {
            const char* room = rmlui_casual_lobby_get_room_code();
            session_state = NETPLAY_SESSION_LOBBY;
            Discovery_Init(false); // Restart LAN beacons

            // Force engine back to menu/lobby idle state instead of parking it
            Menu_ReenterNetworkLobby();

            if (room && room[0]) {
                // Copy room code to local buffer BEFORE calling set_room:
                // get_room_code() returns s_room_code.c_str() and set_room()
                // does s_room_code = room_code — self-assignment through alias.
                char room_buf[16];
                SDL_snprintf(room_buf, sizeof(room_buf), "%s", room);
                rmlui_casual_lobby_set_room(room_buf);
                MenuScreen_Goto(MENU_SCREEN_CASUAL_LOBBY);
                SDL_Log("[netplay] Re-entering LOBBY for casual room %s", room_buf);
            } else {
                MenuScreenId dest =
                    (s_netplay_origin_screen != MENU_SCREEN_NONE) ? s_netplay_origin_screen : MENU_SCREEN_NETWORK_LOBBY;
                if (dest == MENU_SCREEN_NETWORK_LOBBY && s_netplay_origin_native_lan) {
                    extern bool g_lobby_reenter_lan_match;
                    g_lobby_reenter_lan_match = true;
                }
                MenuScreen_Goto(dest);
                SDL_Log("[netplay] Re-entering LOBBY origin screen: %d", dest);
            }
        }
        break;

    case NETPLAY_SESSION_IDLE:
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
        session_state = NETPLAY_SESSION_EXITING;
        break;
    }
}

// === 3SX-private extensions ===

#define EVENT_QUEUE_MAX 8
static NetplayEvent event_queue[EVENT_QUEUE_MAX];
static int event_queue_count = 0;

bool Netplay_IsEnabled() {
    return session_state != NETPLAY_SESSION_IDLE;
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
