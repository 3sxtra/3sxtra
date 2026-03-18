/**
 * @file menu_task.h
 * @brief Accessor functions for cross-slot task[TASK_MENU] access.
 *
 * Phase 2 of the TASK system modernization.  All external code that
 * reads or writes task[TASK_MENU].r_no[] or .condition should use
 * these helpers instead of touching the struct directly.
 *
 * This keeps the struct layout unchanged (netplay rollback safe)
 * while routing cross-slot mutations through a single chokepoint.
 */
#ifndef MENU_TASK_H
#define MENU_TASK_H

#include "sf33rd/Source/Game/menu/menu_task_phases.h"

#include <stdbool.h>

/* ── Phase getters ─────────────────────────────────────────────── */

/** @brief Read task[TASK_MENU].r_no[0] as a MenuTaskPhase. */
MenuTaskPhase MenuTask_GetPhase(void);

/** @brief Read task[TASK_MENU].r_no[1] as a MenuTaskSubPhase. */
MenuTaskSubPhase MenuTask_GetSubPhase(void);

/** @brief Read a raw r_no slot (0–3). For internal sub-states not yet named. */
int MenuTask_GetRNo(int idx);

/* ── Phase setters ─────────────────────────────────────────────── */

/** @brief Write task[TASK_MENU].r_no[0]. */
void MenuTask_SetPhase(MenuTaskPhase phase);

/** @brief Write task[TASK_MENU].r_no[1]. */
void MenuTask_SetSubPhase(MenuTaskSubPhase sub);

/* ── Combined transitions ──────────────────────────────────────── */

/** @brief Set r_no[0] = phase, r_no[1] = MTSP_INIT. */
void MenuTask_GotoPhase(MenuTaskPhase phase);

/* ── Condition ─────────────────────────────────────────────────── */

/** @brief Returns true when task[TASK_MENU].condition == 1. */
bool MenuTask_IsActive(void);

#endif /* MENU_TASK_H */
