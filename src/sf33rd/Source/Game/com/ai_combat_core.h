/**
 * @file ai_combat_core.h
 * @brief Data-driven Combat AI VM — bytecode interpreter for character AI patterns.
 *
 * Replaces 20 hardcoded ai_active_*.c files with a single runtime interpreter
 * that reads pattern data from assets/ai/combat_sequences.dat.
 *
 * Each character's AI is a set of patterns, each pattern is a sequence of
 * steps that call existing AI subroutine functions with serialized arguments.
 */

#ifndef AI_COMBAT_CORE_H
#define AI_COMBAT_CORE_H

#include "structs.h"

/**
 * AI VM Opcodes — maps 1:1 to AI subroutine functions in ai_subroutines.h.
 * Order MUST match the OPCODES dict in scripts/compile_combat_ai.py.
 */
typedef enum {
    AI_OP_END_PATTERN = 0,
    AI_OP_LEVER_OFF,
    AI_OP_LOOK,
    AI_OP_WAIT,
    AI_OP_WALK,
    AI_OP_JUMP,
    AI_OP_HI_JUMP,
    AI_OP_NORMAL_ATTACK,
    AI_OP_NORMAL_ATTACK_SP,
    AI_OP_ADJUST_ATTACK,
    AI_OP_LEVER_ATTACK,
    AI_OP_LEVER_ATTACK_SP,
    AI_OP_COMMAND_ATTACK,
    AI_OP_JUMP_ATTACK,
    AI_OP_JUMP_COMMAND_ATTACK,
    AI_OP_RAPID_COMMAND_ATTACK,
    AI_OP_JUMP_COMMAND_ATTACK_TERM,
    AI_OP_HI_JUMP_ATTACK,
    AI_OP_HI_JUMP_ATTACK_TERM,
    AI_OP_HI_JUMP_COMMAND_ATTACK_TERM,
    AI_OP_CHECK_JUMP_ATTACK_CONDITIONS,
    AI_OP_CHECK_ENEMY_DISTANCE,
    AI_OP_APPROACH_WALK,
    AI_OP_CHECK_SUPER_ART_CONDITIONS,
    AI_OP_CHECK_SA,
    AI_OP_CHECK_EX,
    AI_OP_CHECK_SA_FULL,
    AI_OP_AI_RANDOM_ACTION_SELECT,
    AI_OP_BRANCH_BY_DISTANCE,
    AI_OP_ENABLE_OVERHEAD_ATTACK_FLAG,
    AI_OP_LEVER_ON,
    AI_OP_ONLY_SHOT,
    AI_OP_TURN_OVER_ON,
    AI_OP_SETUP_DENJIN_LEVEL,
    AI_OP_HOLD_ATTACK_BUTTON,
    AI_OP_KEEP_AWAY,
    AI_OP_CHECK_SAFE_RETREAT_SPACE,
    AI_OP_PROVOKE,
    AI_OP_NEXT_ANOTHER_MENU,
    AI_OP_CHECK_MISCELLANEOUS_CONDITIONS,
    AI_OP_ORO_CHECK_JUMP_ATTACK,
    AI_OP_ORO_CHECK_HIGH_JUMP_ATTACK,
    AI_OP_ORO_CHECK_JUMP_COMMAND_ATTACK,
    AI_OP_ORO_CHECK_HIGH_JUMP_COMMAND_ATTACK,
    AI_OP_COUNT
} AIOp;

typedef enum {
    AITABLE_ACTIVE_A = 0,
    AITABLE_ACTIVE_B,
    AITABLE_ACTIVE_C,
    AITABLE_ACTIVE_D,
    AITABLE_SA_ACTIVE_A,
    AITABLE_SA_ACTIVE_B,
    AITABLE_SA_ACTIVE_C,
    AITABLE_SA_ACTIVE_D
} AIActionTableType;

/** @brief Load combat_sequences.dat and action_tables.dat into memory. */
void AICore_Init(void);

/** @brief Execute the current AI pattern step for a character. */
void AICore_ExecutePattern(PlayerEntity* wk);

/** @brief Fetch a single byte from a 3D binary action table [char][lv][rnd]. */
u8 AICore_GetActionTableValue(AIActionTableType table, int dim0, int dim1, int dim2);

/** @brief Free all loaded AI data. */
void AICore_Shutdown(void);

#endif
