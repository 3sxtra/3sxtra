#ifndef FSM_H
#define FSM_H

#include "types.h"
#include "sf33rd/Source/Game/engine/workuser_system.h" // For G_No

typedef enum {
    MAIN_STATE_WAIT_AUTO_LOAD = 0,
    MAIN_STATE_LOOP_DEMO = 1,
    MAIN_STATE_GAME = 2,
    MAIN_STATE_COUNT
} MainState;

typedef enum {
    MODE_TITLE = 0,
    MODE_ATTRACT = 1,
    MODE_FIGHT = 2,
    MODE_VS_SCREEN = 3,
    MODE_WIN_QUOTE = 4,
    MODE_CONTINUE = 5,
    MODE_GAME_OVER = 6,
    MODE_RANKING = 7,
    MODE_ENDING = 8,
    MODE_STAFF_ROLL = 9,
    MODE_TRAINING = 10,
    MODE_OPTIONS = 11,
    MODE_CHALLENGE = 12,
    GAME_STATE_COUNT
} GameModeState;

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Set the Main engine state (G_No[0]) */
void FSM_SetMainState(MainState state);

/** @brief Set the Game Mode state (G_No[1]) and zero out sub-states */
void FSM_SetMode(GameModeState mode);

/** @brief Set the specific sub-state (G_No[2]) and zero out sub-sub-states */
void FSM_SetSubState(u8 sub);

/** @brief Advance to the next sub-state (G_No[2]++) and zero out sub-sub-states */
void FSM_AdvanceSubState(void);

/** @brief Advance to the next sub-sub-state (G_No[3]++) */
void FSM_AdvanceSubSubState(void);

/** @brief Set the Loop_Demo phase counter (G_No[1]) and zero sub-states.
 *  Loop_Demo reuses G_No[1] as a sequential phase index (0–7),
 *  distinct from the GameModeState enum used by Game(). */
void FSM_SetDemoPhase(u8 phase);

/** @brief Advance to the next Loop_Demo phase (G_No[1]++) and zero sub-states. */
void FSM_AdvanceDemoPhase(void);

#ifdef __cplusplus
}
#endif

#endif // FSM_H
