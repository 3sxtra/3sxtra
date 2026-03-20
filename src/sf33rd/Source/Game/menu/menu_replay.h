#pragma once
#include "sf33rd/Source/Game/menu/menu.h"

void Wait_Replay_Check(struct _TASK* task_ptr);
void Reset_Replay(struct _TASK* task_ptr);
void Wait_Replay_Load(void);
void After_Replay(struct _TASK* task_ptr);
void End_Replay_Menu(struct _TASK* task_ptr);
