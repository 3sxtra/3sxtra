#include "netplay.h"
#include "discovery.h"

// Defined in cli_parser.c; default 50000, overridable via --port.
extern unsigned short g_netplay_port;
#include "game_state.h"
#include "gekkonet.h"
#include "main.h"
#include "port/char_data.h"
#include "port/config.h"
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
#include "port/renderer.h"
extern void njUserMain();
extern void SDLGameRenderer_ResetBatchState(); // Reset texture stack between netplay sub-frames
#include "port/sdl/sdl_netplay_ui.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/utils/djb2_hash.h"
#include "types.h"

#include <stdbool.h>
#include <string.h>

#define Game GekkoGame // workaround: upstream GekkoSessionType::Game collides with void Game()
#include "gekkonet.h"
#undef Game
#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>

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

/**
 * @brief Effect allocator state — saved/loaded alongside GameState for rollback.
 *
 * @netplay_sync
 * The effect system (ramcnt.c) manages a pool of WORK slots for projectiles,
 * hit sparks, stage effects, etc. These fields track the free list and execution
 * timers. Without saving/loading these, rollback would reuse stale effect slots.
 *
 * frw[EFFECT_MAX][448]: raw byte arrays backing each WORK slot.
 * frwque[]: free list ordering. head_ix/tail_ix: per-priority linked-list heads/tails.
 * frwctr/frwctr_min: free slot count and low-water mark.
 * exec_tm[]: per-priority execution timers.
 */
typedef struct EffectState {
    s16 frwctr;
    s16 frwctr_min;
    s16 head_ix[8];
    s16 tail_ix[8];
    s16 exec_tm[8];
    uintptr_t frw[EFFECT_MAX][448];
    s16 frwque[EFFECT_MAX];
} EffectState;

/**
 * @brief Complete rollback state — passed to GekkoNet for save/load.
 *
 * @netplay_sync
 * This is the top-level struct that GekkoNet snapshots every frame.
 * It contains:
 *  - gs (GameState): all deterministic game globals (~700+ fields)
 *  - es (EffectState): effect allocator pool and free-list state
 *
 * sizeof(State) is logged at session start for desync debugging.
 */
typedef struct State {
    GameState gs;
    EffectState es;
} State;

static GekkoSession* session = NULL;
static unsigned short local_port = 0;
static unsigned short remote_port = 0;
static char remote_ip_string[64] = { 0 };
static const char* remote_ip = NULL;
static int player_number = 0;
static int player_handle = 0;
static int stun_socket_fd = -1; // Pre-punched STUN socket for internet play
static NetplaySessionState session_state = NETPLAY_SESSION_IDLE;
static u16 input_history[2][INPUT_HISTORY_MAX] = { 0 };
static float frames_behind = 0;
static int frame_skip_timer = 0;
static int transition_ready_frames = 0;

static int stats_update_timer = 0;
static int frame_max_rollback = 0;
static NetworkStats network_stats = { 0 };

// --- Dynamic delay from ping ---
static int    dynamic_delay = DELAY_FRAMES_DEFAULT;
static bool   dynamic_delay_applied = false;
static float  ping_sum = 0;
static float  jitter_sum = 0;
static int    ping_sample_count = 0;
static int    ping_sample_timer = 0;

#if defined(DEBUG)
static int battle_start_frame = -1;
#define STATE_BUFFER_MAX 20
static State state_buffer[STATE_BUFFER_MAX];
#endif

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

// --- Custom adapter for STUN hole-punched connections ---
// Wraps the pre-punched STUN socket fd via Stun_Socket* helpers (in stun.c)
// so GekkoNet reuses the exact socket that completed hole punching,
// preserving the NAT pinhole. The default ASIO adapter creates a new
// socket which may get a different public port on Symmetric NAT.

#define STUN_ADAPTER_RECV_BUF 4096
#define STUN_ADAPTER_MAX_RESULTS 64

static GekkoNetResult* stun_recv_pool[STUN_ADAPTER_MAX_RESULTS];
static int stun_recv_count = 0;

static void stun_adapter_send(GekkoNetAddress* addr, const char* data, int length) {
    if (stun_socket_fd < 0)
        return;

    // Extract null-terminated endpoint string from GekkoNetAddress
    char addr_str[128];
    int copy_len = addr->size < (int)sizeof(addr_str) - 1 ? (int)addr->size : (int)sizeof(addr_str) - 1;
    SDL_memcpy(addr_str, addr->data, copy_len);
    addr_str[copy_len] = '\0';

    Stun_SocketSendTo(stun_socket_fd, addr_str, data, length);
}

