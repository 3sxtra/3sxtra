/**
 * @file main.c
 * @brief Program entry point, frame loop, and task scheduler.
 *
 * Contains main(), the SDL initialization/frame loop, resource and AFS
 * filesystem setup, the legacy task scheduler (cpLoopTask/cpInitTask/
 * cpReadyTask/cpExitTask), and the per-frame game logic dispatch
 * (njUserInit/njUserMain).
 *
 * Part of the game core module.
 * Originally from the PS2 game module.
 */

#include "main.h"
#include "common.h"
#include "netplay/netplay.h"
#include "port/renderer.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_app_config.h"
#include "sf33rd/AcrSDK/common/mlPAD.h"
#include "sf33rd/AcrSDK/ps2/flps2debug.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Common/MemMan.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Common/PPGWork.h"
#include "sf33rd/Source/Compress/zlibApp.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/eff61.h"
#include "sf33rd/Source/Game/effect/effect.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/init3rd.h"
#include "sf33rd/Source/Game/io/gd3rd.h"
#include "sf33rd/Source/Game/io/ioconv.h"
#include "sf33rd/Source/Game/menu/menu.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/rendering/mtrans.h"
#include "sf33rd/Source/Game/rendering/texcash.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/system/ramcnt.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sys_sub2.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "sf33rd/Source/Game/training/training_hud.h"

#include "menu_bridge.h"

#include "port/native_save.h"
#include "structs.h"

#if defined(DEBUG)
#include "sf33rd/Source/Game/debug/debug_config.h"
#endif

#include "port/cli_parser.h"
#include "port/io/afs.h"
#include "port/resources.h"

#include <SDL3/SDL.h>

#include "port/tracy_zones.h"

/* === Named Constants === */
#define CHARACTER_COUNT 20       /**< Number of playable characters (matches useChar[20] in MPP) */
#define TASK_SLOT_COUNT 11       /**< Number of task scheduler slots */
#define PPG_MEMORY_SIZE 0x60000  /**< PPG texture memory pool size (384 KB) */
#define ZLIB_MEMORY_SIZE 0x10000 /**< Zlib decompression buffer size (64 KB) */

extern bool game_paused;

#if defined(_WIN32)
#include <windef.h> // including windows.h causes conflicts with the Polygon struct, so I just included the header where AllocConsole is and the Windows-specific typedefs that it requires.

#include <ConsoleApi.h>
#include <stdio.h>
#endif

#include <memory.h>
#include <stdbool.h>
#include <string.h>

// sbss
s32 system_init_level;
MPP mpp_w;

const char* g_shm_suffix = NULL;

// ⚡ Bolt: Input Lag Test Globals
extern volatile bool g_sim_lag_active;
extern int g_sim_lag_frame;
static int s_lag_test_initial_routine = 0;
static int s_lag_test_initial_routine_1 = 0;
static Uint64 s_lag_test_start_ticks = 0;

// forward decls
static void game_init();
static void game_step_0();
static void game_step_1();
static void init_windows_console();

void distributeScratchPadAddress();
void appCopyKeyData();
u8* mppMalloc(u32 size);
void njUserInit();
void njUserMain();
void cpLoopTask();
void cpInitTask();

/**
 * @brief Initialize the AFS (Archive File System) for reading game data.
 *
 * Tries the standard resources path first, falls back to rom/ folder
 * next to the executable.
 */
static void afs_init() {
    // Try the standard resources path first (e.g. AppData/Roaming/CrowdedStreet/3SX/resources/)
    char* file_path = Resources_GetPath("SF33RD.AFS");

    SDL_PathInfo info;
    if (SDL_GetPathInfo(file_path, &info) && info.type == SDL_PATHTYPE_FILE) {
        AFS_Init(file_path);
        SDL_free(file_path);
        return;
    }
    SDL_free(file_path);

    // Fallback: rom/ folder next to the game executable
    file_path = Resources_GetRomPath("SF33RD.AFS");
    SDL_Log("AFS not found in resources dir, trying: %s", file_path);
    AFS_Init(file_path);
    SDL_free(file_path);
}

/**
 * @brief Pre-render frame step: process input, run game logic, flush rendering.
 *
 * Skipped when the game is paused.
 */
static void step_0() {
    TRACE_ZONE_N("GameLogic");
    if (game_paused) {
        TRACE_ZONE_END();
        return;
    }

    MenuBridge_StepGate();
    AFS_RunServer();
    game_step_0();
    TRACE_ZONE_END();
}

