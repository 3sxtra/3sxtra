/**
 * @file menu_bridge.c
 * @brief Shared-memory bridge between the game and an external overlay menu.
 *
 * On Windows, creates a named shared-memory region to exchange input
 * state and navigation data with a separate menu process. The bridge
 * injects overlay inputs into the game's pad state and exports the
 * current menu navigation position for the overlay to display.
 */
#include "menu_bridge.h"
#include "common.h"

#include <SDL3/SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// Undefine conflicting macros from Windows headers
#ifdef cmb2
#undef cmb2
#endif
#ifdef cmb3
#undef cmb3
#endif
#ifdef s_addr
#undef s_addr
#endif
#endif

#include "game_state.h" // Includes headers for all the globals (workuser.h etc)
#include "sf33rd/Source/Game/engine/slowf.h"
#include "sf33rd/Source/Game/io/pulpul.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include <stdio.h>
#include <string.h>

// Global pointer to shared memory
static MenuBridgeState* g_bridge_state = NULL;

// (Deferred RNG reseed removed — seeds now applied directly in PreTick.
//  This is safe because force_match_active is only set AFTER allow_battle=1,
//  meaning All_Clear_Random_ix() has already been called.)

#ifdef _WIN32
static HANDLE g_hMapFile = NULL;
#endif

/** @brief Create the named shared-memory region (Windows only). */
void MenuBridge_Init(const char* shm_suffix) {
#ifdef _WIN32
    char name[256];
    if (shm_suffix && shm_suffix[0] != '\0') {
        snprintf(name, sizeof(name), "%s_%s", MENU_BRIDGE_SHM_NAME, shm_suffix);
    } else {
        snprintf(name, sizeof(name), "%s", MENU_BRIDGE_SHM_NAME);
    }

    // Create Named Shared Memory
    g_hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE,           // use paging file
                                    NULL,                           // default security
                                    PAGE_READWRITE,                 // read/write access
                                    0,                              // maximum object size (high-order DWORD)
                                    (DWORD)sizeof(MenuBridgeState), // maximum object size (low-order DWORD)
                                    name                            // name of mapping object
    );

    if (g_hMapFile == NULL) {
        printf("[MenuBridge] ERROR: CreateFileMappingA failed (%lu) for '%s'.\n", GetLastError(), name);
        return;
    }

    g_bridge_state = (MenuBridgeState*)MapViewOfFile(g_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MenuBridgeState));

    if (g_bridge_state == NULL) {
        printf("[MenuBridge] ERROR: MapViewOfFile failed (%lu).\n", GetLastError());
        CloseHandle(g_hMapFile);
        g_hMapFile = NULL;
        return;
    }

    memset(g_bridge_state, 0, sizeof(MenuBridgeState));

    SDL_Log("[MenuBridge] Initialized '%s'", name);
#else
    printf("[MenuBridge] Shared Memory not implemented for this platform.\n");
#endif
}

/** @brief Gate frame execution for step-driven replay sync.
 *
 * When step_mode_active=1, busy-waits until Python sets step_requested=1,
 * then clears it and returns. When step_mode_active=0, returns immediately.
 * Has a 5-second timeout to prevent permanent freeze if Python disconnects.
 */
void MenuBridge_StepGate(void) {
    if (!g_bridge_state || !g_bridge_state->step_mode_active)
        return;

    /* Spin-wait for step_requested with timeout */
    Uint64 start = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 timeout = freq * 5; /* 5 seconds */

    while (!g_bridge_state->step_requested) {
        Uint64 elapsed = SDL_GetPerformanceCounter() - start;
        if (elapsed > timeout) {
            printf("[MenuBridge] StepGate timeout! Disabling step mode.\n");
            g_bridge_state->step_mode_active = 0;
            return;
        }
        /* Yield briefly to avoid burning 100% CPU */
        SDL_Delay(0);
    }

    g_bridge_state->step_requested = 0; /* Consumed */
}