static GekkoNetResult** stun_adapter_receive(int* length) {
    // GekkoNet's backend frees each result via free_data() after processing,
    // so we just reset our pointer array here (matching ASIO's _results.clear()).
    stun_recv_count = 0;

    if (stun_socket_fd < 0) {
        *length = 0;
        return (GekkoNetResult**)stun_recv_pool;
    }

    char buf[STUN_ADAPTER_RECV_BUF];
    while (stun_recv_count < STUN_ADAPTER_MAX_RESULTS) {
        char endpoint[48];
        int n = Stun_SocketRecvFrom(stun_socket_fd, buf, sizeof(buf), endpoint, sizeof(endpoint));
        if (n <= 0)
            break;

        int ep_len = (int)SDL_strlen(endpoint);

        GekkoNetResult* res = (GekkoNetResult*)SDL_malloc(sizeof(GekkoNetResult));
        res->addr.data = SDL_malloc(ep_len);
        res->addr.size = ep_len;
        SDL_memcpy(res->addr.data, endpoint, ep_len);
        res->data_len = n;
        res->data = SDL_malloc(n);
        SDL_memcpy(res->data, buf, n);

        stun_recv_pool[stun_recv_count++] = res;
    }

    *length = stun_recv_count;
    return (GekkoNetResult**)stun_recv_pool;
}

static void stun_adapter_free(void* data_ptr) {
    SDL_free(data_ptr);
}

static GekkoNetAdapter stun_adapter = { stun_adapter_send, stun_adapter_receive, stun_adapter_free };

static int compute_delay_from_ping(float avg_ping, float jitter) {
    float effective_rtt = avg_ping + jitter;
    if (effective_rtt < 30.0f)  return 0;
    if (effective_rtt < 70.0f)  return 1;
    if (effective_rtt < 130.0f) return 2;
    if (effective_rtt < 200.0f) return 3;
    return DELAY_FRAMES_MAX;
}

