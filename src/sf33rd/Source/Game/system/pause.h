/**
 * @file pause.h
 * @brief Public API for the game pause/unpause system.
 */
#ifndef PAUSE_H
#define PAUSE_H

#include "structs.h"
#include "types.h"

void dispControllerWasRemovedMessage(s32 x, s32 y, s32 step);

extern void Pause_Task(struct _TASK*);

/* Accessor functions — decouple external modules from TASK_PAUSE internals */
void Pause_SetFlashPhase(u8 phase);
void Pause_SetFlashTimer(u8 timer);
void Pause_KillFlash(void);

#endif
