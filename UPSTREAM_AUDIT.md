# Upstream Audit — Remaining Work

**Fork point**: `45526f2c` · **Last synced upstream commit**: `1fb1bcc6` · **Date**: 2026-03-25

To check for new upstream changes:
```bash
git fetch upstream && git log 1fb1bcc6..upstream/main --oneline --no-merges
```

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
| PR #141 — Fightcade replay tool (`fcade-replays/`) | ✅ Ported |
| PRs #159, #166, #167 — CMake: disable `-Wunused-parameter/-variable` | ✅ Done |
| `stb_ds.h` / `stb_impl.c` — dedicated implementation file | ✅ Done |
| `stop_if()` debug helper (`utils.c`) | ✅ Done |

---

## ✅ Port — Training Mode Improvements (Done)

| PR(s) | Status |
|--------|--------|
| #137, #147, #148 | ✅ Already ported in `aboutspr.c` |
| #153 | ✅ Ported (`effe3.c` no-stun per-frame reset) |

---

## 🟠 Port — Gameplay / QoL


---

## ✅ Investigated — Verdicts & Takeaways

> [!NOTE]
> Each item was investigated by comparing upstream and local code. "Skip" means don't port;
> "Takeaway" means there's a useful lesson even though we won't port the code directly.

### 1. PR #150 — Keymap (`keymap.c/.h`, `config_helpers.c/.h`) → **Skip + Takeaway**

**Verdict**: Skip the code — but note the gap.

**Investigation**: Upstream added a runtime input remapping system. Our `sdl_pad.c` (789 lines) has hardcoded keyboard bindings (WASD + IJKUOP). We have no runtime key remapping — users can only remap via the F1 Controller Setup overlay which maps *gamepad* buttons to game actions, not keyboard keys to gamepad buttons.

**Takeaway**: _Keyboard remapping is a real gap._ If users complain about keybinds, a lighter version of upstream's keymap (just keyboard-to-action mapping, persisted to config) would be the right solution. Not urgent — gamepads are the primary input.

---

### 2. PR #174 — Argparse + `configuration.h` refactor → **Skip (Already Superseded)**

**Verdict**: Skip — we already went further.

**Investigation**: Upstream's PR #174 added argparse and a `configuration.h` struct. Our local `cli_parser.c` is 174 lines with `--help`, `verify_configuration()`, and 15+ flags (`--scale`, `--port`, `--renderer`, `--plugin`, `--volume`, `--window-pos`, `--window-size`, `--shm-suffix`, `--font-test`, `--ui`, `--dump-missing-sprites`, `--test-*`). Our `configuration.h` already mirrors upstream's pattern (and says so in its comment). Upstream's 272-line `main.c` refactor would be a downgrade.

**Takeaway**: _None — we're ahead here._

---

### 3. PR #141 — Fightcade replay tool → **Ported**

**Verdict**: Ported — added as `fcade-replays/` directory.

**Action taken**: Copied the ~250-line Python tool from upstream PR #141. Includes `fcade_replay_tool.py`, `README.md`, and `.gitignore`. Useful as a community/dev tool for importing Fightcade replays.

---

### 4. PRs #159, #166, #167 — CMake tweaks → **Partially Done**

**Verdict**: Adopted the warning suppression; skipped the rest.

**Investigation**: Upstream disabled some warnings and added preprocessor variables. Our build has its own warning policy and defines.

**Action taken**: Added `-Wno-unused-parameter` and `-Wno-unused-variable` to both Clang and GCC `C_DISABLED_WARNINGS` lists in `CMakeLists.txt`. These are noisy in decompiled CPS3 code where many parameters exist by convention.

---

### 5. PRs #156, #168 — Doc updates → **Skip (Already Comprehensive)**

**Verdict**: Skip — our docs cover different ground (RmlUi, netplay lobby, tournaments).

**Investigation**: Checked `docs/building.md` (123 lines). Already covers Windows (MSYS2 with `pacman` + requirements file), Linux (apt + requirements file), macOS (Homebrew + pip), Rust install, cross-compilation (RPi4/Batocera), and Flatpak. Dependency install commands present for all 3 platforms.

**Takeaway**: _No gap — our build docs are already comprehensive._

---

### 6. `stb_ds.h` / `stb_impl.c` → **Done**

**Verdict**: Fixed — created dedicated `src/stb/stb_impl.c`.

**Investigation**: Upstream created a separate `stb_impl.c` (2 lines: `#define STB_DS_IMPLEMENTATION` + `#include "stb/stb_ds.h"`). We had `STB_DS_IMPLEMENTATION` defined inside `replay_game.c` instead. The header itself is identical (v0.67).

**Action taken**: Created `src/stb/stb_impl.c` as the canonical single-point-of-definition. Removed `#define STB_DS_IMPLEMENTATION` from `replay_game.c`. Any new file needing stb_ds data structures can now safely `#include "stb/stb_ds.h"` without worrying about the implementation define.

---

### 7. `src/port/utils.c` — Debug helpers → **Done**

**Verdict**: All helpers now present: `fatal_error`, `not_implemented`, `debug_print`, `stop_if`.

**Investigation**: Upstream added `stop_if(bool condition)` — a conditional debug breakpoint (`__debugbreak()` on Windows, `raise(SIGSTOP)` on Unix). Only active in `DEBUG` builds.

**Action taken**: Added `stop_if()` to `utils.c` (~10 lines) and declared it in `common.h`. Zero callers today; available as a dev primitive for conditional breakpoints.

---

### 8. `common.h` — CPS3 guards → **Already Correct**

**Verdict**: No action needed.

**Investigation**: `#if CPS3` guards exist in all ported decompilation files (`plmain.c` x7, `charset.c` x6, `plcnt.c` x2, `texgroup.c` x1). The `CPS3` macro is **intentionally not defined** in CMakeLists.txt — these guards fence off CPS3-hardware-specific code paths (DMA, hardware registers) that don't apply to the SDL3 port. The guards are working correctly as dead code exclusions.

**Takeaway**: _The pattern is correct._ If we ever target actual CPS3 hardware, define `CPS3` in the build to enable those paths.

---

### 9. `sdl_pad.c` — Keymap integration → **Skip (Follows from #1)**

**Verdict**: Skip — depends on keymap system (item #1) which we're skipping.

**Investigation**: Upstream wired keymap into `sdl_pad.c` (+260 lines). Our `sdl_pad.c` is 789 lines with full gamepad/keyboard/joystick support, controller image integration, raw joystick fallback, rumble, and netplay key guard. Significantly more mature than upstream.

**Takeaway**: _Same as #1_ — if we add keyboard remapping later, it would integrate into our `sdl_pad.c` rather than porting upstream's version.

---

## Summary

| Category | Items | Estimated Effort |
|----------|------:|-----------------| 
| CPS3 decompilation | 3 files, ~494 lines | ✅ Done |
| Training mode | 2 items, ~46 lines | ✅ Done |
| Gameplay / QoL | 0 items | ✅ Done |
| Arcade ROM system | 8+ files, new subsystem | ✅ Done |
| Discussion items | 9 items investigated | ✅ All resolved |
| Takeaway actions | 4 items executed | ✅ Done |
| **Total actionable** | **0 items** | ✅ **All done** |

### Future Backlog (from takeaways)

| Item | Priority | Notes |
|------|----------|-------|
| Keyboard remapping | Low | Hardcoded WASD+IJKUOP; add if users request |