static void configure_gekko() {
    GekkoConfig config;
    SDL_zero(config);

    config.num_players = PLAYER_COUNT;
    config.input_size = sizeof(u16);
    config.state_size = sizeof(State);
    config.max_spectators = 0;
    config.input_prediction_window = 12;

#if defined(DEBUG)
    config.desync_detection = true;
#endif

    if (gekko_create(&session, GekkoGameSession)) {
        gekko_start(session, &config);
    } else {
        printf("Session is already running! probably incorrect.\n");
    }

    if (stun_socket_fd >= 0) {
        // Internet play: reuse the hole-punched STUN socket
        gekko_net_adapter_set(session, &stun_adapter);
        SDL_Log("Using STUN socket fd %d for GekkoNet adapter", stun_socket_fd);
    } else {
#if defined(LOSSY_ADAPTER)
        configure_lossy_adapter();
        gekko_net_adapter_set(session, &lossy_adapter);
#else
        gekko_net_adapter_set(session, gekko_default_adapter(local_port));
#endif
    }

    printf("starting a session for player %d at port %hu\n", player_number, local_port);

    // Temporary: dump key field offsets to map desync diffs
    printf("[offsetof] sizeof(State)=%zu sizeof(GameState)=%zu sizeof(PLW)=%zu\n",
           sizeof(State),
           sizeof(GameState),
           sizeof(PLW));
    // Fields near first diffs (0x1D4-0x2F1 range)
    printf("[offsetof] gs.Score=0x%zx gs.Winner_id=0x%zx gs.Loser_id=0x%zx gs.My_char=0x%zx\n",
           offsetof(GameState, Score),
           offsetof(GameState, Winner_id),
           offsetof(GameState, Loser_id),
           offsetof(GameState, My_char));
    printf("[offsetof] gs.Super_Arts=0x%zx gs.Counter_hi=0x%zx gs.Cursor_X=0x%zx gs.Player_Color=0x%zx\n",
           offsetof(GameState, Super_Arts),
           offsetof(GameState, Counter_hi),
           offsetof(GameState, Cursor_X),
           offsetof(GameState, Player_Color));
    printf("[offsetof] gs.Player_id=0x%zx gs.Player_Number=0x%zx gs.Flip_Flag=0x%zx gs.Lie_Flag=0x%zx\n",
           offsetof(GameState, Player_id),
           offsetof(GameState, Player_Number),
           offsetof(GameState, Flip_Flag),
           offsetof(GameState, Lie_Flag));
    printf("[offsetof] gs.Counter_Attack=0x%zx gs.Attack_Flag=0x%zx gs.Guard_Flag=0x%zx gs.Before_Jump=0x%zx\n",
           offsetof(GameState, Counter_Attack),
           offsetof(GameState, Attack_Flag),
           offsetof(GameState, Guard_Flag),
           offsetof(GameState, Before_Jump));
    printf("[offsetof] gs.Operator_Status=0x%zx gs.Last_Super_Arts=0x%zx gs.Type_of_Attack=0x%zx\n",
           offsetof(GameState, Operator_Status),
           offsetof(GameState, Last_Super_Arts),
           offsetof(GameState, Type_of_Attack));
    printf("[offsetof] gs.Standing_Timer=0x%zx gs.Turn_Over=0x%zx gs.Used_char=0x%zx gs.Break_Com=0x%zx\n",
           offsetof(GameState, Standing_Timer),
           offsetof(GameState, Turn_Over),
           offsetof(GameState, Used_char),
           offsetof(GameState, Break_Com));
    // Fields near 0x300-0x500 range
    printf("[offsetof] gs.Convert_Buff=0x%zx gs.Check_Buff=0x%zx gs.Mode_Type=0x%zx gs.VS_Stage=0x%zx\n",
           offsetof(GameState, Convert_Buff),
           offsetof(GameState, Check_Buff),
           offsetof(GameState, Mode_Type),
           offsetof(GameState, VS_Stage));
    printf("[offsetof] gs.G_No=0x%zx gs.SP_No=0x%zx gs.Select_Arts=0x%zx gs.CP_No=0x%zx gs.CP_Index=0x%zx\n",
           offsetof(GameState, G_No),
           offsetof(GameState, SP_No),
           offsetof(GameState, Select_Arts),
           offsetof(GameState, CP_No),
           offsetof(GameState, CP_Index));
    printf("[offsetof] gs.PL_Wins=0x%zx gs.win_type=0x%zx gs.Conclusion_Type=0x%zx gs.Present_Mode=0x%zx\n",
           offsetof(GameState, PL_Wins),
           offsetof(GameState, win_type),
           offsetof(GameState, Conclusion_Type),
           offsetof(GameState, Present_Mode));
    // Fields near 0x500-0x700 range
    printf("[offsetof] gs.Game_timer=0x%zx gs.Control_Time=0x%zx gs.Round_Level=0x%zx gs.Fade_Number=0x%zx\n",
           offsetof(GameState, Game_timer),
           offsetof(GameState, Control_Time),
           offsetof(GameState, Round_Level),
           offsetof(GameState, Fade_Number));
    printf("[offsetof] gs.PLsw=0x%zx gs.Random_ix16=0x%zx gs.Random_ix32=0x%zx\n",
           offsetof(GameState, PLsw),
           offsetof(GameState, Random_ix16),
           offsetof(GameState, Random_ix32));
    printf("[offsetof] gs.Pattern_Index=0x%zx gs.Attack_Counter=0x%zx gs.Resume_Lever=0x%zx\n",
           offsetof(GameState, Pattern_Index),
           offsetof(GameState, Attack_Counter),
           offsetof(GameState, Resume_Lever));
    printf("[offsetof] gs.Limit_Time=0x%zx gs.Random_ix16_ex=0x%zx gs.Random_ix32_ex=0x%zx\n",
           offsetof(GameState, Limit_Time),
           offsetof(GameState, Random_ix16_ex),
           offsetof(GameState, Random_ix32_ex));
    printf("[offsetof] gs.task=0x%zx gs.plw=0x%zx\n", offsetof(GameState, task), offsetof(GameState, plw));
    printf("[offsetof] gs.bg_w=0x%zx sizeof(BG)=0x%zx bg_end=0x%zx\n",
           offsetof(GameState, bg_w),
           sizeof(BG),
           offsetof(GameState, bg_w) + sizeof(BG));

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

#if defined(DEBUG)
// Per-subsystem checksums for faster desync triage — when a desync fires,
// we can immediately tell which section (player, bg, effects...) diverged.
typedef struct {
    uint32_t plw0;
    uint32_t plw1;
    uint32_t bg;
    uint32_t tasks;
    uint32_t effects;
    uint32_t globals;
    uint32_t combined;
} SectionedChecksum;

static SectionedChecksum saved_section_checksums[STATE_BUFFER_MAX];
static PLW saved_plw_scratch[STATE_BUFFER_MAX][2];

static void dump_state(const State* src, const char* filename) {
    SDL_IOStream* io = SDL_IOFromFile(filename, "w");
    if (io == NULL) {
        SDL_Log("[netplay] dump_state: failed to open '%s' — states/ dir missing?", filename);
        return;
    }
    SDL_WriteIO(io, src, sizeof(State));
    SDL_CloseIO(io);
}

// Forward declarations for sanitization helpers (defined below).
static void sanitize_work_pointers(WORK* w);
static void sanitize_plw_pointers(PLW* p);

/// Sanitize pointer fields in a State copy so the dump is free of ASLR noise.
/// This makes compare_states.py output clean and comparable across peers.
static void sanitize_state_pointers(State* dst) {
    // GameState pointer fields
    dst->gs.rw3col_ptr = NULL;
    dst->gs.ci_pointer = NULL;

    // _TASK function pointers
    for (int i = 0; i < 11; i++) {
        dst->gs.task[i].func_adrs = NULL;
        dst->gs.task[i].callback_adrs = NULL;
    }

    // PLW pointer fields (both players)
    for (int p = 0; p < 2; p++) {
        sanitize_plw_pointers(&dst->gs.plw[p]);
    }

    // Effect WORK pointer fields
    EffectState* es = &dst->es;
    for (int i = 0; i < EFFECT_MAX; i++) {
        WORK* w = (WORK*)es->frw[i];
        if (w->be_flag != 0) {
            sanitize_work_pointers(w);
        }
    }
}

static void dump_saved_state(int frame) {
    // Copy and sanitize to avoid ASLR noise in the dump
    static State sanitized;
    SDL_memcpy(&sanitized, &state_buffer[frame % STATE_BUFFER_MAX], sizeof(State));
    sanitize_state_pointers(&sanitized);

    char filename[100];
    SDL_snprintf(filename, sizeof(filename), "states/%d_%d", player_handle, frame);

    dump_state(&sanitized, filename);
}
#endif

#define SDL_copya(dst, src) SDL_memcpy(dst, src, sizeof(src))

/**
 * @brief Snapshot the complete game state into a State struct.
 *
 * @netplay_sync
 * Called by save_state() on every GekkoSaveEvent. Copies both the GameState
 * (via GameState_Save) and the EffectState (effect pool + free list) into dst.
 * This is the "save" half of the rollback save/load cycle.
 */
static void gather_state(State* dst) {
    // GameState
    GameState* gs = &dst->gs;
    GameState_Save(gs);

    // EffectState
    EffectState* es = &dst->es;
    SDL_copya(es->frw, frw);
    SDL_copya(es->exec_tm, exec_tm);
    SDL_copya(es->frwque, frwque);
    SDL_copya(es->head_ix, head_ix);
    SDL_copya(es->tail_ix, tail_ix);
    es->frwctr = frwctr;
    es->frwctr_min = frwctr_min;
}

#if defined(DEBUG)
// These effect IDs use the WORK_Other_CONN layout (variable-length conn[] tail).
// Derived by auditing every effXX.c that casts to WORK_Other_CONN*.
static bool is_work_other_conn(int id) {
    switch (id) {
    case 16:  // eff16 (Score Breakdown) - Caused F1549 Desync
    case 160: // effg0
    case 170: // effh0
    case 179: // effh9
    case 192: // effj2
    case 211: // effL1
    case 223: // effm3
        return true;
    default:
        return false;
    }
}

// Zero the unused tail of conn[] — entries past num_of_conn are uninitialized
// heap data that differs between peers. Root cause of the F1549 desync.
static void sanitize_work_other_conn(WORK* w) {
    WORK_Other_CONN* wc = (WORK_Other_CONN*)w;
    int count = wc->num_of_conn;
    // Derive the array bound from the actual struct so this stays correct
    // if conn[] is ever resized.
    const int CONN_MAX = (int)SDL_arraysize(wc->conn);
    if (count >= 0 && count < CONN_MAX) {
        size_t bytes = (size_t)(CONN_MAX - count) * sizeof(CONN);
        if (bytes > 0) {
            SDL_memset(&wc->conn[count], 0, bytes);
        }
    }
}

/// Zero pointer fields so they don't pollute checksums (ASLR makes them differ).
/// Only ever called on a scratch copy — never on a state Gekko will restore.
static void sanitize_work_pointers(WORK* w) {
    w->target_adrs = NULL;
    w->hit_adrs = NULL;
    w->dmg_adrs = NULL;
    w->suzi_offset = NULL;
    SDL_zeroa(w->char_table);
    w->se_random_table = NULL;
    w->step_xy_table = NULL;
    w->move_xy_table = NULL;
    w->overlap_char_tbl = NULL;
    w->olc_ix_table = NULL;
    w->rival_catch_tbl = NULL;
    w->curr_rca = NULL;
    w->set_char_ad = NULL;
    w->hit_ix_table = NULL;
    w->body_adrs = NULL;
    w->h_bod = NULL;
    w->hand_adrs = NULL;
    w->h_han = NULL;
    w->dumm_adrs = NULL;
    w->h_dumm = NULL;
    w->catch_adrs = NULL;
    w->h_cat = NULL;
    w->caught_adrs = NULL;
    w->h_cau = NULL;
    w->attack_adrs = NULL;
    w->h_att = NULL;
    w->h_eat = NULL;
    w->hosei_adrs = NULL;
    w->h_hos = NULL;
    w->att_ix_table = NULL;
    w->my_effadrs = NULL;
}

/// Mask rendering-only bits/fields from WORK color fields.
/// - current_colcd, my_col_code: strip 0x2000 player-side palette flag
/// - colcd: fully zeroed (derived from current_colcd by rendering, can differ entirely)
/// - extra_col, extra_col_2: strip 0x2000 palette flag
static void sanitize_work_rendering(WORK* w) {
    w->current_colcd &= ~0x2000;
    w->my_col_code &= ~0x2000;
    w->colcd = 0; // Rendering-derived, not gameplay state
    w->extra_col &= ~0x2000;
    w->extra_col_2 &= ~0x2000;
}

/// Zero all pointer fields and mask rendering bits in a PLW struct.
static void sanitize_plw_pointers(PLW* p) {
    sanitize_work_pointers(&p->wu);
    sanitize_work_rendering(&p->wu);
    p->cp = NULL;
    p->dm_step_tbl = NULL;
    p->as = NULL;
    p->sa = NULL;
    p->py = NULL;
}

/// Save state in state buffer.
/// @return Mutable pointer to state as it has been saved.
static State* note_state(const State* state, int frame) {
    if (frame < 0) {
        frame += STATE_BUFFER_MAX;
    }

    State* dst = &state_buffer[frame % STATE_BUFFER_MAX];
    SDL_memcpy(dst, state, sizeof(State));
    return dst;
}
#endif

/**
 * @brief Save game state for rollback — GekkoNet callback.
 *
 * @netplay_sync
 * Called by GekkoNet on every frame to save the current state. In DEBUG builds,
 * also computes per-subsystem checksums for desync detection and saves sanitized
 * PLW copies for binary comparison when a desync is detected.
 *
 * The checksum only covers a whitelist of gameplay-critical fields (PLW after
 * pointer/rendering sanitization, RNG indices, round state, combat flags,
 * slow-motion flags, super gauge, stun). UI-only fields are saved but not
 * checksummed to reduce false positives from rendering-only divergence.
 */
static void save_state(GekkoGameEvent* event) {
    *event->data.save.state_len = sizeof(State);
    State* dst = (State*)event->data.save.state;

    gather_state(dst);

#if defined(DEBUG)
    const int frame = event->data.save.frame;

    // Activate checksumming from the very first synced frame (not just battle).
    // This catches desyncs during character select, not only during gameplay.
    if (battle_start_frame < 0) {
        battle_start_frame = frame;
        SDL_Log("[P%d] checksumming active from frame %d (G_No[1]=%d)", local_port, frame, G_No[1]);
    }

    const bool checksumming_active = battle_start_frame >= 0;

    note_state(dst, frame);

    // Sanitize non-functional data in dst (safe for rollback restore):
    // inactive effect slots, padding arrays, WORK_Other_CONN unused tails.
    {
        EffectState* es = &dst->es;
        for (int i = 0; i < EFFECT_MAX; i++) {
            WORK* w = (WORK*)es->frw[i];
            if (w->be_flag == 0) {
                s16 before = w->before;
                s16 behind = w->behind;
                s16 myself = w->myself;
                SDL_memset(es->frw[i], 0, sizeof(es->frw[i]));
                w->before = before;
                w->behind = behind;
                w->myself = myself;
            } else {
                SDL_zeroa(w->wrd_free);
                WORK_Other* wo = (WORK_Other*)w;
                SDL_zeroa(wo->et_free);
                if (is_work_other_conn(w->id)) {
                    sanitize_work_other_conn(w);
                }
            }
        }
        note_state(dst, frame);
    }

    if (checksumming_active) {
        // === Focused gameplay checksum ===
        // Instead of checksumming the full 478KB State and sanitizing ~50 fields,
        // we checksum ONLY gameplay-critical data:
        //   PLW[2]: copied and sanitized (pointers, rendering, linked-list zeroed)
        //   Globals: explicit whitelist of deterministic fields
        //   Effects, BG, tasks, zanzou: excluded entirely

        // --- Sanitized PLW copies ---
        static PLW plw_scratch[2];
        for (int p = 0; p < 2; p++) {
            SDL_memcpy(&plw_scratch[p], &dst->gs.plw[p], sizeof(PLW));
            sanitize_plw_pointers(&plw_scratch[p]);
            sanitize_work_rendering(&plw_scratch[p].wu);

            // Linked-list indices and timing differ per allocation order
            plw_scratch[p].wu.before = 0;
            plw_scratch[p].wu.behind = 0;
            plw_scratch[p].wu.myself = 0;
            plw_scratch[p].wu.listix = 0;
            plw_scratch[p].wu.timing = 0;

            // Sweep remaining pointer-like values in PLW
            uintptr_t* words = (uintptr_t*)&plw_scratch[p];
            const size_t count = sizeof(PLW) / sizeof(uintptr_t);
            for (size_t i = 0; i < count; i++) {
                uintptr_t v = words[i];
                if (v > 0x100000000ULL && (v >> 47) == 0) {
                    words[i] = 0;
                }
            }
        }

        // --- Build combined hash from PLW + whitelisted globals ---
        const GameState* gs = &dst->gs;
        uint32_t h = djb2_init();

        // PLW (sanitized)
        h = djb2_update_mem(h, (const uint8_t*)&plw_scratch[0], sizeof(PLW));
        h = djb2_update_mem(h, (const uint8_t*)&plw_scratch[1], sizeof(PLW));

        // RNG indices
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix16, sizeof(gs->Random_ix16));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix32, sizeof(gs->Random_ix32));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix16_ex, sizeof(gs->Random_ix16_ex));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix32_ex, sizeof(gs->Random_ix32_ex));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix16_com, sizeof(gs->Random_ix16_com));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix32_com, sizeof(gs->Random_ix32_com));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix16_ex_com, sizeof(gs->Random_ix16_ex_com));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Random_ix32_ex_com, sizeof(gs->Random_ix32_ex_com));

        // Round/match
        h = djb2_update_mem(h, (const uint8_t*)&gs->Round_num, sizeof(gs->Round_num));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Round_Level, sizeof(gs->Round_Level));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Round_Result, sizeof(gs->Round_Result));
        h = djb2_update_mem(h, (const uint8_t*)&gs->PL_Wins, sizeof(gs->PL_Wins));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Conclusion_Type, sizeof(gs->Conclusion_Type));
        h = djb2_update_mem(h, (const uint8_t*)&gs->win_type, sizeof(gs->win_type));

        // Player identity
        h = djb2_update_mem(h, (const uint8_t*)&gs->My_char, sizeof(gs->My_char));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Super_Arts, sizeof(gs->Super_Arts));

        // Combat flags
        h = djb2_update_mem(h, (const uint8_t*)&gs->Attack_Flag, sizeof(gs->Attack_Flag));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Counter_Attack, sizeof(gs->Counter_Attack));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Guard_Flag, sizeof(gs->Guard_Flag));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Flip_Flag, sizeof(gs->Flip_Flag));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Lie_Flag, sizeof(gs->Lie_Flag));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Attack_Counter, sizeof(gs->Attack_Counter));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Bullet_No, sizeof(gs->Bullet_No));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Bullet_Counter, sizeof(gs->Bullet_Counter));
        h = djb2_update_mem(h, (const uint8_t*)&gs->paring_counter, sizeof(gs->paring_counter));

        // Game flow
        h = djb2_update_mem(h, (const uint8_t*)&gs->Present_Mode, sizeof(gs->Present_Mode));
        h = djb2_update_mem(h, (const uint8_t*)&gs->VS_Stage, sizeof(gs->VS_Stage));

        // Slow motion
        h = djb2_update_mem(h, (const uint8_t*)&gs->SLOW_timer, sizeof(gs->SLOW_timer));
        h = djb2_update_mem(h, (const uint8_t*)&gs->SLOW_flag, sizeof(gs->SLOW_flag));
        h = djb2_update_mem(h, (const uint8_t*)&gs->EXE_flag, sizeof(gs->EXE_flag));

        // Super gauge / stun
        h = djb2_update_mem(h, (const uint8_t*)&gs->super_arts, sizeof(gs->super_arts));
        h = djb2_update_mem(h, (const uint8_t*)&gs->piyori_type, sizeof(gs->piyori_type));
        h = djb2_update_mem(h, (const uint8_t*)&gs->Max_vitality, sizeof(gs->Max_vitality));

        *event->data.save.checksum = h;

        // Per-section checksums for desync triage
        SectionedChecksum sc;
        uint32_t sh;
        sh = djb2_init();
        sh = djb2_update_mem(sh, (const uint8_t*)&plw_scratch[0], sizeof(PLW));
        sc.plw0 = sh;
        sh = djb2_init();
        sh = djb2_update_mem(sh, (const uint8_t*)&plw_scratch[1], sizeof(PLW));
        sc.plw1 = sh;
        sc.bg = 0;
        sc.tasks = 0;
        sc.effects = 0;
        sc.combined = h;
        sc.globals = h ^ sc.plw0 ^ sc.plw1;
        saved_section_checksums[frame % STATE_BUFFER_MAX] = sc;
        SDL_memcpy(&saved_plw_scratch[frame % STATE_BUFFER_MAX][0], &plw_scratch[0], sizeof(PLW));
        SDL_memcpy(&saved_plw_scratch[frame % STATE_BUFFER_MAX][1], &plw_scratch[1], sizeof(PLW));
    }
