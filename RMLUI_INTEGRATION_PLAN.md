# RmlUi Integration Plan — 3SXtra

> **Goal**: Add [RmlUi](https://github.com/mikke89/RmlUi) (an HTML/CSS-based C++ UI library) alongside ImGui. Both UI systems are **always built and available**. A `--ui rmlui` flag or `ui-mode=rmlui` config key switches the active overlay UI at runtime.

---

## 1. Why RmlUi?

| Feature | Benefit for 3SXtra |
|---|---|
| **HTML/CSS workflows** | Menus defined in `.rml` + `.rcss` files — **hot-reloadable**, **moddable** by users |
| **SDL3 native** | First-class SDL3 support since RmlUi 6.1 (preferred default) |
| **All 3SXtra renderers** | `SDL_GL3` → OpenGL, `SDL_GPU` → SDLGPU, `SDL_SDLrenderer` → SDL2D |
| **Spatial navigation** | Built-in d-pad/gamepad nav via CSS `nav-up/down/left/right` — critical for a fighting game |
| **Data binding (MVC)** | Bind C++ game state to UI declaratively — no manual per-frame sync |
| **Flexbox + animations** | Modern layout engine with transitions, transforms, media queries |
| **Sprite sheets** | Native sprite sheet support with high-DPI scaling |
| **FreeType fonts** | Supports the same font files already in `assets/` (`NotoSansJP-Regular.ttf`) |
| **Performance** | Retained-mode DOM — only re-renders on changes, not every frame |
| **Lightweight** | ~1.5 MB compiled, FreeType + STL only |

---

## 2. Architecture Overview

### Both UI Systems — Always Built

ImGui and RmlUi are **both compiled into every build**. Only one is active at runtime, selected by config. This eliminates `#ifdef` complexity and ensures both paths are always tested by CI.

```
┌─────────────────────────────────────────────────────────────────────┐
│  Original CPS3 Menu System (menu.c)                                │ ← Untouched
├──────────────────────────────────┬──────────────────────────────────┤
│  RmlUi GAME Context (384×224)    │  RmlUi WINDOW Context (win_w×h) │
│  Phase 3 game-replacement screens│  Phase 2 overlay/debug menus     │
│  Renders into CPS3 canvas FBO    │  Renders to window surface       │
│  Scales with game, under bezels  │  On top of everything            │
├──────────────────────────────────┴──────────────────────────────────┤
│  ImGui Overlay Menus (fallback)                                     │ ← Active when ui-mode=imgui
├─────────────────────────────────────────────────────────────────────┤
│  Port-Side SDL Text Renderer                                        │ ← Always active (debug HUD)
└─────────────────────────────────────────────────────────────────────┘
```

**Dual-Context Architecture** (added during scaling/bezel fix):

| Context | Resolution | dp_ratio | Renders to | Documents |
|---------|-----------|----------|------------|----------|
| `game` | 384×224 | 1.0 | Canvas FBO (`gl_state.cps3_canvas_fbo`) | All Phase 3 screens (mode menu, HUD, char select, etc.) |
| `window` | win_w×win_h | SDL display scale | Window surface/swapchain | Phase 2 overlays (mods, shaders, training F7, input/frame display) |

Game-context documents use `rmlui_wrapper_*_game_document()` API and register data models on `rmlui_wrapper_get_game_context()`. Window-context documents use the original `rmlui_wrapper_*_document()` API.

Render pipeline order:
1. Game sprites → canvas FBO (384×224)
2. `rmlui_wrapper_render_game()` → composites Phase 3 UI into canvas FBO
3. Canvas blit to window (scaled to letterbox rect)
4. Bezels render on top
5. `rmlui_wrapper_render()` → Phase 2 overlays on window surface
6. Swap buffers

### Switching Mechanism

```
# CLI
3sx.exe --ui rmlui

# Config file (config.ini)
ui-mode = rmlui           # Options: "imgui" (default), "rmlui"
```

---

## 3. Phase 1 — Foundation ✅ COMPLETED

### Status

Phase 1 is **fully implemented and building successfully** on Windows (MSYS2/MinGW64/Clang/LLD/Ninja).

### Key Technical Findings

#### Glad Symbol Conflict (RESOLVED)

RmlUi's GL3 backend (`RmlUi_Renderer_GL3.cpp`) bundles a **header-only glad** (`RmlUi_Include_GL3.h`, glad v2.0.0-beta, GL 3.3 core). This conflicts with the project's `glad_gl_core` library (glad v2.0.8, GL 4.6 core) — both define identical global function pointers (`glad_glBlendFunc`, `gladLoadGLUserPtr`, `GLAD_GL_VERSION_*`, etc.), causing linker duplicate symbol errors.

**Approaches tried and failed:**
1. `-DGLAD_GL_H_` compile definition — suppressed all types, causing compilation errors
2. `-include glad/gl.h` force-include — types available but globals still duplicated
3. `#include "RmlUi_Renderer_GL3.cpp"` embedding — same duplicate globals in one TU

**Solution found:** `RMLUI_GL3_CUSTOM_LOADER` preprocessor define (lines 26-28 of `RmlUi_Renderer_GL3.cpp`). This is the **official RmlUi mechanism** for custom GL loaders:

```cpp
// In RmlUi_Renderer_GL3.cpp:
#elif defined RMLUI_GL3_CUSTOM_LOADER
    #define RMLUI_SHADER_HEADER_VERSION "#version 330\n"
    #include RMLUI_GL3_CUSTOM_LOADER   // ← uses our glad/gl.h
#else
    #define GLAD_GL_IMPLEMENTATION      // ← bundled glad (causes conflicts)
    #include "RmlUi_Include_GL3.h"
#endif
```

Setting `-DRMLUI_GL3_CUSTOM_LOADER=<glad/gl.h>` makes RmlUi use our project's glad loader, completely bypassing the bundled one.

#### Dependency Management (RESOLVED)

`compile.bat` runs `rm -rf build` on every invocation, destroying `FetchContent` caches. Moved RmlUi from `FetchContent` to `build-deps.sh` (cloned into `third_party/rmlui`), matching the pattern used by all other dependencies (SDL3, imgui, glad, etc.).

---

## 4. Files Changed (Phase 1)

### New Files

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_wrapper.h` | C header for RmlUi wrapper (mirrors `imgui_wrapper.h`) |
| `src/port/sdl/rmlui_wrapper.cpp` | RmlUi init, render, shutdown, document management (~250 lines) |
| `src/port/sdl/rmlui_mods_menu.h` | C header for RmlUi mods menu (mirrors `mods_menu.h`) |
| `src/port/sdl/rmlui_mods_menu.cpp` | Data model (17 BindFunc bindings) + per-frame dirty sync (~257 lines) |
| `src/port/sdl/rmlui_shader_menu.h` | C header for RmlUi shader menu |
| `src/port/sdl/rmlui_shader_menu.cpp` | Data model (10 bindings) + FilteredPreset struct/array + event callback (~280 lines) |
| `src/port/sdl/rmlui_stage_config.h` | C header for RmlUi stage config menu |
| `src/port/sdl/rmlui_stage_config.cpp` | Data model (13 layer bindings) + 4 event callbacks + dirty-check macro (~270 lines) |
| `src/port/sdl/rmlui_input_display.h` | C header for RmlUi input display |
| `src/port/sdl/rmlui_input_display.cpp` | InputRow struct + FGC notation (arrows+buttons) + 10-entry history per player (~254 lines) |
| `src/port/sdl/rmlui_frame_display.h` | C header for RmlUi frame display |
| `src/port/sdl/rmlui_frame_display.cpp` | FrameCell struct + 120-frame deque + state-to-CSS-class + advantage stats (~304 lines) |
| `assets/ui/base.rcss` | Global RmlUi theme stylesheet + checkbox/radio/range control styles |
| `assets/ui/test.rml` | Phase 1 test document (top-right overlay panel) |
| `assets/ui/mods.rml` | Mods menu HTML document with data bindings (~110 lines) |
| `assets/ui/mods.rcss` | Mods menu stylesheet — glass panel, form controls (~100 lines) |
| `assets/ui/shaders.rml` | Shader menu — select, radio, data-for preset list, search (~110 lines) |
| `assets/ui/shaders.rcss` | Shader menu stylesheet — scrollable preset list, select/radio (~210 lines) |
| `assets/ui/stage_config.rml` | Stage config — tab bar, range sliders with value readout, action buttons (~140 lines) |
| `assets/ui/stage_config.rcss` | Stage config stylesheet — tab bar, layer panel, button row (~250 lines) |
| `src/port/sdl/rmlui_training_menu.h` | C header for RmlUi training menu |
| `src/port/sdl/rmlui_training_menu.cpp` | 9 BindFunc bindings (checkboxes) + config persistence + dirty-check macro (~160 lines) |
| `src/port/sdl/rmlui_control_mapping.h` | C header for RmlUi control mapping |
| `src/port/sdl/rmlui_control_mapping.cpp` | Controller binding display + per-player layout (~321 lines) |
| `src/port/sdl/rmlui_netplay_ui.h` | C header for RmlUi netplay overlay |
| `src/port/sdl/rmlui_netplay_ui.cpp` | HUD badge + diagnostics panel + bar charts + toast system (~406 lines) |
| `assets/ui/training.rml` | Training options — master hitbox toggle + sub-toggles + data overlays |
| `assets/ui/training.rcss` | Training menu stylesheet |
| `assets/ui/input_display.rml` | P1/P2 input history panels with data-for binding |
| `assets/ui/input_display.rcss` | Input display stylesheet — transparent panels, arrow/button text |
| `assets/ui/frame_display.rml` | Frame meter — centered bar with data-class state coloring |
| `assets/ui/frame_display.rcss` | Frame display stylesheet — color-coded cells, stats text |
| `assets/ui/control_mapping.rml` | Control mapping — per-player binding tables (~88 lines) |
| `assets/ui/control_mapping.rcss` | Control mapping stylesheet (~226 lines) |
| `assets/ui/netplay.rml` | Netplay overlay — HUD badge, diagnostics, bar charts, toasts (~72 lines) |
| `assets/ui/netplay.rcss` | Netplay stylesheet — badge, graph bars, toast animation (~175 lines) |

### Modified Files

| File | Change |
|---|---|
| `CMakeLists.txt` | `add_subdirectory` for RmlUi, backend sources with `RMLUI_GL3_CUSTOM_LOADER`, link `rmlui` library |
| `build-deps.sh` | Added RmlUi 6.2 clone into `third_party/rmlui` |
| `tools/batocera/rpi4/download-deps_rpi4.sh` | Added RmlUi 6.2 clone for cross-compile |
| `src/port/config.h` | Added `CFG_KEY_UI_MODE` |
| `src/port/cli_parser.c` | `--ui <imgui\|rmlui>` — session-only flag (not persisted to config) |
| `src/port/sdl/sdl_app.c` | Init/shutdown/frame/render dispatch for both UI systems |
| `src/port/sdl/sdl_app_input.c` | Event routing dispatch based on `use_rmlui` flag |

### CMake Integration Details

```cmake
# In CMakeLists.txt:

# 1. RmlUi added via add_subdirectory (not FetchContent — survives rm -rf build)
add_subdirectory("${THIRD_PARTY_DIR}/rmlui" rmlui)

# 2. Backend sources compiled as part of 3sx target
add_executable(3sx ...
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Platform_SDL.cpp
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_GL3.cpp
)

# 3. Custom GL loader to avoid glad conflicts
set_source_files_properties(
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_GL3.cpp
    PROPERTIES
        COMPILE_DEFINITIONS "RMLUI_GL3_CUSTOM_LOADER=<glad/gl.h>;RMLUI_SDL_VERSION_MAJOR=3"
)

# 4. Link the rmlui core library
target_link_libraries(3sx PRIVATE rmlui)
```

---

## 5. Phased Implementation

### Phase 1 — Foundation ✅

- [x] Add RmlUi dependency (v6.2, `third_party/rmlui`)
- [x] Create `rmlui_wrapper.h` / `.cpp` with extern C interface
- [x] Resolve glad duplicate symbol conflict via `RMLUI_GL3_CUSTOM_LOADER`
- [x] Integrate into `sdl_app.c` lifecycle (init, shutdown, frame, render)
- [x] Integrate into `sdl_app_input.c` (event dispatch)
- [x] Add `--ui` CLI argument and `ui-mode` config key
- [x] Create `assets/ui/base.rcss` and `test.rml`
- [x] Update `build-deps.sh` and batocera download scripts
- [x] Build passes on Windows (MSYS2/MinGW64/Clang)

### Phase 2 — Menu Migration ✅ COMPLETED

Port each ImGui overlay menu to an `.rml` + `.rcss` document pair with C++ data bindings.

- [x] Add data binding infrastructure to `rmlui_wrapper` (`get_context()` accessor)
- [x] Mods menu → `rmlui_mods_menu.cpp` + `mods.rml` + `mods.rcss`
- [x] Shader menu → `rmlui_shader_menu.cpp` + `shaders.rml` + `shaders.rcss`
- [x] Stage config → `rmlui_stage_config.cpp` + `stage_config.rml` + `stage_config.rcss`
- [x] Input display → `rmlui_input_display.cpp` + `input_display.rml` + `input_display.rcss`
- [x] Frame display → `rmlui_frame_display.cpp` + `frame_display.rml` + `frame_display.rcss`
- [x] Training overlay → `rmlui_training_menu.cpp` + `training.rml` + `training.rcss`
- [x] Control mapping → `rmlui_control_mapping.cpp` + `control_mapping.rml` + `control_mapping.rcss`
- [x] Netplay UI → `rmlui_netplay_ui.cpp` + `netplay.rml` + `netplay.rcss`

#### Key Technical Finding: Data Model API

RmlUi 6.2 uses `ctx->CreateDataModel("name")` (not `GetDataModelConstructor`). The returned `DataModelConstructor` accepts `BindFunc()` for getter/setter lambdas — ideal for bridging C globals (`ModdedStage_*`, `Debug_w[]`, etc.) to the declarative HTML/CSS UI.

The mods menu uses 17 `BindFunc` bindings with per-frame dirty-checking to sync external state changes.

#### Key Technical Finding: Filtered Preset List (Shader Menu)

The shader menu has 2,242+ Libretro presets. Rendering all via `data-for` is impractical. Solution:
- `RegisterStruct<FilteredPreset>()` + `RegisterArray<vector<FilteredPreset>>()` to define the type
- A C++ `std::vector<FilteredPreset>` is rebuilt when the search filter string changes (via setter)
- The DOM only renders matching presets, keeping DOM size proportional to results
- `BindEventCallback("select_preset", ...)` handles clicks on preset items

#### Session-Only `--ui` Flag

The `--ui` CLI flag was initially config-persisted (`Config_SetString`), causing all subsequent runs to default to the last-used mode. Fixed by switching to a session-only global `g_ui_mode_rmlui` — users must explicitly pass `--ui rmlui` each time.

#### RCSS Gotchas (vs Standard CSS)

These differences were discovered during the mods menu migration and apply to all future menu migrations:

| CSS Feature | RmlUi RCSS Equivalent |
|---|---|
| `display: block` on `div`, `p`, `h1`… | **Must be explicit** — elements default to inline |
| `border: 1dp solid #color` | `border-width: 1dp;` + `border-color: #color;` — no `solid` keyword |
| `border-bottom: 1dp solid #color` | `border-bottom-width:` + `border-bottom-color:` |
| Font family auto-detection | Uses the TTF's internal name (e.g. `"Noto Sans JP Thin"` not `"Noto Sans JP"`) |
| `<input type="checkbox">` | Renders at 0×0 by default — **must set explicit `width`/`height`** |
| Checkbox checked state | Style via `input.checkbox:checked { background-color: ...; }` |
| `<input>` inside `<label>` | Use as siblings instead: `<input/><label>text</label>` |
| `display: flex` | Supported, but block layout is more reliable for simple forms |
| `gap` property | Not supported |
| `pointer-events` | Not supported — use `opacity` for disabled look |
| Range slider parts | `slidertrack`, `sliderbar`, `sliderarrowdec`, `sliderarrowinc` |
| `transition: prop 0.1s linear` | Tweening function must use `-in`/`-out`/`-in-out` suffix: `linear-in-out`, `cubic-in-out`, etc. Bare `linear` is invalid |
| `transition: color 0.1s, bg 0.1s` | No comma-separated multi-property transitions. List all properties first: `transition: color background-color 0.1s linear-in-out;` |
| `data-attr-class="foo{{expr}}"` | **Invalid** — `data-attr-*` values are pure data expressions, not interpolated strings. Use `data-class-<name>="bool_expr"` for toggling, or `data-attr-class="'foo-' + variable"` for string concat |
| `data-style-width="{{expr}}%"` | **Invalid** — use pure expression: `data-style-width="(expr) + '%'"` |
| `{{expr}}` in text content | **Valid** — `{{}}` interpolation only works inside element text (innerHTML), not in attribute values |

#### Cross-Compilation: Freetype

RmlUi requires Freetype for font rendering. On MSYS2/Windows it's auto-detected, but for RPi4 aarch64 cross-compilation, Freetype must be built from source:
- Added to `download-deps_rpi4.sh` — downloads + cross-compiles Freetype 2.13.3 (static, minimal)
- `rebuild-and-deploy.sh` passes `-DFreetype_ROOT` pointing to the cross-built library

> **Note**: RmlUi lacks a direct `ImGui::PlotLines` equivalent. Netplay ping graphs will require a custom RmlUi element or canvas-based approach.

### Phase 3 — Full CPS3 Replacement (Strategy B) — IN PROGRESS

Replace the CPS3 sprite-font HUD and menu rendering with RmlUi documents, giving every screen a fully customizable HTML/CSS-based UI while preserving the underlying game logic and state machines.

#### Architecture: The Bypass Pattern

The CPS3 rendering is **not** interleaved with game logic — it's cleanly separated at call sites. Phase 3 exploits this by adding a `use_rmlui` conditional at each rendering call site, replacing the `scfont`/effect-based rendering with RmlUi document updates.

**Key Insight: `Disp_Cockpit` Gate** — All in-game HUD rendering lives behind a single flag in `Game2_1()` (`game.c`):

```c
if (Disp_Cockpit) {
    Time_Control();           // Round timer
    vital_cont_main();        // Health bars
    player_face();            // Character portraits
    player_name();            // Player name labels
    combo_cont_main();        // Combo counter
    stngauge_cont_main();     // Stun gauge
    spgauge_cont_main();      // Super Art meter
    Sa_frame_Write();         // SA frame data
    Score_Sub();              // Score display
    Flash_Lamp();             // Flash effects
    Disp_Win_Record();        // Win count indicators
}
```

**Strategy**: When `use_rmlui` is true, skip the `scfont_put`/sprite rendering calls inside each function but **keep the logic** (gauge fill calculation, timer countdown, color state). Then feed the computed values to RmlUi data bindings.

Each HUD subsystem has two concerns:
1. **Logic** — gauge fill math, timer decrement, flash state, color cycling
2. **Rendering** — `scfont_sqput()`, `vital_put()`, `stun_put()`, `counter_write()`

These are already partially separated — many functions check `No_Trans` before rendering. Phase 3 adds `use_rmlui` checks at the same sites.

#### Per-Component Toggles (Mods Menu)

Each RmlUi HUD component is **individually toggle-able** from the mods/debug menu. This lets users mix CPS3 sprites with RmlUi selectively:

| Toggle | Default (RmlUi mode) | Effect when OFF |
|---|---|---|
| `rmlui_hud_health` | ON | Falls back to CPS3 `vital_put` sprites |
| `rmlui_hud_timer` | ON | Falls back to CPS3 `counter_write` sprites |
| `rmlui_hud_stun` | ON | Falls back to CPS3 `stun_put` sprites |
| `rmlui_hud_super` | ON | Falls back to CPS3 `spgauge` sprites |
| `rmlui_hud_combo` | ON | Falls back to CPS3 `SSPutDec` combo text |
| `rmlui_hud_names` | ON | Falls back to CPS3 `SSPutStr` name text |
| `rmlui_hud_faces` | ON | Falls back to CPS3 `face_base_put` sprites |
| `rmlui_hud_wins` | ON | Falls back to CPS3 `Disp_Win_Record` sprites |
| `rmlui_menu_mode` | ON | Falls back to CPS3 `effect_61` mode menu |
| `rmlui_menu_option` | ON | Falls back to CPS3 `effect_61` option menu |
| `rmlui_screen_title` | ON | Falls back to CPS3 "PRESS START" text |
| `rmlui_screen_winner` | ON | Falls back to CPS3 effect-based winner |
| `rmlui_screen_continue` | ON | Falls back to CPS3 continue countdown |
| `rmlui_screen_gameover` | ON | Falls back to CPS3 game-over screen |
| `rmlui_screen_select` | ON | Falls back to CPS3 char-select text elements |
| `rmlui_menu_game_option` | ON | Falls back to CPS3 `effect_61`/`effect_64` game option |
| `rmlui_menu_button_config` | ON | Falls back to CPS3 `effect_23` button grid |
| `rmlui_menu_sound` | ON | Falls back to CPS3 `effect_61`/`effect_A8` sound menu |
| `rmlui_menu_extra_option` | ON | Falls back to CPS3 `Dir_Move_Sub` extra option |
| `rmlui_menu_sysdir` | ON | Falls back to CPS3 `Dir_Move_Sub` system direction |
| `rmlui_menu_training` | ON | Falls back to CPS3 `effect_61` training selector |
| `rmlui_menu_lobby` | ON | Falls back to CPS3/ImGui lobby (uses existing toggle) |
| `rmlui_screen_vs_result` | ON | Falls back to CPS3 `effect_A0`/`effect_91` VS tally |
| `rmlui_menu_memory_card` | ON | Falls back to CPS3 `effect_61`/`effect_64` save/load menu |
| `rmlui_menu_blocking_tr` | ON | Falls back to CPS3 `effect_A3` blocking training menu |
| `rmlui_menu_blocking_tr_opt` | ON | Falls back to CPS3 `effect_A3` blocking training options |

These toggles are stored as globals (same pattern as existing `ModdedStage_*` flags) and exposed via `rmlui_mods_menu.cpp` `BindFunc` bindings. They are **not** config-persisted — RmlUi mode enables all by default, and users can disable individual components per session.

#### Phase 3.1 — In-Game Fight HUD ✅ IMPLEMENTED

**What**: Health bars, timer, stun gauge, super meter, combo counter, player names, win record.

**Bypass Points — Exact Call Anatomy:**

| Function | File | Logic to Keep | Render Calls to Skip |
|---|---|---|---|
| `vital_cont_main()` | `vital.c` | `vital_control()` — fill math, `colnum` color state | `vital_parts_allwrite()` → `scfont_sqput`, `vital_put`, `vital_base_put` |
| `stngauge_cont_main()` | `stun.c` | `stngauge_control()` — stun math, `sflag`/`g_or_s` | `stun_put()`, `stun_base_put()`, `stun_mark_write()`, `stun_gauge_waku_write()` |
| `spgauge_cont_main()` | `spgauge.c` | `spgauge_control()` — stock/fill state machines | `spgauge_base_put()` at top + internal `sa_gauge_trans()`, `sa_waku_trans()`, `sa_moji_trans()` |
| `count_cont_main()` | `count.c` | `counter_control()` — timer tick, `flash_r_num` | `counter_write()` (sole render fn) → several `scfont_sqput` calls |
| `combo_cont_main()` | `cmb_win.c` | `combo_control()` — chain detection, `SCORE_PLUS()` | `combo_window_trans()` per player (display-only path) |
| `player_face()` | `sc_sub.c` | none | `face_base_put()` + sprite writes |
| `player_name()` | `sc_sub.c` | none | `SSPutStr()` calls |
| `Disp_Win_Record()` | `sc_sub.c` | none | Sprite win-counter writes |

**Exact bypass pattern** (same for every subsystem — illustrated for health):
```c
// vital.c — vital_control(), inside No_Trans == 0 guard:
if (No_Trans == 0) {
    if (!use_rmlui || !rmlui_hud_health)
        vital_parts_allwrite(pl);   // CPS3 sprites
}
// vital.c — vital_cont_main(), EXE_flag path:
if (!use_rmlui || !rmlui_hud_health) {
    vital_parts_allwrite(0);
    vital_parts_allwrite(1);
}
```

##### Subsystem-Specific Source Notes

**Health** (`vital.c`): `vit[N].colnum` thresholds — `1`=green (HP≥0x31), `2`=yellow, `3`=red (HP<0x31, i.e. <49/160). `vit[N].cred` is the drain-bar fill; it lags behind `vital_new` by decrementing one unit per frame until they match. `omop_vt_bar_disp[N]` — if 0, shows a "silver" disabled bar (`silver_vital_put`).

**Timer** (`count.c`): `counter_write()` is the sole render site — all branches that call it are gateable through that function itself. `flash_r_num` becomes non-zero at 30s; `Counter_hi == 10` triggers a faster flash rate. `mugen_flag == true` → render ∞ symbol. Binding uses `round_timer` (integer display value), not `Counter_hi` directly — both equal after each `counter_control()` call.

**Stun** (`stun.c`): `sdat[N].slen = piyori_type[N].genkai / 8` determines the rendered bar width in sprite units. The blink state alternates via `sdat[N].g_or_s` every 2 frames when stunned (`sflag == 1`). When `sdat[N].proccess_dead` is set (player KO'd), stun is 0 and hidden. The `stun_gauge_waku_write()` call at the end of `stngauge_cont_main()` renders the background frame — also skip in RmlUi mode.

**Super Art** (`spgauge.c`): The critical readable state lives in `spg_dat[N]` (type `SPG_DAT`, declared in `spgauge.h`) — **not** in `PLW.sa` (which is `SA_WORK*`, a run-time pointer that may bounce between players). `spg_dat[N].max` = filled stocks, `spg_dat[N].time` = per-stock fill level (0-127), `spg_dat[N].spg_dotlen` = max fill = `super_arts[N].gauge_len`, `Super_Arts[N]` (global in `workuser.h`) = selected SA variant (1/2/3) = max stock count, `spg_dat[N].sa_flag` nonzero = SA timer is active. When `spg_dat[N].max >= Super_Arts[N]`: all stocks full, show "MAX" indicator. `super_arts[N].gauge_type == 1` = timed SA (stocks drain in real time). The internal render functions (`sa_gauge_trans`, `sa_waku_trans`, `sa_moji_trans`) are static to `spgauge.c` — bypass by gating `spgauge_base_put()` at the top of `spgauge_cont_main()` and letting the rest of the logic run without rendering.

**Combo** (`cmb_win.c`): The ring buffer `cmst_buff[PL][cst_read[PL]]` holds the pending display entry. Key fields: `hit_hi * 10 + hit_low` = hit count, `kind` (0=normal, 1=arts, 2=SA finish, 4=first attack, 5=reversal, 6=parry). `cmb_stock[PL] > 0` means there is a combo to display. Safe split: always call `combo_control(PL)` (scoring logic), only call `combo_window_trans(PL)` when `!rmlui_hud_combo`.

**New Files:**

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_phase3_toggles.h` | Shared header — declares all 26 per-component `extern bool` toggle globals |
| `src/port/sdl/rmlui_game_hud.h` | C header — init/update/shutdown for fight HUD |
| `src/port/sdl/rmlui_game_hud.cpp` | Data model: 33 `BindFunc` bindings + dirty-check update + all 26 toggle global definitions (~286 lines) |
| `assets/ui/game_hud.rml` | Fight HUD layout — HP bars, timer, stun/SA gauges, combo, names, win pips (~128 lines) |
| `assets/ui/game_hud.rcss` | Fight HUD styling — SF3 color palette, HP drain transitions, stun blink, SA pulse, timer flash (~190 lines) |

**Data Bindings:**

> **Note**: Max HP is constant across all characters. `Max_vitality = 160` (defined in `init3rd.c`). Per-character HP differences are handled via **damage scaling** — each character has an `original_vitality` value from `Com_Vital_Unit_Data[char][damage_level][difficulty]` that modifies the `dmcal_d` damage divisor. The health bar always shows 0–160 regardless of character. `Vital_Handicap` further scales `vital_new` in non-arcade modes (VS, training).

| Binding | Source Global | Type | Notes |
|---|---|---|---|
| `p1_health` / `p2_health` | `plw[N].wu.vital_new` | int | 0–0xA0 (160). Updates every frame |
| `p1_health_max` | `Max_vitality` | int | Always 160 |
| `p1_drain` / `p2_drain` | `vit[N].cred` | int | Red trail bar fill; lags behind `vital_new` by -1/frame |
| `p1_hp_color` / `p2_hp_color` | `vit[N].colnum` | int | 1=green (≥0x31), 2=yellow, 3=red (<0x31) |
| `round_timer` | `round_timer` | int | 0–99; updates every 60 frames via `counter_control()` |
| `timer_flash` | `flash_r_num` | bool | True when <30s (triggers CSS pulse animation) |
| `timer_infinite` | `mugen_flag` | bool | Show ∞ symbol; set when `Time_Limit == -1` |
| `p1_stun` / `p2_stun` | `sdat[N].cstn` | int | Stun fill (0 to `genkai`) |
| `p1_stun_max` / `p2_stun_max` | `piyori_type[N].genkai` | int | Per-char max stun (varies, e.g. 128–160) |
| `p1_stun_active` / `p2_stun_active` | `sdat[N].sflag` | bool | True when stunned (triggers blink CSS animation) |
| `p1_sa_stocks` / `p2_sa_stocks` | `spg_dat[N].max` | int | Filled stock count (0 to `Super_Arts[N]`) |
| `p1_sa_stocks_max` / `p2_sa_stocks_max` | `Super_Arts[N]` | int | Max stocks for selected SA variant (1/2/3) |
| `p1_sa_fill` / `p2_sa_fill` | `spg_dat[N].time` | int | Per-stock fill level (0–127) |
| `p1_sa_fill_max` / `p2_sa_fill_max` | `spg_dat[N].spg_dotlen` | int | Max fill = `super_arts[N].gauge_len` |
| `p1_sa_active` / `p2_sa_active` | `spg_dat[N].sa_flag != 0` | bool | SA timer currently active |
| `p1_sa_max` / `p2_sa_max` | `spg_dat[N].max >= Super_Arts[N]` | bool | All stocks full; show MAX badge |
| `p1_combo_count` / `p2_combo_count` | `cmst_buff[N][cst_read[N]].hit_hi * 10 + hit_low` | int | Active combo hits |
| `p1_combo_kind` / `p2_combo_kind` | `cmst_buff[N][cst_read[N]].kind` | int | 0=normal, 1=arts, 2=SA finish, 4=first attack, 5=reversal, 6=parry |
| `p1_combo_active` / `p2_combo_active` | `cmb_stock[N] > 0` | bool | Combo window visible |
| `p1_name` / `p2_name` | `My_char[N]` → name table | string | Character name string (static per fight) |
| `p1_wins` / `p2_wins` | `Win_Record[N]` / `VS_Win_Record[N]` | int | Round wins (0–3); `u16` type |
| `is_fight_active` | `Play_Game == 1` | bool | Show/hide HUD trigger |

**CSS Layout Strategy:**
```
┌────────────────────────── 640px ─────────────────────────┐
│ [P1 FACE] [══════ HP ══════] [TIMER] [══════ HP ══════] [P2 FACE] │
│           [━━━━━ STUN ━━━━]         [━━━━━ STUN ━━━━]            │
│           [SA ●●░░░░░░]             [SA ●●░░░░░░]                │
│ [WIN ●●○] [NAME: RYU    ]           [NAME: KEN    ] [WIN ●●○]    │
│ [P1 COMBO: 12 HITS  5000pts]   [P2 COMBO: ...]                   │
└──────────────────────────────────────────────────────────────────┘
```
Health bar: two overlapping `<div>` children under a fixed container — `.hp-drain` (red, full width minus drain %) behind `.hp-fill` (colored by `hp-color` class). Both use `transition: width 0.08s linear`. Stun blink uses `animation: stun-blink 66ms steps(1) infinite` when class `stun-active` is set. SA segments: `store_max` child `<div>` elements; filled ones get class `sa-filled`. Timer pulse: `animation: timer-pulse 400ms ease-in-out infinite` on class `timer-flash`.

#### Phase 3.2 — Mode Menu ✅ IMPLEMENTED

**What**: Replace the `SSPutStr` + `effect_61` cursor/text system in `Mode_Select()` (`menu.c`, lines 243–387).

The CPS3 `Mode_Select()` uses:
- `effect_61_init()` to spawn 7 text items (Arcade, Versus, Training, Network, Replay, Option, Exit)
- `effect_04_init()` to spawn the cursor highlight bar
- `MC_Move_Sub()` + `Check_Menu_Lever()` for d-pad navigation
- `IO_Result == 0x100` for button press → dispatches via `Menu_Cursor_Y[0]`

**Bypass site** (in `menu.c` → `Mode_Select()`, state machine case 0):
```c
case 0:  // init
    if (use_rmlui && rmlui_menu_mode) {
        rmlui_mode_menu_show();
    } else {
        effect_61_init(...);   // 7 text items
        effect_04_init(...);   // cursor bar
    }
    break;
```

**Input bridge**: `rmlui_mode_menu.cpp` registers an event callback `on_item_select(idx)` → `Menu_Cursor_Y[0] = idx; IO_Result = 0x100;` — the existing case 3 dispatch fires unchanged.

**Bindings:**
- `menu_cursor` — `Menu_Cursor_Y[0]` (0–6; drives highlighted item)
- `network_available` — bool (hide "Network" if GekkoNet unavailable)

**CSS design**: Full-height centered panel. Each `<button>` has `nav-up`/`nav-down` CSS pointing to siblings. Focus state: `border-left: 4dp solid gold; background: rgba(255,220,0,0.15)`. Panel slides in from left via `@keyframes slide-in` on show.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_mode_menu.h` | C header — init/update/show/hide/shutdown |
| `src/port/sdl/rmlui_mode_menu.cpp` | Data model: `menu_cursor` + `network_available` bindings + `select_item` event callback (~112 lines) |
| `assets/ui/mode_menu.rml` | 7 menu items with spatial navigation (nav-up/down), cursor class binding (~84 lines) |
| `assets/ui/mode_menu.rcss` | Glass panel, gold focus state, slide-in entrance animation (~83 lines) |

---

#### Phase 3 Implementation Notes (Lessons Learned)

> Recorded during Phase 3.1 and 3.2 implementation. These apply to all future Phase 3 work.

##### Including Real Game Headers from C++

The C++ data model files (`.cpp`) must access game struct types (`PLW`, `VIT`, `SDAT`, `SPG_DAT`, `CMST_BUFF`, `PiyoriType`, etc.) and their extern global instances. **Do NOT redeclare these structs locally** — the real types are defined in `structs.h` (pulled transitively by `workuser.h`) and have complex layouts (e.g. `PLW` is ~350 fields). Local redeclarations will inevitably diverge and cause `typedef redefinition with different types` errors.

**Pattern**: Include the real game headers inside an `extern "C" {}` block:

```cpp
extern "C" {
#include "sf33rd/Source/Game/engine/spgauge.h"  // SPG_DAT, spg_dat[2]
#include "sf33rd/Source/Game/engine/workuser.h"  // PLW, My_char, Win_Record, Super_Arts, Mode_Type...
#include "sf33rd/Source/Game/engine/plcnt.h"     // piyori_type[2], plw[2] externs
#include "sf33rd/Source/Game/engine/cmb_win.h"   // CMST_BUFF, cmst_buff, cmb_stock, cst_read

// Only declare externs for globals NOT in any header:
extern VIT  vit[2];
extern SDAT sdat[2];
extern s16  round_timer;
extern u8   Play_Game;
} // extern "C"
```

All game headers are pure C (no C++ constructs) and safe inside `extern "C"`.

##### SA Gauge: Use `spg_dat[]`, Not `PLW.sa->`

The plan originally documented SA gauge reads via `plw[N].sa->store` / `plw[N].sa->gauge.s.h`. While `PLW.sa` is a valid pointer (`SA_WORK*`), the **correct source for HUD display** is `spg_dat[N]` (`SPG_DAT` struct in `spgauge.h`):

| Value | Correct Source | Wrong Source | Why |
|---|---|---|---|
| Filled stocks | `spg_dat[N].max` | `plw[N].sa->store` | `spg_dat` is the rendered gauge state |
| Per-stock fill | `spg_dat[N].time` | `plw[N].sa->gauge.s.h` | `.time` is the display-ready fill level |
| SA active | `spg_dat[N].sa_flag != 0` | `plw[N].sa->ok == -1` | `.sa_flag` is the rendered active state |
| Max stocks | `Super_Arts[N]` (global) | `plw[N].sa->store_max` | SA variant 1/2/3 = max stock count |
| Max fill | `spg_dat[N].spg_dotlen` | — | Already correct in original plan |

##### Network Availability in Mode Menu

The mode menu needs to know if "Network" is available. There is no `Netplay_IsAvailable()` function in the codebase. Use the **compile-time** `NETPLAY_ENABLED` define (set by CMakeLists.txt for Debug/RelWithDebInfo builds) via a local `static inline bool netplay_is_available()` helper.

##### Type Aliases in Game Code

Game types use fixed-width aliases from `types.h`: `s8`=`int8_t`, `u8`=`uint8_t`, `s16`=`int16_t`, `u16`=`uint16_t`. `Win_Record[2]` and `VS_Win_Record[2]` are `u16` (not `s8`). `Mode_Type` is a `ModeType` enum (in `types.h`) with values like `MODE_VERSUS`, `MODE_ARCADE`, etc. — do not redefine with `#define`.

##### Bypass Pattern (Confirmed Working)

The `if (!use_rmlui || !rmlui_hud_<component>)` guard pattern works correctly. Applied in 5 game source files:
- `vital.c` — wraps `vital_parts_allwrite()` calls
- `stun.c` — wraps `stun_put`, `stun_base_put`, `stun_mark_write`, `stun_gauge_waku_write`
- `spgauge.c` — wraps `spgauge_base_put()` loop
- `count.c` — wraps all `counter_write()` calls
- `game.c` — wraps `player_face()`, `player_name()`, `Disp_Win_Record()`
- `menu.c` — wraps `effect_61`/`effect_04`/`effect_64` in `Option_Select()` and `Game_Option()` case 0
- `entry.c` — wraps `SSPutStr` in `Disp_00_0()` (title screen)

Game logic (timers, state machines, combo detection, gauge fill math) is **never** bypassed.

##### Build Integration Pattern

New Phase 3 `.cpp` files follow the same pattern as existing Phase 2 RmlUi components:
1. Add to `CMakeLists.txt` `add_executable(3sx ...)` source list (after `rmlui_netplay_ui.cpp`)
2. Add `#include` in `sdl_app.c` (after the Phase 2 includes)
3. Call `_init()` after `rmlui_control_mapping_init()` in `SDLApp_Init()`
4. Call `_update()` inside the `if (use_rmlui)` render block, before `rmlui_wrapper_render()`

##### Menu_Cursor_Y Type Gotcha (Phase 3.9/3.10)

`Menu_Cursor_Y` is `s8[2]` (not `short[4]`). The mode menu (`rmlui_mode_menu.cpp`) originally used `extern short Menu_Cursor_Y[4]` which compiles because it doesn't include `workuser.h`. But any file that includes `workuser.h` (for `Convert_Buff`, `Present_Mode`, etc.) will get the real declaration and trigger a `redeclaration with a different type` error.

**Fix**: Always include `workuser.h` inside `extern "C" {}` and never manually declare `Menu_Cursor_Y` or `IO_Result`.

##### Data Binding Expression Syntax (Phase 3.1/3.2/3.9/3.10)

RmlUi `data-attr-*` and `data-style-*` attribute values are **pure data expressions** — the entire attribute value is parsed as one expression. The `{{}}` interpolation syntax is **only valid inside element text content** (innerHTML), not in attribute values.

**Wrong** (causes "Could not find variable name" errors):
```html
<!-- WRONG: {{}} inside data-attr-class -->
<div data-attr-class="menu-item{{cursor == 0 ? ' focused' : ''}}"/>
<!-- WRONG: {{}} inside data-style-width -->
<div data-style-width="{{(hp * 100 / max)}}%"/>
```

**Correct** — use `data-class-*` for boolean class toggling, pure expressions for `data-attr-*`, and string concatenation for `data-style-*`:
```html
<!-- RIGHT: data-class-<name> with boolean expression -->
<div class="menu-item" data-class-focused="cursor == 0"/>
<!-- RIGHT: string concatenation in data-style -->
<div data-style-width="(hp * 100 / max) + '%'"/>
<!-- RIGHT: data-attr-class with pure expression -->
<div data-attr-class="'hp-fill hp-col-' + hp_color"/>
<!-- RIGHT: {{}} inside text content -->
<div class="opt-label">{{game_opt_label_0}}</div>
```

The `data-class-<name>` view is the idiomatic RmlUi approach for dynamic class toggling — it adds/removes the CSS class `<name>` based on a boolean expression. Multiple `data-class-*` attributes can coexist on the same element:
```html
<button class="menu-item" data-class-focused="cursor == 3" data-class-item-disabled="!available"/>
```

**Files fixed**: `game_hud.rml`, `option_menu.rml`, `mode_menu.rml`, `game_option.rml` — all converted from `data-attr-class` with `{{}}` to `data-class-*` boolean bindings and pure `data-attr-class` expressions.

##### Convert_Buff Index Mapping (Phase 3.10)

`Convert_Buff[4][2][12]` is indexed as `[buffer_type][cursor_id][setting_index]`:
- `Convert_Buff[0][cursor_id][]` — **Game Option** settings (Difficulty, Time Limit, Rounds, etc.)
- `Convert_Buff[1][cursor_id][]` — **Button Config** settings (per-player button assignments)
- `Convert_Buff[2][0][]` — **Screen Adjust** settings (X/Y offset, display size, screen mode)
- `Convert_Buff[3][1][]` — **Sound** settings (stereo/mono, BGM/SE levels, BGM type)

The initial plan incorrectly stated `Convert_Buff[3][2][]` for Game Option — this is **out-of-bounds** (second dimension is size 2, max index 1). Confirmed via `GO_Move_Sub_LR()` in `menu_input.c` line 682.

#### Phase 3.3 — Title Screen ✅ IMPLEMENTED

**What**: "PRESS START" text + attract-mode idle.

**Exact CPS3 mechanism** (`entry.c`): `Disp_00_0()` calls `SSPutStr(15, Insert_Y, 9, "PRESS START BUTTON")`. `Entry_00()` manages the blink via `E_No[1]` sub-states: case 2 = visible (50 frames), case 3 = hidden (30 frames). Additional "PRESS 1P START" / "PRESS 2P START" prompts appear when `G_No[1] == 3 || G_No[1] == 5`.

**Bypass** in `Disp_00_0()` (actual implementation):
```c
static void Disp_00_0() {
    if (save_w[1].extra_option.contents[3][5] == 0) return;
    if (use_rmlui && rmlui_screen_title) {
        /* CSS blink animation handles the visibility cycle */
        return;
    }
    SSPutStr(15, Insert_Y, 9, "PRESS START BUTTON");
    // ...
}
```

> **Design change from plan**: The blink is handled entirely by CSS `@keyframes` — no `title_visible` binding is needed. The document is shown/hidden at the `sdl_app.c` level. Only one dynamic binding (`show_2p_prompt`) is used.

**Bindings:**
- `show_2p_prompt` — `G_No[1] == 3 || G_No[1] == 5`

**CSS design**: Blink is driven purely by CSS keyframes: `animation: title-blink 1.33s step-end infinite` (62% visible, 38% hidden — matches the 50f/30f original cycle). 1P prompt styled in blue, 2P in red.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_title_screen.h` | C header — init/update/show/hide/shutdown |
| `src/port/sdl/rmlui_title_screen.cpp` | Data model: `show_2p_prompt` binding (~80 lines) |
| `assets/ui/title.rml` | "PRESS START BUTTON" + conditional 1P/2P prompts (~20 lines) |
| `assets/ui/title.rcss` | CSS blink animation, positioned text prompts (~75 lines) |

#### Phase 3.4 — Winner / Loser Scene ✅ IMPLEMENTED

**What**: "WINNER" banner, win streak display, score labels.

**Exact CPS3 mechanism** (`win.c` — `Win_2nd()`): Spawns these `effect_76` text objects (→ **replace**):
- `0x37` — winner character name banner
- `0x35`, `0x34`, `0x2B`, `0x3A`, `0x2C` — score labels and values
- `0x38` — round/win streak label  ; `0x2D` — win streak counter
- `Setup_Wins_OBJ()` — "1st WIN" / "2nd WIN+" via `0x2E`/`0x2F`, `0x30`/`0x31`

Keep: `effect_L1_init(1..6)` (sparkles), `effect_B8_init(WINNER, 0x3C)` (char victory anim).

**Loser path** (`Lose_2nd`): Fewer objects — `0x37`, `0x40`, `0x36`, `0x39`, `0x2D`. RmlUi loser doc shows a subdued variant (darker, no score breakdown).

**Bypass** in `Win_2nd()`:
```c
static void Win_2nd() {
    Switch_Screen(0); M_No[0]++;
    if (use_rmlui && rmlui_screen_winner)
        rmlui_win_screen_show();
    else {
        spawn_effect_76(0x37, ...); /* ... all text objects ... */
        Setup_Wins_OBJ();
    }
    effect_L1_init(1); /* ... 1..6 */ effect_B8_init(WINNER, 0x3C); // always
}
```

**Key globals**: `Winner_id`, `WGJ_Score = Continue_Coin[Winner_id] + Score[Winner_id][Play_Type]`, `WGJ_Win = Win_Record[Winner_id]` (or `VS_Win_Record` in VS mode).

**Bindings:** `winner_name`, `winner_score`, `winner_wins`, `streak_text` ("1st WIN" vs "2nd WIN+"), `is_versus_mode`.

**Timing note**: Show RmlUi document *after* `Switch_Screen_Revival(1)` completes in `Win_3rd()` — not in `Win_2nd()` itself — to align with the wipe transition.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_win_screen.h/.cpp` | Winner ID, score, win streak bindings |
| `assets/ui/win.rml` + `win.rcss` | Winner banner with CSS entrance animation |

#### Phase 3.5 — Continue Screen ✅ IMPLEMENTED

**What**: Countdown timer, "CONTINUE?" text, portrait effects.

**Exact CPS3 mechanism** (`continue.c`): `Setup_Continue_OBJ()` in `Continue_1st()` spawns:
- `effect_76(0x3B..0x3F)` — "CONTINUE?", countdown digits, labels → **replace**
- `effect_49_init(4/8)`, `effect_95_init(4/8/1/2)`, `effect_A9` — portrait/BG effects → **keep**

`Continue_2nd()` waits until `Continue_Count[LOSER] < 0`. The countdown value is in `Continue_Count_Down[LOSER]` (the visible integer, 0–9). `Continue_Count[LOSER]` drives the internal timer.

**Bypass** in `Setup_Continue_OBJ()`:
```c
static void Setup_Continue_OBJ() {
    effect_49_init(4); effect_49_init(8);   // always — portraits
    effect_95_init(4); effect_95_init(8); effect_95_init(1); effect_95_init(2); // always
    if (use_rmlui && rmlui_screen_continue)
        rmlui_continue_show();
    else {
        spawn_effect_76(0x3B, ...); /* ... 0x3C–0x3F */ 
    }
}
```

**Bindings:** `continue_count` (`Continue_Count_Down[LOSER]`, 0–9), `continue_active` (`Cont_No[0] < 2`), `loser_char` (`My_char[LOSER]` → name string).

**CSS design**: Large countdown number with `@keyframes count-tick` (scale 1.3→1.0 on each change). Color shifts: 0–3 = red, 4–6 = yellow, 7–9 = white.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_continue.h/.cpp` | Countdown timer, continue state bindings |
| `assets/ui/continue.rml` + `continue.rcss` | Countdown overlay with CSS number animation |

#### Phase 3.6 — Game Over / Results ✅ IMPLEMENTED

**What**: "GAME OVER" text, result stats, score breakdown.

**Exact CPS3 mechanism** (`gameover.c`): `GameOver_2nd()` case 2 calls `Setup_Result_OBJ()` which spawns:
- `spawn_effect_76(0x32, 3, 1)` — "GAME OVER" title → **replace**
- `spawn_effect_76(0x33, 3, 1)` — result subtitle → **replace**
- `effect_L1_init(7..0xE)` — 8 result stat lines (score, character, rounds, etc.) → **replace**
- `spawn_effect_76(0x41, 3, 1)` — called separately, further label → **replace**

**Bypass** in `GameOver_2nd()` case 2:
```c
if (FadeOut(1, 8, 8) != 0) {
    /* ... */
    if (use_rmlui && rmlui_screen_gameover)
        rmlui_gameover_show();
    else {
        Setup_Result_OBJ();
        spawn_effect_76(0x41, 3, 1);
    }
    return;
}
```

**Timing note**: `GO_No[1]` state 4 (`FadeIn`) must complete before showing the RmlUi document. The document should appear on state 5/6 transition, after `FadeIn()` returns non-zero. Use `G_Timer` timeout (`Result_Timer[Player_id]`) to auto-dismiss.

**Bindings:** `gameover_score` (`Score[Player_id][Play_Type]`), `gameover_char` (`My_char[Player_id]`), `gameover_rounds_won` / `gameover_rounds_lost` (`Win_Record[N]`).

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_gameover.h/.cpp` | Result data bindings (score, wins, character) |
| `assets/ui/gameover.rml` + `gameover.rcss` | Results panel with fade-in transition |

#### Phase 3.7 — Character Select Screen (Hardest) ✅ IMPLEMENTED (text overlay only)

**What**: Timer, character name plates, SA selection labels — as an overlay over the existing sprite system.

**Architecture of `sel_pl.c`** (2134 lines, 4 major sub-machines):
1. `Sel_PL_Control()` — top-level dispatcher
2. `OBJ_1st()` — spawns all select UI sprite objects (`effect_38` name plate, `effect_52` SA icons, `effect_K6` cursor bracket, `effect_39` player indicator, `effect_42` stage info, `effect_69` red lines, `effect_70` portraits)
3. `Player_Select_Control()` — per-player cursor navigation (5 phases)
4. `Face_Control()` — 19-portrait BG layer slide animation via `effect_93_init()` → **keep entirely**

**Overlay-first approach** (do NOT replace sprite objects):
The portrait grid (`effect_70_init(1..19)`), cursor bracket, and red-line animations are wired into `Order[]`/`Order_Timer[]`/`Order_Dir[]` routing. Replacing them risks breaking the entire select flow. Instead:
- **Keep all sprite objects** as-is
- **Replace only text-rendered elements** with RmlUi overlay:
  - Select timer (from `select_timer.c` — `SelectTimer_Draw()`)
  - Character name display (the `SSPutStr`/`effect_38` text — not the sprite name plate)
  - SA name labels
  - "SELECT YOUR CHARACTER" header text

**Timer bypass** (`select_timer.c`, `SelectTimer_Main()`):
```c
void SelectTimer_Main() {
    // decrement logic — always runs
    if (!use_rmlui || !rmlui_screen_select)
        SelectTimer_Draw();   // scfont render only
}
```

**Bindings:** `select_timer` (`Select_Timer`, 0–0x30/0x20), `p1_char_name` / `p2_char_name` (`My_char[N]` → name string, updates on cursor move), `p1_sa_name` / `p2_sa_name` (selected SA name from `super_arts[]`), `sel_status` (`Select_Status[0]`: 1=P1 selecting, 2=P2, 3=both).

> **Warning**: This is the highest-risk item. Tackle this **last** — keep all portrait/cursor/BG sprites, replace only text elements.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_char_select.h/.cpp` | Timer, name, SA label bindings |
| `assets/ui/char_select.rml` + `char_select.rcss` | Timer, name, SA overlay (no portraits) |

#### Phase 3.8 — VS Screen ✅ IMPLEMENTED (text overlay only)

`vs_shell.c` is pure tilemap data (66KB of tile arrays). The VS screen animation is entirely sprite-driven.

> **Recommendation**: Keep the VS screen sprite animation intact. Add an RmlUi overlay for the text elements only (character names, "VS" label, stage name). This gives a modern look without reimplementing the iconic slide-in animation.

**Text elements to replace** (identified via `SSPutStr` call sites in `vs_shell.c`):
- P1 character name (left side)
- P2 character name (right side)
- Stage name (bottom center)

**Bindings:** `vs_p1_name` / `vs_p2_name` (`My_char[N]` → name string), `vs_stage_name` (stage name string), `vs_ready` (shown when VS screen phase begins).

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_vs_screen.h/.cpp` | Character name and stage name bindings |
| `assets/ui/vs_screen.rml` + `vs_screen.rcss` | Text overlay with entrance animations |

---

#### Phase 3.9 — Option Menu (Sub-Menu Dispatcher) ✅ IMPLEMENTED

**What**: The "OPTION MENU" list — Game Option, Button Config., Screen Adjust, Sound, Save/Load, Extra Option, Exit. This is `Option_Select()` in `menu.c` (line 1346).

**Exact CPS3 mechanism**: `Option_Select()` case 0 spawns 6 or 7 `effect_61` items (char_index `0x2F`–`0x35` for the locked variant; `7`–`13` for the unlocked/Extra Option variant) plus one `effect_04` cursor bar. The item count depends on `save_w[Present_Mode].Extra_Option` and `Unlock_All` flags.

**Bypass** (actual implementation — two separate branches for locked/unlocked):
```c
case 0:
    Menu_in_Sub(task_ptr);
    // ... Order[] + effect_57 always run ...
    if (save_w[Present_Mode].Extra_Option == 0 && save_w[Present_Mode].Unlock_All == 0) {
        if (use_rmlui && rmlui_menu_option) {
            rmlui_option_menu_show();
        } else {
            effect_04_init(1, 4, 0, 0x48);
            // ... 6 effect_61 items ...
        }
        Menu_Cursor_Move = 6;
        break;
    }
    if (use_rmlui && rmlui_menu_option) {
        rmlui_option_menu_show();
    } else {
        effect_04_init(1, 1, 0, 0x48);
        // ... 7 effect_61 items ...
    }
    Menu_Cursor_Move = 7;
    break;
```

**Input bridge**: Same pattern as Mode Menu — event callback → `Menu_Cursor_Y[0] = idx; IO_Result = 0x100;`. Cancel callback → `IO_Result = 0x200;`.

**Bindings:**
- `option_cursor` — `Menu_Cursor_Y[0]` (via `workuser.h`, type `s8`)
- `extra_option_available` — `save_w[Present_Mode].Extra_Option || save_w[Present_Mode].Unlock_All` (show/hide 7th "Extra Option" item via `data-if`)

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_option_menu.h` | C header — init/update/show/hide/shutdown |
| `src/port/sdl/rmlui_option_menu.cpp` | Data model: `option_cursor` + `extra_option_available` bindings + `select_item`/`cancel` callbacks (~110 lines) |
| `assets/ui/option_menu.rml` | 7 menu items (Extra Option conditional via `data-if`), spatial nav (~80 lines) |
| `assets/ui/option_menu.rcss` | Glass panel, gold focus, slide-in animation (~85 lines) |

---

#### Phase 3.10 — Game Option Screen ✅ IMPLEMENTED

**What**: Difficulty, Time Limit, Rounds (1P/VS), Damage Level, Guard Judgment, Analog Stick, Handicap (VS), Player1/Player2 (VS), Default Setting, Exit. `Game_Option()` in `menu.c` (line 1806).

**Exact CPS3 mechanism**: Case 0 spawns 12 `effect_61` row labels (char_index `0x19`–`0x24`, `master_player=2`) and 10 `effect_64` value columns (slots `0x5D`–`0x66`, using `Setup_Index_64[]` lookup). `effect_64` differences from `effect_61`: value column items have left/right navigation built in. Main input loop calls `Game_Option_Sub(0/1)` and `Button_Exit_Check()`.

**Bypass** (actual implementation): Gate all `effect_61_init`/`effect_64_init` calls in case 0. `Game_Option_Sub()` runs unchanged — it reads/writes `Convert_Buff[0][0][]`. RmlUi reads the same buffer each frame with dirty checking.

> **⚠️ Corrected**: The original plan stated `Convert_Buff[3][2][]` — this is **out-of-bounds**. Game Option values live at `Convert_Buff[0][cursor_id][]`, confirmed via `GO_Move_Sub_LR()` in `menu_input.c`.

**Key globals** (mapped from `Convert_Buff[0][0][ix]`):

| Index | Setting | Range | Formatter |
|---|---|---|---|
| `[0]` | Difficulty | 0–7 | `"1"`..`"8"` |
| `[1]` | Time Limit | 0–3 | `"30"`, `"60"`, `"99"`, `"NONE"` |
| `[2]` | Rounds (1P) | 0–2 | `"1"`, `"2"`, `"3"` |
| `[3]` | Rounds (VS) | 0–2 | `"1"`, `"2"`, `"3"` |
| `[4]` | Damage Level | 0–4 | `"1"`..`"5"` |
| `[5]` | Guard Judgment | 0–1 | `"OLD"`, `"NEW"` |
| `[6]` | Analog Stick | 0–1 | `"ENABLE"`, `"DISABLE"` |
| `[7]` | Handicap (VS) | 0–1 | `"ON"`, `"OFF"` |
| `[8]` | Player1 (VS) | 0–1 | `"HUMAN"`, `"COM"` |
| `[9]` | Player2 (VS) | 0–1 | `"HUMAN"`, `"COM"` |

**Bindings**: `game_opt_cursor` (`Menu_Cursor_Y[0]`), 10× `game_opt_label_N` (static strings), 10× `game_opt_value_N` (formatted from `Convert_Buff[0][0][N]` via per-row formatter functions).

**CSS design**: Two-column inline layout — `opt-label` (155dp left) and `opt-value` (90dp right, text-align right). Focused row highlighted with gold left border. Footer hint shows "◀ ▶ CHANGE VALUE".

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_game_option.h` | C header — init/update/show/hide/shutdown |
| `src/port/sdl/rmlui_game_option.cpp` | Data model: 10 formatter functions + 20 BindFunc bindings + dirty-check update (~190 lines) |
| `assets/ui/game_option.rml` | Two-column table with 10 rows + footer hint (~95 lines) |
| `assets/ui/game_option.rcss` | Inline label/value layout, slide-in animation (~100 lines) |

---

#### Phase 3.11 — Button Config Screen ✅ IMPLEMENTED

**What**: Per-player button remapping grid (L.PUNCH, M.PUNCH, H.PUNCH, NONE ×2, L.KICK, M.KICK, H.KICK, NONE, VIBRATION OFF, DEFAULT SETTING, EXIT for both P1 and P2). `Button_Config()` in `menu.c` (line 1879).

**Exact CPS3 mechanism**: Case 0 spawns:
- 12 `effect_23` for P1 label column (slots `0x50`–`0x5B`, `master_player=2`, `disp_kind=2`)
- 12 `effect_23` for P2 label column (slots `0x5C`–`0x67`, `master_player=2`, `disp_kind=3`)
- 9 `effect_23` for P1 button assignments (slots `0x78`–`0x80`, `disp_index` = 0 or 1 for "NONE")
- 9 `effect_23` for P2 button assignments (slots `0x81`–`0x89`)
- 2 `effect_66` background boxes (slots `0x8A`/`0x8B`, `cg_type=7/8`)

Input handled by `Button_Config_Sub(PL_id)` in `menu_input.c`.

**Bypass site**: Gate all `effect_23_init`/`effect_66_init` in case 0. `Button_Config_Sub()` still runs — it writes to `Key_Disp_Work[PL_id][]`. RmlUi reads `Key_Disp_Work` per frame.

**Key globals**: `Key_Disp_Work[2][12]` — per-player, 12-slot button assignment table. `Copy_Key_Disp_Work()` is called at init to copy working state. `Restore_Key_Disp_Work()` on cancel.

**Bindings**: `p1_buttons[]` / `p2_buttons[]` (12 string values from `Key_Disp_Work`), `bc_cursor` (`Menu_Cursor_Y[0]`), `bc_player_side` (which player side is highlighted).

**CSS design**: Two side-by-side panels (P1 left, P2 right), each with a 12-row table. Currently selected cell highlighted. Button icons use icon font or badge elements. `DEFAULT SETTING` / `EXIT` at the bottom of both panels.

> **Note**: This screen is the most complex for RmlUi due to input capture — `Button_Config_Sub` reads raw button presses to detect "what button did the player just press?" The RmlUi document must suppress all `KEY` events during this capture phase and let the underlying `Button_Config_Sub()` native loop run.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_button_config.h/.cpp` | Key_Disp_Work bindings, P1/P2 state |
| `assets/ui/button_config.rml` + `button_config.rcss` | Dual-column button grid |

---

#### Phase 3.12 — Sound Test / Screen Adjust ✅ IMPLEMENTED

**What (Sound)**: Audio (Stereo/Mono), BGM Level (0–15), SE Level (0–15), BGM Select (Arrange/Original), BGM Test, Exit. `Sound_Test()` in `menu.c` (line 1967).

**Exact CPS3 mechanism (Sound)**: Case 0 spawns:
- 7 `effect_61` row labels (char_index `0x3B`–`0x41`, `master_player=2`, font `0x7047`)
- 4 `effect_64` value items (slots `0x57`–`0x5A`, using `ixSoundMenuItem[]` = {10,11,11,12})
- 4 `effect_A8` slider items (slots `0x78`–`0x7B`) — the BGM/SE level sliders
- 1 `effect_04` cursor bar

`effect_A8` = animated slider widget specific to volume controls. Main loop calls `Sound_Cursor_Sub(0/1)`.

**Key globals**: `Convert_Buff[3][1][0]` = audio mode (0=stereo, 1=mono), `[1]` = BGM level, `[2]` = SE level, `[3]` = BGM type (0=arrange, 1=original), `[5]` = BGM test track index. `sys_w.sound_mode`, `sys_w.bgm_type`, `bgm_level`, `se_level`.

**What (Screen Adjust)**: X/Y position, X/Y range, filter, default, exit. Entered via `Option_Select` → `Screen_Adjust` AT entry (AT index `3`). The screen adjust uses direct slider rendering — no dedicated function found separately, reuses the option rendering pipeline with different data.

**Bindings (Sound)**: `snd_cursor` (`Menu_Cursor_Y[0]`), `snd_audio_mode` (`Convert_Buff[3][1][0]`), `snd_bgm_level` / `snd_se_level` (`[1]`/`[2]`), `snd_bgm_type` (`[3]`), `snd_test_track` (`[5]`).

**CSS design**: Volume sliders use `<input type="range">` styled as custom elements, or a manual `<div class="slider-fill">` with `width: calc(var(--level) / 15 * 100%)`. The "BGM TEST" entry toggles a `playing` class on confirm.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_sound_menu.h/.cpp` | Audio mode, level, and BGM test bindings |
| `assets/ui/sound_menu.rml` + `sound_menu.rcss` | Volume sliders + mode toggles |

---

#### Phase 3.13 — System Direction (Dipswitch) + Direction Menu ✅ IMPLEMENTED

**What**: System Direction = 4 dipswitch categories (SYSTEM, DIRECTION, SAVE, LOAD, EXIT) each leading to a page of individual toggle options. Direction Menu = per-character direction overrides across multiple pages. Both use the same `Dir_Move_Sub()` rendering pipeline.

**Exact CPS3 mechanism (System Direction)**: `System_Direction()` case 0 spawns 4 `effect_61` nav items (char_index `0x2B`–`0x2E`) and one `effect_64` for the page value display. Main input is `System_Dir_Move_Sub(PL_id)`. Page navigation via left/right dispatches to `Direction_Menu()` sub-pages.

**Exact CPS3 mechanism (Direction Menu)**: `Direction_Menu()` uses `Setup_Next_Page()` + `Dir_Move_Sub()` — each page is rebuilt from the `dir_data` tables. The current page (`Menu_Page`, 0 to `Page_Max`) determines which dipswitch settings are shown. Has both `system_dir[]` (global config) and Description text rendering via `Message_Data`.

**Bypass strategy**: Both screens share the `Dir_Move_Sub` / `Setup_Next_Page` pipeline which drives sprite re-rendering on each page turn. The bypass must:
1. Gate all `effect_61_init`/`effect_64_init` in the init case
2. Gate `Setup_Next_Page()` sprite spawning — keep the data update, skip the effect spawning
3. Expose current page and all toggle values as bindings

**Key globals**: `direction_working[1].contents[page][row]` — per-page, per-row toggle values. `Menu_Page` — current page index. `Page_Max` = `Check_SysDir_Page()`. `system_dir[1]` — the live working copy.

**Bindings**: `sysdir_page` (`Menu_Page`), `sysdir_page_max` (`Page_Max`), `sysdir_cursor` (`Menu_Cursor_Y[0]`), `sysdir_values[][]` (2D array of toggle values for current page), description text from `msgSysDirTbl`.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_sysdir.h/.cpp` | Page, toggle table, description bindings |
| `assets/ui/sysdir.rml` + `sysdir.rcss` | Paged dipswitch toggle table |

---

#### Phase 3.14 — Extra Option (4 Pages) ✅ IMPLEMENTED

**What**: Life Gauge Type, 1P/2P Vitality, Guard Type, Rapid Fire, Bonus Stage (page 1); Parry options (page 2); VS/training-specific toggles (page 3); Screen Text Display, Life/Timer/Stun/SA/Press Start gauge display toggles (page 4). `Extra_Option()` in `menu.c` (line 3813).

**Note**: Page 4 of Extra Option (`SCREEN TEXT DISPLAY ... ON`, `LIFE GAUGE DISPLAY`, `TIMER DISPLAY`, `STUN GAUGE DISPLAY`, `S.A.GAUGE DISPLAY`, `PRESS START DISPLAY`) directly controls visibility of existing CPS3 HUD elements. These flags are stored in `save_w[Present_Mode].extra_option.contents[3][0..5]` and already affect `omop_cockpit`, `omop_vt_bar_disp`, etc. **RmlUi HUD components must respect these same flags** — the Phase 3.1 bypass logic already checks `omop_cockpit`; the individual gauge flags should also be checked.

**Exact CPS3 mechanism**: Uses `Dir_Move_Sub()` + `Setup_Next_Page()` across `Page_Max = 3` pages (0–3). No `effect_61`/`effect_04` cursor — the `Dir_Move_Sub` pipeline handles cursor bar internally via the extra-option sprite data in `ex_data.h`.

**Bypass strategy**: Same as Direction Menu — gate `Setup_Next_Page()` sprite spawning, keep data updates. Key difference: `save_w[Present_Mode].extra_option.contents[Menu_Page][ix]` holds all values as a flat 2D array.

**HUD integration (page 4 flags)**:
- `contents[3][0]` — `SCREEN TEXT DISPLAY` — master toggle for all text overlays
- `contents[3][1]` — `LIFE GAUGE DISPLAY` — gates `omop_vt_bar_disp` → maps to `rmlui_hud_health` bypass check
- `contents[3][2]` — `TIMER DISPLAY` — gates timer display → `rmlui_hud_timer`
- `contents[3][3]` — `STUN GAUGE DISPLAY` → `rmlui_hud_stun`
- `contents[3][4]` — `S.A.GAUGE DISPLAY` → `rmlui_hud_super`
- `contents[3][5]` — `PRESS START DISPLAY` → `rmlui_screen_title`

> **Important**: RmlUi HUD per-component toggles (Phase 3.1) must also check the Extra Option page 4 flags — they are the canonical player-visible settings. When `SCREEN TEXT DISPLAY = OFF`, all Phase 3 overlays must hide even if `rmlui_hud_*` session toggles are ON.

**Bindings**: `extra_page` (`Menu_Page`), `extra_cursor` (`Menu_Cursor_Y[0]`), `extra_values[][]` (`save_w[Present_Mode].extra_option.contents`), description text.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_extra_option.h/.cpp` | 4-page data model, HUD flag coupling |
| `assets/ui/extra_option.rml` + `extra_option.rcss` | Paged toggle table; page indicator |

---

#### Phase 3.15 — Training Sub-Menus ✅ IMPLEMENTED

**What**: Training Mode selector (Normal/Parrying/Trials/Exit), Normal Training pause menu (8 items: 3 recording modes + Dummy Setting, Training Option, Parry Training Option, Character Change, Exit), Dummy Setting (6+5 `effect_A3` items), Training Option (similar structure).

**Exact CPS3 mechanism (Training Mode selector)**: `Training_Mode()` spawns 4 `effect_61` items (string indices `0x35`, `0x36`, `66`, `0x37` = Normal Training, Parrying Training, Trials, Exit). Standard `effect_04` cursor bar.

**Exact CPS3 mechanism (Normal Training menu)**: `Normal_Training()` uses `Training_Init_Sub()` then spawns 8 `effect_A3` items at y=56+16×ix — these are the in-game pause table rows. `effect_A3` = the in-game training row widget (label + value in one effect). Input via `MC_Move_Sub` on `Decide_ID` (the champion player's controller).

**Exact CPS3 mechanism (Dummy Setting)**: `Dummy_Setting()` spawns 6 `effect_A3` items at y=80+16×ix (left column, Action/Block Type/Parry Type/Stun Mash/Wakeup Mash/Default) and 5 `effect_A3` at y=80+16×ix (right column). The `Training[]` struct holds all values.

**Bypass strategy**: Training sub-menus are shown while the game is running (`Game_pause = 0x81`). They appear as overlays. The bypass:
- Gate `effect_A3_init` spawning in `Normal_Training` / `Dummy_Setting` / `Training_Option`
- Gate `effect_61_init` in `Training_Mode`
- All value reads go through `Training[0].contents[][]` / `Training[2].contents[][]`

**Key globals**: `Training[0..2]` struct (contents[player][group][slot]), `Training_Cursor`, `Decide_ID`, `Champion`, `New_Challenger`.

> **Scope recommendation**: Start with just `Training_Mode()` (4 items, trivial). Add Normal Training pause menu last — it runs mid-game and must not disrupt game loop timing.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_training_menus.h/.cpp` | Training selector + pause menu bindings |
| `assets/ui/training_mode.rml` + `training_mode.rcss` | Training mode selector |
| `assets/ui/training_hud.rml` + `training_hud.rcss` | In-game training pause menu overlay |

---

#### Phase 3.16 — Network Lobby ✅ IMPLEMENTED

**What**: LAN AUTO-CONN, LAN CONNECT (peer list), NET AUTO-CONN, CONNECT PEER, SEARCH MATCH, EXIT. Currently rendered using `effect_61` (6 items) with `SSPutStr2`-based peer list and popup overlays (`NetLobby_DrawIncomingPopup`, `NetLobby_DrawOutgoingPopup`). `Network_Lobby()` in `menu.c` (line 591).

**Current state**: The network lobby already has a dual-rendering path — it uses `SDLNetplayUI_SetNativeLobbyActive(true)` to switch between native (CPS3 sprite + `SSPutStr2`) and the ImGui lobby (`sdl_netplay_ui.cpp`). Phase 3.16 would replace the CPS3/`SSPutStr2` path with RmlUi — the ImGui lobby toggle mechanism already provides the scaffolding.

**Elements to replace**:
- 6 `effect_61` menu items (lobby actions)
- `SSPutStr2`-based peer name list (LAN discovered peers, Internet peers)
- `NetLobby_DrawIncomingPopup()` / `NetLobby_DrawOutgoingPopup()` — raw `Renderer_Queue2DPrimitive` + `SSPutStrPro` popups

**Bypass site**: In `Network_Lobby()` case 1 (phase 2, rebuild), gate the 6 `effect_61` items and let the existing `SDLNetplayUI_SetNativeLobbyActive` flag control which path renders.

> **Note**: The peer list and popup overlays use `Renderer_Queue2DPrimitive` directly (not the effect system). These can be replaced cleanly with RmlUi `<li>` elements and a modal overlay triggered by `SDLNetplayUI_HasPendingInvite()` / `HasOutgoingChallenge()`.

**Bindings**: `lobby_cursor` (`Menu_Cursor_Y[0]`), `lan_auto_connect` (`Config_GetBool(CFG_KEY_NETPLAY_AUTO_CONNECT)`), `lan_peers[]` (from `Discovery_GetPeers()`), `net_peers[]`, `has_incoming_challenge`, `has_outgoing_challenge`, `challenger_name`, `challenger_ping`.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_network_lobby.h/.cpp` | Lobby item + peer list + popup bindings |
| `assets/ui/network_lobby.rml` + `network_lobby.rcss` | Lobby list + peer panels + popup modal |

---

#### Phase 3.17 — VS Result Screen ✅ IMPLEMENTED

**What**: Post-session win/loss tally after a VS match ends — shows P1/P2 win counts, win percentages, and character portraits via animated bars. Entered via AT index 16 (`VS_Result()`), `menu.c` line 2595.

**Exact CPS3 mechanism**: `VS_Result()` case 1 spawns:
- `effect_A0_init(0, VS_Win_Record[N], N, 3, ...)` × 2 — animated win-count number widgets for each player
- `effect_A0_init(0, ave[N], 2/3, 3, ...)` × 2 — win percentage (0–100) animated widgets
- `effect_91_init(PL, ix, 0, 71, char_ix2, 0)` × 6 — character portrait thumbnails (3 per player)
- `effect_66_init(91, 12, ...)` — result panel background box
- `effect_66_init(138/139, 24/25, ...)` — two decorative background strips
- `Setup_Win_Lose_OBJ()` — spawns the "WIN" / "LOSE" text objects

**Input loop** (`VS_Result_Select_Sub`): case 4 — each player can choose to Rematch (→ `Setup_VS_Mode`), return to Mode Select, or exit to title. This is the only screen where both players simultaneously navigate.

**Bypass site** (case 1, after `Menu_Common_Init()`):
```c
if (use_rmlui && rmlui_screen_vs_result) {
    rmlui_vs_result_show(VS_Win_Record[0], VS_Win_Record[1], ave[0], ave[1]);
} else {
    effect_A0_init(0, VS_Win_Record[0], 0, 3, 0, 0, 0);
    // ... all effect_A0, effect_91, effect_66 spawns ...
    Setup_Win_Lose_OBJ();
}
```

**Key globals**: `VS_Win_Record[0]` / `VS_Win_Record[1]` — session win counts. `ave[0]` / `ave[1]` — computed win %; both calculated in-function before spawn site. `My_char[N]` → character name for portraits.

**Bindings**: `vs_p1_wins` / `vs_p2_wins` (`VS_Win_Record[0/1]`), `vs_p1_pct` / `vs_p2_pct` (computed percentages), `vs_p1_char` / `vs_p2_char` (`My_char[N]` name), `vs_result_phase` (case 4 sub-state for continue/rematch prompt).

**CSS design**: Centered split panel — P1 left half, P2 right half. Win count displayed as a large animated number (CSS `@keyframes count-up` counting from 0). Percentage shown as a horizontal bar fill. Character name in a portrait label band at top.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_vs_result.h/.cpp` | Win record + percentage bindings |
| `assets/ui/vs_result.rml` + `vs_result.rcss` | Split panel with animated win counts |

---

#### Phase 3.18 — Memory Card (Save/Load) ✅ IMPLEMENTED

**What**: The "Save/Load" option from the Option Menu — 4 items (LOAD DATA, SAVE DATA, DELETE DATA, EXIT) with a slot selector. `Memory_Card()` in `menu.c` (line 2125). AT index 13.

**Exact CPS3 mechanism**: Case 0 spawns:
- 4 `effect_61` row labels (char_index `0x15`–`0x18`, `master_player=1`, font `0x7047`): LOAD DATA, SAVE DATA, DELETE DATA, EXIT
- 1 `effect_64` value item (slot `0x61`, type 0, value col 2) — the save slot selector (slot 0 / slot 1 / slot 2)
- `effect_66_init(0x8A, 8, 2, 1, ...)` — background panel
- `effect_04_init(2, 2, 2, 0x48)` — cursor bar

Input handled by `Memory_Card_Sub(PL_id)`. Sub-cases 4–6 run `Save_Load_Menu()` — the actual card read/write progress screen.

**Bypass site** (case 0 spawns — gate all effect spawns):
```c
case 0:
    FadeOut(...);
    // ...
    if (use_rmlui && rmlui_menu_memory_card) {
        rmlui_memory_card_show();   // 4-item menu, slot selector
    } else {
        effect_61_init(...);  // × 4
        effect_64_init(...);
        effect_66_init(...);
        effect_04_init(...);
    }
```

> **Note**: Sub-cases 4–6 (`Save_Load_Menu`) show progress/confirmation dialogs using `SSPutStr` and further `effect_61` calls. These also need to be gated. However, since native saves (`NativeSave_*`) replace the original memory card path entirely in 3SXtra, the Save_Load_Menu code may never actually execute — verify this before implementing.

**Key globals**: `Menu_Cursor_Y[0]` (0–3 = LOAD/SAVE/DELETE/EXIT), `Convert_Buff[3][0][0]` = selected slot index (0–2).

**Bindings**: `mc_cursor` (`Menu_Cursor_Y[0]`), `mc_slot` (`Convert_Buff[3][0][0]`), `mc_phase` (sub-case for progress display).

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_memory_card.h/.cpp` | 4-item menu + slot selector bindings |
| `assets/ui/memory_card.rml` + `memory_card.rcss` | Save/load slot selection panel |

---

#### Phase 3.19 — Blocking Training Sub-Menus ✅ IMPLEMENTED

**What**: The Parrying Training mode variants — `Blocking_Training()` (the pause menu for parry training, 6 `effect_A3` items) and `Blocking_Tr_Option()` (the parry-specific option screen, 6+4 `effect_A3` items). Entered when `Training_Mode()` cursor = 1 (Parrying Training). `menu.c` lines 3327 and 3470.

**Exact CPS3 mechanism (`Blocking_Training`)**: Case 0 spawns 6 `effect_A3_init(1, 12, ix, ix, 0, 112, y, 0)` at y=72+16×ix — items are: Normal Training, Replay Training, Parry Training Option, Recording Setting, Button Config, Exit. Same `effect_A3` widget pattern as `Normal_Training()`.

**Exact CPS3 mechanism (`Blocking_Tr_Option`)**: Case 0 spawns:
- 2 header `effect_A3_init(1, 22, 99, 0/1, 1, 51, 56/106, 1)` — section headers
- 6 `effect_A3_init(1, 17, ix, ix, 1, 64, y, 0)` — left-column option labels (with non-uniform y spacing: gap after row 2 and row 4)
- 4 `effect_A3_init(1, group, ix, ix, 1, 264, y, 0)` — right-column values (groups 18–21)

Input via `Dummy_Move_Sub(task_ptr, Champion, 1, 0, 5)` — same as `Dummy_Setting()` but for parry-specific training data.

**Bypass strategy**: Identical to `Normal_Training()` / `Dummy_Setting()` (Phase 3.15) — gate `effect_A3_init` spawning, read values from `Training[1].contents[][]` (parry training uses index 1). The `Blocking_Tr_Option` right-column spacing irregularity (extra gaps at rows 2 and 4) must be reproduced in CSS via explicit `margin-top`.

**Key globals**: `Training[1].contents[][]` — parry training settings. `Training_Index = 1` (blocking) vs `3` (blocking options). `Champion` / `New_Challenger` — which player is the champion.

**Bindings**: `blocking_menu_cursor` (`Menu_Cursor_Y[0]`), `blocking_menu_items[]` (6 label strings from `training_letter_data[]` at indices relevant to parry), `blocking_opt_values[]` (`Training[1].contents` rows 17–21).

> **Scope note**: `Blocking_Training()` is essentially identical to `Normal_Training()` with different item labels. If Phase 3.15 is proven, `Blocking_Training()` is a near-copy with `Training_Index = 1`. Implement immediately after 3.15.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_training_menus.h/.cpp` | Extend existing file with blocking training bindings |
| `assets/ui/blocking_training.rml` + `blocking_training.rcss` | Parry training pause menu |
| `assets/ui/blocking_tr_option.rml` + `blocking_tr_option.rcss` | Parry training option screen |

---

#### Phase 3.20 — Replay Save / Load ✅ IMPLEMENTED

**What**: Save Replay (`Save_Replay()`, AT index 17) and Load Replay (`Load_Replay()`, AT index 6) screens. Both already use the `ReplayPicker` overlay (`replay_picker.h`/`.c`) for the file selection UI — replacing the original memory card file grid. `menu.c` lines 2752 and 1745.

**Current state**: Both screens call `ReplayPicker_Open(mode)` + `ReplayPicker_Update()` in their case 3 — the CPS3 sprite elements are minimal (only a background `effect_57` + `effect_66` staging panel set up in case 0 via `Setup_Replay_Sub()`). The actual replay list is shown by `ReplayPicker`, which uses `SSPutStr`-based rendering.

**Elements to replace**:
- `Setup_Replay_Sub()` spawns `effect_57` (animated panel) + `effect_66` (background box) — these can stay as background chrome
- `ReplayPicker` renders the file name list via `SSPutStr`/`SSPutStrPro` — **replace this renderer with an RmlUi document**

**Bypass approach**: Rather than gating at `Save_Replay()`/`Load_Replay()`, implement an RmlUi-based `replay_picker` renderer: when `use_rmlui`, `ReplayPicker_Open()` loads an RmlUi document instead of setting up its internal `SSPutStr` state. `ReplayPicker_Update()` then drives the RmlUi document's bindings. This isolates the change to `replay_picker.c`.

**Key globals** (in `replay_picker`): Slot list (names + timestamps from `NativeSave_GetReplaySlots()`), `ReplayPicker_GetSelectedSlot()` return value, picker mode (0=load, 1=save).

**Bindings**: `replay_slots[]` (array of `{slot_name, timestamp, is_empty}` structs), `replay_cursor` (selected slot index), `replay_mode` ("SAVE" or "LOAD" label), `replay_slot_count`.

> **Note**: This is the cleanest Phase 3 item — the existing `ReplayPicker` abstraction means the bypass is entirely inside one file (`replay_picker.c`), with zero changes to `menu.c` itself.

| File | Purpose |
|---|---|
| `src/port/ui/replay_picker.c` | Add RmlUi rendering path inside existing picker |
| `src/port/sdl/rmlui_replay_picker.h/.cpp` | Slot list data model + event callbacks |
| `assets/ui/replay_picker.rml` + `replay_picker.rcss` | File slot grid with save/load mode indicator |

---

#### Phase 3.21 — Arcade-Flow Text Overlays ✅ IMPLEMENTED

**What**: In-round text prompts rendered on the losing/idle player's side during arcade play: "CONTINUE?", "GAME OVER", "PRESS 1P/2P START", "PLEASE WAIT", and the personal countdown digit. These appear *while the winner continues fighting* — separate from the dedicated `continue.c` / `gameover.c` screens (which are already bypassed).

**Source file**: `entry.c` — 6 functions with 14 ungated `SSPutStr` / `SSPutDec` call sites:
- `Loser_Sub_1P()` / `Loser_Sub_2P()` — "CONTINUE?" on the loser's side (lines 912, 927)
- `Entry_Continue_Sub()` — "CONTINUE?" + `Disp_Personal_Count()` countdown digit (line 978)
- `Flash_Start()` — "PRESS 1P START" / "PRESS 2P START" blink cycle + fallback "CONTINUE?" (lines 1148–1179)
- `Flash_Please()` — "PLEASE WAIT" blink (line 1211)
- `In_Game_Sub()` — timed "GAME OVER" (line 1086)
- `In_Over_Sub()` — persistent "GAME OVER" (line 1121)

All calls are guarded by `save_w[1].extra_option.contents[3][5]` (PRESS START DISPLAY flag) — same flag already used by Phase 3.3 title screen.

**Bypass strategy**: Add `use_rmlui && rmlui_screen_entry_text` gates around each `SSPutStr`/`SSPutDec` call site. The RmlUi component reads the same variables (`E_Number`, `F_No1`, `F_Timer`, `Continue_Count`, `DE_X`, `LOSER`, `PL_id`) to determine which text to show.

**Bindings**: `entry_text_type` (enum: CONTINUE, GAME_OVER, PRESS_START, PLEASE_WAIT), `entry_pl_id` (0 or 1), `entry_countdown` (`Continue_Count[PL_id]`), `entry_visible` (blink state from `F_Timer`/`F_No1`).

> **Note**: These text prompts use a simple blink pattern identical to the title screen. CSS `@keyframes` can handle the blink entirely. The RmlUi document should be a single overlay positioned at `DE_X[PL_id]` equivalent screen coordinates.

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_entry_text.h/.cpp` | Per-player entry text state bindings |
| `assets/ui/entry_text.rml` + `entry_text.rcss` | Text overlay with CSS blink |

---

#### Phase 3.22 — Pause Text Overlay ✅ IMPLEMENTED

**What**: "1P PAUSE" / "2P PAUSE" flashing text and "Please reconnect the controller" message during game pause.

**Source files**:
- `menu_draw.c` — `Flash_1P_or_2P()` (line 83): Blink cycle using `SSPutStr2(20, 9, 9, "1P PAUSE")` / `"2P PAUSE"`, controlled by `task_ptr->r_no[3]` sub-state and `task_ptr->free[0]` timer (0x3C visible / 0x1E hidden frames).
- `pause.c` — `Flash_Pause_2nd()` (line 121): Same "1P/2P PAUSE" via `SSPutStr2` with `Pause_ID` determining which label.
- `pause.c` — `dispControllerWasRemovedMessage()` (line 151): Multi-line "Please reconnect the controller to controller port 1/2" via `SSPutStrPro`.

Zero `use_rmlui` gates in either file.

**Bypass strategy**: Add `use_rmlui && rmlui_screen_pause` gates in `Flash_1P_or_2P()` and `Flash_Pause_2nd()` around the `SSPutStr2` calls. CSS-driven blink animation (same pattern as title screen). Controller disconnect message is a separate static text overlay shown/hidden by `Flash_Pause_4th()` state.

**Bindings**: `pause_player_id` (`Pause_ID`), `pause_visible` (blink state), `controller_disconnected` (Flash_Pause_4th active), `disconnect_port` (1 or 2).

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_pause_overlay.h/.cpp` | Pause text + controller disconnect bindings |
| `assets/ui/pause.rml` + `pause.rcss` | Pause text with CSS blink + disconnect message |

---

#### Phase 3.23 — Trial Mode HUD ✅ IMPLEMENTED

**What**: Trial combo step list, header, gauge alert, and "COMPLETE!" flash — the in-game overlay shown during Trials training mode.

**Source file**: `trials.c` — `trials_draw()` (line 213), 5 `SSPutStrPro_Scale` call sites:
- Header: character name + "Trial N" (line 234)
- "MAX GAUGE" alert when gauge is full (line 237)
- Per-step list: step display names with color coding (green=done, yellow=current, white=pending) (line 258)
- "kadai" (challenge count) sub-label (line 265)
- "COMPLETE!" flash with color animation on trial success (line 273)

**Key globals**: `g_trials_state` struct — `current_trial`, `current_step`, `completed`, `step_status[]`. `trials_data.inc` provides the trial definitions.

**Bypass strategy**: Add `use_rmlui && rmlui_screen_trials` gate at the top of `trials_draw()`. The RmlUi document renders the step list as a vertical `<div>` list with per-step `data-class-completed` / `data-class-active` bindings. "COMPLETE!" uses CSS `@keyframes` flash.

**Bindings**: `trial_char_name`, `trial_number` (1-indexed), `trial_steps[]` (array of `{name, status}`), `trial_complete` (bool), `trial_max_gauge` (bool).

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_trials_hud.h/.cpp` | Trial step list + completion state bindings |
| `assets/ui/trials_hud.rml` + `trials_hud.rcss` | Step list overlay with color-coded progress |

---

#### Phase 3.24 — Training Stun Counter ✅ IMPLEMENTED

**What**: "STUN: N" text counter shown per-player during training mode, displaying accumulated combo stun.

**Source file**: `training_hud.c` — `training_hud_draw_stun()` (line 20): Single `SSPutStr_Bigger(hud_x, hud_y, 5, stun_str, 1.0f, 0, 1.0f)` call at coordinates `(10, 60)` for P1 and `(250, 60)` for P2.

Guarded by `g_training_menu_settings.show_stun` — already has a user toggle. Only renders when `Mode_Type == MODE_NORMAL_TRAINING || MODE_TRIALS`.

**Bypass strategy**: Trivial — add `use_rmlui && rmlui_hud_training_stun` gate around the `SSPutStr_Bigger` call. Can be folded into the existing `rmlui_game_hud` component (Phase 3.1) as an additional binding, or into the training HUD document.

**Bindings**: `p1_combo_stun` / `p2_combo_stun` (`g_training_state.p1.combo_stun` / `.p2.combo_stun`).

> **Scope note**: This is the simplest remaining gap — one `SSPutStr_Bigger` call. Could be a 15-minute addition to an existing component.

| File | Purpose |
|---|---|
| Extend `rmlui_game_hud.cpp` or `rmlui_training_menus.cpp` | Add stun counter binding |
| Extend `game_hud.rml` or `training_hud.rml` | Add stun counter text element |

---

#### Phase 3.25 — Win Counter & Copyright Text ✅ IMPLEMENTED

**What**: In-game "WIN"/"WINS" counter (HUD element) and boot-screen copyright notice.

**Source file**: `sys_sub.c`:
- `Disp_Win_Record()` (line 344) → `Disp_Win_Record_Sub()` (line 400): Renders "WIN"/"WINS" label + digit count via `SSPutStr` + `SSPutDec` at HUD positions `zz=5` (P1) or `zz=43` (P2). Guarded by `omop_cockpit`. Called from `game.c:Disp_Cockpit` block.
- `Disp_Copyright()` (line ~1773): Renders boot copyright text via `SSPutStrPro`. Appears once at game startup.

**Win counter bypass check**: `Disp_Win_Record()` is called from the `Disp_Cockpit` block in `game.c`, which already has a `use_rmlui && rmlui_hud_*` gate pattern. **Verify** whether `Disp_Win_Record()` is inside an existing bypass — if so, this is already covered.

**Copyright bypass**: Low priority — the copyright text appears only once at boot and is nearly invisible. Could be gated or left as-is.

**Bindings**: `p1_win_count` / `p2_win_count` (`Win_Record[N]` or `VS_Win_Record[N]`), `win_mode` (arcade vs VS).

| File | Purpose |
|---|---|
| Extend `rmlui_game_hud.cpp` | Add win record binding (may already be partially covered) |
| Extend `game_hud.rml` | Add win count display element |

---

#### Phase 3.26 — Network Lobby Peer List & Popups ✅ IMPLEMENTED

**What**: The `SSPutStr_Bigger` / `SSPutStr2` / `SSPutStrPro` rendering for peer discovery lists, connection status text, and challenge popups within `Network_Lobby()`. Phase 3.16 bypassed only the 6 `effect_61` menu items — the dynamic peer rendering remains ungated.

**Source file**: `menu.c` — `Network_Lobby()` phases 4–8 (lines 862–1000+):
- **LAN/NET toggle values**: `SSPutStr_Bigger` for "ON"/"OFF" labels (lines 872–879)
- **Section headers**: "LAN DISCOVERED", "INTERNET PEERS" (lines 887, 893)
- **Peer names**: LAN peer name/IP + NET peer name/status (lines 912–943)
- **Status bar**: Connection status text at bottom (lines 969–999)
- **Incoming popup**: `NetLobby_DrawIncomingPopup()` — "INCOMING CHALLENGE!" + challenger name + ping + ACCEPT/DECLINE (`SSPutStr2`/`SSPutStrPro`, lines 489–521)
- **Outgoing popup**: `NetLobby_DrawOutgoingPopup()` — "CONNECTING..." + target name + ping + CANCEL (lines 577–603)

Partially gated: some calls are behind `if (!use_rmlui || !rmlui_menu_lobby)` but many (especially the popup draws and peer list) are NOT.

**Bypass strategy**: Extend the existing `rmlui_network_lobby.cpp` component to include bindings for peer list data (names, IPs, ping), connection status, and popup state. Gate all remaining `SSPutStr_Bigger` calls in phases 4–8 with `use_rmlui && rmlui_menu_lobby`.

**Bindings** (extend existing): `lan_peers[]` (array of `{name, ip, ping}`), `net_peers[]`, `connection_status` (string), `incoming_popup_visible`, `outgoing_popup_visible`, `popup_challenger_name`, `popup_ping`.

| File | Purpose |
|---|---|
| Extend `rmlui_network_lobby.cpp` | Add peer list array + popup state bindings |
| Extend `network_lobby.rml` + `network_lobby.rcss` | Add peer list panels + popup modal |

---

#### Phase 3.27 — Name Entry / Ranking Screen ✅ IMPLEMENTED

**What**: Arcade-mode name entry (3-letter initials for high score) and ranking board display. Uses CPS3 sprite-based keyboard and ranking list.

**Source file**: `entry.c` — `Name_Input()` (called at line 753 from `Entry_Main_Sub` case 2). The name entry screen uses `rank_name_w[PL_id].code[]` for input state, and `Ranking_Data[]` for the ranking table. Display uses CPS3 effect objects and `SSPutStr`-based text.

**Ranking display**: `ranking.c` likely handles the visual ranking board — not yet audited in detail.

> **Priority**: This is the lowest-priority gap. Name entry only appears in arcade mode after a game-over with a high score — most modern players never see it. The ranking display is similarly niche.

**Bypass strategy**: Gate `Name_Input()`'s sprite rendering. RmlUi document would show a 3-character selector (A–Z, 0–9 grid) with cursor navigation via data bindings.

**Bindings**: `name_chars[3]` (current entered characters), `name_cursor_x` / `name_cursor_y` (grid position), `ranking_entries[]` (top-5 array of `{rank, name, score, character}`).

| File | Purpose |
|---|---|
| `src/port/sdl/rmlui_name_entry.h/.cpp` | Name input grid + ranking table bindings |
| `assets/ui/name_entry.rml` + `name_entry.rcss` | Character grid selector + ranking display |

---

#### Implementation Order

Phase 3.1 ✅ (Fight HUD) → 3.2 ✅ (Mode Menu) → 3.9 ✅ (Option Menu) → 3.10 ✅ (Game Option) → 3.3 ✅ (Title Screen) → 3.4 ✅ (Winner/Loser) → 3.5 ✅ (Continue) → 3.6 ✅ (Game Over) → 3.17 ✅ (VS Result) → 3.18 ✅ (Memory Card) → 3.12 ✅ (Sound) → 3.14 ✅ (Extra Option) → 3.13 ✅ (System Direction) → 3.15 ✅ (Training Selector) → 3.19 ✅ (Blocking Training) → 3.11 ✅ (Button Config) → 3.16 ✅ (Network Lobby) → 3.20 ✅ (Replay Save/Load) → 3.7 ✅ (Character Select) → 3.8 ✅ (VS Screen) → 3.24 ✅ (Training Stun) → 3.22 ✅ (Pause Text) → 3.21 ✅ (Arcade-Flow Text) → 3.25 ✅ (Win Counter/Copyright) → 3.23 ✅ (Trial HUD) → 3.26 ✅ (Lobby Peer List) → 3.27 ✅ (Name Entry)

**Progress**: 27 of 27 sub-phases implemented. ✅ Phase 3 COMPLETE.

**Rationale**: Phases 3.1–3.20 follow the original rationale. The 7 new items are ordered by complexity: Training Stun (3.24) is trivial (1 SSPutStr call, foldable into existing HUD). Pause Text (3.22) is simple and high-visibility. Arcade-Flow Text (3.21) has more call sites but uses the same CSS blink pattern. Win Counter (3.25) may already be partially gated — needs verification. Trial HUD (3.23) has dynamic step lists. Lobby Peer List (3.26) extends an existing component but has 50+ rendering calls. Name Entry (3.27) is lowest priority — arcade-only, rarely seen.

#### Phase 3 — Shared Implementation Patterns

**Dirty-check macro** (from Phase 2 pattern — apply to all Phase 3 bindings):
```cpp
#define DIRTY_IF_CHANGED(model, name, old_val, new_val) \
    if ((old_val) != (new_val)) { (old_val) = (new_val); (model).DirtyVariable(name); }
```

**Effect object skip pattern**: For screens like `Win_2nd`, `Continue`, `GameOver` where `effect_76` text objects are spawned at a specific phase entry, bypass at the `spawn_effect_76()` call site — not inside each effect's implementation. This avoids modifying effect system internals.

**Transition timing**: RmlUi documents for screen-transition events (Winner, GameOver) should be shown *after* `Switch_Screen_Revival()` completes — not simultaneously with `spawn_effect_76()`. Match the timing to avoid documents appearing under wipe overlays.

**Per-screen init/shutdown**: Each Phase 3 component follows the same lifecycle as Phase 2 menus:
```cpp
void rmlui_game_hud_init();      // called on game start → CreateDataModel, LoadDocument
void rmlui_game_hud_update();    // called once per frame (dirty-check only)
void rmlui_game_hud_shutdown();  // called on game end → document->Close()
```

---

## 6. Renderer Backend Matrix

| 3SXtra Renderer | RmlUi Backend | Header | Status |
|---|---|---|---|
| OpenGL (`--renderer gl`) | `RenderInterface_GL3` | `RmlUi_Renderer_GL3.h` | ✅ Integrated |
| SDL GPU (`--renderer gpu`) | `RenderInterface_SDL_GPU` | `RmlUi_Renderer_SDL_GPU.h` | ✅ Integrated |
| SDL2D (`--renderer sdl`) | `RenderInterface_SDL` | `RmlUi_Renderer_SDL.h` | ✅ Integrated |

All three renderer backends are now supported. The wrapper (`rmlui_wrapper.cpp`) selects the appropriate `RenderInterface` at init time based on `SDLApp_GetRenderer()`.

---

## 7. Verification

### Build

```bash
# Standard build (compile.bat handles everything)
.\compile.bat
```

### Phase 1–2 Runtime Testing Checklist

- [ ] `3sx.exe` (default) — ImGui menus work as before
- [ ] `3sx.exe --ui rmlui` — game loads, test overlay visible in top-right
- [ ] `3sx.exe --ui rmlui` + F3 — mods menu renders with checkboxes, labels, dividers
- [ ] Checkbox toggles work (click changes state + game effect)
- [ ] Debug options greyed out when not in-game, active during gameplay
- [ ] `3sx.exe --help` — shows `--ui` option
- [ ] Game renders normally in both modes (no visual corruption)

### Phase 3 Runtime Testing (per sub-phase)

Each Phase 3.x sub-phase follows this pattern:

1. `.\compile.bat` — clean build
2. `3sx.exe` — vanilla/ImGui mode unchanged (regression test)
3. `3sx.exe --ui rmlui` — RmlUi mode with the new screen
4. Visual correctness: RmlUi elements match game state
5. State machine correctness: Transitions work identically (same `G_No` flow)
6. Edge cases per screen (timeout, perfect, stun, etc.)
7. Per-component toggle test: disable individual RmlUi components → CPS3 fallback renders

---

## 8. Risk Assessment

| Risk | Severity | Mitigation |
|---|---|---|
| **Glad symbol conflicts** | ~~Critical~~ Resolved | Used `RMLUI_GL3_CUSTOM_LOADER` — the official mechanism |
| **FreeType dependency** | ~~Low~~ Resolved | Auto-detected on MSYS2; cross-built from source for RPi4 aarch64 |
| **Binary size increase** | Low | ~1.5 MB — acceptable |
| **GL state conflicts** | Medium | RmlUi GL3 renderer saves/restores all modified GL state |
| **Dual-mode maintenance** | Medium | Both modes share same game state — only presentation differs |
| **Graph/Plot widget gap** | Medium | Custom element needed for ping history if migrating netplay UI |
| **Bypassing scfont breaks logic** | Low | Each subsystem's logic functions remain untouched. Only rendering calls are skipped. No state machine globals change |
| **CPS3/RmlUi timing mismatch** | Low | RmlUi update runs per-frame in the same loop. Data bindings read the same globals as sprite rendering |
| **Effect objects behind RmlUi** | Medium | Text-label `effect_76`/`effect_L1` spawns also skipped in RmlUi mode. Non-text effects (char animations, BG) continue normally |
| **Character Select complexity** | High | Treat as overlay-first (Phase 3.7) — replace text only, keep portrait sprites. Full replacement is optional stretch goal |
| **Regression in ImGui mode** | Low | All bypasses gated on `use_rmlui`. Default mode is completely unchanged |



### Phase 3 Implementation Notes (Lessons Learned)

**Header dependencies**: Not all game globals live in `workuser.h`. Audio globals (`bgm_level`, `se_level`) are in `sound3rd.h`. Always verify with `rg "extern.*varname"` before assuming `workuser.h` covers it.

**VS_Result percentage calculation**: The win percentage logic uses integer-only math with a 0-clamp-to-1 rule. The bypass must compute percentages *before* the gate (they’re passed to `rmlui_vs_result_show()` as arguments) so the calculation stays in `menu.c` and isn’t duplicated in C++.

**Memory Card case 0 structure**: Unlike simpler screens, `Memory_Card()` mixes `Order[]` writes (needed for the state machine) with `effect_*` spawns (visual only). The gate wraps only the visual effects; `Order[]` and `Setup_File_Property()` remain unconditional.

**Sound_Test case 0**: Uses 4 different effect types (`effect_57`, `effect_04`, `effect_64`, `effect_A8`, `effect_61`) — the most complex single-case bypass so far. All are pure visual; `Convert_Buff[]` init and `Menu_Cursor_Move` run unconditionally.

**Build system pattern**: Each new component needs 3 integration points in `sdl_app.c` (include, init, update) and 1 in `CMakeLists.txt` (source file). No shutdown calls are needed — `rmlui_wrapper_shutdown()` handles document cleanup.

**Toggle naming convention**: Screen overlays use `rmlui_screen_*` (e.g., `rmlui_screen_winner`), menus use `rmlui_menu_*` (e.g., `rmlui_menu_sound`). All defined in `rmlui_phase3_toggles.h`, declared in `rmlui_game_hud.cpp`.

**Extra Option data model (Phase 3.14)**: The `Ex_Title_Data[4][7]` and `Ex_Letter_Data[4][7][17]` arrays contain trailing `/....` separators in labels. The data model strips these at bind time with `rfind('/')`. Values are fetched via `save_w[1].extra_option.contents[Menu_Page][row]` and displayed by indexing into `Ex_Letter_Data[page][row][value]`. A cache struct (`ExtraOptionCache`) tracks `{page, cursor, values[8]}` to avoid unnecessary model dirtying.

**Type mismatches across extern declarations (Phase 3.15)**: `Training_Index` is declared as `u8` in `workuser.h` but was erroneously declared as `extern s16` in the new .cpp. Since `workuser.h` is already included via `structs.h`, redundant extern declarations should be removed entirely — let the header provide the canonical type.

**Unified component for related screens (Phase 3.15+3.19)**: `rmlui_training_menus.h/.cpp` handles 6 screens (Training Mode, Normal Training, Dummy Setting, Training Option, Blocking Training, Blocking Tr Option) in a single data model with separate show/hide function pairs. This avoids 6 separate data model registrations and reduces init/update overhead. Labels are hardcoded in RML documents rather than dynamically bound — they never change.

**Char Select / VS Screen as text overlays (Phase 3.7+3.8)**: These have deep sprite coupling (portrait animations, stage backgrounds). The initial implementation overlays only text elements: `Select_Timer` for char select, P1/P2 character names (via `plw[N].wu.char_index` → local roster name table) for VS screen. Portrait sprite replacement is a stretch goal.

**Correct struct member names**: `WORK.char_no` doesn't exist — it's `WORK.char_index` (s16, line 381 of structs.h). `Timer_Value` doesn't exist — the select screen timer is `Select_Timer` (s8, line 76 of workuser.h). Always verify struct members with `grep_search` on the header before referencing them.

**Batch implementation (Phases 3.11–3.20, 3.7–3.8)**: Once the pattern is proven, multiple components can be created in a single session. The skeleton for each is: (1) `.h` header with `extern "C"` init/update/show/hide/shutdown, (2) `.cpp` with `CreateDataModel` + `BindFunc` + `DirtyVariable`, (3) `.rml` with `data-model` + `data-class-focused`, (4) `.rcss` with panel layout + `.focused` highlight + accent color theme. Integration is mechanical: 3 lines in `sdl_app.c`, 1 line in `CMakeLists.txt`, 1 include + 2 bypass gates in `menu.c`.

**Dual-context architecture (Scaling/Bezel fix)**: A single RmlUi context at window resolution caused Phase 3 screens to render tiny (384dp body in a ~1024px window) and overlap bezels. The fix: two RmlUi contexts — a **game context** (384×224, dp_ratio=1.0) that renders into the canvas FBO (so menus scale with the game and sit under bezels), and a **window context** (win_w×win_h) for Phase 2 overlays. Phase 3 components use `rmlui_wrapper_get_game_context()` / `rmlui_wrapper_show_game_document()`, Phase 2 uses the original `rmlui_wrapper_get_context()` / `rmlui_wrapper_show_document()`. Text renders at 384×224 resolution (matching CPS3 quality), which benefits from shader upscaling. Canvas FBO access: `gl_state.cps3_canvas_fbo` (GL3), `SDLGameRendererSDL_GetCanvas()` (SDL2D), `SDLGameRendererGPU_GetCanvasTexture()` (GPU).
