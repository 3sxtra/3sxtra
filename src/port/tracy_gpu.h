// tracy_gpu.h — C-callable GPU profiling wrapper for Tracy.
// Provides init, per-frame collect, and named GPU zone helpers.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Call once after the OpenGL context is created and glad is loaded.
void TracyGpu_Init(void);

/// Call once per frame, AFTER SDL_GL_SwapWindow().
void TracyGpu_Collect(void);

/// Begin/end a named GPU zone. Must be paired.
/// 'name' must be a string literal.
/// These use GL timer queries to measure actual GPU execution time.
void TracyGpu_BeginZone(const char* name, const char* file, int line);
void TracyGpu_EndZone(void);

#ifdef __cplusplus
}
#endif

// Convenience macros
// Convenience macros
#ifdef TRACY_ENABLE
// GPU Profiling disabled due to driver instability/crashes
#define TRACE_GPU_ZONE(name) ((void)0)
#define TRACE_GPU_ZONE_END() ((void)0)
#define TRACE_GPU_INIT() ((void)0)
#define TRACE_GPU_COLLECT() ((void)0)
#else
#define TRACE_GPU_ZONE(name) ((void)0)
#define TRACE_GPU_ZONE_END() ((void)0)
#define TRACE_GPU_INIT() ((void)0)
#define TRACE_GPU_COLLECT() ((void)0)
#endif