/** @brief Inject overlay inputs into the game's pad state (called before game tick). */
void MenuBridge_PreTick(void) {
    if (!g_bridge_state)
        return;

    // Debug print once every 300 frames (5 sec)
    static int tick_debug = 0;
    if (tick_debug++ % 300 == 0) {
        // printf("[MenuBridge] PreTick running. Active: %d\n", g_bridge_state->menu_input_active);
    }

    // Input Injection
    // If menu control is active, override inputs
    if (g_bridge_state->menu_input_active || g_bridge_state->selfplay_active) {
        // Inject P1
        p1sw_0 = g_bridge_state->p1_input;
        p1sw_buff = g_bridge_state->p1_input;

        // Inject P2
        p2sw_0 = g_bridge_state->p2_input;
        p2sw_buff = g_bridge_state->p2_input;

        // Prevent njUserMain() from zeroing our inputs.
        // It checks (MODE_VERSUS && Play_Mode==1 && Replay_Status==1)
        // and clears p1sw_0/p2sw_0 to 0 — wiping our injection.
        if (g_bridge_state->selfplay_active) {
            Replay_Status[0] = 0;
            Replay_Status[1] = 0;
            // Force both players as human-operated so Player_move() uses
            // our injected inputs instead of cpu_algorithm()
            Operator_Status[0] = 1;
            Operator_Status[1] = 1;
            plw[0].wu.pl_operator = 1;
            plw[1].wu.pl_operator = 1;
        }
    }

    // Force Match: Lightweight RNG seeding only.
    // Menu navigation (char select, SA, etc.) is handled by Python's MenuNavigator.
    // The game picks the correct stage through the normal menu flow.
    // Seeds applied directly here in PreTick — safe because Python only sets
    // force_match_active AFTER allow_battle=1 (All_Clear_Random_ix() already ran).
    if (g_bridge_state->force_match_active) {
        g_bridge_state->force_match_active = 0;

        printf("[MenuBridge] Seeding RNG: "
               "RNG16=%04X RNG32=%04X RNG16ex=%04X RNG32ex=%04X\n",
               (uint16_t)g_bridge_state->fm_rng_16,
               (uint16_t)g_bridge_state->fm_rng_32,
               (uint16_t)g_bridge_state->fm_rng_16_ex,
               (uint16_t)g_bridge_state->fm_rng_32_ex);

        Random_ix16 = g_bridge_state->fm_rng_16;
        Random_ix32 = g_bridge_state->fm_rng_32;
        Random_ix16_ex = g_bridge_state->fm_rng_16_ex;
        Random_ix32_ex = g_bridge_state->fm_rng_32_ex;
        // Mirror to COM and BG variants (matches Set_Replay_Header in sys_sub.c)
        Random_ix16_com = Random_ix16;
        Random_ix32_com = Random_ix32;
        Random_ix16_ex_com = Random_ix16_ex;
        Random_ix32_ex_com = Random_ix32_ex;
        Random_ix16_bg = Random_ix16;

        printf("[MenuBridge] RNG applied in PreTick: ix16=%04X ix32=%04X ix16ex=%04X ix32ex=%04X\n",
               (uint16_t)Random_ix16,
               (uint16_t)Random_ix32,
               (uint16_t)Random_ix16_ex,
               (uint16_t)Random_ix32_ex);
    }
}

