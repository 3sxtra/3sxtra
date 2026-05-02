/**
 * @file fsm.c
 * @brief Finite State Machine accessors for the game state hierarchy.
 *
 * Wraps raw g_state.G_No[] mutations with named, bounds-checked functions.
 * This is the only module that should directly write to g_state.G_No[].
 */

#include "sf33rd/Source/Game/fsm.h"
#include "game_state.h"
#include "port/I_System.h"

void FSM_SetMainState(MainState state) {
    if (state >= MAIN_STATE_COUNT) {
        I_Error("FSM_SetMainState(): invalid state %d!", state);
        return;
    }
    g_state.G_No[0] = (u8)state;
}

void FSM_SetMode(GameModeState mode) {
    if (mode >= GAME_STATE_COUNT) {
        I_Error("FSM_SetMode(): invalid mode %d!", mode);
        return;
    }
    g_state.G_No[1] = (u8)mode;
    g_state.G_No[2] = 0;
    g_state.G_No[3] = 0;
}

void FSM_SetSubState(u8 sub) {
    g_state.G_No[2] = sub;
    g_state.G_No[3] = 0;
}

void FSM_AdvanceSubState(void) {
    g_state.G_No[2]++;
    g_state.G_No[3] = 0;
}

void FSM_AdvanceSubSubState(void) {
    g_state.G_No[3]++;
}

void FSM_SetDemoPhase(u8 phase) {
    g_state.G_No[1] = phase;
    g_state.G_No[2] = 0;
    g_state.G_No[3] = 0;
}

void FSM_AdvanceDemoPhase(void) {
    g_state.G_No[1]++;
    g_state.G_No[2] = 0;
    g_state.G_No[3] = 0;
}
