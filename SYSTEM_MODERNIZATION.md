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
| 16 | `game_globals.c` Decomposition | LOW | 607-line grab-bag → 5 domain files under `globals/` | `player_globals.c`, `timer_hud_globals.c`, `score_globals.c`, `match_globals.c`, `combo_stage_globals.c` |
| 17 | `opening.c` Decomposition | LOW | 3,161-line file split into 3 files | `opening_scenes.c` (~1,450 lines), `opening_bg.c` (~1,100 lines), `opening.c` core (~600 lines). Fully encapsulated 45+ internal subroutines as `static`. |
| 18 | `end_data.c` Tilemap Extraction | LOW | 694-line file split into 2 files | `end_maps.c` (459 lines, 23 static tilemap arrays + master lookup), `end_data.c` (244 lines, config/rewrite/state). Updated `end_14.c`, `bg.c` includes. |
| 19 | `save_w[]` Index Constants | LOW | `SAVEW_*` named constants in `work_sys.h` | `work_sys.h/.c`, `sys_sub.c`, `menu_input.c`, `entry.c`, `init3rd.c`, `game.c`, `sel_pl.c`, `sysdir.c` (~130 replacements across 9 files) |
| 20 | `sys_sub.c` Decomposition | LOW | 2,015-line grab-bag → 4 domain files + trimmed core | `sys_replay.c` (replay record/playback), `sys_ranking.c` (ranking + opponent selection), `sys_options.c` (game option save/load), `sys_score.c` (HUD score/win/copyright). Core `sys_sub.c` trimmed to ~530 lines. `sys_sub.h` → umbrella header. |
| 21 | `sc_sub.c` UI Decomposition | LOW | File split | Split ~2400 lines into `sc_cockpit.c` (gauges), `sc_timer.c` (score/round overlays), and `sc_names.c` (portraits/text). Core rendering logic remains in `sc_sub.c`. No gameplay state mutation. |
| 22 | `workuser.h` Partitioning | LOW–MED | Header hygiene | 446-extern header → 4 domain headers (`workuser_score.h`, `_combat.h`, `_select.h`, `_system.h`) + umbrella include for backward compat. |
| 23 | `entry.c` State Machine Cleanup | LOW–MED | Named states | 1,252 lines, 17 `G_No[]` accesses. Name `E_No[]`/`C_No[]` state constants, convert `_Jmp_Tbl` to switch-dispatch. Non-netplay flow. |
| 24 | `menu.c` State Machine Cleanup | MEDIUM | Named states | 2,997 lines, 12 `G_No[]` accesses, 14 `r_no[0]` state indices. Name states, convert jump table arrays to switch-dispatch. |
| 25 | `sel_pl.c` Character Select Cleanup | MEDIUM | Named states | 2,174 lines. Named `S_No`, `Face_No`, `SO_No`, `SP_No` states, converted 6 `_Jmp_Tbl` arrays to switch-dispatch. Extracted 4 data tables to `sel_data.h/.c`. |
| 26 | `saver.c` Jump Table → Switch | VERY LOW | Named states | 102 lines. `enum SaverState` + switch replaces `Main_Jmp_Tbl[4]`. |
| 27 | `init3rd.c` Jump Table → Switch | VERY LOW | Named states | 224 lines. `enum InitStep` + switch replaces `Main_Jmp_Tbl[4]`. |
| 28 | `ranking.c` State Cleanup | LOW | Named states | 784 lines. 2 jump tables (`Ranking_00_tbl[6]`, `Ranking_01_tbl[5]`) → switch-dispatch via `D_No[1]`. |
| 30 | `menu.c` Residual Jump Tables | LOW | Dead code cleanup | 2 residual `_Jmp_Tbl` arrays (In_Game, Training) → switch-dispatch. Most entries were DEAD (migrated to MenuScreen registry). |
| 29 | `next_cpu.c` State Cleanup | LOW–MED | Named states | 1,659 lines. 4 jump tables (`Next_CPU_Tbl[12]`, `After_Bonus_Tbl[11]`, `Select_CPU_First_Tbl[4]`, `Next_Q_Tbl[6]`) → switch-dispatch via `SC_No[0]`. Used fallthrough for duplicate entries. |
| 31 | `manage.c` Match Management Cleanup | MEDIUM | Switch-dispatch | 2,620 lines. 7 jump tables (`Management_Jmp_Tbl[13]`, `SC2[5]`, `SC5[8]`, `SC7[10]`, `SC8[4]`, `SC81[4]`, `SC12[10]`) → switch-dispatch via `C_No[]`. Removed 7 `#define` count macros. |
| 32 | `Debug.c` Jump Table → Switch | VERY LOW | Named states | 624 lines. `Main_Jmp_Tbl[3]` → `enum DebugTaskState` + switch via `r_no[0]`. 3 states (Init, 1st, 2nd). |
| 33 | `win_pl.c` Win Dispatch → Switch | LOW | Named states | 1,674 lines. `win_jp_tbl[16]` → switch via `winner_type_tbl[]` lookup. Per-character victory pose dispatch. Animation-only. |
| 34 | `gd3rd.c` Data Table Extraction | LOW | Data separation | 2,177→390 lines. 294-entry `ldreq_tbl[]` and 43-entry `ldreq_ix[]` extracted to `gd_data.c` / `gd_data.h`. Logic core only ~390 lines. |
| 35 | `sound3rd.c` Magic Number Cleanup | LOW | Named constants | 3 raw `SsRequest()` calls → `SND_MENU_CURSOR`, `SND_MENU_SELECT`, `SND_DIR_CURSOR` from `sound_ids.h`. |
| 36 | `bg_sub.c` Magic Number Cleanup | LOW–MED | Named constants | 1,287 lines. ~80 raw hex values → named constants in new `bg_constants.h`. Zoom bitmasks, chase flags, scroll zones, screen geometry, stage indices, scroll speed. Zero logic changes. |
| 37 | `bg.c` Decomposition | LOW–MED | File split | 1,538→~750 lines. Texture loading → `bg_load.c` (~380 lines), tile rewrite setup → `bg_rewrite.c` (~155 lines), core rendering/scroll/zoom remains in `bg.c`. `bg.h` → umbrella header. |
| 38 | `appear.c` State Cleanup | MEDIUM | Named `routine_no[]` states | 2,103 lines, 221 `routine_no[]` accesses. `APPEAR_RNO_COMPLETE`, `APPEAR_RNO_PHASE`, `APPEAR_RNO_TYPE`, `APPEAR_RNO_GOUKI` named indices. Animation-only, runs before round start. |
| 39 | `chren3rd.c` Decomposition | LOW | Already Factored | `chren3rd.c` is exclusively the 37,664-byte `obj_group_table`. Palette management is already isolated in `color3rd.c` and shadow rendering in `aboutspr.c`. No further splits required. |
| 47 | `vs_shell.c` VS Screen Data | LOW | Already Factored | 648 lines. File is exclusively one `const u8 VS_Shell_Active_Data[20][4][8][16]` array (67 KB tilemap data). Zero functions. Single consumer: `com_sub.c`. No decomposition needed. |
| 49 | `mtrans.c` Matrix Rendering | LOW | Already Factored | 2,817 lines. 99% dense rendering logic (SIMD, GPU compute, matrix transforms). Only ~70 lines of `static const` data (`flptbl[4]`, `bright_type[4][16]`). Not worth splitting. |
| 40 | `menu.c` Decomposition | LOW | File split | 3,480 lines. Split into `menu_network.c`, `menu_training.c`, `menu_replay.c`, `menu_save.c`, and core `menu.c`. Functions are clearly delineated by domain. |
| 41 | `demo00.c` / `demo01.c` / `demo02.c` Demo States | LOW | Per-function enums | 6 enums (`WarningState`, `CapLogoState`, `TitleState`, `TitleDashState`, `Demo00State`, `Demo01State`) in `demo_states.h`. Replaced generic `DEMO_STATE_*` with semantic names across 3 files. Fixed 5 implicit declaration warnings via missing `#include`s. |
| 42 | `sysdir.c` Magic Numbers | VERY LOW | Named constants | 397 lines. 3 new `Dipswitch`/`Dipswitch2` enum entries (`DIP_GUARD_CHECK_ENABLED`, `DIP2_SA_GAUGE_AUTOFILL_DISABLED`, `DIP2_SA_GAUGE_INCREMENTAL_FILL_DISABLED`). 12 raw hex → named constants across `sysdir.c` (9) and `pls02.c` (3). Zero logic changes. |
| 43 | `staff.c` Credits Constants | VERY LOW | Named constants | 504 lines. 17 raw hex → named constants via new `staff_constants.h` (14 defines). 2 BGM IDs added to `sound_ids.h`. Button mask → `SWK_ATTACKS`. Zero logic changes. |
| 46 | `ranking.c` Residual `D_No[]` States | LOW | Named states | 784 lines. 17 raw `D_No[]` accesses → 3 enums (`Ranking00State[6]`, `Ranking01State[5]`, `Ranking01FadeState[3]`). Named switch case labels + 2 direct assignments. Finishes task #28 cleanup. |
| 48 | `cmd_main.c` Input Bitmask Constants | LOW | Named constants | 1,870 lines. 122 raw hex → named constants via new `cmd_constants.h` (30 defines). Lever masks, button groups, flip flags, multi-press detection, switch patterns. Zero logic changes. |
---

