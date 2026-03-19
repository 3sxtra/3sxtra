#include "port/task_api.h"
#include "sf33rd/Source/Game/system/work_sys.h" /* extern struct _TASK task[11] */

/* ── Generic Task Accessors ────────────────────────────────────────────── */

struct _TASK* Task_GetPtr(TaskID id) {
    return &task[id];
}

bool Task_IsActive(TaskID id) {
    return task[id].condition == 1;
}

void Task_Activate(TaskID id) {
    task[id].condition = 1;
}

void Task_Deactivate(TaskID id) {
    task[id].condition = 0;
}

/* ── Specific Task State Setters ───────────────────────────────────────── */

void PauseTask_SetPhase(int phase) {
    task[TASK_PAUSE].r_no[2] = (u8)phase;
}

void PauseTask_SetTimer(int timer) {
    task[TASK_PAUSE].free[0] = (s16)timer;
}

void Saver2_Task_SetPhase(int phase) {
    task[7].r_no[0] = (u8)phase;
}
