# Upstream Audit — Remaining Work

**Fork point**: `45526f2c` · **Upstream HEAD**: `upstream/main` (`1fb1bcc6`) · **Date**: 2026-03-24

> [!NOTE]
> This document tracks only **unported upstream changes we want**. Completed and skipped items have been removed.

---

## ✅ Completed / Skipped

| Item | Status |
|------|--------|
| PR #160 — Dead code removal (`bin2obj/` DATA_SECTION, `common.h`) | ✅ Ported |
| PR #192 — Replay parsing improvements | ✅ Ported |
| PR #165, #145 — CPS3 Decompilation (`plmain.c`, `charset.c`, `plcnt.c`) | ✅ Ported |
| PR #172, #176, #181, #185, #191 — Arcade ROM System (`rom_load`, `minizip-ng`, char data) | ✅ Ported |
| PR #146 — Matchmaking (`matchmaking.c/.h`) | ⏭️ Skip — we have lobby server |
| PR #163 — Network menu (`netplay_screen.c/.h`) | ⏭️ Skip — we have RmlUi lobby |
| PR #155, #158, #189 — Test runner | ⏭️ Skip — we have CMocka framework |
| PR #170 — zlib removal | ⏭️ Skip — both sides deleted |
| 27 PORTED_DELETE files (GekkoNet, zlib, bin2obj header) | ✅ Already handled |
| 59 DIVERGED files with only-removal or already-ported changes | ✅ Already clean |

---

## 🟠 Port — Training Mode Improvements

| PR(s) | File(s) | Lines | What |
|--------|---------|------:|------|
| #137 | `eff00.c` | 12 | Training hitbox display flag |
| #137, #147, #148 | `aboutspr.c` | ~40 | Hitbox/hurtbox colors, spacing fix |
| #147, #152 | `effa3.c` | 15 | Throw/projectile box colors |
| #153 | `effe3.c`, `menu.c` | ~6 | No-stun training setting |
| #154 | `sys_sub.c` | small | Immediate training menu input |
| #179 | `sys_sub.c` | ~19 | Auto-skip training transitions |

---

## 🟠 Port — Gameplay / QoL

| PR(s) | File(s) | Lines | What |
|--------|---------|------:|------|
| #164, #187 | `sc_sub.c/.h`, `entry.c`, `count.c`, `flash_lp.c`, `vital.c`, `game.c` | ~80 | **Draw Player Over HUD** — z-order fix |
| #186 | `sel_pl.c`, `n_input.c` | 8 | Attack buttons on press start |
| #188 | `next_cpu.c` | 14 | Skip VS between CPU matches |
| #190 | `entry.c`, `manage.c` | ~18 | Skip all VS screens |
| #193 | `pause.c` | 8 | Skip "Press x to pause" menu |
| #194 | `emlShim.c` | 6 | Sound priority on voice stop |
| #143, #177 | `select_timer.c` | 4 | Revert `Unit_Of_Timer` to 50 + reduce pacing delay |
| #162 | `dc_ghost.c` | small | Increase 2D box limit |

---

## ❓ Needs Discussion — Keep or Skip?

| Item | Context | My Lean |
|------|---------|---------|
| PR #150 — Keymap (`keymap.c/.h`, `config_helpers.c/.h`) | Input remapping system. We handle input config differently. | **Skip** — we have our own input system |
| PR #174 — Argparse (`argparse/`, `args.c/.h`, `configuration.h`) + main refactor (272 lines in `main.c`) | CLI argument parsing. We diverged heavily on `main.c`. | **Skip** — too much divergence, low value |
| PR #141 — Fightcade replay tool (`fcade-replays/`) | Python tool for downloading Fightcade replays. Not in-game. | **Skip** — external tool, not core |
| PRs #159, #166, #167 — CMake tweaks (disable warnings, preprocessor vars, Windows debug) | Build config. 33 lines in `CMakeLists.txt`. | **Skip** — our build differs |
| PR #156, #168 — Doc updates (`building.md`, `config.md`) | Documentation. Our docs differ. | **Skip** — our docs differ |
| `stb_ds.h` / `stb_impl.c` — PARTIAL | Upstream added stb_ds implementation file. We have our own version. | **Unsure** — check if we're missing anything |
| `src/port/utils.c` — 20 lines (`fatal_error`, `not_implemented`, `stop_if`) | Debug helper functions. | **Unsure** — might be useful |
| MISSING_FILE — `common.h` test runner hooks + `#if CPS3` guards | Upstream added hooks in headers we restructured. | **Check** — CPS3 guards may be needed for accuracy |
| `sdl_pad.c` — Keymap integration (+260 lines) | Upstream wired keymap system into SDL pad. | **Skip** if we skip keymap |

---

## Summary

| Category | Items | Estimated Effort |
|----------|------:|-----------------|
| CPS3 decompilation | 3 files, ~494 lines | ✅ Done |
| Training mode | 6 items, ~90 lines | Low |
| Gameplay / QoL | 8 items, ~140 lines | Low (most trivial) |
| Arcade ROM system | 8+ files, new subsystem | ✅ Done |
| **Total actionable** | **~14 items** | |