## Next Wave: Safe Improvement Candidates (March 2026 Audit — Wave 2)

### State Machine / Magic Number Cleanup

| # | Component | Risk | Pattern | Key Files |
|---|-----------|------|---------|-----------| 
| 44 | `menu_input.c` Residual Hex Constants | LOW | Named constants | 2,121 lines. **141 raw hex literals** remain (UI positioning/formatting). Button constants already done (#15). |
| 45 | `menu_network.c` Hex Constants | LOW | Named constants | 1,898 lines. **114 raw hex literals** (network menu UI positioning/state). Menu-side only, no netplay sync risk. |


### Large Data Files (Documentation / Auto-Generation)

| # | Component | Risk | Pattern | Key Files |
|---|-----------|------|---------|-----------|
| 50 | `sound_lookup.c` Sound Table | MED | Auto-gen candidate | 3,016 lines (**280 KB**). Massive lookup table. Could be script-generated & documented rather than hand-maintained. |
| 51 | `charset.c` Character Set Tables | MED | Data isolation | 2,926 lines (76 KB). Character set mapping. Engine layer — uses `routine_no[]`. Touch with care. |
| 52 | `caldir.c` Directional Calc Tables | MED | Data isolation | 1,098 lines (88 KB). Directional calculation lookups. Pure data, engine-critical. |



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

see compile.bat / recompile.bat