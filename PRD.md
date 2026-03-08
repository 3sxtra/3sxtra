# PRD: Code Health II — Logging Consistency & Extern Cleanup

## Overview
Second pass of code health improvements across the 3SX port layer.
Focus: inconsistent logging APIs, scattered `extern` declarations, and file-level organization.
All changes are behavior-preserving — no new features, no game logic changes.

## Goals
- Unify all logging to `SDL_Log` (eliminate raw `printf`/`fprintf` in non-fatal paths)
- Centralize scattered `extern` declarations into shared headers
- Reduce boilerplate across files that repeat the same patterns
- Improve readability and maintainability without changing behavior

## Conventions (DO NOT deviate)
- Run `.\lint.bat` and `.\recompile.bat` after every task — both must pass
- Keep each task to ≤ 50 lines changed where possible
- Preserve all public API signatures — no breaking changes
- Do NOT touch `CMakeLists.txt` unless absolutely required
- Do NOT touch game logic, netplay protocol, or shader pipelines

---

## Task 1: Replace `fprintf(stderr)` with `SDL_Log` in `modded_bgm.c`
File: `src/port/sound/modded_bgm.c`

7 calls use `fprintf(stderr, "ModdedBGM: ...")` for logging. The rest of the port layer uses `SDL_Log()`.
These are not fatal errors — they're init failures, load warnings, and playback status messages.

**Action:**
1. Replace all 7 `fprintf(stderr, "ModdedBGM: ...")` calls with `SDL_Log("ModdedBGM: ...")`
2. Remove the `#include <stdio.h>` if no other `printf`/`fprintf`/`snprintf` calls remain (check `snprintf` usage first — keep if still needed)

Acceptance criteria:
- All 7 calls converted
- `.\recompile.bat` succeeds
- `.\lint.bat` passes

---

## Task 2: Replace `fprintf(stderr)` with `SDL_Log` in `adx.c` and `imgui_font.cpp`
Files:
- `src/port/sound/adx.c` — 2 `fprintf(stderr)` calls (ADX decoding error, init failure)
- `src/port/imgui_font.cpp` — 2 `fprintf(stderr)` calls (font load failures)

These are operational error messages, not fatal crashes. The rest of these subsystems use `SDL_Log`.

**Action:**
1. Replace all 4 `fprintf(stderr, ...)` calls with `SDL_Log(...)` equivalents
2. Keep `src/port/utils.c` fprintf calls as-is — those are in the fatal error handler where SDL may not be initialized

Acceptance criteria:
- 4 calls converted (2 in `adx.c`, 2 in `imgui_font.cpp`)
- `utils.c` left unchanged
- `.\recompile.bat` succeeds
- `.\lint.bat` passes

---

## Task 3: Replace `printf` with `SDL_Log` in `native_save.c`
File: `src/port/save/native_save.c`

2 diagnostic `printf` calls for non-file-I/O logging:
- `printf("[NativeSave] ERROR: rename %s -> %s failed: ...")` (line ~210)
- `printf("[NativeSave] Save directory: %s\n", ...)` (line ~227)

The `fprintf(f, ...)` calls that write save-file data must stay — those write to actual files, not logs.

**Action:**
1. Replace only the 2 `printf(...)` diagnostic calls with `SDL_Log(...)`
2. Keep all `fprintf(f, ...)` calls — those are file I/O, not logging
3. Remove the `\n` from the format string (SDL_Log adds its own newline)

Acceptance criteria:
- 2 `printf` calls converted to `SDL_Log`
- All `fprintf(f, ...)` file writes unchanged
- `.\recompile.bat` succeeds
- `.\lint.bat` passes

---

## Task 4: Centralize `extern bool use_rmlui` into a shared header
Currently, 25 files each have their own `extern bool use_rmlui;` declaration at file scope.
This should be in a single shared header that all files include.

The toggle globals are already properly centralized in `rmlui_phase3_toggles.h`. 
`use_rmlui` is the master switch defined in `sdl_app.c` — it belongs in a port-layer header
that game-side code can include.

**Action:**
1. Add `extern bool use_rmlui;` to `rmlui_phase3_toggles.h` (this header is already included by all 25 game-side files that need it, and it already has the correct `extern "C"` guards)
2. Remove the per-file `extern bool use_rmlui;` from all 25 files
3. For `sdl_app_input.c` and `sdl_netplay_ui.cpp` (which don't include the toggles header), either add the include or keep the local extern — whichever is simpler

Acceptance criteria:
- `extern bool use_rmlui` appears in exactly one header
- All 25+ per-file declarations removed
- `.\recompile.bat` succeeds
- `.\lint.bat` passes
- No behavioral changes

---

## Task 5: Remove stale `extern` declarations in `sdl_app_input.c`
File: `src/port/sdl/app/sdl_app_input.c`

This file has two `extern bool use_rmlui;` declarations (lines 88 and 162) — both inside function bodies.
These are unusual (function-scoped extern is a code smell) and should be handled by the header from Task 4.

**Action:**
1. After Task 4, verify `sdl_app_input.c` includes the header (or add it)
2. Remove both function-scoped `extern bool use_rmlui;` declarations
3. Check for any other function-scoped `extern` declarations in this file and clean them up

Acceptance criteria:
- No function-scoped `extern` declarations remain
- `.\recompile.bat` succeeds
- `.\lint.bat` passes

---

## Task 6: Add `/** @brief */` doc comments to undocumented public functions in `native_save.c`
File: `src/port/save/native_save.c`

Many of the public `NativeSave_*` functions lack doc comments. This is inconsistent with the rest
of the port layer (e.g., `sdl_app.c` functions all have `/** @brief */` after the code health loop).

**Action:**
1. Add `/** @brief */` doc comments to all public (non-static) `NativeSave_*` functions
2. Do not modify function logic or signatures

Acceptance criteria:
- All public functions in `native_save.c` have `/** @brief */` comments
- `.\recompile.bat` succeeds
- `.\lint.bat` passes

---

## Task 7: Verify full build and lint pass
After all refactoring tasks, run the complete build and lint pipeline.

Steps:
1. `.\lint.bat` — all C/C++ and Python checks
2. `.\recompile.bat` — full incremental build

Acceptance criteria:
- `.\lint.bat` passes without new issues
- `.\recompile.bat` succeeds
- No regressions

---

## Task 8: Clean up and document
Review all changed files for:
- No orphaned `#include <stdio.h>` that were only used by removed `printf`/`fprintf` calls
- No duplicate `extern` declarations remaining
- Consistent formatting

Acceptance criteria:
- No dead `#include` lines
- No duplicate `extern` declarations
- All changes verified as behavior-preserving
