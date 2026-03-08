# PRD: Code Health III — Modularizing God Files

## Overview
Third pass of code health improvements across the 3SX port layer.
Focus: breaking down massive "god files" (`sdl_game_renderer_sdl.c`, `sdl_game_renderer_gpu.c`, `netplay.c`) by extracting obvious, self-contained sub-components into new files or existing appropriate homes.
All changes are behavior-preserving — no new features, no game logic changes.

## Goals
- Extract the 1100-line Software Rasterizer out of the SDL2D basic renderer file
- Extract the LZ77 Compute Shader pipeline out of the SDL_GPU main renderer file
- Move the ~20 Netplay state backup/restore functions into the existing `game_state.c`
- Improve readability and maintainability without changing behavior

## Conventions (DO NOT deviate)
- Run `.\lint.bat` and `.\recompile.bat` after every task — both must pass
- Run `cd build_tests && ctest --output-on-failure` after every task
- When adding new files, they MUST be added to `CMakeLists.txt`
- Preserve all public API signatures — no breaking changes
- Do NOT touch game logic, netplay protocol, or shader pipelines

---

## Task 1: Extract Software Rasterizer from `sdl_game_renderer_sdl.c`
File: `src/port/sdl/renderer/sdl_game_renderer_sdl.c` (2300+ lines, 99 KB)

This file contains the high-level SDL2D renderer implementation, but half of it is a custom CPU-bound 3D software rasterizer used for rendering distorted textured quads when backend hardware acceleration isn't available.

**Action:**
1. Create `src/port/sdl/renderer/sdl_game_renderer_sdl_sw.h` and `sdl_game_renderer_sdl_sw.c`
2. Move all `sw_raster_*` functions (e.g., `sw_raster_textured`, `sw_raster_solid`, `sw_raster_triangle`, `sw_render_frame`), `dt_mark_rect`, `lerp_fcolors`, and the `SwTriVert` struct to the new files
3. Expose only the necessary entry points in the new header (e.g., `SWRaster_Init`, `SWRaster_RenderFrame`, `SWRaster_Shutdown`)
4. Update `sdl_game_renderer_sdl.c` to include the new header and call the extracted functions
5. Add the new `.c` file to `src/port/sdl/renderer/CMakeLists.txt` (or the root `CMakeLists.txt`, wherever port files are defined)

Acceptance criteria:
- File split cleanly with minimal public API surface in the new header
- `.\recompile.bat` succeeds
- `.\lint.bat` passes
- `ctest` passes

---

## Task 2: Extract LZ77 Compute Pipeline from `sdl_game_renderer_gpu.c`
File: `src/port/sdl/renderer/sdl_game_renderer_gpu.c` (1900+ lines, 80 KB)

The `SDL_GPU` backend has a dedicated compute shader pipeline for decompressing LZ77 textures on the GPU. This is self-contained state that clutters the main graphics pipeline code.

**Action:**
1. Create `src/port/sdl/renderer/sdl_game_renderer_gpu_compute.h` and `.c`
2. Move `SDLGameRendererGPU_LZ77Available`, `SDLGameRendererGPU_LZ77Enqueue`, and any static state/functions supporting them (e.g., `lz77_jobs`, ring buffers, init/shutdown compute state) into the new files
3. Update `sdl_game_renderer_gpu.c` to call initialization and dispatch functions from the new file during its render passes
4. Add the new `.c` file to `CMakeLists.txt`

Acceptance criteria:
- File split cleanly
- `.\recompile.bat` succeeds
- `.\lint.bat` passes
- `ctest` passes

---

## Task 3: Move State Management from `netplay.c` to `game_state.c`
Files: 
- `src/netplay/netplay.c` (1500+ lines, 57 KB)
- `src/netplay/game_state.c` (already exists, handles native save states)

`netplay.c` contains the network session loop, but it also tightly couples ~20 functions for traversing and sanitizing the game's memory state (`sanitize_work_pointers`, `dump_state`, `save_state`, `load_state`, `gather_state`). These properly belong in `game_state.c`, which already handles `GameState_Save` and `GameState_Load`.

**Action:**
1. Move all `sanitize_*` functions, `dump_state`, `save_state`, `load_state`, and `gather_state` from `netplay.c` to `game_state.c`
2. Move the definition of `State` (and `EffectState`, etc., if applicable) from `netplay.c` to `game_state.h`
3. Expose the necessary high-level state boundary functions in `game_state.h` for `netplay.c` to call during rollbacks
4. Ensure no circular dependencies are introduced between `netplay.h` and `game_state.h`

Acceptance criteria:
- `netplay.c` loses ~350 lines of state traversal code
- `.\recompile.bat` succeeds
- `.\lint.bat` passes
- `ctest` passes (crucially, the `test_game_state*.c` and `test_netplay_*.c` tests)

---

## Task 4: Verify full build, lint, and test pass
After all refactoring tasks, run the complete pipeline to ensure absolute parity.

Steps:
1. `.\lint.bat` — all C/C++ and Python checks
2. `.\recompile.bat` — full incremental build
3. `cd build_tests && ctest --output-on-failure`

Acceptance criteria:
- `.\lint.bat` passes without new issues
- `.\recompile.bat` succeeds
- `ctest` passes (37/37)
- Total line reduction across the 3 god files: ~1650 lines moved to better homes

---

## Task 5: Clean up and document
Review all changed files for:
- Proper `/** @brief */` documentation on newly exposed public cross-module APIs
- No dead `#include` lines
- No duplicate `extern` declarations
- Consistent formatting and copyright headers (if applicable to new files)

Acceptance criteria:
- Clean modular boundaries established
- Readability improved significantly
