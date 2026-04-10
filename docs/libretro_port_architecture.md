# 3SX Libretro Port Architecture

**Document Version:** 2.0 (State-of-the-Art Libretro Integration)
**Subject:** Refactoring the 3SX Engine (`src/main.c`, `src/port/*`) into a standard Libretro core.

## 1. Objective and Core Philosophy
The objective is to expose the 3SX engine internal simulation and renderer as a dynamic library (`3sx_libretro.so/.dll`) conforming to the Libretro API (`libretro.h`). 

**The Zenith of Modular Emulation:**
We strictly partition the "Core" (the pure mathematical game state) from the "Shell" (the frontend). 
- **The Core (3SX)** exclusively manages deterministic state emulation, texture decoding, and hardware-accelerated draw submission. It relies on zero external side-effects.
- **The Frontend (RetroArch/Batocera)** acts as the absolute orchestrator: gathering inputs, syncing audio, managing window context, and orchestrating state-rewinding networks.

## 2. Inversion of the Application Lifecycle
Currently, 3SX is an executive program. `src/main.c` spins an infinite `while(is_running)` loop encapsulating `SDLApp_PollEvents()`, `step_0()` (simulation/rendering), and `step_1()` (audio sync).

**Implementation:**
The loop must be inverted to a callback model dictated by the frontend.
- `main.c` is bypassed entirely. A new entrypoint `libretro.c` exposes `retro_run()`. 
- `retro_run()` executes `step_0()` and `step_1()` precisely once per invocation. Frame pacing and delta-time tracking are surrendered entirely to the Libretro frontend.
- `retro_get_system_av_info()` dynamically maps 3SX’s resolution (e.g. 384x224 originally) and tightly enforces a 59.58 Hz refresh rate.

## 3. Hardware-Accelerated Rendering Context
The 3SX engine relies heavily on discrete GPU techniques (e.g., LZ77 decoding using compute shaders). A pure software pixel-buffer stream (`retro_video_refresh_t`) is inefficient and violates SOTA performance standards.

**Implementation:**
- `libretro.c` calls `retro_set_environment(RETRO_ENVIRONMENT_SET_HW_RENDER)` to acquire an OpenGL Core (4.3+) or Vulkan context during `retro_load_game()`.
- `libretro_renderer.c` captures standard `Renderer_Flush2DPrimitives()` and renders directly to the frontend-provided Framebuffer Object (FBO).
- Scaling, post-processing CRT shaders, and presentation are yielded natively to the libretro host.

## 4. Input Wiring & VFS (Virtual File System)
The engine currently binds logic directly to SDL gamepad polling and standard file I/O.

**Implementation:**
- **Input (Unified Event Queue):** We swap `src/port/sdl/app/sdl_app_input.c` for a Libretro hook. On each `retro_run()`, the engine queries `retro_input_state_t` (via `RETRO_DEVICE_JOYPAD`), and immediately pushes it into a centralized $O(1)$ **Unified Event Queue** (as mandated by the DOOM Equivalent analysis) before bitmasking into the legacy CPS3 struct (`PLsw`).
- **File System:** `src/port/io/afs.c` explicitly calls `SDL_IOFromFile`. This is replaced with `retro_vfs_file_t` handles, allowing seamless reading of assets directly from `.zip` archives or network blocks mounted natively by the Libretro VFS.

## 5. Synchronous Audio Subsystem & Rollback Bypass
`src/port/sound/spu.c` and `adx.c` actively manage threads and push PCM samples to `SDL_OpenAudioDeviceStream`.

**Implementation:**
- **Asynchronous threads are strictly forbidden.** 
- The SPU and ADX drivers must be refactored to populate a static ring buffer mathematically synced to the 59.58hz clock. At the precise conclusion of `retro_run()`, the aggregated PCM buffer is batch-submitted via `retro_audio_sample_batch_t`, ensuring zero deadlocks and perfect AV synchronization.
- **Aggressive Short-Circuiting:** During rollback catch-up frames, the frontend will intentionally drop AV callbacks. The core must detect this and $O(1)$ short-circuit the execution of all GPU draw submissions and audio processing loops, adhering to modern Quake 3 standards.

## 6. Mathematical Serialization & Rollback (Addressing Petrov's Rigor)
Libretro frontends orchestrate their own rollback netplay without internal engine intervention. This requires the core to be mathematically deterministic via `retro_serialize()` and `retro_unserialize()`.

**Implementation:**
- Internal `src/netplay/game_state.c` UDP logic is destroyed/bypassed.
- **Rigor in Serialization:** The size returned by `retro_serialize_size()` cannot be an approximation. We must construct a contiguous State Snapshot struct that packs:
  1. The legacy `task` array (`cpLoopTask` states)
  2. Statically allocated BSS segments containing logic (`system_init_level`, `mpp_w`, `io_w`, `PLsw`).
  3. The entirety of the `mppMalloc` memory arenas.
  4. The exact state of the audio synthesized timers.
- **Fail-Fast Sentinels:** Any uninitialized pointers existing within the heap must be seeded with Sentinels (`0xFFFFFFFF`). Pointers must be serialized as offsets rather than absolute memory addresses to survive teardown and rewind, guaranteeing a hard crash over a silent failure if memory drifts.

## 7. Frontend UI and Core Options (Addressing Rossi's Usability)
A libretro core strictly emulates the game. It must not emulate a frontend, yet users must retain complete configuration capabilities previously offered by `RmlUi`.

**Implementation:**
- **Excision:** The `MenuBridge` and `rmlui_casual_lobby` are compiled out (`#ifndef LIBRETRO`).
- **Core Options Architecture:** To ensure an intuitive UX, we use Libretro's advanced Option Categories API (`retro_set_core_options_v2`).
- Configuration data (like Arcade Balance, Regional Settings, or HUD Toggles) will be exported as categorized, localized drop-downs inside the RetroArch menu (e.g., "Gameplay -> Game Mode -> [Arcade / Training / Console]").
- Each option will carry an explicitly registered tooltip detailing exactly how it impacts emulation, guaranteeing users are empowered and informed without requiring our proprietary UI.