/**
 * @brief Post-render frame step: update timers, screen transitions, BGM.
 *
 * Runs after the frame has been rendered to the screen.
 */
static void step_1() {
    TRACE_ZONE_N("PostRender");
    if (game_paused) {
        TRACE_ZONE_END();
        return;
    }

    game_step_1();
    TRACE_ZONE_END();
}

/** @brief Application entry point. Parses CLI, runs SDL frame loop. */
int main(int argc, char* argv[]) {
    bool is_running = true;

    ParseCLI(argc, argv);

    init_windows_console();
    SDLApp_Init();

    /* ── Synchronous resource check + game init ──────────────────
     * Verify required assets exist BEFORE entering the main loop.
     * On desktop: triggers the folder-dialog copy flow if missing.
     * On RPi4/headless: logs an error (no dialog backend). */
    if (!Resources_CheckIfPresent()) {
#ifdef PLATFORM_RPI4
        fatal_error("Resources not found. Place SF33RD.AFS in the rom/ folder "
                    "next to the executable.");
#else
        /* Desktop: run the dialog-based copy flow synchronously */
        while (!Resources_CheckIfPresent()) {
            if (!Resources_RunResourceCopyingFlow()) {
                SDL_Delay(16); /* yield while waiting for dialog */
            }
        }
#endif
    }

    afs_init();
    game_init();

    Menu_UpdateNetworkLabel();

    /* Timing state for decoupled rendering mode (F5 + VSync ON) */
    Uint64 last_tick_time = SDL_GetTicksNS();
    Uint64 game_accumulator = 0;

    while (is_running) {
        /* Determine whether game logic should tick this iteration.
         *
         * Capped (!uncapped):           always tick (1:1 game-to-render, pacer in EndFrame)
         * Uncapped + VSync OFF:         always tick (full speed benchmarking)
         * Uncapped + VSync ON:          tick only when accumulator >= target_frame_time (decoupled)
         */
        bool should_tick_game;

        if (SDLApp_IsFrameRateUncapped() && SDLApp_IsVSyncEnabled()) {
            /* === Decoupled mode === */
            Uint64 now = SDL_GetTicksNS();
            Uint64 elapsed = now - last_tick_time;
            last_tick_time = now;

            /* Cap elapsed to prevent spiral of death (~3 frames max) */
            Uint64 target_ns = SDLApp_GetTargetFrameTimeNS();
            Uint64 max_elapsed = target_ns * 3;
            if (elapsed > max_elapsed)
                elapsed = max_elapsed;

            game_accumulator += elapsed;
            should_tick_game = (game_accumulator >= target_ns);
            if (should_tick_game) {
                game_accumulator -= target_ns;
            }
        } else {
            /* Normal or full-speed mode — always tick */
            should_tick_game = true;
            /* Keep timing state fresh for next decoupled switch */
            last_tick_time = SDL_GetTicksNS();
            game_accumulator = 0;
        }

        if (should_tick_game) {
            SDLApp_BeginFrame();
            step_0();
            SDLApp_EndFrame();
            TRACE_SUB_BEGIN("PollEvents");
            is_running = SDLApp_PollEvents();
            TRACE_SUB_END();
            step_1();
        } else {
            /* Re-present the existing canvas (no game logic, no FBO clear) */
            SDLApp_PresentOnly();
            TRACE_SUB_BEGIN("PollEvents");
            is_running = SDLApp_PollEvents();
            TRACE_SUB_END();
        }
        TRACE_FRAME_MARK();
    }

    AFS_Finish();
    SDLApp_Quit();
    return 0;
}

/** @brief Attach to or create a Windows console for stdout/stderr output. */
static void init_windows_console() {
#if defined(_WIN32)
    // attaches to an existing console for printouts. Works with windows CMD but not MSYS2
    if (AttachConsole(ATTACH_PARENT_PROCESS) == 0) {
        // if fails, then allocate a new console
        AllocConsole();
    }
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif
}

/**
 * @brief One-time game initialization.
 *
 * Sets up the rendering layer, memory system, PPG textures, sound engine,
 * priority system, memory card, and the menu bridge.
 */
static void game_init() {
#if defined(DEBUG)
    DebugConfig_Init();
#endif

    flInitialize();
    flSetRenderState(FLRENDER_BACKCOLOR, 0);
    system_init_level = 0;
    ppgWorkInitializeApprication();
    distributeScratchPadAddress();
    Renderer_Init();
    MenuBridge_Init(g_shm_suffix);
    njUserInit();
    palCreateGhost();
    ppgMakeConvTableTexDC();
    appSetupBasePriority();
    NativeSave_Init();
}

