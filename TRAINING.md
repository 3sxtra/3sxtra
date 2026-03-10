# Training Mode — Architecture & Status

## Overview

Training mode uses a **three-layer architecture**. The native CPS3 menu system
is preserved untouched; all modded features live in a parallel Lua/RmlUI layer.

```
┌─────────────────────────────────────────────────┐
│  Layer 3: Lua Training Engine (effie modules)   │
│  training_main.lua → dummy_control, recording,  │
│  prediction, hud, training modes                │
├─────────────────────────────────────────────────┤
│  Layer 2: RmlUI Menus & Data Bindings           │
│  F7 = training.rml (DUMMY / DISPLAY tabs)       │
│  Pause skins = rmlui_training_menus.cpp         │
│  Trials HUD  = rmlui_trials_hud.cpp            │
├─────────────────────────────────────────────────┤
│  Layer 1: Native CPS3 Menu System               │
│  menu.c → Training_Mode selector                │
│  menu_input.c → Dummy_Move_Sub, cursor logic    │
│  Original 5-item Dummy Setting menu preserved   │
└─────────────────────────────────────────────────┘
```

## Data Flow

```
F7 key
  → SDLApp_ToggleTrainingMenu()          [sdl_app.c]
  → toggle_overlay("training", pause=true)
  → training.rml rendered (DUMMY / DISPLAY tabs)
  → Data model "training" bindings       [rmlui_training_menu.cpp]
  → Writes directly to g_dummy_settings  [training_dummy.h]
  → Lua reads via engine.get_dummy_settings()  [lua_engine_bridge.cpp]
  → training_main.lua:map_dummy_settings()
  → dummy_control.lua applies behavior
```

## What Was Done

### Phase 1 — Native Menu Rewrite

The upstream CPS3 Dummy Setting menu (crowded-street/3sx) had 6 items:
ACTION, GUARD, QUICK STANDING, STUN, DEFAULT SETTING, EXIT.
Commit `08f888ac` rewrote this to 8 items with fighting-game-standard names:
ACTION, BLOCK TYPE, PARRY TYPE, STUN MASH, WAKEUP MASH, WAKEUP REVERSAL,
GUARD DIRECTION, DEFAULT SETTING.

**Key values:**
- `Menu_Max_Data_Tr[0][0]` = `{4, 4, 4, 6, 6, 0}` (rewritten from upstream `{4, 6, 2, 2, 0, 0}`)
- `Letter_Data_A3` expanded from `[23][8]` → `[26][8]` with matching labels
- `Dummy_Move_Sub(…, 0, 0, 7)` — 8 items (upstream was `max=5`, 6 items)
- `sync_dummy_settings_from_menu()` reads indices 1-4 for block/parry/mash
- Extended settings (tech throws, fast wakeup, guard direction, wakeup reversal)
  live exclusively in the RmlUI layer

### Phase 2 — F7 Modded Training Menu

Replaced the simple 9-checkbox F7 overlay with a tabbed menu:

| Tab | Settings | Interaction |
|-----|----------|-------------|
| **DUMMY** | Block, Parry, Stun Mash, Wakeup Mash, Tech Throw, Fast Wakeup, Wakeup Reversal, Guard Low | Click-to-cycle for enums, checkbox for bools |
| **DISPLAY** | Hitboxes (master + sub-toggles), Frame Advantage, Stun Timer, Input History, Frame Meter | Checkbox toggles |

- All settings persist to INI config across sessions
- Game freezes when menu is open
- Labels come from `_str()` functions co-located with enums in `training_dummy.h`

## Key Files

| File | Role |
|------|------|
| `src/sf33rd/Source/Game/training/training_dummy.h` | Enum definitions + `_str()` / `_count()` label functions |
| `src/sf33rd/Source/Game/training/training_dummy.c` | C dummy behavior (block, parry, mash, reversal) |
| `src/port/sdl/rmlui/rmlui_training_menu.cpp` | RmlUI data model, config persistence, click cycling |
| `src/port/sdl/rmlui/lua_engine_bridge.cpp` | `engine.get_dummy_settings()` → Lua bridge |
| `src/lua/training_main.lua` | `map_dummy_settings()` → wires C struct to Lua dummy |
| `build_tests/assets/ui/training.rml` | F7 menu template (DUMMY / DISPLAY tabs) |
| `build_tests/assets/ui/training.rcss` | CPS3-inspired dark theme styling |
| `src/sf33rd/Source/Game/menu/menu_input.c` | Native menu cursor logic, guarded sync function |
| `src/sf33rd/Source/Game/menu/menu.c` | Native menu state machines (untouched) |
| `src/port/config/config.h` | `CFG_KEY_DUMMY_*` persistence keys |

### Phase 3 Tier 1 — HUD Overlay Infrastructure

Built the always-on RmlUI overlay that effie's `hud.lua` will render into:

| File | Role |
|------|------|
| `rmlui_training_hud.h/.cpp` | Data model `"training_hud"` with 9 text fields + dirty check |
| `effie_hud.rml` | Always-on overlay (P1 left / P2 right) with life, meter, stun, advantage |
| `effie_hud.rcss` | CPS3-inspired text colors (green life, blue meter, orange stun) |
| `lua_engine_bridge.cpp` | Added `engine.set_hud_text(field, value)` + `engine.set_hud_gauge(field, fill)` |

### Phase 3 Tier 1 — Active HUD Data

Removed C stun counter (no-op stub for ABI compat). Wired live data push:

| Data | Source | RmlUI Field |
|------|--------|-------------|
| Life | `gamestate.P1/P2.life` | `p1_life` / `p2_life` |
| Meter | `gamestate.P1/P2.meter_gauge` | `p1_meter` / `p2_meter` |
| Stun | `gamestate.P1/P2.stun_bar` | `p1_stun` / `p2_stun` |
| Frame Advantage | `frame_advantage.lua` tracker | `p1_advantage` / `p2_advantage` + `advantage_class` |

## What Remains

### Immediate — Visual Verification
- Launch game → enter training mode → confirm HUD text appears
- Check z-order (text should be on top of game render)

### Phase 3 Tier 2: Gauge Bars
- Parry timing, charge moves, denjin, air combo timer → `<div>` with width-bound fill

### Phase 3 Tier 3: Position-Tracked Elements
- Stun timer above head, red parry miss, blocking direction → absolute positioning

### Phase 4: Training Modes
- Defense, Footsies, Jump-ins modules from effie's Lua
- RmlUI sub-pages + session stats

### Phase 5: Recording System
- `recording.lua` (input recording/playback, slot management) → RmlUI panel

## Guards & Safety

| Guard | Location | Purpose |
|-------|----------|---------|
| `g_lua_dummy_active` | `com_pl.c:191,206` | Gates C dummy override in `CPU_Sub()` |
| `g_lua_dummy_active` | `menu_input.c:2359` | Skips `sync_dummy_settings_from_menu()` |
| `Menu_Max_Data_Tr[2][2][6]` | `menu_input.c:2340` | Rewritten from upstream CPS3 — controls cursor bounds for dummy setting values |
| `Config_HasKey()` | `rmlui_training_menu.cpp` | Safe config load on init |

## Test Coverage

- `test_menu_bridge` — 3/3 (struct packing, SHM init, step gate)
- `test_trials` — 2/2 (navigation, validation flow)
- `test_stun` — 9/9 (STUN networking, socket safety)
- `test_smoke` — 1/1 (basic assertion)
