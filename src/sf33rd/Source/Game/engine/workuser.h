/**
 * @file workuser.h
 * @brief Extern declarations for gameplay globals (workuser). Umbrella header for backward compat.
 *
 * @netplay_sync
 * All variables in the "Serialized" section below are saved/loaded as part of
 * GameState in game_state.h. If you add a new global here, you must also add
 * the corresponding field to GameState and to GameState_Save/GameState_Load.
 *
 * Variables in the "Non-serializable" section (pointers to ROM tables) do not
 * affect determinism and are excluded from rollback state.
 */
#ifndef WORKUSER_H
#define WORKUSER_H

#include "workuser_score.h"
#include "workuser_combat.h"
#include "workuser_select.h"
#include "workuser_system.h"

#endif // WORKUSER_H
