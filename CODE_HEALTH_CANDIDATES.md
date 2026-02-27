# Code Health Improvement Candidates

> Full codebase scan — generated 2026-02-27
> Status: ✅ Scan complete

## Legend
| Tag | Meaning |
|-----|---------|
| DUP | Code duplication / near-identical functions |
| CMPLX | High cyclomatic complexity / long function |
| LONG | Excessively long file (>500 lines) |
| MAGIC | Magic numbers / unnamed constants |
| STYLE | Style / formatting inconsistency |

## Previously Refactored ✅
- `engine/pls03.c` — DUP: `check_full_gauge_attack` → extracted helper
- `engine/spgauge.c` — DUP: four gauge-init functions → single parameterized helper
- `engine/grade.c` — DUP: table-lookup → extracted helper
- `engine/pls01.c` — DUP: refactored
- `engine/pls00.c` — DUP: `TO_nm_XXXXX` → parameterized helper
- `engine/cmb_win.c` — DUP: combo helpers + pointer aliases
- `engine/pow_pow.c` — DUP: damage calc → helper + simplified `Additinal_Score_DM`
- `engine/plpdm.c` — DUP: `subtract_dm_vital` → shared helper
- `engine/hitefef.c`, `hitefpl.c`, `hitplef.c`, `hitplpl.c` — DUP: hit check duplication
- `training/training_hud.c` — DUP: refactored
- `screen/win.c`, `continue.c`, `gameover.c` — DUP: `spawn_effect_76` shared
- `engine/plpcu.c` — DUP: 9 duplicate scdmd stubs → reuse function pointers in dispatch table
- `system/pause.c` — DUP: `Setup_Pause`/`Setup_Come_Out` → `setup_pause_common` helper
- `engine/pls02.c` — DUP: 10 RNG generators → `rng_next` inline helper
- `rendering/aboutspr.c` — DUP: `sort_push_requestA`/`B` → shared `sort_push_request_box_impl` helper
- `com/com_pl.c` — DUP: `Com_Active`/`Follow`/`Passive`/`VS_Shell` → `com_dispatch_char` helper
- `screen/entry.c` — DUP: operator-activation block (4×) → `activate_new_operators` helper
- `screen/entry.c` — DUP: 3 identical `_1st` + 2 identical `_2nd` → `entry_phase_1st`/`entry_end_2nd` helpers
- `stage/bg_sub.c` — DUP: `Bg_Family_Set` 4 variants → `bg_family_set_layer` helper; `bg_pos_hosei_sub2`/`sub3` → `bg_pos_hosei_impl` helper
- `engine/hitcheck.c` — DUP: `nise_combo_work`/`add_combo_work` → shared `add_combo_work_impl` helper

---

## Candidates

### engine/

| # | File | Lines | Tag | Description | Risk | Priority |
|---|------|-------|-----|-------------|------|----------|
| 1 | `manage.c` | 2620 | LONG/CMPLX | 99 functions, massive state machine. Sub-phases (`5_0`–`5_7`, `7_0`–`7_9`, `8_0`–`8_3`) **already use table-driven dispatch** (`SC5_Jmp_Tbl`, `SC7_Jmp_Tbl`). Individual sub-functions are all unique logic — **not DUP, just LONG**. | Med | Low |
| 2 | `charset.c` | 2927 | LONG | 168 functions — script interpreter. Many `comm_*` functions are structurally identical (read params, apply, return). Could benefit from macro generation or table dispatch. Mostly functional though. | High | Low |
| 3 | `cmd_main.c` | 1936 | LONG/DUP | `check_0`–`check_26` are 27 separate input-check functions. Many share the same template (read lever, compare, advance). Could benefit from parameterized dispatch for simpler checks. | High | Low |
| ~~4~~ | ~~`hitcheck.c`~~ | ~~2066~~ | ~~LONG/DUP~~ | ~~`nise_combo_work`/`add_combo_work` → shared `add_combo_work_impl` helper. `defense_sky`/`defense_ground` already have extracted helpers~~ | — | ✅ Done |
| 5 | `plcnt.c` | 1566 | LONG/MAGIC | Huge data tables (`super_arts_data[20][4]`, `super_arts_DATA[20][4]`) — duplicated with different values. Many magic numbers in init. | High | Low |
| 6 | `plpnm.c` | 1324 | LONG | 50 functions. `Normal_42000`/`Normal_47000` share overall 5-case switch structure + preamble, but case-0 init differs substantially (different data tables, rl_flag sources, hit_stop setup, gauge functions). **Not cleanly consolidable** without a config struct that would obscure intent. | Med | Low |
| 7 | `plmain.c` vs `plmain2.c` | 976+270 | DUP | `player_mv_0000`/`player_mvbs_0000` share ~35 lines of init, but differ in `auto_guard` value, SA gauge init (complex switch vs simple call), `resurrection_resv`/`omop_vital_timer` fields. Cross-file refactoring needed (shared header). Moderate benefit but higher risk. | Med | Low |
| ~~8~~ | ~~`plpcu.c`~~ | ~~378~~ | ~~DUP~~ | ~~Consolidated scdmd stubs → dispatch table~~ | — | ✅ Done |
| 9 | `plpat.c` | 827 | LONG | 16 attack-level handlers. `Attack_00000`/`Attack_01000` share pattern (setup, cancel-check, char_move). Moderate DUP. | Med | Med |
| ~~10~~ | ~~`pls02.c`~~ | ~~1189~~ | ~~LONG/DUP~~ | ~~10 RNG generators → `rng_next` inline helper~~ | — | ✅ Done |
| 11 | `caldir.c` | 1099 | LONG | Mostly constant data tables (trig/direction). Not refactorable — it's ROM data. | N/A | Skip |
| 12 | `plpatuni.c` | 844 | LONG | Character-specific attack functions. Moderate repetition in attack patterns. | Med | Low |
| 13 | `plmain.c` | 976 | CMPLX | `sag_union` (lines 592–784) = 192 lines. Complex SA gauge update with nested conditionals. Could extract sub-helpers. | Med | Med |
| 14 | `plmain.c` | 976 | CMPLX | `check_omop_vital` (lines 864–975) = 111 lines. Switch has 5 cases (0,2,3,4 + fallthrough), each with distinct logic. Cases 3→4 share `vital_new++` with cap via intentional fallthrough. **Not DUP** — a complex state machine. | Med | Skip |

