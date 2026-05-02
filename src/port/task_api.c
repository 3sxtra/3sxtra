#include "port/task_api.h"
#include "game_state.h"
#include "sf33rd/Source/Game/system/work_sys.h" /* extern struct _TASK task[11] */

#include <assert.h>

/* ── Generic Task Accessors ────────────────────────────────────────────── */

struct _TASK* Task_GetPtr(TaskID id) {
    assert(id < 11);
    return &task[id];
}

bool Task_IsActive(TaskID id) {
    assert(id < 11);
    return task[id].condition == 1;
}

void Task_Activate(TaskID id) {
    assert(id < 11);
    task[id].condition = 1;
}

void Task_Deactivate(TaskID id) {
    assert(id < 11);
    task[id].condition = 0;
}

/* ── Specific Task State Setters ───────────────────────────────────────── */

void PauseTask_SetPhase(int phase) {
    task[TASK_PAUSE].r_no[2] = (u8)phase;
}

void PauseTask_SetTimer(int timer) {
    task[TASK_PAUSE].free[0] = (u8)timer;
}

void Saver2_Task_SetPhase(int phase) {
    task[TASK_SAVER2].r_no[0] = (u8)phase;
}
