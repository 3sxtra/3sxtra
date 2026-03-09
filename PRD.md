# PRD: Code Health V — Hygiene Sweep (Inline Externs, Dead Includes, Magic Numbers)

## Overview
Fifth pass of code health improvements across the 3SX port layer.
Focus: eliminating inline `extern` declarations in `.c` files, auditing and removing unnecessary `#include <stdio.h>` / `<stdlib.h>`, and replacing magic numbers with named constants.
All changes are behavior-preserving — no new features, no game logic changes.

## Skills Available
The agent should leverage these skills from `.agents/skills/`:
- **kaizen** — small, incremental changes; verify after each task
- **memory-safety-patterns** — init/shutdown pairs, guard clauses
- **code-review-checklist** — structured review after each change
- **c-cpp-pro** — idiomatic C patterns, const correctness

## Goals
- Replace all inline `extern` declarations in `.c` files with proper `#include` of the declaring header
- Audit `#include <stdio.h>` across all port layer files — remove where only `SDL_Log` is used (no `printf/fprintf/snprintf/sprintf/sscanf/fopen/fclose/fgets/fputs/FILE`)
- Replace magic number thresholds in `sdl_pad.c` with named constants
- Add missing `@file`/`@brief` doc comments to files that lack them

## Conventions (DO NOT deviate)
- Run `.\lint.bat` and `.\recompile.bat` after every task — both must pass
- Run `cd build_tests && ctest --output-on-failure` after every task
- Preserve all public API signatures — no breaking changes
- Do NOT touch game logic, netplay protocol, or shader pipelines
- Read the `kaizen` and `code-review-checklist` skills before starting

---

## Task 1: Eliminate inline `extern` declarations in port-layer `.c` files
The following files have inline `extern` declarations that should be replaced with proper `#include`:

1. `src/port/sdl/app/sdl_app_bezel.c` — lines 30-31:
   `extern SDL_GPUCommandBuffer* SDLGameRendererGPU_GetCommandBuffer(void);`
   `extern SDL_GPUTexture* SDLGameRendererGPU_GetSwapchainTexture(void);`
   → Move to `sdl_game_renderer_gpu.h` or `sdl_game_renderer_internal.h`, then `#include`

2. `src/port/sdl/renderer/sdl_game_renderer.c` — line 70:
   `extern void SDLGameRendererGL_ResetBatchState(void);`
   → Move to `sdl_game_renderer_internal.h` or appropriate header, then `#include`

3. `src/port/sdl/renderer/sdl_text_renderer.c` — lines 106-110:
   `extern u32 flDebugStrHan;`
   `extern u32 flDebugStrCtr;`
   `extern void* flPS2GetSystemBuffAdrs(unsigned int handle);`
   → Find or create the proper header for these CPS3 stubs, then `#include`

4. `src/port/sdl/renderer/sdl_game_renderer_gpu_lz77.c` — line 183:
   `extern Sint16* dctex_linear;`
   → Find the declaring header (likely in the CPS3 engine headers), then `#include`

5. `src/netplay/game_state.c` — line 56:
   `extern unsigned short g_netplay_port;`
   → Should be in `netplay.h`, then `#include`

**Action:**
- For each inline `extern`, find or create the proper header declaration
- Replace the inline `extern` with a proper `#include`
- Verify no circular dependencies are introduced

Acceptance criteria:
- Zero inline `extern` declarations remaining in port-layer `.c` files
- `.\recompile.bat` succeeds
- `.\lint.bat` passes
- `ctest` passes

---

## Task 2: Audit and remove unnecessary `#include <stdio.h>`
24 port-layer files include `<stdio.h>`. Many of these only use `SDL_Log` for output and `snprintf` (which is provided by `<stdio.h>`).

**Action:**
1. For each file that includes `<stdio.h>`, search for actual `stdio.h` usage: `printf`, `fprintf`, `snprintf`, `sprintf`, `sscanf`, `scanf`, `fopen`, `fclose`, `fread`, `fwrite`, `fgets`, `fputs`, `FILE`, `perror`
2. If the file uses `snprintf` (very common for path building), `<stdio.h>` is still needed — skip it
3. If the file uses ONLY `SDL_Log` and no `stdio.h` functions, remove the `#include <stdio.h>`
4. Also check if `<stdlib.h>` is truly needed (any `malloc/calloc/realloc/free/exit/atoi/strtol`?)

Acceptance criteria:
- Unnecessary `<stdio.h>` includes removed
- `.\recompile.bat` succeeds
- `.\lint.bat` passes
- `ctest` passes

---

## Task 3: Replace magic number thresholds in `sdl_pad.c`
`sdl_pad.c` uses several unlabeled numeric thresholds for joystick input:
- `24000` (joystick axis event threshold, line 241)
- `20000` (keyboard "any input active" joystick check, line 640)
- `16000` (joystick axis direction detection, lines 736/738)
- `8000` (trigger/stick threshold for `IsAnyInputActive`, used in 6+ places)

**Action:**
1. Define named constants at the top of `sdl_pad.c`:
   ```c
   #define SDLPAD_AXIS_EVENT_THRESHOLD  24000
   #define SDLPAD_AXIS_ACTIVE_THRESHOLD 20000
   #define SDLPAD_AXIS_DIR_THRESHOLD    16000
   #define SDLPAD_TRIGGER_THRESHOLD      8000
   #define SDLPAD_STICK_THRESHOLD        8000
   ```
2. Replace all magic numbers with the named constants
3. Add a brief comment explaining what each threshold means

Acceptance criteria:
- Zero magic number thresholds in `sdl_pad.c`
- `.\recompile.bat` succeeds
- `.\lint.bat` passes
- `ctest` passes

---

## Task 4: Add missing `@file`/`@brief` doc comments
Scan all port-layer `.c` and `.h` files for missing `@file`/`@brief` doc comments.
Focus on files modified in prior loops (the new sub-modules should already have them).

**Action:**
1. Check `sdl_app_input.c`, `sdl_app_shader_config.c`, `config.c`, `cli_parser.c`, `paths.c`, `menu_bridge.c`, `modded_stage.c`, `stage_config.c`, `afs.c`, `native_save.c`
2. Add `@file` and `@brief` doc comments to any file that lacks them
3. Follow the pattern already established in new files (e.g., `sdl_app_scale.c`)

Acceptance criteria:
- All port-layer `.c` files have `@file`/`@brief` headers
- `.\recompile.bat` succeeds
- `.\lint.bat` passes

---

## Task 5: Final verification and summary
Run the complete pipeline to ensure absolute parity.

Steps:
1. `.\lint.bat` — all C/C++ checks
2. `.\recompile.bat` — full incremental build
3. `cd build_tests && ctest --output-on-failure`

Acceptance criteria:
- `.\lint.bat` passes without new issues
- `.\recompile.bat` succeeds
- `ctest` passes
- All inline externs eliminated, dead includes removed, magic numbers named