#endif
}

/**
 * @brief Restore game state from a rollback — GekkoNet callback.
 *
 * @netplay_sync
 * Called by GekkoNet when a rollback is needed. Restores all globals from the
 * saved State snapshot: GameState_Load for game globals, then manually restores
 * the effect pool state (frw, frwque, head_ix, tail_ix, frwctr, frwctr_min).
 */
static void load_state(const State* src) {
    // GameState
    const GameState* gs = &src->gs;
    GameState_Load(gs);

    // EffectState
    const EffectState* es = &src->es;
    SDL_copya(frw, es->frw);
    SDL_copya(exec_tm, es->exec_tm);
    SDL_copya(frwque, es->frwque);
    SDL_copya(head_ix, es->head_ix);
    SDL_copya(tail_ix, es->tail_ix);
    frwctr = es->frwctr;
    frwctr_min = es->frwctr_min;
}

static void load_state_from_event(GekkoGameEvent* event) {
    const State* src = (State*)event->data.load.state;
    load_state(src);
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
static void advance_game(GekkoGameEvent* event, bool render) {
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
            printf("🔴 player syncing\n");
            push_event(NETPLAY_EVENT_SYNCHRONIZING);
            break;

        case GekkoPlayerConnected:
            printf("🔴 player connected\n");
            push_event(NETPLAY_EVENT_CONNECTED);
            break;

        case GekkoPlayerDisconnected:
            printf("🔴 player disconnected\n");
            push_event(NETPLAY_EVENT_DISCONNECTED);
            if (session_state != NETPLAY_SESSION_EXITING && session_state != NETPLAY_SESSION_IDLE) {
                clean_input_buffers();
                Soft_Reset_Sub();
                session_state = NETPLAY_SESSION_EXITING;
            }
            break;

        case GekkoSessionStarted:
            printf("🔴 session started\n");
            session_state = NETPLAY_SESSION_RUNNING;
            break;

        case GekkoDesyncDetected: {
            const int frame = event->data.desynced.frame;
            printf("⚠️ desync detected at frame %d (local: 0x%08x, remote: 0x%08x)\n",
                   frame,
                   event->data.desynced.local_checksum,
                   event->data.desynced.remote_checksum);

#if defined(DEBUG)
            // Log per-section checksums from the SANITIZED scratch copy
            // (raw state_buffer contains pointer/rendering noise).
            SectionedChecksum sc = saved_section_checksums[frame % STATE_BUFFER_MAX];
            printf("  sections: plw0=0x%08x plw1=0x%08x bg=0x%08x tasks=0x%08x fx=0x%08x globals=0x%08x\n",
                   sc.plw0,
                   sc.plw1,
                   sc.bg,
                   sc.tasks,
                   sc.effects,
                   sc.globals);
            dump_saved_state(frame);

            // Dump sanitized PLW copies for direct binary comparison
            {
                const PLW* sp = saved_plw_scratch[frame % STATE_BUFFER_MAX];
                char fn[100];
                SDL_snprintf(fn, sizeof(fn), "states/%d_%d_plw0_san", player_handle, frame);
                SDL_IOStream* io = SDL_IOFromFile(fn, "w");
                if (io) { SDL_WriteIO(io, &sp[0], sizeof(PLW)); SDL_CloseIO(io); }
                SDL_snprintf(fn, sizeof(fn), "states/%d_%d_plw1_san", player_handle, frame);
                io = SDL_IOFromFile(fn, "w");
                if (io) { SDL_WriteIO(io, &sp[1], sizeof(PLW)); SDL_CloseIO(io); }
            }

            // Per-field hash breakdown for the sanitized PLW
            for (int p = 0; p < 2; p++) {
                const PLW* sp = &saved_plw_scratch[frame % STATE_BUFFER_MAX][p];
                const WORK* wu = &sp->wu;
                printf("  plw[%d] field hashes:\n", p);
                #define FIELD_HASH(label, ptr, sz) do { \
                    uint32_t fh = djb2_init(); \
                    fh = djb2_update_mem(fh, (const uint8_t*)(ptr), (sz)); \
                    printf("    %-24s 0x%08x (%zu bytes)\n", label, fh, (size_t)(sz)); \
                } while(0)

                FIELD_HASH("wu.be_flag..type",     &wu->be_flag, 6);
                FIELD_HASH("wu.work_id+id",        &wu->work_id, 4);
                FIELD_HASH("wu.dead_f",            &wu->dead_f, sizeof(wu->dead_f));
                FIELD_HASH("wu.routine_no",        &wu->routine_no, sizeof(wu->routine_no));
                FIELD_HASH("wu.old_rno",           &wu->old_rno, sizeof(wu->old_rno));
                FIELD_HASH("wu.hit_stop+quake",    &wu->hit_stop, 4);
                FIELD_HASH("wu.position_xyz",      &wu->position_x, 6);
                FIELD_HASH("wu.next_xyz",          &wu->next_x, 6);
                FIELD_HASH("wu.xyz",               &wu->xyz, sizeof(wu->xyz));
                FIELD_HASH("wu.mvxy",              &wu->mvxy, sizeof(wu->mvxy));
                FIELD_HASH("wu.direction",         &wu->direction, sizeof(wu->direction));
                FIELD_HASH("wu.vitality+vital_new", &wu->vitality, 6);
                FIELD_HASH("wu.cmoa..cmb3",        &wu->cmoa, 18 * sizeof(UNK11));
                FIELD_HASH("wu.cmwk",              &wu->cmwk, sizeof(wu->cmwk));
                FIELD_HASH("wu.char_state",        &sp->wu.char_state, sizeof(sp->wu.char_state));
                FIELD_HASH("wu.att",               &wu->att, sizeof(wu->att));
                FIELD_HASH("wu.hf+hit_mark",       &wu->hf, 8);
                FIELD_HASH("wu.attpow+defpow",     &wu->attpow, 4);
                FIELD_HASH("wu.shell_ix",          &wu->shell_ix, sizeof(wu->shell_ix));
                FIELD_HASH("wu.wrd_free",          &wu->wrd_free, sizeof(wu->wrd_free));
                FIELD_HASH("spmv_ng_flag",         &sp->spmv_ng_flag, 8);
                FIELD_HASH("player_number",        &sp->player_number, sizeof(sp->player_number));
                FIELD_HASH("zuru_*",               &sp->zuru_timer, 6);
                FIELD_HASH("tsukami_*",            &sp->tsukami_num, 6);
                FIELD_HASH("guard_*",              &sp->old_gdflag, 4);
                FIELD_HASH("sa_related",           &sp->sa_stop_sai, 6);
                FIELD_HASH("combo_type",           &sp->combo_type, sizeof(sp->combo_type));
                FIELD_HASH("dead_flag",            &sp->dead_flag, sizeof(sp->dead_flag));
                FIELD_HASH("ukemi_*",              &sp->ukemi_ok_timer, 6);
                FIELD_HASH("old_pos_data",         &sp->old_pos_data, sizeof(sp->old_pos_data));
                FIELD_HASH("image_setup+data",     &sp->image_setup_flag, 4);
                FIELD_HASH("tk_dageki..konjyou",   &sp->tk_dageki, 8);
                FIELD_HASH("reserv_add_y",         &sp->reserv_add_y, sizeof(sp->reserv_add_y));

                // Full PLW hash for cross-check
                FIELD_HASH("FULL PLW",             sp, sizeof(PLW));
                #undef FIELD_HASH
            }

            // Global context: player identity and key globals
            {
                const State* st = &state_buffer[frame % STATE_BUFFER_MAX];
                const GameState* gs = &st->gs;
                printf("  globals context:\n");
                printf("    My_char: [%d, %d]  Player_Color: [%d, %d]\n",
                       gs->My_char[0], gs->My_char[1],
                       gs->Player_Color[0], gs->Player_Color[1]);
                printf("    Random_ix16: %d  Random_ix32: %d\n",
                       gs->Random_ix16, gs->Random_ix32);
                printf("    Player_id: %d  Player_Number: %d\n",
                       gs->Player_id, gs->Player_Number);
                printf("    charset_id: plw0=%d plw1=%d\n",
                       gs->plw[0].wu.charset_id, gs->plw[1].wu.charset_id);
            }
#endif

            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_WARNING, "Netplay", "Desync detected — the session will be terminated.", NULL);
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

    static int log_throttle = 0;
    if (game_event_count == 0 && (log_throttle++ % 120 == 0)) {
        printf("[netplay] no game events from gekko (state=%d)\n", session_state);
    } else if (game_event_count > 0) {
        log_throttle = 0;
    }

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
            if (ns.avg_ping > 0) {
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
                dynamic_delay, ping_sample_count,
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

void Netplay_SetStunSocket(int fd) {
    // If we already hold a STUN socket, close it first
    if (stun_socket_fd >= 0 && stun_socket_fd != fd) {
        Stun_SocketClose(stun_socket_fd);
    }
    stun_socket_fd = fd;
}

void Netplay_Begin() {
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

#if defined(DEBUG)
    // Reset so checksumming activates correctly for rematches.
    battle_start_frame = -1;
#endif

    session_state = NETPLAY_SESSION_TRANSITIONING;
}

void Netplay_EnterLobby() {
    session_state = NETPLAY_SESSION_LOBBY;
    Discovery_Init(Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT));
}

void Netplay_Run() {
    switch (session_state) {
    case NETPLAY_SESSION_LOBBY:
        Discovery_Update();

        {
            static uint32_t handshake_ready_since = 0;
            bool local_auto = Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT);
            int local_challenge = Discovery_GetChallengeTarget();
            bool should_be_ready = false;
            NetplayDiscoveredPeer* target_peer = NULL;

            NetplayDiscoveredPeer peers[16];
            int count = Discovery_GetPeers(peers, 16);

            bool we_initiated = false;
            for (int i = 0; i < count; i++) {
                // If we explicitly challenge them AND they explicitly challenge us OR have auto-connect on
                if (local_challenge == (int)peers[i].instance_id) {
                    if (peers[i].is_challenging_me || peers[i].wants_auto_connect) {
                        target_peer = &peers[i];
                        should_be_ready = true;
                        we_initiated = true; // We set ChallengeTarget first
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
                    we_initiated = (local_id < peers[i].instance_id);
                    break;
                }
            }

            Discovery_SetReady(should_be_ready);

            if (should_be_ready && target_peer && target_peer->peer_ready) {
                if (handshake_ready_since == 0) {
                    handshake_ready_since = SDL_GetTicks();
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
                    Netplay_SetLocalPort(g_netplay_port);
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
            if (stun_socket_fd >= 0) {
                Stun_SocketClose(stun_socket_fd);
                stun_socket_fd = -1;
            }

#ifndef LOSSY_ADAPTER
            // also cleanup default socket.
            gekko_default_adapter_destroy();
#endif
        }

        Discovery_Shutdown();
        session_state = NETPLAY_SESSION_IDLE;
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