/**
 * @brief Per-frame game logic (pre-render).
 *
 * Sets background color, reads input, runs netplay, dispatches the task
 * scheduler via njUserMain(), flushes 2D primitives, runs effects, then flips.
 */
static void game_step_0() {
    // ⚡ Bolt: Input Lag Test Trigger (F12)
    {
        static bool s_f12_prev = false;
        const Uint8* s = SDL_GetKeyboardState(NULL);
        bool f12_down = s[SDL_SCANCODE_F12];
        if (f12_down && !s_f12_prev && !g_sim_lag_active) {
            // Only start if character is in sub-routine 0 (Idle/Neutral) to ensure clean test
            if (plw[0].wu.routine_no[1] == 0) {
                g_sim_lag_active = true;
                g_sim_lag_frame = system_timer;
                s_lag_test_start_ticks = SDL_GetPerformanceCounter();
                // plw[0] is Player 1
                s_lag_test_initial_routine = plw[0].wu.routine_no[0];
                s_lag_test_initial_routine_1 = plw[0].wu.routine_no[1];
                SDL_Log("Bolt: Lag Test STARTED at frame %d. Waiting for state change...", g_sim_lag_frame);
            } else {
                SDL_Log("Bolt: Lag Test SKIPPED. Character not idle (R1=%d).", plw[0].wu.routine_no[1]);
            }
        }
        s_f12_prev = f12_down;
    }

    flSetRenderState(FLRENDER_BACKCOLOR, 0xFF000000);

    if (Debug_w[DEBUG_BLUE_BACK]) {
        flSetRenderState(FLRENDER_BACKCOLOR, 0xFF0000FF);
    }

    appSetupTempPriority();

    TRACE_SUB_BEGIN("Input");
    flPADGetALL();
    keyConvert();
    TRACE_SUB_END();

#if defined(DEBUG)
    if (!test_flag) {
        if (mpp_w.sysStop) {
            sysSLOW = 1;

            switch (io_w.data[1].sw_new) {
            case SWK_LEFT_STICK:
                mpp_w.sysStop = false;
                // fallthrough

            case SWK_LEFT_SHOULDER:
                Slow_Timer = 1;
                break;

            default:
                switch (io_w.data[1].sw & (SWK_LEFT_SHOULDER | SWK_LEFT_TRIGGER)) {
                case SWK_LEFT_SHOULDER | SWK_LEFT_TRIGGER:
                    if ((sysFF = Debug_w[DEBUG_FAST]) == 0) {
                        sysFF = 1;
                    }

                    sysSLOW = 1;
                    Slow_Timer = 1;

                    break;

                case SWK_LEFT_TRIGGER:
                    if (Slow_Timer == 0) {
                        if ((Slow_Timer = Debug_w[DEBUG_SLOW]) == 0) {
                            Slow_Timer = 1;
                        }

                        sysFF = 1;
                    }

                    break;

                default:
                    Slow_Timer = 2;
                    break;
                }

                break;
            }
        } else if (io_w.data[1].sw_new & SWK_LEFT_STICK) {
            mpp_w.sysStop = true;
        }
    }
#endif

    if ((Play_Mode != 3 && Play_Mode != 1) || (Game_pause != 0x81)) {
        p1sw_1 = p1sw_0;
        p2sw_1 = p2sw_0;
        p3sw_1 = p3sw_0;
        p4sw_1 = p4sw_0;
        p1sw_0 = p1sw_buff;
        p2sw_0 = p2sw_buff;
        p3sw_0 = p3sw_buff;
        p4sw_0 = p4sw_buff;

        if ((task[TASK_MENU].condition == 1) && (Mode_Type == MODE_PARRY_TRAINING) && (Play_Mode == 1)) {
            const u16 sw_buff = p2sw_0;
            p2sw_0 = p1sw_0;
            p1sw_0 = sw_buff;
        }
    }

    MenuBridge_PreTick();

    appCopyKeyData();

    mpp_w.inGame = false;

    NetplaySessionState current_net_state = Netplay_GetSessionState();

    if (current_net_state != NETPLAY_SESSION_IDLE) {
        TRACE_SUB_BEGIN("Netplay");
        Netplay_Run();

        // Match upstream behavior from PR 146: Flush the 2D polygon buffer each
        // frame when the game's normal render loop isn't running, preventing
        // the 100-item limit from overflowing during connection and matchmaking waits.
        Renderer_Flush2DPrimitives();

        TRACE_SUB_END();

        // Re-read state in case Netplay_Run changed it (e.g. EXITING -> IDLE)
        current_net_state = Netplay_GetSessionState();
    }

    // Only run game loop directly if we are in IDLE or LOBBY mode.
    // In TRANSITIONING, CONNECTING, and RUNNING modes, Netplay_Run() calls step_game() automatically.
    if (current_net_state == NETPLAY_SESSION_IDLE || current_net_state == NETPLAY_SESSION_LOBBY) {
        TRACE_SUB_BEGIN("GameTasks");
        njUserMain();

        // ⚡ Bolt: Input Lag Test Detection
        if (g_sim_lag_active) {
            // Check if Action Status (routine_no[0] or routine_no[1]) has changed
            if (plw[0].wu.routine_no[0] != s_lag_test_initial_routine ||
                plw[0].wu.routine_no[1] != s_lag_test_initial_routine_1) {

                int delta = system_timer - g_sim_lag_frame;
                Uint64 end_ticks = SDL_GetPerformanceCounter();
                double ms_latency =
                    (double)(end_ticks - s_lag_test_start_ticks) * 1000.0 / (double)SDL_GetPerformanceFrequency();

                SDL_Log("Bolt: Lag Test RESULT: %d frames (%.3f ms latency). Detected State Change R1: %d -> %d",
                        delta,
                        ms_latency,
                        s_lag_test_initial_routine_1,
                        plw[0].wu.routine_no[1]);
                g_sim_lag_active = false; // End test
            }
        }

        seqsBeforeProcess();
        TRACE_SUB_END();

        TRACE_SUB_BEGIN("Render2D");
        Renderer_Flush2DPrimitives();
        seqsAfterProcess();
        TRACE_SUB_END();
    }

    disp_effect_work();

    MenuBridge_PostTick();

    training_hud_draw();

    flFlip(0);
}

