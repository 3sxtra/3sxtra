# Agent Instructions

Work on exactly one task per iteration.

## Rules
- Read PRD.md first
- Read progress.txt before making changes
- Do not edit PRD.md
- Append progress updates to progress.txt
- Prefer small, testable changes

## Project-specific conventions
- All changes must be behavior-preserving — no new features, no game logic changes
- Run `.\lint.bat` and `.\recompile.bat` after every task — both must pass
- Do NOT touch `CMakeLists.txt` unless absolutely required by the refactor
- Preserve all public API signatures and header contracts
- New helper functions must have `/** @brief */` doc comments
- Source files are in `src/port/` (C/C++ port layer)
- The build uses **MSYS2 MinGW64** with **Clang** and **Ninja**

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
