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
#include "sf33rd/Source/Game/system/work_sys.h"
#include <stdio.h>
#include <string.h>

// Global pointer to shared memory
static MenuBridgeState* g_bridge_state = NULL;

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

    printf("[MenuBridge] Success! Initialized '%s' at %p\n", name, (void*)g_bridge_state);
#else
    printf("[MenuBridge] Shared Memory not implemented for this platform.\n");
#endif
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
    if (g_bridge_state->menu_input_active) {
        // Inject P1
        p1sw_0 = g_bridge_state->p1_input;
        p1sw_buff = g_bridge_state->p1_input;

        // Inject P2
        p2sw_0 = g_bridge_state->p2_input;
        p2sw_buff = g_bridge_state->p2_input;
    }
}

/** @brief Export current navigation state (G_No, cursor, chars) to shared memory. */
void MenuBridge_PostTick(void) {
    if (!g_bridge_state)
        return;

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
}
