# Agent Instructions

Work on exactly one task per iteration.

## Rules
- Read PRD.md first
- Read progress.txt before making changes
- Do not edit PRD.md
- Append progress updates to progress.txt
- Prefer small, testable changes

## Project-specific conventions
- Test framework: **CMocka** (already in the build via FetchContent)
- Test files go in `tests/unit/test_<name>.c`
- Register tests in `tests/unit/CMakeLists.txt` using the `add_unit_test()` helper
- Follow the include/link patterns already established (see `target_link_sdl3()`, etc.)
- Study existing tests like `test_radix_sort.c` and `test_config.c` for style reference
- All types (`s8`, `s16`, `s32`, `u8`, `u16`, `u32`, `f32`, `MTX`, `Vec3`) come from `types.h` / `structs.h` in `src/include/`
- Source files are compiled directly into test targets (not linked from a library)

## Build and verify
- Tests use a **separate build directory**: `build_tests` (not `build`)
- The build pipeline uses **MSYS2 MinGW64** with **Clang** and **Ninja**
- Configure: `CC=clang cmake -B build_tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON`
- Build a specific test: `cmake --build build_tests --target <test_name>`
- Run a specific test: `./build_tests/tests/unit/<test_name>.exe`
- Run all tests: `cd build_tests && ctest --output-on-failure`
- Run filtered: `cd build_tests && ctest --output-on-failure -R <pattern>`
- Run relevant tests before marking a task complete
- If a build or test fails, fix it before proceeding
- See `tools/compile_tests.bat` for the full Windows build pipeline reference

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