/** @brief Export current navigation state (G_No, cursor, chars) to shared memory. */
void MenuBridge_PostTick(void) {
    if (!g_bridge_state)
        return;

    // (Deferred RNG reseed removed — seeds now applied directly in PreTick)

    // Frame counter (for external tool sync)
    // Use Interrupt_Timer which increments every frame unconditionally (in game_step_1)
    g_bridge_state->frame_count = Interrupt_Timer;

    // Combat-active flag (matches is_in_match from CPS3 Lua dumper)
    g_bridge_state->allow_battle = Allow_a_battle_f;

    // Export Navigation State
    memcpy(g_bridge_state->nav_G_No, G_No, 4);
    memcpy(g_bridge_state->nav_S_No, S_No, 4);

    g_bridge_state->nav_Play_Type = Play_Type;
    g_bridge_state->nav_Play_Game = Play_Game;

    // Character Selection
    g_bridge_state->nav_My_char[0] = My_char[0];
    g_bridge_state->nav_My_char[1] = My_char[1];

    g_bridge_state->nav_Super_Arts[0] = Super_Arts[0];
    g_bridge_state->nav_Super_Arts[1] = Super_Arts[1];

    // Cursor Feedback
    // Clamp cursor indices to bounds [0, 2][0, 7] roughly, but array is [3][8]
    int p1_x = Cursor_X[0];
    int p1_y = Cursor_Y[0];
    int p2_x = Cursor_X[1];
    int p2_y = Cursor_Y[1];

    g_bridge_state->nav_Cursor_X[0] = (int8_t)p1_x;
    g_bridge_state->nav_Cursor_Y[0] = (int8_t)p1_y;
    g_bridge_state->nav_Cursor_X[1] = (int8_t)p2_x;
    g_bridge_state->nav_Cursor_Y[1] = (int8_t)p2_y;

    // Safe lookup for ID_of_Face
    // Check bounds to prevent crash if cursor is somehow wild
    if (p1_y >= 0 && p1_y < 3 && p1_x >= 0 && p1_x < 8) {
        g_bridge_state->nav_Cursor_Char[0] = ID_of_Face[p1_y][p1_x];
    } else {
        g_bridge_state->nav_Cursor_Char[0] = -1;
    }

    if (p2_y >= 0 && p2_y < 3 && p2_x >= 0 && p2_x < 8) {
        g_bridge_state->nav_Cursor_Char[1] = ID_of_Face[p2_y][p2_x];
    } else {
        g_bridge_state->nav_Cursor_Char[1] = -1;
    }

    // SA cursor position
    g_bridge_state->nav_Cursor_SA[0] = (int8_t)Arts_Y[0];
    g_bridge_state->nav_Cursor_SA[1] = (int8_t)Arts_Y[1];

    // Screen sub-state (for FIGHT banner detection)
    memcpy(g_bridge_state->nav_C_No, C_No, 4);

    // Game state for parity testing
    g_bridge_state->game_timer = Game_timer;
    g_bridge_state->p1_health = plw[0].wu.vital_new;
    g_bridge_state->p2_health = plw[1].wu.vital_new;
    g_bridge_state->p1_pos_x = plw[0].wu.position_x;
    g_bridge_state->p1_pos_y = plw[0].wu.position_y;
    g_bridge_state->p2_pos_x = plw[1].wu.position_x;
    g_bridge_state->p2_pos_y = plw[1].wu.position_y;
    g_bridge_state->p1_facing = plw[0].wu.direction;
    g_bridge_state->p2_facing = plw[1].wu.direction;
    g_bridge_state->p1_meter = spg_dat[0].current_spg;
    g_bridge_state->p2_meter = spg_dat[1].current_spg;
    g_bridge_state->p1_stun = sdat[0].cstn;
    g_bridge_state->p2_stun = sdat[1].cstn;
    g_bridge_state->p1_busy = plw[0].do_not_move;
    g_bridge_state->p2_busy = plw[1].do_not_move;

    // RNG state for parity testing
    g_bridge_state->rng_16 = Random_ix16;
    g_bridge_state->rng_32 = Random_ix32;
    g_bridge_state->rng_16_ex = Random_ix16_ex;
    g_bridge_state->rng_32_ex = Random_ix32_ex;

    // Extended game state for parity testing
    g_bridge_state->p1_action =
        ((uint32_t)(uint16_t)plw[0].wu.routine_no[0]) | ((uint32_t)(uint16_t)plw[0].wu.routine_no[1] << 16);
    g_bridge_state->p2_action =
        ((uint32_t)(uint16_t)plw[1].wu.routine_no[0]) | ((uint32_t)(uint16_t)plw[1].wu.routine_no[1] << 16);
    g_bridge_state->p1_animation = plw[0].wu.cg_number;
    g_bridge_state->p2_animation = plw[1].wu.cg_number;
    g_bridge_state->p1_posture = plw[0].guard_flag;
    g_bridge_state->p2_posture = plw[1].guard_flag;
    g_bridge_state->p1_freeze =
        (uint8_t)(plw[0].wu.hit_stop > 255 ? 255 : (plw[0].wu.hit_stop < 0 ? 0 : plw[0].wu.hit_stop));
    g_bridge_state->p2_freeze =
        (uint8_t)(plw[1].wu.hit_stop > 255 ? 255 : (plw[1].wu.hit_stop < 0 ? 0 : plw[1].wu.hit_stop));
    g_bridge_state->is_in_match = Allow_a_battle_f;
}
