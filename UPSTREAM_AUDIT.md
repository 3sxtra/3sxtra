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
| PR #137, #147, #152, #154, #179 — Training hitboxes/menus (`eff00.c`, `effa3.c`, `sys_sub.c`) | ✅ Ported |
| PR #153 — No-stun training setting (`menu.c`) | ✅ Ported |
| PR #188, #190 — Skip VS screens (`next_cpu.c`, `entry.c`) | ✅ Ported |
| PR #143, #177 — Unit_Of_Timer to 50 (`select_timer.c`) | ✅ Ported |
| PR #193 — Skip "Press x to pause" menu (`pause.c`) | ✅ Already ported (test guard exists) |
| PR #194 — Sound priority on voice stop (`emlShim.c`) | ✅ Ported |
| PR #186 — Attack buttons on press start (`sel_pl.c`, `entry.c`, `game.c`) | ✅ Already ported |
| PR #162 — Increase 2D box limit (`dc_ghost.c` → `renderer.c`) | ✅ Already ported (`RENDER_2D_PRIM_MAX 200`) |
| PR #190 — Skip all VS screens (`manage.c` part) | ✅ Already ported |
| PR #164, #187 — Draw Player Over HUD (`sc_sub`, `sc_cockpit`, `vital`, `game.c`) | ✅ Ported |
| PR #137, #147, #148 — Hitbox/hurtbox colors (`aboutspr.c`) | ✅ Already ported |
| PR #153 — No-stun training setting (`effe3.c`) | ✅ Ported |

---

## ✅ Port — Training Mode Improvements (Done)

| PR(s) | Status |
|--------|--------|
| #137, #147, #148 | ✅ Already ported in `aboutspr.c` |
| #153 | ✅ Ported (`effe3.c` no-stun per-frame reset) |

---

## 🟠 Port — Gameplay / QoL


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
| Training mode | 2 items, ~46 lines | ✅ Done |
| Gameplay / QoL | 0 items | ✅ Done |
| Arcade ROM system | 8+ files, new subsystem | ✅ Done |
| **Total actionable** | **0 items** | ✅ **All done** |
