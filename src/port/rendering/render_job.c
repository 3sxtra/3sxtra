/**
 * @file render_job.c
 * @brief Render job thread pool implementation.
 *
 * Fixed-size worker pool using SDL_Thread / SDL_Mutex / SDL_Condition.
 * Workers spin on a condition variable waiting for jobs. The main thread
 * submits jobs and then blocks on WaitAll() until completion.
 *
 * Memory model: jobs[] is written by the main thread before signaling,
 * and read by workers after waking. No atomics needed — the mutex
 * provides the happens-before relationship.
 */
#include "port/render_job.h"

#include <SDL3/SDL.h>
#include <string.h>

/* ─── Constants ──────────────────────────────────────────────────────── */

#define MAX_WORKERS 4
#define MAX_JOBS 16

/* ─── Internal State ─────────────────────────────────────────────────── */

typedef struct {
    /* Job queue (main thread writes, workers read) */
    RenderJob jobs[MAX_JOBS];
    int job_count;       /* total jobs in current batch */
    int next_job;        /* index of next unclaimed job */
    int completed_count; /* jobs finished so far */

    /* Thread pool */
    SDL_Thread* workers[MAX_WORKERS];
    int worker_count;
    bool shutdown; /* signal workers to exit */

    /* Synchronization */
    SDL_Mutex* mutex;
    SDL_Condition* work_available; /* signaled when new jobs arrive */
    SDL_Condition* all_done;       /* signaled when all jobs complete */
} JobQueue;

static JobQueue s_queue;

/* ─── Worker Thread ──────────────────────────────────────────────────── */

static int worker_thread_fn(void* data) {
    (void)data;

    SDL_LockMutex(s_queue.mutex);
    for (;;) {
        /* Wait for work or shutdown */
        while (s_queue.next_job >= s_queue.job_count && !s_queue.shutdown) {
            SDL_WaitCondition(s_queue.work_available, s_queue.mutex);
        }

        if (s_queue.shutdown) {
            SDL_UnlockMutex(s_queue.mutex);
            return 0;
        }

        /* Claim a job */
        int idx = s_queue.next_job++;
        RenderJob job = s_queue.jobs[idx];
        SDL_UnlockMutex(s_queue.mutex);

        /* Execute outside the lock */
        if (job.fn) {
            job.fn(job.pass_index, job.userdata);
        }

        /* Mark completion */
        SDL_LockMutex(s_queue.mutex);
        s_queue.completed_count++;
        if (s_queue.completed_count >= s_queue.job_count) {
            SDL_SignalCondition(s_queue.all_done);
        }
    }
}

/* ─── Public API ─────────────────────────────────────────────────────── */

void RenderJobQueue_Init(int worker_count) {
    memset(&s_queue, 0, sizeof(s_queue));

    if (worker_count <= 0) {
        /* Synchronous mode — no threads */
        return;
    }

    if (worker_count > MAX_WORKERS)
        worker_count = MAX_WORKERS;

    s_queue.mutex = SDL_CreateMutex();
    s_queue.work_available = SDL_CreateCondition();
    s_queue.all_done = SDL_CreateCondition();

    s_queue.worker_count = worker_count;
    for (int i = 0; i < worker_count; i++) {
        char name[32];
        SDL_snprintf(name, sizeof(name), "RenderWorker%d", i);
        s_queue.workers[i] = SDL_CreateThread(worker_thread_fn, name, NULL);
    }

    SDL_Log("RenderJobQueue: initialized with %d worker thread(s)", worker_count);
}

void RenderJobQueue_Shutdown(void) {
    if (s_queue.worker_count == 0)
        return;

    SDL_LockMutex(s_queue.mutex);
    s_queue.shutdown = true;
    SDL_BroadcastCondition(s_queue.work_available);
    SDL_UnlockMutex(s_queue.mutex);

    for (int i = 0; i < s_queue.worker_count; i++) {
        SDL_WaitThread(s_queue.workers[i], NULL);
        s_queue.workers[i] = NULL;
    }

    SDL_DestroyCondition(s_queue.all_done);
    SDL_DestroyCondition(s_queue.work_available);
    SDL_DestroyMutex(s_queue.mutex);

    s_queue.worker_count = 0;
    SDL_Log("RenderJobQueue: shut down");
}

void RenderJobQueue_Submit(const RenderJob* jobs, int count) {
    if (count <= 0)
        return;

    /* Fallback: if no workers, run synchronously */
    if (s_queue.worker_count == 0) {
        RenderJobQueue_RunSync(jobs, count);
        return;
    }

    if (count > MAX_JOBS)
        count = MAX_JOBS;

    SDL_LockMutex(s_queue.mutex);
    memcpy(s_queue.jobs, jobs, count * sizeof(RenderJob));
    s_queue.job_count = count;
    s_queue.next_job = 0;
    s_queue.completed_count = 0;
    SDL_BroadcastCondition(s_queue.work_available);
    SDL_UnlockMutex(s_queue.mutex);
}

void RenderJobQueue_WaitAll(void) {
    if (s_queue.worker_count == 0)
        return; /* sync mode — already done */

    SDL_LockMutex(s_queue.mutex);
    while (s_queue.completed_count < s_queue.job_count) {
        SDL_WaitCondition(s_queue.all_done, s_queue.mutex);
    }
    SDL_UnlockMutex(s_queue.mutex);
}

void RenderJobQueue_RunSync(const RenderJob* jobs, int count) {
    for (int i = 0; i < count; i++) {
        if (jobs[i].fn) {
            jobs[i].fn(jobs[i].pass_index, jobs[i].userdata);
        }
    }
}

bool RenderJobQueue_IsThreaded(void) {
    return s_queue.worker_count > 0;
}
