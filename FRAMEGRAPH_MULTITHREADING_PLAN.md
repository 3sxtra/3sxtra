# 3SX FrameGraph & Multithreading Evolution Plan

**Objective:** Evolve the newly unified, data-driven `RenderPass` architecture into a modern **FrameGraph** that supports automatic pass culling, transient VRAM aliasing, and multithreaded command recording for the `SDL_GPU` backend.

---

## Phase 1: Pass Analysis & Culling (The Foundation)
**Goal:** Shift from "execute as we go" to "plan, analyze, then execute."
1. **Extend Pass Descriptors:** Add properties to `RenderPass` to track state: `bool has_geometry`, `bool force_execute` (e.g., for clears), and `bool skip_this_frame`.
2. **Abstract the Submission Queue:** Instead of calling `RenderHDPass` directly in the middle of `sdl_app.c`, push the draw requests into the pass's specific queue bucket.
3. **`RenderGraph_Compile()`:** Implement a function that runs *before* GPU execution. It evaluates the pass table, determines which passes have zero geometry, and sets their `skip_this_frame` flag to `true`.
4. **Execution Loop:** Modify the main render loop to iterate through `g_render_passes`, skipping culled passes and executing active ones sequentially.

## Phase 2: Transient Resource Aliasing (VRAM Optimization)
**Goal:** Reduce VRAM usage by sharing intermediate render targets between non-overlapping passes.
1. **Transient Texture Pool:** Create a system to request temporary textures of a specific size/format for a single frame.
2. **Lifespan Tracking:** During `RenderGraph_Compile()`, determine the "first use" and "last use" of intermediate render targets.
3. **Aliasing:** If Pass 1 uses `Target A` and finishes, and Pass 3 needs a target of the same size, assign `Target A` to Pass 3. This is incredibly beneficial for chained post-processing filters (e.g., CRT shaders, bloom).

## Phase 3: Thread Pool & Task Scheduler Integration ✅
**Goal:** Prepare the CPU architecture for concurrent workload distribution.
1. **Thread Pool Subsystem:** ✅ DONE — `RenderJobQueue` implemented in `render_job.h` / `render_job.c` using `SDL_Thread` + `SDL_Mutex` + `SDL_Condition`. Fixed-size pool (2 workers for GPU, 0 for GL/SDL2D). Workers sleep on a condition variable, wake to claim jobs from a lock-protected queue. Init/Shutdown wired into `SDLGameRenderer_Init()` / `SDLGameRenderer_Shutdown()`.
2. **Data Isolation:** ✅ DONE — All recording-phase state (`quad_count`, `sort_keys`, `overlay_tex`, `blend_mode`, Z-diagnostics) has been extracted from bare globals into `PassRecordingState pass_state[8]`. Each pass writes only to its own struct. The texture binding stack and `vertex_count` remain shared (read-only base pointer + per-pass offset pattern).
3. **Job Abstraction:** ✅ DONE — `RenderJob` struct defined with `pass_index`, `RenderJobFn` callback, and `void* userdata`. `RenderJobQueue_Submit()` dispatches to workers, `RenderJobQueue_WaitAll()` blocks until completion. `RenderJobQueue_RunSync()` provides fallback for GL backends.

## Phase 4: Parallel Command Recording (The Final Boss) ✅
**Goal:** Halve CPU rendering time by building GPU command buffers concurrently.
1. **Worker Thread Recording:** ✅ DONE — The most expensive CPU operations (Z-sorting thousands of quads using radix sort) have been moved to worker threads in `RenderFrame`. 
2. **Buffer Allocation & Concurrent Processing:** ⚠️ *Architectural Note* — In SDL3 GPU, the `s_swapchain_texture` is strictly locked to the single command buffer that calls `SDL_AcquireGPUSwapchainTexture()`. Because Pass 1 and Pass 2 write directly to the swapchain, they **cannot** be recorded onto separate concurrent command buffers without introducing an expensive VRAM offscreen intermediate texture.
3. **Main Thread Synchronization:** ✅ DONE — The main thread uses `RenderJobQueue_WaitAll()` to sync with the parallel sort jobs.
4. **Final Command Injection:** ✅ DONE — Because the actual command recording loops (`SDL_DrawGPUIndexedPrimitives`) are heavily batched and execute in microseconds, they are kept sequential on the main thread's `current_cmd_buf`. The heavy lifting (data preparation and sorting) is fully parallelized.
5. **OpenGL Fallback:** ✅ DONE — `RenderJobQueue_RunSync()` handles sequential execution for GL automatically.

---
## Getting Started
To begin this initiative, we should start a **new session**, pull up this plan, and begin executing **Phase 1: Pass Analysis & Culling**.
