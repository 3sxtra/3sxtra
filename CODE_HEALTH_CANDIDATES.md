# Code Health Improvement Candidates

> Full codebase scan — generated 2026-02-27
> Status: 🔄 Scan in progress…

## Legend
| Tag | Meaning |
|-----|---------|
| DUP | Code duplication / near-identical functions |
| CMPLX | High cyclomatic complexity / long function |
| DEAD | Dead or unreachable code |
| MAGIC | Magic numbers / unnamed constants |
| NAME | Poor naming / misleading identifiers |
| LONG | Excessively long file (>500 lines) |
| STYLE | Style / formatting inconsistency |
| MACRO | Macro abuse or candidate for inline function |

## Previously Refactored (from conversation history)
- [x] `engine/pls03.c` — DUP: `check_full_gauge_attack` redundant SA checks → extracted helper
- [x] `engine/spgauge.c` — DUP: four gauge-init functions consolidated → single parameterized helper
- [x] `engine/grade.c` — DUP: table-lookup duplication → extracted helper
- [x] `engine/pls01.c` — DUP: refactored
- [x] `engine/pls00.c` — DUP: `TO_nm_XXXXX` functions consolidated → parameterized helper
- [x] `engine/cmb_win.c` — DUP: combo_message_set/combo_pts_set → helpers + pointer aliases
- [x] `engine/pow_pow.c` — DUP: damage calc duplication → helper + simplified `Additinal_Score_DM`
- [x] `engine/plpdm.c` — DUP: `subtract_dm_vital`/`subtract_dm_vital_aiuchi` → shared helper
- [x] `engine/hitefef.c`, `hitefpl.c`, `hitplef.c`, `hitplpl.c` — DUP: hit check duplication
- [x] `training/training_hud.c` — DUP: refactored
- [x] `screen/win.c`, `screen/continue.c`, `screen/gameover.c` — DUP: `effect_76_init` + Order → shared `spawn_effect_76`

---

## Candidates Found

### engine/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### screen/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### com/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### effect/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### training/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### stage/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### ending/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### system/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### io/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### rendering/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### ui/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### sound/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### animation/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### demo/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### menu/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### opening/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### debug/

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

### Root-level files

| # | File | Tag | Description | Risk | Priority |
|---|------|-----|-------------|------|----------|

---

## Summary Statistics
- Total candidates: 0
- Scan progress: 0%