/**
 * @brief Per-frame game logic (post-render).
 *
 * Increments interrupt/record timers, updates screen transitions,
 * interleaved rendering, and the BGM server.
 */
static void game_step_1() {
    Interrupt_Timer += 1;
    Record_Timer += 1;

    Scrn_Renew();
    Irl_Family();
    Irl_Scrn();
    BGM_Server();
}

u8 dctex_linear_mem[0x800];
u8 texcash_melt_buffer_mem[0x1000];
u8 tpu_free_mem[0x2000];

/**
 * @brief Assign legacy PS2 scratch-pad memory addresses to their modern equivalents.
 *
 * The PS2 used on-chip scratch-pad RAM for fast temporary buffers. On modern
 * platforms these are ordinary heap-allocated arrays.
 */
void distributeScratchPadAddress() {
    dctex_linear = (s16*)dctex_linear_mem;
    texcash_melt_buffer = (u8*)texcash_melt_buffer_mem;
    tpu_free = (TexturePoolUsed*)tpu_free_mem;
}

/**
 * @brief Get the player's most-used character index.
 *
 * Scans mpp_w.useChar[] to find the character with the highest usage count.
 * Debug override: if Debug_w[DEBUG_MC_FAVORITE_PLNUM] is set, returns that value minus one.
 *
 * @return Character index (0-based), or 0 if no character has been used.
 */
s32 mppGetFavoritePlayerNumber() {
    s32 i;
    s32 max = 1;
    s32 num = 0;

    if (Debug_w[DEBUG_MC_FAVORITE_PLNUM]) {
        return Debug_w[DEBUG_MC_FAVORITE_PLNUM] - 1;
    }

    for (i = 0; i < CHARACTER_COUNT; i++) {
        if (max <= mpp_w.useChar[i]) {
            max = mpp_w.useChar[i];
            num = i + 1;
        }
    }

    return num;
}

/** @brief Copy per-player raw pad input into the PLsw double-buffer. */
void appCopyKeyData() {
    PLsw[0][1] = PLsw[0][0];
    PLsw[1][1] = PLsw[1][0];
    PLsw[0][0] = p1sw_buff;
    PLsw[1][0] = p2sw_buff;
}

/** @brief Allocate memory from the legacy foundation library heap. */
u8* mppMalloc(u32 size) {
    return flAllocMemory(size);
}

/**
 * @brief Legacy "Ninja User Init" — one-time game subsystem initialization.
 *
 * Initializes the memory manager, sequencer, PPG, zlib, RAM control,
 * sound system, and creates the first task (Init_Task).
 */
