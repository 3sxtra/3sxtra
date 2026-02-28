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
┌─────────────────────────────────────────────┐
│  Original CPS3 Menu System (menu.c)          │ ← Untouched
├─────────────────────────────────────────────┤
│  ImGui Overlay Menus (default)               │ ← Active when ui-mode=imgui
│  mods_menu, shader_menu, training_menu, etc. │
├────────────────────── OR ────────────────────┤
│  RmlUi Overlay Menus                         │ ← Active when ui-mode=rmlui
│  .rml documents + .rcss stylesheets          │
│  Data binding to game state                  │
├─────────────────────────────────────────────┤
│  Port-Side SDL Text Renderer                 │ ← Always active (debug HUD)
└─────────────────────────────────────────────┘
```

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
| `assets/ui/base.rcss` | Global RmlUi theme stylesheet |
| `assets/ui/test.rml` | Phase 1 test document (top-right overlay panel) |
| `assets/ui/mods.rml` | Mods menu HTML document with data bindings (~137 lines) |
| `assets/ui/mods.rcss` | Mods menu stylesheet — glass panel, form controls (~130 lines) |

### Modified Files

| File | Change |
|---|---|
| `CMakeLists.txt` | `add_subdirectory` for RmlUi, backend sources with `RMLUI_GL3_CUSTOM_LOADER`, link `rmlui` library |
| `build-deps.sh` | Added RmlUi 6.2 clone into `third_party/rmlui` |
| `tools/batocera/rpi4/download-deps_rpi4.sh` | Added RmlUi 6.2 clone for cross-compile |
| `src/port/config.h` | Added `CFG_KEY_UI_MODE` |
| `src/port/cli_parser.c` | Added `--ui <imgui\|rmlui>` argument |
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

### Phase 2 — Menu Migration (in progress)

Port each ImGui overlay menu to an `.rml` + `.rcss` document pair with C++ data bindings.

- [x] Add data binding infrastructure to `rmlui_wrapper` (`get_context()` accessor)
- [x] Mods menu → `rmlui_mods_menu.cpp` + `mods.rml` + `mods.rcss`
- [ ] Shader menu
- [ ] Stage config
- [ ] Training overlay
- [ ] Input display
- [ ] Frame display
- [ ] Control mapping
- [ ] Netplay UI

#### Key Technical Finding: Data Model API

RmlUi 6.2 uses `ctx->CreateDataModel("name")` (not `GetDataModelConstructor`). The returned `DataModelConstructor` accepts `BindFunc()` for getter/setter lambdas — ideal for bridging C globals (`ModdedStage_*`, `Debug_w[]`, etc.) to the declarative HTML/CSS UI.

The mods menu uses 17 `BindFunc` bindings with per-frame dirty-checking to sync external state changes.

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

#### Cross-Compilation: Freetype

RmlUi requires Freetype for font rendering. On MSYS2/Windows it's auto-detected, but for RPi4 aarch64 cross-compilation, Freetype must be built from source:
- Added to `download-deps_rpi4.sh` — downloads + cross-compiles Freetype 2.13.3 (static, minimal)
- `rebuild-and-deploy.sh` passes `-DFreetype_ROOT` pointing to the cross-built library

> **Note**: RmlUi lacks a direct `ImGui::PlotLines` equivalent. Netplay ping graphs will require a custom RmlUi element or canvas-based approach.

### Phase 3 — Advanced (future, optional)

- In-game HUD overlay documents (health bars, timer, combos)
- Replacement of the CPS3 `menu.c` state machine with RmlUi documents
- Custom RmlUi decorators for SF3 visual effects

---

## 6. Renderer Backend Matrix

| 3SXtra Renderer | RmlUi Backend | Header | Status |
|---|---|---|---|
| OpenGL (`--renderer gl`) | `RenderInterface_GL3` | `RmlUi_Renderer_GL3.h` | ✅ Integrated |
| SDL GPU (`--renderer gpu`) | `RenderInterface_SDL` | `RmlUi_Renderer_SDL.h` | ⬜ Future |
| SDL2D (`--renderer sdl`) | `RenderInterface_SDL` | `RmlUi_Renderer_SDL.h` | ⬜ Future |

Currently defaults to the GL3 renderer. GPU and SDL2D backends will be added when needed (they don't have the glad conflict issue).

---

## 7. Verification

### Build

```bash
# Standard build (compile.bat handles everything)
.\compile.bat
```

### Runtime Testing Checklist

- [ ] `3sx.exe` (default) — ImGui menus work as before
- [ ] `3sx.exe --ui rmlui` — game loads, test overlay visible in top-right
- [ ] `3sx.exe --ui rmlui` + F3 — mods menu renders with checkboxes, labels, dividers
- [ ] Checkbox toggles work (click changes state + game effect)
- [ ] Debug options greyed out when not in-game, active during gameplay
- [ ] `3sx.exe --help` — shows `--ui` option
- [ ] Game renders normally in both modes (no visual corruption)

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
