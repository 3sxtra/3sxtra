/**
 * @file tracy_gpu.cpp
 * @brief Tracy GPU profiling bridge for OpenGL.
 *
 * Uses Tracy's low-level C++ API with OpenGL timer queries to measure
 * actual GPU execution times. Called from C code via tracy_gpu.h macros.
 * Ref: Tracy manual §3.9.1 (OpenGL), TracyOpenGL.hpp source.
 */

#include "port/tracy_gpu.h"

#ifdef TRACY_ENABLE

#include <glad/gl.h>
#include <stdio.h>
#include <string.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>

// ── Configuration ────────────────────────────────────────────
// Minimal query count for stability
static constexpr int QUERY_COUNT = 1024;
static_assert((QUERY_COUNT & (QUERY_COUNT - 1)) == 0, "QUERY_COUNT must be power of 2");

// ── State ────────────────────────────────────────────────────
static GLuint s_queries[QUERY_COUNT];
static int s_head = 0; // next slot to issue a query into
static int s_tail = 0; // next slot to collect results from
static bool s_ready = false;
static uint8_t s_ctx = 0; // Tracy GPU context id
static int s_depth = 0;   // nesting depth — tracks unpaired Begin calls

// Fast modular wrap (power-of-2)
static inline int wrap(int v) {
    return v & (QUERY_COUNT - 1);
}

// Consume the next query slot.
// If buffer is full, advance tail (drop oldest) to make room.
static inline uint16_t nextSlot() {
    // Check if full (head + 1 == tail)
    if (wrap(s_head + 1) == s_tail) {
        // Buffer full!
        printf("TracyGpu: Ring buffer full! Dropping oldest query.\n");
        s_tail = wrap(s_tail + 1);
    }

    uint16_t slot = (uint16_t)s_head;
    s_head = wrap(s_head + 1);
    return slot;
}

extern "C" {

// ─────────────────────────────────────────────────────────────
// Init — call once after GL context + glad are ready
// ─────────────────────────────────────────────────────────────
void TracyGpu_Init(void) {
    printf("TracyGpu_Init: Starting...\n");

    // Clear any pending GL errors (just once, no looping)
    while (glGetError() != GL_NO_ERROR)
        ;

    // Try to allocate queries
    glGenQueries(QUERY_COUNT, s_queries);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("TracyGpu_Init: Failed to alloc queries (0x%x). GPU profiling disabled.\n", err);
        return;
    }

    // Calibration
    GLint64 gpu_ts = 0;
    glGetInteger64v(GL_TIMESTAMP, &gpu_ts);
    int64_t cpu_ts = tracy::Profiler::GetTime();

    err = glGetError();
    if (err != GL_NO_ERROR || gpu_ts == 0) {
        printf("TracyGpu_Init: Calibration failed (0x%x). GPU profiling disabled.\n", err);
        // Clean up queries if we can't use them
        glDeleteQueries(QUERY_COUNT, s_queries);
        return;
    }

    // Register context
    {
        auto* item = tracy::Profiler::QueueSerial();
        tracy::MemWrite(&item->hdr.type, tracy::QueueType::GpuNewContext);
        tracy::MemWrite(&item->gpuNewContext.cpuTime, cpu_ts);
        tracy::MemWrite(&item->gpuNewContext.gpuTime, (int64_t)gpu_ts);
        tracy::MemWrite(&item->gpuNewContext.thread, tracy::GetThreadHandle());
        tracy::MemWrite(&item->gpuNewContext.period, 1.0f);
        tracy::MemWrite(&item->gpuNewContext.context, s_ctx);
        tracy::MemWrite(&item->gpuNewContext.flags, (uint8_t)0);
        tracy::MemWrite(&item->gpuNewContext.type, tracy::GpuContextType::OpenGl);
        tracy::Profiler::QueueSerialFinish();
    }

    s_head = 0;
    s_tail = 0;
    s_depth = 0;
    s_ready = true;
    printf("TracyGpu_Init: Initialized successfully.\n");
}

