# Agent Instructions

Work on exactly one task per iteration.

## Rules
- Read PRD.md first
- Read progress.txt before making changes
- Do not edit PRD.md
- Append progress updates to progress.txt
- Prefer small, testable changes

## Project-specific conventions
- Training system source is in `src/sf33rd/Source/Game/training/` (C game logic)
- Menu integration is in `src/sf33rd/Source/Game/menu/menu_input.c`
- Lua bridge is in `src/port/sdl/rmlui/lua_engine_bridge.cpp`
- Lua reference code is in `src/lua/3rd_training_lua-main/src/control/` (read-only reference — do not modify)
- Compat layer is in `src/lua/compat/` (read-only reference — do not modify)
- Run `.\lint.bat` and `.\recompile.bat` after every task — both must pass
- Run `cd build_tests && ctest --output-on-failure` after verify tasks
- Preserve all public API signatures and header contracts
- New helper functions must have `/** @brief */` doc comments
- The build uses **MSYS2 MinGW64** with **Clang** and **Ninja**
- `CMakeLists.txt` uses `file(GLOB_RECURSE GAME_SRC src/*.c)`, so new `.c` files are auto-detected but require a cmake reconfigure

## Naming conventions
- Enums: `DummyXxxType` (e.g., `DummyBlockType`, `DummyTechThrowType`)
- Settings: fields in `DummySettings` struct in `training_dummy.h`
- Menu sync: `sync_dummy_settings_from_menu()` in `menu_input.c`
- Lever input: `Lever_Buff[dummy_id]` bitfield (0x01=up, 0x02=down, 0x04=left, 0x08=right, 0x10=LP, 0x20=MP, 0x40=HP, 0x100=LK, 0x200=MK, 0x400=HK)

## Build and verify
- Build: `.\recompile.bat` (incremental MSYS2 build)
- Lint: `.\lint.bat` (clang-format + clang-tidy + Python linters)
- Run relevant tests before marking a task complete
- If a build or lint fails, fix it before proceeding

## If blocked
- Write the blocker clearly in progress.txt
- Do not skip ahead to the next task

---

## Tool Quirks & Workarounds

### grep_search Tool

#### Known Issues
1. **Single-file SearchPath fails silently** — Using a file path as `SearchPath` returns "No results found" even when the pattern is present. The tool only reliably works with **directory** paths.
2. **`.antigravityignore` path concatenation bug** — The tool concatenates the SearchPath + absolute ignore-file path instead of treating it as absolute. Cosmetic noise, doesn't block results.
3. **50-result cap** — Large searches get exhausted on duplicates (e.g., `effect/`) before reaching relevant files.
4. **`Includes` with directory names as filenames** — If the target file shares a name with a directory (e.g., `config.py` when `config/` exists), the filter may match the directory. Use the actual filename or a wildcard glob.

#### Workarounds
- **Never** use `SearchPath` pointing to a single file — use `view_file` or `view_code_item` instead.
- **Always** use `Includes` globs to filter (e.g., `["*.c", "*.h"]`) and narrow `SearchPath` to the relevant subdirectory.
- To target a specific file, use the **parent directory** as `SearchPath` + `Includes: ["filename.c"]`.
- For tricky or broad searches, use `run_command` with `rg` directly — it works perfectly.
- Exclude noisy directories by searching `src/sf33rd/Source/Game/<subfolder>/` directly instead of the entire tree.

#### Parameter Reference

| Parameter | Works? | Notes |
|-----------|--------|-------|
| `Query` (string) | ✅ | Required. Literal search string. |
| `SearchPath` (string) | ⚠️ | Must be a **directory**, not a file. |
| `MatchPerLine` (bool) | ✅ | `true` = lines + content, `false` = filenames only. |
| `Includes` (array) | ✅ | Glob patterns like `["*.c"]`. Bare filenames work if unambiguous. |
| `CaseInsensitive` (bool) | ✅ | Works as expected. |
| `IsRegex` (bool) | ✅ | Enables regex in Query. |
