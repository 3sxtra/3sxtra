#ifndef SPU_SORT_DISPATCH_H
#define SPU_SORT_DISPATCH_H

#include <stdint.h>
#include <stdbool.h>
#include "ps3_renderer_gcm.h"

/**
 * @brief Initialize the SPU sort subsystem.
 *
 * Loads the SPU sort ELF, creates a 1-thread SPU thread group,
 * and starts it in a polling loop waiting for sort commands.
 * Must be called after sys_spu_initialize() and SPURS init.
 */
void SPUSort_Init(void);

/**
 * @brief Shut down the SPU sort subsystem.
 *
 * Signals the SPU thread to exit and joins the thread group.
 */
void SPUSort_Shutdown(void);

/**
 * @brief Submit render tasks for SPU-accelerated sorting.
 *
 * Copies {z, original_index} from the render task array into a
 * DMA-aligned staging buffer, signals the SPU to sort, then
 * copies sorted indices back.
 *
 * @param tasks     Pointer to the render task array.
 * @param task_count Number of tasks to sort (max 8192).
 * @return true if SPU sort completed, false if fallback to PPU is needed.
 */
bool SPUSort_Execute(GcmRenderTask* tasks, int task_count);

#endif /* SPU_SORT_DISPATCH_H */
