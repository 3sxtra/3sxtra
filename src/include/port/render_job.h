/**
 * @file render_job.h
 * @brief Lightweight render job scheduler for FrameGraph Phase 3/4.
 *
 * Provides a fixed-size thread pool (2 workers) for dispatching render
 * pass recording jobs concurrently. Uses SDL_Thread + SDL_Mutex + SDL_Condition.
 *
 * For backends that don't support concurrent recording (OpenGL), jobs
 * execute sequentially on the calling thread via RenderJobQueue_RunSync().
 *
 * Usage:
 *   RenderJobQueue_Init();
 *   ...
 *   RenderJob jobs[2];
 *   jobs[0] = (RenderJob){ .pass_index = 0, .fn = record_canvas, .userdata = &ctx };
 *   jobs[1] = (RenderJob){ .pass_index = 1, .fn = record_hd,     .userdata = &ctx };
 *   RenderJobQueue_Submit(jobs, 2);    // dispatch to thread pool
 *   RenderJobQueue_WaitAll();          // block until all complete
 *   ...
 *   RenderJobQueue_Shutdown();
 */
#ifndef RENDER_JOB_H
#define RENDER_JOB_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Function signature for a render pass recording job. */
typedef void (*RenderJobFn)(int pass_index, void* userdata);

/** @brief A single dispatchable render job. */
typedef struct RenderJob {
    int pass_index; /**< Which render pass this job records */
    RenderJobFn fn; /**< Recording function to execute */
    void* userdata; /**< Opaque context (e.g., per-pass state pointer) */
} RenderJob;

/**
 * @brief Initialize the render job thread pool.
 *
 * Creates `worker_count` background threads that sleep until jobs arrive.
 * Pass 0 for `worker_count` to disable threading (all jobs run synchronously).
 *
 * @param worker_count Number of worker threads (0 = synchronous mode).
 */
void RenderJobQueue_Init(int worker_count);

/**
 * @brief Shut down the thread pool and join all workers.
 */
void RenderJobQueue_Shutdown(void);

/**
 * @brief Submit an array of jobs to the thread pool.
 *
 * Jobs are dispatched to idle workers. If the pool is full, the calling
 * thread blocks until a worker becomes available.
 *
 * @param jobs  Array of RenderJob descriptors.
 * @param count Number of jobs to submit.
 */
void RenderJobQueue_Submit(const RenderJob* jobs, int count);

/**
 * @brief Block the calling thread until all submitted jobs have completed.
 */
void RenderJobQueue_WaitAll(void);

/**
 * @brief Execute jobs synchronously on the calling thread (no threading).
 *
 * Used as a fallback for OpenGL backends or when the thread pool is disabled.
 *
 * @param jobs  Array of RenderJob descriptors.
 * @param count Number of jobs to execute.
 */
void RenderJobQueue_RunSync(const RenderJob* jobs, int count);

/**
 * @brief Query whether the job queue is running in threaded mode.
 * @return true if worker threads are active, false if synchronous-only.
 */
bool RenderJobQueue_IsThreaded(void);

#ifdef __cplusplus
}
#endif

#endif /* RENDER_JOB_H */
