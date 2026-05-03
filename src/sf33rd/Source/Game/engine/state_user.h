/**
 * @file state_user.h
 * @brief Extern declarations for gameplay globals (state_user). Umbrella header for backward compat.
 *
 * @netplay_sync
 * All variables in the "Serialized" section below are saved/loaded as part of
 * GameState in game_state.h. If you add a new global here, you must also add
 * the corresponding field to GameState and to GameState_Save/GameState_Load.
 *
 * Variables in the "Non-serializable" section (pointers to ROM tables) do not
 * affect determinism and are excluded from rollback state.
 */
#ifndef STATE_USER_H
#define STATE_USER_H

#include "state_score.h"
#include "state_combat.h"
#include "state_select.h"
#include "state_system.h"

#endif // STATE_USER_H
