#ifndef AI_COMBAT_CORE_H
#define AI_COMBAT_CORE_H

#include "structs.h"

// AI VM Command Opcodes
typedef enum {
    AI_CMD_WALK,
    AI_CMD_ATTACK,
    AI_CMD_WAIT,
    AI_CMD_GUARD,
    AI_CMD_CALLBACK,
    AI_CMD_END_PATTERN
} AICmdOpcode;

void AICore_Init(void);
void AICore_ExecutePattern(PlayerEntity* wk);

#endif