void njUserInit() {
    s32 i;
    u32 size;

    sysFF = 1;
    mpp_w.sysStop = false;
    mpp_w.inGame = false;
    mpp_w.language = 0;
    mmSystemInitialize();
    flGetFrame(&mpp_w.fmsFrame);
    seqsInitialize(mppMalloc(seqsGetUseMemorySize()));
    ppg_Initialize(mppMalloc(PPG_MEMORY_SIZE), PPG_MEMORY_SIZE);
    zlib_Initialize(mppMalloc(ZLIB_MEMORY_SIZE), ZLIB_MEMORY_SIZE);
    size = flGetSpace();
    mpp_w.ramcntBuff = mppMalloc(size);
    Init_ram_control_work(mpp_w.ramcntBuff, size);

    for (i = 0; i < CHARACTER_COUNT; i++) {
        mpp_w.useChar[i] = 0;
    }

    Interrupt_Timer = 0;
    Disp_Size_H = 100;
    Disp_Size_V = 100;
    Country = 4;

    if (Country == 0) {
        while (1) {}
    }

    Init_sound_system();
    Init_bgm_work();
    sndInitialLoad();
    cpInitTask();
    cpReadyTask(TASK_INIT, Init_Task);
}

/**
 * @brief Legacy "Ninja User Main" — per-frame game update.
 *
 * Clears timing lags, processes replay status for both players,
 * runs the task scheduler, and handles replay/fake-recording logic.
 */
void njUserMain() {
    CPU_Time_Lag[0] = 0;
    CPU_Time_Lag[1] = 0;
    CPU_Rec[0] = 0;
    CPU_Rec[1] = 0;

    Check_Replay_Status(0, Replay_Status[0]);
    Check_Replay_Status(1, Replay_Status[1]);

    cpLoopTask();

    if ((Game_pause != 0x81) && (Mode_Type == MODE_VERSUS) && (Play_Mode == 1)) {
        if ((plw[0].wu.pl_operator == 0) && (CPU_Rec[0] == 0) && (Replay_Status[0] == 1)) {
            p1sw_0 = 0;

            Check_Replay_Status(0, 1);

            if (Debug_w[DEBUG_DISP_REC_STATUS]) {
                flPrintColor(0xFFFFFFFF);
                flPrintL(0x10, 0xA, "FAKE REC! PL1");
            }
        }

        if ((plw[1].wu.pl_operator == 0) && (CPU_Rec[1] == 0) && (Replay_Status[1] == 1)) {
            p2sw_0 = 0;

            Check_Replay_Status(1, 1);

            if (Debug_w[DEBUG_DISP_REC_STATUS]) {
                flPrintColor(0xFFFFFFFF);
                flPrintL(0x10, 0xA, "FAKE REC!     PL2");
            }
        }
    }
}

/**
 * @brief Task scheduler loop — iterates all 11 task slots.
 *
 * Tasks in condition 1 (active) call their function pointer.
 * Tasks in condition 2 (ready) transition to active next frame.
 * Tasks in condition 0 or 3 are inactive/paused.
 */
void cpLoopTask() {
    disp_ramcnt_free_area();

#if defined(DEBUG)
    if (sysSLOW) {
        if (--Slow_Timer == 0) {
            sysSLOW = 0;
            Game_pause &= 0x7F;
        } else {
            Game_pause |= 0x80;
        }
    }
#endif

    for (s32 i = 0; i < TASK_SLOT_COUNT; i++) {
        struct _TASK* task_ptr = &task[i];

        switch (task_ptr->condition) {
        case 1:
            task_ptr->func_adrs(task_ptr);
            break;

        case 2:
            task_ptr->condition = 1;
            break;

        case 3:
            break;
        }
    }
}

/** @brief Clear all 11 task slots to zero. */
void cpInitTask() {
    memset(&task, 0, sizeof(task));
}

/**
 * @brief Create a new task in slot `num`.
 *
 * Clears the task struct, sets the function pointer, and marks it as
 * condition 2 ("ready" — will activate next frame).
 */
void cpReadyTask(TaskID num, void* func_adrs) {
    struct _TASK* task_ptr = &task[num];

    memset(task_ptr, 0, sizeof(struct _TASK));

    task_ptr->func_adrs = func_adrs;
    task_ptr->condition = 2;
}

/**
 * @brief Destroy a task in slot `num`.
 *
 * Sets condition to 0 (inactive) and calls the task's callback if one is set.
 */
void cpExitTask(TaskID num) {
    struct _TASK* task_ptr = &task[num];

    task_ptr->condition = 0;

    if (task_ptr->callback_adrs != NULL) {
        task_ptr->callback_adrs();
    }
}
