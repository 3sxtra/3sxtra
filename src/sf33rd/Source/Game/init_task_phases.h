/**
 * @file init_task_phases.h
 * @brief Named constants for TASK_INIT r_no[0] magic numbers.
 *
 * These replace raw integer assignments used when external systems
 * (sys_sub.c, menu.c, game.c) read or write task[TASK_INIT].r_no[0].
 */
#ifndef INIT_TASK_PHASES_H
#define INIT_TASK_PHASES_H

/**
 * @brief Phases of the Init task (task[TASK_INIT].r_no[0]).
 */
typedef enum InitTaskPhase {
    ITP_BOOT = 0,    /**< Initial state after cpReadyTask — first-time init */
    ITP_RUNNING = 1, /**< Game loop active — normal operation */
} InitTaskPhase;

#endif /* INIT_TASK_PHASES_H */