// ─────────────────────────────────────────────────────────────
// BeginZone — issue a GL timestamp query + emit zone begin.
// Always succeeds (overwrites old data if full).
// ─────────────────────────────────────────────────────────────
void TracyGpu_BeginZone(const char* name, const char* file, int line) {
    if (!s_ready)
        return;

    s_depth++;

    uint16_t slot = nextSlot();
    glQueryCounter(s_queries[slot], GL_TIMESTAMP);

    const auto srcloc =
        ___tracy_alloc_srcloc_name((uint32_t)line, file, strlen(file), name, strlen(name), name, strlen(name), 0);

    auto* item = tracy::Profiler::QueueSerial();
    tracy::MemWrite(&item->hdr.type, tracy::QueueType::GpuZoneBeginAllocSrcLocSerial);
    tracy::MemWrite(&item->gpuZoneBegin.cpuTime, tracy::Profiler::GetTime());
    tracy::MemWrite(&item->gpuZoneBegin.srcloc, srcloc);
    tracy::MemWrite(&item->gpuZoneBegin.thread, tracy::GetThreadHandle());
    tracy::MemWrite(&item->gpuZoneBegin.queryId, slot);
    tracy::MemWrite(&item->gpuZoneBegin.context, s_ctx);
    tracy::Profiler::QueueSerialFinish();
}

// ─────────────────────────────────────────────────────────────
// EndZone — issue a GL timestamp query + emit zone end.
// Only emits if there's a matching Begin (depth > 0).
// Always succeeds (overwrites old data if full).
// ─────────────────────────────────────────────────────────────
void TracyGpu_EndZone(void) {
    if (!s_ready || s_depth <= 0)
        return;

    s_depth--;

    uint16_t slot = nextSlot();
    glQueryCounter(s_queries[slot], GL_TIMESTAMP);

    auto* item = tracy::Profiler::QueueSerial();
    tracy::MemWrite(&item->hdr.type, tracy::QueueType::GpuZoneEndSerial);
    tracy::MemWrite(&item->gpuZoneEnd.cpuTime, tracy::Profiler::GetTime());
    tracy::MemWrite(&item->gpuZoneEnd.queryId, slot);
    tracy::MemWrite(&item->gpuZoneEnd.context, s_ctx);
    tracy::Profiler::QueueSerialFinish();
}

// ─────────────────────────────────────────────────────────────
// Collect — read back completed query results.
// Call once per frame, after SDL_GL_SwapWindow().
// ─────────────────────────────────────────────────────────────
void TracyGpu_Collect(void) {
    if (!s_ready)
        return;

    static int frame_count = 0;
    if (++frame_count % 300 == 0) { // Log every ~5 seconds at 60fps
        printf("TracyGpu_Collect: Alive. Head=%d Tail=%d\n", s_head, s_tail);
    }

    while (s_tail != s_head) {
        GLuint available = 0;
        glGetQueryObjectuiv(s_queries[s_tail], GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available)
            break;

        GLuint64 timestamp = 0;
        glGetQueryObjectui64v(s_queries[s_tail], GL_QUERY_RESULT, &timestamp);

        auto* item = tracy::Profiler::QueueSerial();
        tracy::MemWrite(&item->hdr.type, tracy::QueueType::GpuTime);
        tracy::MemWrite(&item->gpuTime.gpuTime, (int64_t)timestamp);
        tracy::MemWrite(&item->gpuTime.queryId, (uint16_t)s_tail);
        tracy::MemWrite(&item->gpuTime.context, s_ctx);
        tracy::Profiler::QueueSerialFinish();

        s_tail = wrap(s_tail + 1);
    }
}

} // extern "C"

#else // !TRACY_ENABLE

extern "C" {
void TracyGpu_Init(void) {}
void TracyGpu_BeginZone(const char* name, const char* file, int line) {
    (void)name;
    (void)file;
    (void)line;
}
void TracyGpu_EndZone(void) {}
void TracyGpu_Collect(void) {}
}

#endif