### screen/

| # | File | Lines | Tag | Description | Risk | Priority |
|---|------|-------|-----|-------------|------|----------|
| 15 | `sel_pl.c` | 2134 | LONG | 83 functions. Despite similar naming, `PL_Sel_1st`–`5th`, `Face_1st`–`4th`, `Handicap_1`–`4` are **all distinct logic** (unique phase implementations). `Handicap_Vital_Move_Sub` mirrors P1/P2 directions but that's inherent game logic. **Not DUP** — just LONG due to many phases. | Med | Skip |
| ~~16~~ | ~~`entry.c`~~ | ~~1560~~ | ~~LONG/DUP~~ | ~~Entry-phase template → `entry_phase_1st`/`entry_end_2nd` helpers~~ | — | ✅ Done |
| ~~17~~ | ~~`ranking.c`~~ | ~~771~~ | ~~DUP~~ | ~~Extracted common ranking layout function~~ | — | ✅ Done |
| 18 | `n_input.c` | 568 | MAGIC | `scfont_sqput`/`scfont_put` calls with raw position numbers. Could use named constants. | Low | Low |

### com/

| # | File | Lines | Tag | Description | Risk | Priority |
|---|------|-------|-----|-------------|------|----------|
| 19 | `com_sub.c` | 5953 | LONG | **Largest file** — 192 functions. AI subroutine library. Many nearly-identical "Term" functions (`EM_Term`, `Jump_Term`, `JA_Term`, etc.) with similar parameter patterns. Major DUP candidate. | High | Med |
| ~~20~~ | ~~`com_pl.c`~~ | ~~1941~~ | ~~LONG/DUP~~ | ~~Extracted `com_dispatch_char` helper~~ | — | ✅ Done |
| 21 | `com_data.c` | — | MAGIC | Likely large data tables. Needs inspection. | Low | Low |

### effect/

| # | File | Lines | Tag | Description | Risk | Priority |
|---|------|-------|-----|-------------|------|----------|
| 22 | `eff*.c` (204 files) | varies | DUP | Many `eff` files follow identical patterns: `effect_XX_init`, `effect_XX_move`, `effect_XX_die`. The move/die functions often have the same `sort_push_request` → `char_move` → check-end template. Massive cross-file DUP, but risky to consolidate individual effect behaviors. | High | Low |

### ui/

| # | File | Lines | Tag | Description | Risk | Priority |
|---|------|-------|-----|-------------|------|----------|
| ~~23~~ | ~~`sc_sub.c`~~ | ~~2473~~ | ~~LONG/DUP~~ | ~~`SSPutStr`/`SSPutStr2` → `SSPutStr_impl` helper; `scfont_put` → delegates to `scfont_sqput`; `scfont_sqput2` inverse branches → single loop with ternary~~ | — | ✅ Done |
| 24 | `flash_lp.c` | — | — | Needs inspection for DUP. | Low | Low |

### rendering/

