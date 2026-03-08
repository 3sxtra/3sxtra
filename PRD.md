# PRD: Code Health — Systematic Refactoring

## Overview
Clean up code health issues across the 3SX port layer.
Focus: dead code removal, god-file decomposition, duplicated logic consolidation, and naming consistency.
All changes are behavior-preserving — no new features, no game logic changes.

## Goals
- Remove dead code (`#if 0` blocks, deprecated functions, unused variables)
- Reduce file size of oversized source files via extract-function/extract-file refactoring
- Eliminate duplicated logic by introducing small helper functions
- Improve readability and maintainability without changing behavior

## Conventions (DO NOT deviate)
- Run `.\lint.bat` and `.\recompile.bat` after every task — both must pass
- Keep each task to ≤ 50 lines changed where possible
- Preserve all public API signatures — no breaking changes
- Preserve all `extern` declarations and header contracts
- Do NOT touch game logic, netplay protocol, or shader pipelines
- Do NOT modify `CMakeLists.txt` unless absolutely required by the refactor

---

## Task 1: Remove deprecated ImGui lobby dead code from `sdl_netplay_ui.cpp`
File: `src/port/sdl/netplay/sdl_netplay_ui.cpp` (1350 lines, 53 KB)

Two `#if 0` blocks contain ~242 lines of dead code from the deprecated ImGui lobby:
- Lines 414–422: `ImGuiSpinner()` function and its comment
- Lines 906–1140: Entire deprecated ImGui lobby rendering window

**Action:**
1. Delete both `#if 0` / `#endif` blocks and all code between them
2. Delete the comment on line 414 referencing the deprecated code
3. Delete the comment on lines 903–905 referencing the deprecated ImGui rendering
4. Verify the file still compiles and the lobby (native + RmlUi) works as before

Acceptance criteria:
- Both `#if 0` blocks are gone — no dead code remains
- `.\recompile.bat` succeeds
- `.\lint.bat` passes (or only has pre-existing issues)
- Net reduction: ~242 lines

---

## Task 2: Extract `SDLApp_Toggle*` helpers to reduce duplication in `sdl_app.c`
File: `src/port/sdl/app/sdl_app.c` (2630 lines, 97 KB)

Six toggle functions follow nearly identical patterns:
- `SDLApp_ToggleModsMenu()` (line 2401)
- `SDLApp_ToggleShaderMenu()` (line 2416)
- `SDLApp_ToggleStageConfigMenu()` (line 2431)
- `SDLApp_ToggleDevOverlay()` (line 2444)
- `SDLApp_ToggleTrainingMenu()` (line 2457)

Each does: toggle a `show_*` bool → update `game_paused` → `SDL_ShowCursor()` → show/hide RmlUi document.
`SDLApp_ToggleMenu()` (line 2388) is slightly different (no RmlUi show, only hide) — keep it as-is.

**Action:**
1. Create a `static void toggle_overlay(bool* flag, const char* rmlui_doc_name)` helper
2. Refactor the 5 similar toggle functions to call the helper
3. Keep `SDLApp_ToggleMenu()`, `SDLApp_ToggleFullscreen()`, `SDLApp_ToggleBezel()`, and `SDLApp_ToggleDebugHUD()` as-is (they have unique logic)

Acceptance criteria:
- All 5 toggle functions use the shared helper
- `.\recompile.bat` succeeds
- `.\lint.bat` passes
- Total toggle code reduced by ~40 lines

---

## Task 3: Extract shader/preset pass-through wrappers
File: `src/port/sdl/app/sdl_app.c`

Seven functions are pure one-line pass-throughs to `SDLAppShader_*`:
- `SDLApp_GetShaderModeLibretro()` → `SDLAppShader_IsLibretroMode()`
- `SDLApp_SetShaderModeLibretro()` → `SDLAppShader_SetMode()`
- `SDLApp_GetCurrentPresetIndex()` → `SDLAppShader_GetCurrentIndex()`
- `SDLApp_SetCurrentPresetIndex()` → `SDLAppShader_SetCurrentIndex()`
- `SDLApp_GetAvailablePresetCount()` → `SDLAppShader_GetAvailableCount()`
- `SDLApp_GetPresetName()` → `SDLAppShader_GetPresetName()`
- `SDLApp_LoadPreset()` → `SDLAppShader_LoadPreset()`
- `SDLApp_ToggleShaderMode()` → `SDLAppShader_ToggleMode()`
- `SDLApp_CyclePreset()` → `SDLAppShader_CyclePreset()`
- `SDLApp_CycleScaleMode()` → `cycle_scale_mode()`

