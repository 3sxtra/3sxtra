/**
 * @file init_task.h
 * @brief Accessor functions for cross-slot task[TASK_INIT] access.
 *
 * Phase 3 of the TASK system modernization.  All external code that
 * reads or writes task[TASK_INIT].r_no[] or .condition should use
 * these helpers instead of touching the struct directly.
 *
 * This keeps the struct layout unchanged (netplay rollback safe)
 * while routing cross-slot mutations through a single chokepoint.
 */
#ifndef INIT_TASK_H
#define INIT_TASK_H

#include "sf33rd/Source/Game/init_task_phases.h"

#include <stdbool.h>

/* ── Phase getters ─────────────────────────────────────────────── */

/** @brief Read task[TASK_INIT].r_no[0] as an InitTaskPhase. */
InitTaskPhase InitTask_GetPhase(void);

/* ── Phase setters ─────────────────────────────────────────────── */

/** @brief Write task[TASK_INIT].r_no[0]. */
void InitTask_SetPhase(InitTaskPhase phase);

/** @brief Zero r_no[1..3], leaving r_no[0] untouched. */
void InitTask_ResetSubPhases(void);

/** @brief Zero all r_no[0..3]. */
void InitTask_ClearAllRNo(void);

/* ── Condition ─────────────────────────────────────────────────── */

/** @brief Set task[TASK_INIT].condition = 0 (deactivate the task). */
void InitTask_Deactivate(void);

/** @brief Returns true when task[TASK_INIT].condition == 1. */
bool InitTask_IsActive(void);

#endif /* INIT_TASK_H */