| # | File | Lines | Tag | Description | Risk | Priority |
|---|------|-------|-----|-------------|------|----------|
| ~~25~~ | ~~`aboutspr.c`~~ | ~~711~~ | ~~DUP~~ | ~~Extracted `sort_push_request_box_impl` helper~~ | — | ✅ Done |

### stage/

| # | File | Lines | Tag | Description | Risk | Priority |
|---|------|-------|-----|-------------|------|----------|
| 26 | `bg.c` | 1494 | LONG/CMPLX | `scr_trans` alone is 505 lines (567–1072). Massive rendering function. `Bg_Texture_Load_EX`/`Bg_Texture_Load2`/`Bg_Texture_Load_Ending` share texture setup patterns. | Med | Med |
| ~~27~~ | ~~`bg_sub.c`~~ | ~~1316~~ | ~~LONG/DUP~~ | ~~`Bg_Family_Set` 4 variants → `bg_family_set_layer` helper; `bg_pos_hosei_sub2`/`sub3` → `bg_pos_hosei_impl` helper~~ | — | ✅ Done |
| 28 | `bg000.c`–`bg190.c` | varies | DUP | Per-stage background files. Many follow identical patterns. Cross-file DUP but risky to consolidate (stage-specific behavior). | High | Low |

### ending/

| # | File | Lines | Tag | Description | Risk | Priority |
|---|------|-------|-----|-------------|------|----------|
| 29 | `end_00.c`–`end_20.c` | varies | DUP | 20+ per-character ending files. Likely follow similar init/move/display patterns. Cross-file DUP. | Med | Low |
| 30 | `end_main.c` | — | — | Needs inspection for ending state machine DUP. | Med | Low |

### system/

| # | File | Lines | Tag | Description | Risk | Priority |
|---|------|-------|-----|-------------|------|----------|
| ~~31~~ | ~~`pause.c`~~ | ~~294~~ | ~~DUP~~ | ~~Extracted `setup_pause_common` helper~~ | — | ✅ Done |
| 32 | `sys_sub.c`, `sys_sub2.c` | — | — | Needs inspection. | Low | Low |

### Root-level files

| # | File | Lines | Tag | Description | Risk | Priority |
|---|------|-------|-----|-------------|------|----------|
| 33 | `game.c` | 1965 | LONG/CMPLX | 48 functions. State machine with `Game00`–`Game12` and sub-states. `Game06` (lines 1010–1153) = 143 lines handling game-over branching. `Game03` (lines 782–897) = 115 lines. Complex but may not be safely decomposeable. | Med | Low |
| 34 | `game_globals.c` | 11441 bytes | — | Large globals file. Needs inspection for dead/unused globals. | Low | Low |

### message/

| # | File | Lines | Tag | Description | Risk | Priority |
|---|------|-------|-----|-------------|------|----------|
| 35 | `plXXwin_en.c`, `plXXtlk_en.c`, `plXXend_en.c` | varies | — | 62 message data files. Purely string data — no refactoring needed. | N/A | Skip |

---

## Top Priority Quick Wins 🎯

All original quick wins have been completed! ✅

1. ~~**#10 `pls02.c` RNG functions** → `rng_next` inline helper~~ ✅
2. ~~**#8 `plpcu.c` scdmd stubs** → parameterized dispatch~~ ✅
3. ~~**#17 `ranking.c` layout duplication** → shared layout function~~ ✅
4. ~~**#31 `pause.c` Setup_Pause/Setup_Come_Out** → `setup_pause_common` helper~~ ✅
5. ~~**#25 `aboutspr.c` sort_push_request variants** → `sort_push_request_box_impl` helper~~ ✅
6. ~~**#20 `com_pl.c` Com_Active/Follow/Passive/VS_Shell** → `com_dispatch_char` helper~~ ✅

### Next candidates to consider

| Priority | # | File | Tag | Description |
|----------|---|------|-----|-------------|
| Med | 9 | `plpat.c` | LONG | Attack-level handlers share setup/cancel/char_move pattern |
| Med | 13 | `plmain.c` | CMPLX | `sag_union` = 192 lines — could extract sub-helpers |
| Med | 26 | `bg.c` | LONG/CMPLX | `Bg_Texture_Load_EX`/`Load2`/`Load_Ending` share texture setup |
| Med | 19 | `com_sub.c` | LONG | Nearly-identical "Term" functions — major DUP candidate |

## Summary Statistics
- Total candidates: **35** (excluding skips)
- Scan coverage: **~450+ C files** across **14 directories**
- Files > 1000 lines: **16** files
- Files > 2000 lines: **5** files (`com_sub.c`, `charset.c`, `manage.c`, `sc_sub.c`, `sel_pl.c`)
- Largest file: `com_sub.c` at **5953 lines**
