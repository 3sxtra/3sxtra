# Legacy Architecture Modernization Targets

**Fact-Check Status:** Verified. All structural claims regarding the `struct _TASK` memory layout (in `src/include/structs.h`), its instantiations, and the presence of `_Jmp_Tbl` tables in `screen/` and `system/` files have been rigorously checked against the `src/sf33rd/` codebase and proven exactly accurate.

Following the pattern established by the Menu Backend Migration, there are several components of the game engine that use identical architectural concepts (nested jump tables and integer state arrays) and could benefit from a similar modernization effort.

---

## ✅ Completed Modernizations (March 2026)

All LOW and MEDIUM risk registry migrations are done. Each converted a legacy jump table / function pointer array into a data-driven registry or enum-dispatch system.

| # | Component | Risk | Pattern | Key Files |
|---|-----------|------|---------|-----------|
| 1 | Player/Character Select Hub | LOW | Delegation wrapper → `MenuScreen` registry | `ms_char_select.c` |
| 2 | Transient Flow Screens | LOW | `MenuScreen` callbacks | `ms_continue.c`, `ms_win.c`, `ms_gameover.c`, `ms_saver.c` |
| 3 | Pre-Game Flow Screens & Demos | LOW | Thin wrapper → `MenuScreen` | `ms_ranking.c`, `ms_demo.c` |
| 4 | Stage Background Animations | LOW | `StageBgCallbacks` registry | `stage_bg_registry.h/.c`, 22× `sb_*.c` |
| 5 | Sound Effect Dispatch | LOW | `SeHandlerType` enum-tag table + `Se_Dispatch()` | `se_data.h`, `se_data.c`, 10 call-site files |
| 6 | Pause System | MEDIUM | Named state enums + switch | `pause.c`, `pause.h`, `reset.c` |
| 7 | Character Entrance Animations | MEDIUM | `AppearTypeCallbacks` registry | `appear_registry.h/.c`, `ap_all.c` |
| 8 | TASK System (Phases 1–3) | MEDIUM | Named constants + accessor functions | `menu_task.h/.c`, `init_task.h/.c`, `task_api.h/.c`, 15 migrated files |
| 9 | `Debug_w[]` Magic Number Constants | LOW | `DebugOption` enum in `debug_config.h` | 38 files already use `DEBUG_*` named constants |
| 10 | `Country` Region Code Constants | VERY LOW | `#define` constants in `country_region.h` | 12 files, `eff*.c`, `init3rd.c`, `game.c`, etc. |
| 11 | SE Handler Visibility Reduction | LOW | Forward decls in `se_data.c`, removed from `se.h` | `se.h`, `se_data.c` |
| 12 | `Bonus_Voice_Data` Integration | LOW | `Check_Bonus_SE` moved into `Se_Dispatch` pre-processing | `se.c`, `se_data.c`, `se_data.h` |
| 13 | Remaining `task[TASK_*]` Direct Accesses | LOW | Generic accessors + `TASK_SAVER2` enum | `task_api.h/.c`, `main.h`, 15 game-logic files migrated |
| 14 | `save_w[Present_Mode]` Accessor | LOW | `CurrentSave()` inline accessor in `work_sys.h` | 17 files, 91 call-sites migrated |
| 15 | `menu_input.c` Button Constants | LOW | Raw hex → `SWK_*` enum constants from `pad.h` | 62 replacements across 20+ functions |

---

## Next Wave: Safe Improvement Candidates (Sorted by Priority)

### 1. `game_globals.c` Decomposition
**Risk: 🟢 LOW** · **Effort: MEDIUM** · **1 → many files**

`game_globals.c` is a 606-line dump of global variable definitions — a grab-bag of unrelated state (player data, stage config, timer state, mode flags). Splitting into domain-specific files is purely organizational.

---

### 2. `opening.c` Decomposition
**Risk: 🟢 LOW** · **Effort: LARGE** · **3,161 lines → split by scene**

The opening cinematic is a single 3,161-line file with 19 scene handlers (`op_100_move`..`op_118_move`), 3 BG layer dispatchers, 241 hex constants, and 54 `switch` statements. Purely presentation code — zero risk to gameplay or netplay. Could split into `opening_scenes.c`, `opening_bg.c`, `opening_title.c`.

---

### 3. `ending/` Data Table Extraction
**Risk: 🟢 LOW** · **Effort: MEDIUM** · **20+ files, ~7K lines total**

The `ending/` directory has 20 per-character ending files (`end_00.c`..`end_20.c`) with heavy hex-literal magic numbers (300+ across the directory). The data tables in `end_data.c` (693 lines) could be separated more cleanly from the animation logic. Purely cinematic, no gameplay impact.

---

## 🔒 High Risk — Do Not Touch

### 7. Top-Level Game State Machine (`game.c`)
*   **Risk Level:** **HIGH**. Controls transitions into/out of gameplay. `G_No[]` is synchronized during netplay rollback — structural changes risk desyncs.
*   **Current State:** Relies on `G_No[]` to navigate nested dispatch tables: `Main_Jmp_Tbl`, `Game_Jmp_Tbl`, `Game00_Jmp_Tbl`, etc.

### 8. CPU AI and Player State Logic (`com/com_pl.c`)
*   **Risk Level:** **HIGH**. **DO NOT TOUCH IF PRESERVING MECHANICS.** 100% chance of altering character behavior, breaking combo timing, and causing rollback desyncs.
*   **Current State:** AI states use `Com_Jmp_Tbl`, `Damage_Jmp_Tbl`, `Float_Jmp_Tbl` with cryptic `CP_No[]` magic-number arrays.

### 9. Visual Effects System (`effect/eff*.c`)
*   **Risk Level:** **HIGH**. **DO NOT TOUCH IF PRESERVING MECHANICS.** Projectiles and hit-sparks govern frame-freeze timings and are deeply intertwined with the effect dispatch system.
*   **Current State:** ~140 effect files, each using `routine_no[]` with massive `switch` statements.

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

---