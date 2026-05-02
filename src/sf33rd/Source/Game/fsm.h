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
    MODE_CHAR_SELECT = 1,
    MODE_FIGHT = 2,
    MODE_WIN_RESULT = 3,
    MODE_LOSE_RESULT = 4,
    MODE_NEXT_CPU = 5,
    MODE_GAME_OVER = 6,
    MODE_CONTINUE = 7,
    MODE_ENDING = 8,
    MODE_BONUS = 9,
    MODE_AFTER_BONUS = 10,
    MODE_NEXT_Q = 11,
    MODE_CHALLENGE = 12,
    GAME_STATE_COUNT
} GameModeState;

typedef enum {
    DEMO_PHASE_INIT = 0,
    DEMO_PHASE_LOGO = 1,
    DEMO_PHASE_TITLE = 2,
    DEMO_PHASE_FIGHT_1 = 3,
    DEMO_PHASE_RANKING_1 = 4,
    DEMO_PHASE_FIGHT_2 = 5,
    DEMO_PHASE_RANKING_2 = 6,
    DEMO_PHASE_COUNT
} DemoPhase;

typedef enum {
    FIGHT_SUB_INIT = 0,
    FIGHT_SUB_UPDATE = 1,
    FIGHT_SUB_RESET = 2,
    FIGHT_SUB_WAIT_START = 3,
    FIGHT_SUB_BG_ONLY = 4,
    FIGHT_SUB_FINISH = 5,
    FIGHT_SUB_WAIT_SEEK_1 = 6,
    FIGHT_SUB_WAIT_SEEK_2 = 7,
    FIGHT_SUB_COUNT
} FightSubState;

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Set the Main engine state (fsm[0]) */
void FSM_SetMainState(MainState state);

/** @brief Set the Game Mode state (fsm[1]) and zero out sub-states */
void FSM_SetMode(GameModeState mode);

/** @brief Set the specific sub-state (fsm[2]) and zero out sub-sub-states */
void FSM_SetSubState(u8 sub);

/** @brief Advance to the next sub-state (fsm[2]++) and zero out sub-sub-states */
void FSM_AdvanceSubState(void);

/** @brief Advance to the next sub-sub-state (fsm[3]++) */
void FSM_AdvanceSubSubState(void);

/** @brief Set the Loop_Demo phase counter (fsm[1]) and zero sub-states.
 *  Loop_Demo reuses fsm[1] as a sequential phase index (0–7),
 *  distinct from the GameModeState enum used by Game(). */
void FSM_SetDemoPhase(u8 phase);

/** @brief Advance to the next Loop_Demo phase (fsm[1]++) and zero sub-states. */
void FSM_AdvanceDemoPhase(void);

#ifdef __cplusplus
}
#endif

#endif // FSM_H