These are only called from the input handler (`control_mapping.cpp` or RmlUi) and could potentially be replaced by direct calls to `SDLAppShader_*` if the callers already include the shader header, eliminating ~30 lines of boilerplate.

**Action:**
1. Check all callers of each `SDLApp_*Shader*` / `SDLApp_*Preset*` wrapper
2. If all callers already have access to `SDLAppShader_*` functions, update callers to call the shader layer directly
3. Remove the now-unused wrappers from `sdl_app.c` and `sdl_app.h`
4. If some callers cannot easily include the shader header (e.g., C callers needing a C-compatible header), keep the wrapper and document why

Acceptance criteria:
- `.\recompile.bat` succeeds
- `.\lint.bat` passes
- Every wrapper either removed (with callers updated) or documented as necessary
- Net reduction: ~20–30 lines if all can be removed

---

## Task 4: Deduplicate bezel draw-left / draw-right in `SDLApp_EndFrame()`
File: `src/port/sdl/app/sdl_app.c`

The bezel rendering section (lines ~2001–2034) has two nearly identical blocks:
```c
// Draw Left
if (cached_bezels.left) {
    GLuint tex = (GLuint)(intptr_t)cached_bezels.left;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(s_pt_loc_source, 0);
    int tw = 0, th = 0;
    TextureUtil_GetSize(cached_bezels.left, &tw, &th);
    glUniform4f(s_pt_loc_source_size, ...);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
// Draw Right  — identical except `cached_bezels.right` and vertex offset 6
```

**Action:**
1. Extract a `static void draw_bezel_quad(void* texture, int vertex_offset)` helper
2. Replace both blocks with two calls to the helper
3. Net reduction: ~15 lines

Acceptance criteria:
- Bezel rendering works identically (both bezels still draw)
- `.\recompile.bat` succeeds
- `.\lint.bat` passes

---

## Task 5: Extract debug HUD rendering from `SDLApp_EndFrame()`
File: `src/port/sdl/app/sdl_app.c`

The debug HUD text rendering (lines ~2066–2110) is a self-contained 44-line block inside the massive `SDLApp_EndFrame()` function. It builds three formatted strings (fps_text, mode_text, shader_text), composites them, and draws with `SDLTextRenderer_DrawText()`.

**Action:**
1. Extract a `static void render_debug_hud(int win_w, int win_h, const SDL_FRect* viewport)` function
2. Move the entire block (lines ~2066–2110) into it
3. Call it from `SDLApp_EndFrame()` with `if (show_debug_hud) render_debug_hud(...);`

Acceptance criteria:
- Debug HUD renders identically when toggled on
- `.\recompile.bat` succeeds
- `.\lint.bat` passes
- `SDLApp_EndFrame()` is ~44 lines shorter

---

## Task 6: Extract menu rendering dispatch from `SDLApp_EndFrame()`
File: `src/port/sdl/app/sdl_app.c`

The menu/overlay rendering dispatch block (lines ~2112–2176) is another self-contained section that dispatches between RmlUi and ImGui for each overlay type (menu, shader, mods, stage config, dev overlay, training, netplay).

**Action:**
1. Extract a `static void render_overlays(int win_w, int win_h)` function
2. Move the dispatch logic into it
3. Call it from `SDLApp_EndFrame()`

Acceptance criteria:
- All overlays (menu, shader, mods, stage config, dev, training, netplay) render identically
- `.\recompile.bat` succeeds
- `.\lint.bat` passes
- `SDLApp_EndFrame()` is ~65 lines shorter

---

## Task 7: Verify full build and lint pass
After all refactoring tasks, run the complete build and lint pipeline.

Steps:
1. `.\lint.bat` — all C/C++ and Python checks
2. `.\recompile.bat` — full incremental build
3. `cd build_tests && ctest --output-on-failure` — all unit tests still pass (if test build is configured)

Acceptance criteria:
- `.\lint.bat` passes without new issues
- `.\recompile.bat` succeeds
- No regressions in any existing unit tests
- Total line reduction across all tasks: ~380–400 lines

---

## Task 8: Clean up and document
Review all changed files for:
- Consistent doc comments on new helper functions
- No orphaned forward declarations
- No orphaned `#include` directives that were only used by removed code

Acceptance criteria:
- All new helper functions have `/** @brief */` doc comments
- No dead `#include` lines remain
- No dead forward declarations remain
